//
// written by BUzer for HL: Paranoia modification
//
//		2006

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "gl_local.h"
#include <stringlib.h>

/*
========================
LoadWorldMaterials

build a material for each world texture
========================
*/
void LoadWorldMaterials( void )
{
	char	diffuse[128], bumpmap[128], glossmap[128], depthmap[128];

	if( tr.world_materials ) return; // already existed

	tr.world_materials = (material_t *)Mem_Alloc( sizeof( material_t ) * worldmodel->numtextures );

	for( int i = 0; i < worldmodel->numtextures; i++ )
	{
		texture_t *tx = worldmodel->textures[i];
		material_t *mat = &tr.world_materials[i];

		// bad texture? 
		if( !tx || !tx->name[0] ) continue;

		// make cross-links for consistency
		tx->material = mat;
		mat->pSource = tx;

		// build material names
		Q_snprintf( diffuse, sizeof( diffuse ), "textures/%s.tga", tx->name );
		Q_snprintf( bumpmap, sizeof( bumpmap ), "textures/%s_norm.tga", tx->name );
		Q_snprintf( glossmap, sizeof( glossmap ), "textures/%s_gloss.tga", tx->name );
		Q_snprintf( depthmap, sizeof( depthmap ), "textures/%s_height.tga", tx->name );

		if( FILE_EXISTS( diffuse ))
		{
			FREE_TEXTURE( tx->gl_texturenum ); // release wad-texture
			tx->gl_texturenum = LOAD_TEXTURE( diffuse, NULL, 0, 0 );
		}
		mat->gl_diffuse_id = tx->gl_texturenum;	// so engine can be draw HQ image for gl_renderer 0

		if( FILE_EXISTS( bumpmap ))
			mat->gl_normalmap_id = LOAD_TEXTURE( bumpmap, NULL, 0, TF_UNCOMPRESSED|TF_NORMALMAP );
		else mat->gl_normalmap_id = tr.normalmapTexture;

		if( FILE_EXISTS( glossmap ))
			mat->gl_specular_id = LOAD_TEXTURE( glossmap, NULL, 0, 0 );
		else mat->gl_specular_id = tr.blackTexture;

		if( FILE_EXISTS( depthmap ))
			mat->gl_heightmap_id = LOAD_TEXTURE( depthmap, NULL, 0, 0 );
		else mat->gl_heightmap_id = tr.whiteTexture;

		if( mat->gl_normalmap_id != tr.normalmapTexture )
			mat->flags |= BRUSH_HAS_BUMP;

		if( mat->gl_specular_id != tr.blackTexture )
			mat->flags |= BRUSH_HAS_SPECULAR;

		if( mat->gl_heightmap_id != tr.whiteTexture )
			mat->flags |= BRUSH_HAS_PARALLAX;

		if( RENDER_GET_PARM( PARM_TEX_FLAGS, mat->gl_specular_id ) & TF_HAS_ALPHA )
			mat->flags |= BRUSH_GLOSSPOWER;

		if( tx->name[0] == '{' )
			mat->flags |= BRUSH_TRANSPARENT;

		if( !Q_strnicmp( tx->name, "scroll", 6 ))
			mat->flags |= BRUSH_CONVEYOR;

		if( !Q_strnicmp( tx->name, "{scroll", 7 ))
			mat->flags |= (BRUSH_CONVEYOR|BRUSH_TRANSPARENT);

		if( !Q_strncmp( tx->name, "mirror", 6 ) || !Q_strncmp( tx->name, "reflect", 7 ))
			mat->flags |= BRUSH_REFLECT;

		if( !Q_strncmp( tx->name, "movie", 5 ))
			mat->flags |= BRUSH_FULLBRIGHT;

		if( tx->name[0] == '!' || !Q_strncmp( tx->name, "water", 5 ))
			mat->flags |= (BRUSH_REFLECT|BRUSH_HAS_BUMP);

		// setup material constants
		matdesc_t *desc = R_FindMaterial( tx->name );

		mat->glossExp = desc->glossExp;
		mat->parallaxScale = desc->parallaxScale;
		mat->parallaxSteps = desc->parallaxSteps;
		mat->lightRemap[0] = desc->lightRemap[0];
		mat->lightRemap[1] = desc->lightRemap[1];
		mat->lightRemap[2] = desc->lightRemap[2];
		mat->lightRemap[3] = desc->lightRemap[3];
		mat->reflectScale = desc->reflectScale;
		mat->refractScale = desc->refractScale;

		if( mat->flags & BRUSH_HAS_BUMP && mat->flags & BRUSH_HAS_SPECULAR && mat->reflectScale > 0.0f )
			mat->flags |= BRUSH_REFLECT_SPEC;
	}
}

/*
========================
LoadDetailTextures

load details textures a separate because engine
loads them after lightmap building
========================
*/
void LoadDetailTextures( void )
{
	for( int i = 0; i < worldmodel->numtextures; i++ )
	{
		texture_t *tx = worldmodel->textures[i];
		material_t *mat = &tr.world_materials[i];

		// bad texture? 
		if( !tx || !tx->name[0] ) continue;

		// get detail scale before texture has been overrided
		GET_DETAIL_SCALE( tx->gl_texturenum, &mat->detailScale[0], &mat->detailScale[1] );

		if( mat->detailScale[0] <= 0.0f ) mat->detailScale[0] = 1.0f;
		if( mat->detailScale[1] <= 0.0f ) mat->detailScale[1] = 1.0f;

		mat->gl_detail_id = tx->dt_texturenum;

		// setup material flags
		if( mat->gl_detail_id )
			mat->flags |= BRUSH_HAS_DETAIL;
	}
}

/*
========================
FreeWorldMaterials

purge all materials
========================
*/
void FreeWorldMaterials( void )
{
	if( !tr.world_materials ) return;

	for( int i = 0; i < worldmodel->numtextures; i++ )
	{
		material_t *mat = &tr.world_materials[i];

		if( !mat->pSource ) continue;	// not initialized?

		if( mat->gl_normalmap_id && mat->gl_normalmap_id != tr.normalmapTexture )
			FREE_TEXTURE( mat->gl_normalmap_id );

		if( mat->gl_specular_id && mat->gl_specular_id != tr.blackTexture )
			FREE_TEXTURE( mat->gl_specular_id );

		if( mat->gl_heightmap_id && mat->gl_heightmap_id != tr.whiteTexture )
			FREE_TEXTURE( mat->gl_heightmap_id );
	}

	Mem_Free( tr.world_materials );
	tr.world_materials = NULL;
}

void R_ProcessWorldData( model_t *mod, qboolean create )
{
	worldmodel = mod;

	if( create ) LoadWorldMaterials();
	else FreeWorldMaterials();
}

// ===========================
// Sorting textures pointers in worldmodel->textures array by detail texture id.
// This may allow us make less state switches during rendering
// ===========================
int CompareDetailIDs( texture_t *a, texture_t *b )
{
	return (a->dt_texturenum > b->dt_texturenum);
}

// ===========================
// UpdateWorldExtradata
//
// optimized version of CreateExtDataForTextures
// ===========================
void UpdateWorldExtradata( const char *worldname )
{
	static char	cached_worldname[64];
	int		i;

	if( !Q_strcmp( worldname, cached_worldname ))
		return;

	// store levelname for catch map changing
	Q_strncpy( cached_worldname, worldname, sizeof( cached_worldname ));

	// delete all textures from previous level
	DeleteWorldExtradata();

	// correctly loading detail textures
	LoadDetailTextures ();

	// sort textures by detail
	texture_t **tex = (texture_t**)worldmodel->textures;

	for( i = 0 + 1; i < worldmodel->numtextures; i++ )
	{
		texture_t *t = tex[i];

		for( int j = i - 1; j >= 0 && CompareDetailIDs( tex[j], t ); j-- )
			tex[j+1] = tex[j];

		tex[j+1] = t;
	}

	// mark surfaces
	for( i = 0; i < worldmodel->numsurfaces; i++ )
	{
		msurface_t *surf = &worldmodel->surfaces[i];

		surf->flags &= ~(SURF_BUMPDATA|SURF_SPECULAR); // make sure that flag isn't set..

		if( surf->flags & ( SURF_DRAWSKY|SURF_DRAWTURB ))
			continue;
		
		if( StyleIndex( surf, BUMP_BASELIGHT_STYLE ) == -1 )
			continue;

		if( StyleIndex( surf, BUMP_ADDLIGHT_STYLE ) == -1 )
			continue;

		if( StyleIndex( surf, BUMP_LIGHTVECS_STYLE ) == -1 )
			continue;

		// now we have all the bump styles
		surf->flags |= SURF_BUMPDATA;
		
		// has normal map for this texture?
		if( !surf->texinfo->texture->material )
			continue;

		material_t *pMat = surf->texinfo->texture->material;

		if( pMat->gl_normalmap_id != tr.normalmapTexture && pMat->gl_specular_id != tr.blackTexture )
		{
			// surface ready to be drawn with specular
			surf->flags |= SURF_SPECULAR;
		}		
	}

	GenerateVertexArray();

	CreateWorldMeshCache();
}

// ===========================
// DeleteWorldExtradata
//
// working version of DeleteExtTextures
// ===========================
void DeleteWorldExtradata( void )
{
	DestroyWorldMeshCache ();
}