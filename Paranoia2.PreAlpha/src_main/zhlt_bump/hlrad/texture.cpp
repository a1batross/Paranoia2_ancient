#include "qrad.h"

int g_numtextures;
radtexture_t *g_textures;

typedef struct waddir_s
{
	struct waddir_s *next;
	char path[_MAX_PATH];
} waddir_t;
waddir_t *g_waddirs = NULL;

void AddWadFolder (const char *path)
{
	waddir_t *waddir;
	waddir = (waddir_t *)malloc (sizeof (waddir_t));
	hlassume (waddir != NULL, assume_NoMemory);
	{
		waddir_t **pos;
		for (pos = &g_waddirs; *pos; pos = &(*pos)->next);
		waddir->next = *pos;
		*pos = waddir;
	}
	safe_snprintf (waddir->path, _MAX_PATH, "%s", path);
}

typedef struct
{
	int filepos;
	int disksize;
	int size;
	char type;
	char compression;
	char pad1, pad2;
	char name[16];
} lumpinfo_t;

typedef struct wadfile_s
{
	struct wadfile_s *next;
	char path[_MAX_PATH];
	FILE *file;
	int filesize;
	int numlumps;
	lumpinfo_t *lumpinfos;
} wadfile_t;

wadfile_t *g_wadfiles = NULL;
bool g_wadfiles_opened;

radtexture_t *LookupTexture(const char *name)
{
	for (int i = 0; i < g_numtextures; i++ )
	{
		if (!stricmp(g_textures[i].name, name))
		{
			return &g_textures[i];
		}
	}
	return NULL;
}

static int CDECL lump_sorter_by_name (const void *lump1, const void *lump2)
{
	lumpinfo_t *plump1 = (lumpinfo_t *)lump1;
	lumpinfo_t *plump2 = (lumpinfo_t *)lump2;
	return strcasecmp (plump1->name, plump2->name);
}

void OpenWadFile (const char *name, bool fullpath = false)
{
	int i;
	wadfile_t *wad;
	wad = (wadfile_t *)malloc (sizeof (wadfile_t));
	hlassume (wad != NULL, assume_NoMemory);
	{
		wadfile_t **pos;
		for (pos = &g_wadfiles; *pos; pos = &(*pos)->next)
			;
		wad->next = *pos;
		*pos = wad;
	}
   if (fullpath)
   {
	safe_snprintf (wad->path, _MAX_PATH, "%s", name);
	wad->file = fopen (wad->path, "rb");
	if (!wad->file)
	{
		Error ("Couldn't open %s", wad->path);
	}
   }
   else
   {
	waddir_t *dir;
	for (dir = g_waddirs; dir; dir = dir->next)
	{
		safe_snprintf (wad->path, _MAX_PATH, "%s\\%s", dir->path, name);
		if (wad->file = fopen (wad->path, "rb"))
		{
			break;
		}
	}
	if (!dir)
	{
		Fatal (assume_COULD_NOT_LOCATE_WAD, "Could not locate wad file %s", name);
		return;
	}
   }
	Log ("Using Wadfile: %s\n", wad->path);
	wad->filesize = q_filelength (wad->file);
	struct
	{
		char identification[4];
		int numlumps;
		int infotableofs;
	} wadinfo;
	if (wad->filesize < sizeof (wadinfo))
	{
		Error ("Invalid wad file '%s'.", wad->path);
	}
	SafeRead (wad->file, &wadinfo, sizeof (wadinfo));
	wadinfo.numlumps  = LittleLong(wadinfo.numlumps);
	wadinfo.infotableofs = LittleLong(wadinfo.infotableofs);
	if (strncmp (wadinfo.identification, "WAD2", 4) && strncmp (wadinfo.identification, "WAD3", 4))
		Error ("%s isn't a Wadfile!", wad->path);
	wad->numlumps = wadinfo.numlumps;
	if (wad->numlumps < 0 || wadinfo.infotableofs < 0 || wadinfo.infotableofs + wad->numlumps * sizeof (lumpinfo_t) > wad->filesize)
	{
		Error ("Invalid wad file '%s'.", wad->path);
	}
	wad->lumpinfos = (lumpinfo_t *)malloc (wad->numlumps * sizeof (lumpinfo_t));
	hlassume (wad->lumpinfos != NULL, assume_NoMemory);
    if (fseek (wad->file, wadinfo.infotableofs, SEEK_SET))
		Error ("File read failure: %s", wad->path);
	for (i = 0; i < wad->numlumps; i++)
	{
		SafeRead (wad->file, &wad->lumpinfos[i], sizeof (lumpinfo_t));
		if (!TerminatedString(wad->lumpinfos[i].name, 16))
		{
			wad->lumpinfos[i].name[16 - 1] = 0;
			Warning("Unterminated texture name : wad[%s] texture[%d] name[%s]\n", wad->path, i, wad->lumpinfos[i].name);
		}
		wad->lumpinfos[i].filepos = LittleLong(wad->lumpinfos[i].filepos);
		wad->lumpinfos[i].disksize = LittleLong(wad->lumpinfos[i].disksize);
		wad->lumpinfos[i].size = LittleLong(wad->lumpinfos[i].size);
	}
	qsort (wad->lumpinfos, wad->numlumps, sizeof (lumpinfo_t), lump_sorter_by_name);
}

void OpenWadFiles ()
{
	if (!g_wadfiles_opened)
	{
		g_wadfiles_opened = true;
		if (!g_waddirs)
		{
			Warning ("No wad directories have been set.");
		}
		else
		{
			Log ("Opening wad files from directories:");
			waddir_t *dir;
			for (dir = g_waddirs; dir; dir = dir->next)
			{
				Log (" %s ", dir->path);
			}
			Log( "\n" );
		}
		const char *value = ValueForKey (&g_entities[0], "wad");
		char path[MAX_VALUE];
		int i, j;
		for (i = 0, j = 0; i < strlen(value) + 1; i++)
		{
			if (value[i] == ';' || value[i] == '\0')
			{
				path[j] = '\0';
				if (path[0])
				{
					char name[MAX_VALUE];
					ExtractFile (path, name);
					DefaultExtension (name, ".wad");
					OpenWadFile (name);
				}
				j = 0;
			}
			else
			{
				path[j] = value[i];
				j++;
			}
		}
		CheckFatal ();
	}
}

void CloseWadFiles ()
{
	if (g_wadfiles_opened)
	{
		g_wadfiles_opened = false;
		wadfile_t *wadfile, *next;
		for (wadfile = g_wadfiles; wadfile; wadfile = next)
		{
			next = wadfile->next;
			free (wadfile->lumpinfos);
			fclose (wadfile->file);
			free (wadfile);
		}
		g_wadfiles = NULL;
	}
}

void DefaultTexture (radtexture_t *tex, const char *name)
{
	int i;
	tex->width = 16;
	tex->height = 16;
	strcpy (tex->name, name);
	tex->name[16 - 1] = '\0';
	tex->data = (byte *)malloc (tex->width * tex->height);
	hlassume (tex->data != NULL, assume_NoMemory);
	for (i = 0; i < 256; i++)
	{
		VectorFill (tex->palette[i], 0x80);
	}
	for (i = 0; i < tex->width * tex->height; i++)
	{
		tex->data[i] = 0x00;
	}
}

void LoadTexture (radtexture_t *tex, const miptex_t *mt, int size)
{
	int i, j;
	const miptex_t *header = mt;
	const byte *data = (const byte *)mt;
	tex->width = header->width;
	tex->height = header->height;
	strcpy (tex->name, header->name);
	tex->name[16 - 1] = '\0';
	if (tex->width <= 0 || tex->height <= 0 || tex->width % (2 * 1 << (MIPLEVELS - 1)) != 0 || tex->height % (2 * (1 << (MIPLEVELS - 1))) != 0)
	{
		Error ("Texture '%s': dimension (%dx%d) is not multiple of %d.", tex->name, tex->width, tex->height, 2 * (1 << (MIPLEVELS - 1)));
	}
	int mipsize;
	for (mipsize = 0, i = 0; i < MIPLEVELS; i++)
	{
		if (mt->offsets[i] != sizeof (miptex_t) + mipsize)
		{
			Error ("Texture '%s': unexpected miptex offset.", tex->name);
		}
		mipsize += (tex->width >> i) * (tex->height >> i);
	}
	if (size < sizeof (miptex_t) + mipsize + 2 + 256 * 3)
	{
		Error ("Texture '%s': no enough data.", tex->name);
	}
	if (*(unsigned short *)&data[sizeof (miptex_t) + mipsize] != 256)
	{
		Error ("Texture '%s': palette size is not 256.", tex->name);
	}
	tex->data = (byte *)malloc (tex->width * tex->height);
	hlassume (tex->data != NULL, assume_NoMemory);
	for (i = 0; i < tex->height; i++)
	{
		for (j = 0; j < tex->width; j++)
		{
			tex->data[i * tex->width + j] = data[sizeof (miptex_t) + i * tex->width + j];
		}
	}
	for (i = 0; i < 256; i++)
	{
		for (j = 0; j < 3; j++)
		{
			tex->palette[i][j] = data[sizeof (miptex_t) + mipsize + 2 + i * 3 + j];
		}
	}
}

void LoadTextureFromWad (radtexture_t *tex, const miptex_t *header)
{
	tex->width = header->width;
	tex->height = header->height;
	strcpy (tex->name, header->name);
	tex->name[16 - 1] = '\0';
	wadfile_t *wad;
	for (wad = g_wadfiles; wad; wad = wad->next)
	{
		lumpinfo_t temp, *found;
		strcpy (temp.name, tex->name);
		found = (lumpinfo_t *)bsearch (&temp, wad->lumpinfos, wad->numlumps, sizeof (lumpinfo_t), lump_sorter_by_name);
		if (found)
		{
			Developer (DEVELOPER_LEVEL_MESSAGE, "Texture '%s': found in '%s'.\n", tex->name, wad->path);
			if (found->type != 67 || found->compression != 0)
				continue;
			if (found->disksize < sizeof (miptex_t) || found->filepos < 0 || found->filepos + found->disksize > wad->filesize)
			{
				Warning ("Texture '%s': invalid texture data in '%s'.", tex->name, wad->path);
				continue;
			}
			miptex_t *mt = (miptex_t *)malloc (found->disksize);
			hlassume (mt != NULL, assume_NoMemory);
			if (fseek (wad->file, found->filepos, SEEK_SET))
				Error ("File read failure");
			SafeRead (wad->file, mt, found->disksize);
			if (!TerminatedString(mt->name, 16))
			{
				Warning("Texture '%s': invalid texture data in '%s'.", tex->name, wad->path);
				free (mt);
				continue;
			}
			Developer (DEVELOPER_LEVEL_MESSAGE, "Texture '%s': name '%s', width %d, height %d.\n", tex->name, mt->name, mt->width, mt->height);
			if (strcasecmp (mt->name, tex->name))
			{
				Warning("Texture '%s': texture name '%s' differs from its reference name '%s' in '%s'.", tex->name, mt->name, tex->name, wad->path);
			}
			LoadTexture (tex, mt, found->disksize);
			free (mt);
			break;
		}
	}
	if (!wad)
	{
		Warning ("Texture '%s': texture is not found in wad files.", tex->name);
		DefaultTexture (tex, tex->name);
		return;
	}
}

void LoadTextures ()
{
	Log ("Load Textures:\n");
	AddWadFolder (g_Wadpath);
	g_numtextures = g_texdatasize? ((dmiptexlump_t *)g_dtexdata)->nummiptex: 0;
	g_textures = (radtexture_t *)malloc (g_numtextures * sizeof (radtexture_t));
	hlassume (g_textures != NULL, assume_NoMemory);
	int i;
	for (i = 0; i < g_numtextures; i++)
	{
		int offset = ((dmiptexlump_t *)g_dtexdata)->dataofs[i];
		int size = g_texdatasize - offset;
		radtexture_t *tex = &g_textures[i];
		if (offset < 0 || size < sizeof (miptex_t))
		{
			Warning ("Invalid texture data in '%s'.", g_source);
			DefaultTexture (tex, "");
		}
		else
		{
			miptex_t *mt = (miptex_t *)&g_dtexdata[offset];
			if (mt->offsets[0])
			{
				Developer (DEVELOPER_LEVEL_MESSAGE, "Texture '%s': found in '%s'.\n", mt->name, g_source);
				Developer (DEVELOPER_LEVEL_MESSAGE, "Texture '%s': name '%s', width %d, height %d.\n", mt->name, mt->name, mt->width, mt->height);
				LoadTexture (tex, mt, size);
			}
			else
			{
				OpenWadFiles ();
				LoadTextureFromWad (tex, mt);
			}
		}
	}

	Log ("%i textures referenced\n", g_numtextures);
	CloseWadFiles ();
}