/*******************************************************
*
*  Lightmaps loading.
*
*  written by BUzer for Half-Life: Paranoia modification
*
********************************************************/

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "cdll_int.h"
#include "com_model.h"
#include <stringlib.h>
#include <mathlib.h>
#include "entity_types.h"
#include "gl_local.h"

#define BLOCKLIGHTS_SIZE	(68*68)
#define LIGHTBUFFER_SIZE	(BLOCK_SIZE_MAX*BLOCK_SIZE_MAX*4)	// single lightmap size

// ~12 Mbytes in three buffers
byte		baselightmap[LIGHTBUFFER_SIZE];
byte		addlightmap[LIGHTBUFFER_SIZE];
byte		deluxemap[LIGHTBUFFER_SIZE];
size_t		baselightmap_size = 0;
size_t		addlightmap_size = 0;
size_t		lightvecs_size = 0;
static char	cached_worldname[64];
color24		r_blocklights[BLOCKLIGHTS_SIZE];
int		lightmaps_initialized = 0;
float		current_brightness = 0.0f;
float		current_contrast = 0.0f;
float		current_gamma = 0.0f;

//
// HELPERS
//
int StyleIndex ( msurface_t *surf, int style )
{
	for( int i = 0; i < MAXLIGHTMAPS && surf->styles[i] != 255; i++ )
	{
		if( surf->styles[i] == style )
			return i;
	}
	return -1;
}

float ApplyGamma( float value )
{
	if( current_gamma == 1 ) return (value * current_contrast + current_brightness);
	return (pow( value, current_gamma ) * current_contrast + current_brightness);
}

void Clamp( const Vector &vec, color24 *out )
{
	Vector in = vec;

	in *= 255.0f;

	float max = ( max( in.x, max( in.y, in.z )));
	if( max > 255 )
	{
		float scale = 255.0f / max;
		in *= scale;
	}

	in.x = bound( 0.0f, in.x, 255.0f );
	in.y = bound( 0.0f, in.y, 255.0f );
	in.z = bound( 0.0f, in.z, 255.0f );

	out->r = (byte)in.x;
	out->g = (byte)in.y;
	out->b = (byte)in.z;
}

int LightmapSize( msurface_t *surf )
{
	return ((surf->extents[0] / LM_SAMPLE_SIZE) + 1) * ((surf->extents[1] / LM_SAMPLE_SIZE) + 1);
}

//===================
// UploadBlocklights
// copies lightmap data from blocklights to common lightmap texture
//===================
void UploadBlocklights( msurface_t *surf, const char *basename, int indexes[], int txFlags, const byte *buffer, size_t &bufsize )
{
	color24 *bl = r_blocklights;
	byte *dest = (byte *)buffer;

	// LM_AllocBlock is finished to build
	// previous lightmap. Now time to upload it
	if( !surf->light_s && !surf->light_t && bufsize != 0 )
	{
		int lmNum = (surf->lightmaptexturenum - 1);
		indexes[lmNum] = CREATE_TEXTURE( va( "*%s%i", basename, lmNum ), BLOCK_SIZE, BLOCK_SIZE, buffer, txFlags ); 
		memset( dest, 0, BLOCK_SIZE * BLOCK_SIZE * 4 ); // clear buffer for debug
		bufsize = 0; // now buffer was flushed
	}

	dest += (surf->light_t * BLOCK_SIZE + surf->light_s) * 4;	
	int smax = (surf->extents[0] / LM_SAMPLE_SIZE) + 1;
	int tmax = (surf->extents[1] / LM_SAMPLE_SIZE) + 1;
	int stride = (BLOCK_SIZE * 4) - (smax << 2);
	bufsize += (smax * tmax * 4);

	for( int i = 0; i < tmax; i++, dest += stride )
	{
		for( int j = 0; j < smax; j++, bl++, dest += 4 )
		{
			dest[0] = bl->r;
			dest[1] = bl->g;
			dest[2] = bl->b;
			dest[3] = 255;
		}
	}
}

//===================
// BuildLightMap_FromOriginal
//
// Builds lightmap from original zero style
//===================
void BuildLightMap_FromOriginal( msurface_t *surf )
{
	int size = LightmapSize( surf );

	if( !surf->samples || size > BLOCKLIGHTS_SIZE )
		return;

	color24 *lightmap = surf->samples;

	for( int i = 0; i < size; i++ )
	{
		Vector col;
		col[0] = (float)lightmap[i].r / 255.0f;
		col[1] = (float)lightmap[i].g / 255.0f;
		col[2] = (float)lightmap[i].b / 255.0f;

		col[0] = ApplyGamma( col[0] ) / 2.0f;
		col[1] = ApplyGamma( col[1] ) / 2.0f;
		col[2] = ApplyGamma( col[2] ) / 2.0f;
		Clamp( col, &r_blocklights[i] );
	}

	UploadBlocklights( surf, "baselight", tr.baselightmap, TF_LIGHTMAP, baselightmap, baselightmap_size );

	memset( r_blocklights, 0, sizeof( r_blocklights ));
	UploadBlocklights( surf, "addlight", tr.addlightmap, TF_LIGHTMAP, addlightmap, addlightmap_size );

	memset( r_blocklights, 127, sizeof( r_blocklights ));
	UploadBlocklights( surf, "lightvecs", tr.lightvecs, TF_DELUXEMAP, deluxemap, lightvecs_size );
}

//===================
// BuildLightMap_FromBaselight
//
// Builds lightmap from baselight - for faces that doesnt have
// direct light in bump mode, but have it in normal mode
//===================
void BuildLightMap_FromBaselight( msurface_t *surf )
{
	int size = LightmapSize( surf );

	if( !surf->samples || size > BLOCKLIGHTS_SIZE )
		return;

	color24 *lightmap = surf->samples + size * StyleIndex( surf, BUMP_BASELIGHT_STYLE );

	for( int i = 0; i < size; i++ )
	{
		Vector col;
		col[0] = (float)lightmap[i].r / 255.0f;
		col[1] = (float)lightmap[i].g / 255.0f;
		col[2] = (float)lightmap[i].b / 255.0f;

		col[0] = ApplyGamma( col[0] );
		col[1] = ApplyGamma( col[1] );
		col[2] = ApplyGamma( col[2] );
		Clamp( col, &r_blocklights[i] );
	}

	UploadBlocklights( surf, "baselight", tr.baselightmap, TF_LIGHTMAP, baselightmap, baselightmap_size );

	memset( r_blocklights, 0, sizeof( r_blocklights ));
	UploadBlocklights( surf, "addlight", tr.addlightmap, TF_LIGHTMAP, addlightmap, addlightmap_size );

	memset( r_blocklights, 127, sizeof( r_blocklights ));
	UploadBlocklights( surf, "lightvecs", tr.lightvecs, TF_DELUXEMAP, deluxemap, lightvecs_size );
}

//===================
// BuildLightMap_FromBumpStyle
//
// Uses specified bump style to fill lightmap block data.
//===================
void BuildLightMap_FromBumpStyle( msurface_t *surf, int rstyle, int gammacorrection )
{
	int size = LightmapSize( surf );

	if( !surf->samples || size > BLOCKLIGHTS_SIZE )
		return;

	color24 *target_offset = surf->samples + size * StyleIndex( surf, rstyle );
	color24 *baselight_offset, *addlight_offset, *lightvecs_offset;

	if( gammacorrection )
	{
		baselight_offset = surf->samples + size * StyleIndex( surf, BUMP_BASELIGHT_STYLE );
		addlight_offset = surf->samples + size * StyleIndex( surf, BUMP_ADDLIGHT_STYLE );
		lightvecs_offset = surf->samples + size * StyleIndex( surf, BUMP_LIGHTVECS_STYLE );
	}

	for( int i = 0; i < size; i++ )
	{
		if( gammacorrection )
		{
			// calculate "combine" lightmap and apply gamma to it
			Vector baselight, addlight, result;
			Vector scale = g_vecZero;

			baselight[0] = (float)baselight_offset[i].r / 255.0f;
			baselight[1] = (float)baselight_offset[i].g / 255.0f;
			baselight[2] = (float)baselight_offset[i].b / 255.0f;

			addlight[0] = (float)addlight_offset[i].r / 255.0f;
			addlight[1] = (float)addlight_offset[i].g / 255.0f;
			addlight[2] = (float)addlight_offset[i].b / 255.0f;

			float dot = (float)lightvecs_offset[i].b;
			dot = (dot / 127.0f) - 1.0f;

			result = ((addlight * dot) + baselight) * 2.0f;

			if( result.x ) scale.x = ApplyGamma( result.x ) / result.x;
			if( result.y ) scale.y = ApplyGamma( result.y ) / result.y;
			if( result.z ) scale.z = ApplyGamma( result.z ) / result.z;

			result.x = (float)target_offset[i].r / 255.0f * scale[0];
			result.y = (float)target_offset[i].g / 255.0f * scale[1];
			result.z = (float)target_offset[i].b / 255.0f * scale[2];
			Clamp( result, &r_blocklights[i] );
		}
		else
		{
			if( target_offset[i].r == 127 && target_offset[i].g == 127 && target_offset[i].b == 190 )
				target_offset[i].b = 127;

			r_blocklights[i].r = target_offset[i].r;
			r_blocklights[i].g = target_offset[i].g;
			r_blocklights[i].b = target_offset[i].b;
		}
	}

	switch( rstyle )
	{
	case BUMP_BASELIGHT_STYLE:
		UploadBlocklights( surf, "baselight", tr.baselightmap, TF_LIGHTMAP, baselightmap, baselightmap_size );
		break;
	case BUMP_ADDLIGHT_STYLE:
		UploadBlocklights( surf, "addlight", tr.addlightmap, TF_LIGHTMAP, addlightmap, addlightmap_size );
		break;
	case BUMP_LIGHTVECS_STYLE:
		UploadBlocklights( surf, "lightvecs", tr.lightvecs, TF_DELUXEMAP, deluxemap, lightvecs_size );
		break;
	default:	HOST_ERROR( "BuildLightMap_FromBumpStyle: invalid bump style %i on a face %i\n", rstyle, ( surf - worldmodel->surfaces ));
		break;
	}
}

qboolean LightmapsNeedsUpdate( void )
{
	if( !worldmodel )
		return FALSE;

	if( !lightmaps_initialized )
		return TRUE;

	if( current_gamma != cv_gamma->value )
		return TRUE;

	if( current_brightness != cv_brightness->value )
		return TRUE;

	if( current_contrast != cv_contrast->value )
		return TRUE;

	if( Q_strcmp( worldmodel->name, cached_worldname ))
		return TRUE;

	return FALSE;
}

//===================
// UpdateLightmaps
// 
// (Must be called after BumpMarkFaces)
// detects map and light settings changes and loads appropriate lightmaps
//===================
void UpdateLightmaps( void )
{
	if( !LightmapsNeedsUpdate( ))
		return;

	lightmaps_initialized = true;
	current_gamma = cv_gamma->value;
	current_brightness = cv_brightness->value;
	current_contrast = cv_contrast->value;
	if( current_gamma < 0.0f ) current_gamma = 1.0f;

	// store levelname for catch map changing
	Q_strncpy( cached_worldname, worldmodel->name, sizeof( cached_worldname ));
	ALERT( at_aiconsole, "\n>> Loading additional lightmaps for %s\n", worldmodel->name );
	ALERT( at_aiconsole, "gamma: %f\nbrightness: %f\ncontrast: %f\n", current_gamma, current_brightness, current_contrast );
	memset( baselightmap, 0, sizeof( baselightmap ));
	memset( addlightmap, 0, sizeof( addlightmap ));
	memset( deluxemap, 0, sizeof( deluxemap ));

	baselightmap_size = 0;
	addlightmap_size = 0;
	lightvecs_size = 0;

	int i;

	// release old lightmaps
	for( i = 0; i < tr.lightmapstotal; i++ )
	{
		if( !tr.baselightmap[i] ) break;
		FREE_TEXTURE( tr.baselightmap[i] );
	}

	for( i = 0; i < tr.lightmapstotal; i++ )
	{
		if( !tr.addlightmap[i] ) break;
		FREE_TEXTURE( tr.addlightmap[i] );
	}

	for( i = 0; i < tr.lightmapstotal; i++ )
	{
		if( !tr.lightvecs[i] ) break;
		FREE_TEXTURE( tr.lightvecs[i] );
	}

	memset( tr.baselightmap, 0, sizeof( tr.baselightmap ));
	memset( tr.addlightmap, 0, sizeof( tr.addlightmap ));
	memset( tr.lightvecs, 0, sizeof( tr.lightvecs ));
	tr.lightmapstotal = -1;

	//==================================
	// Load BASELIGHT style lightmaps
	//==================================
	for( i = 0; i < worldmodel->numsurfaces; i++ )
	{
		msurface_t *surf = &worldmodel->surfaces[i];

		if( surf->flags & ( SURF_DRAWSKY|SURF_DRAWTURB ))
			continue;

		if( surf->lightmaptexturenum >= MAX_LIGHTMAPS )
		{
			ALERT( at_warning, "surface %d uses lightmap index %d greater than MAX_LIGHTMAPS!\n", i, surf->lightmaptexturenum );
			continue;
		}

		// update lightmaps count
		if( surf->lightmaptexturenum >= tr.lightmapstotal )
			tr.lightmapstotal = surf->lightmaptexturenum + 1;

		if( StyleIndex( surf, BUMP_BASELIGHT_STYLE ) == -1 )
		{
			// face doesn't have bump lightmaps
			BuildLightMap_FromOriginal( surf );
			continue;
		}

		if( FBitSet( surf->flags, SURF_BUMPDATA ))
		{
			// now we have all the three styles
			BuildLightMap_FromBumpStyle( surf, BUMP_BASELIGHT_STYLE, TRUE );
			BuildLightMap_FromBumpStyle( surf, BUMP_ADDLIGHT_STYLE, TRUE );
			BuildLightMap_FromBumpStyle( surf, BUMP_LIGHTVECS_STYLE, FALSE );
		}
		else
		{
			// face has baselight style, but no other bump styles.
			// use this as single lightmap
			BuildLightMap_FromBaselight( surf );
		}
	}

	// probably map has no light
	if( tr.lightmapstotal == -1 )
		return;

	// loading remaining blocks
	int lmNum = (tr.lightmapstotal - 1);

	if( baselightmap_size )
	{
		tr.baselightmap[lmNum] = CREATE_TEXTURE( va( "*baselight%i", lmNum ), BLOCK_SIZE, BLOCK_SIZE, baselightmap, TF_LIGHTMAP ); 
		baselightmap_size = 0;
	}

	if( addlightmap_size )
	{
		tr.addlightmap[lmNum] = CREATE_TEXTURE( va( "*addlight%i", lmNum ), BLOCK_SIZE, BLOCK_SIZE, addlightmap, TF_LIGHTMAP );
		addlightmap_size = 0;
	}

	if( lightvecs_size )
	{
		tr.lightvecs[lmNum] = CREATE_TEXTURE( va( "*lightvecs%i", lmNum ), BLOCK_SIZE, BLOCK_SIZE, deluxemap, TF_DELUXEMAP );
		lightvecs_size = 0;
	}

	ALERT( at_console, "Loaded %d additional lightmaps\n", tr.lightmapstotal * 3 );
}