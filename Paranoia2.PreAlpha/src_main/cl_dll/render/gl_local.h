/*
gl_local.h - renderer local definitions
this code written for Paranoia 2: Savior modification
Copyright (C) 2013 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef GL_LOCAL_H
#define GL_LOCAL_H

#include "gl_export.h"
#include "ref_params.h"
#include "com_model.h"
#include "r_studioint.h"
#include "glsl_shader.h"
#include "gl_framebuffer.h"
#include "gl_frustum.h"
#include "features.h"
#include <matrix.h>

// limits
#define MAX_VISIBLE_PACKET	1024	// can be changed
#define MAX_VISIBLE_GROUP	256
#define MAX_SORTED_FACES	32768	// bmodels only
#define MAX_SORTED_MESHES	8192	// studio only
#define MAX_CACHED_MATRICES	512	// 32 kb cache
#define MAXARRAYVERTS	8192
#define MAX_TEXTURES	4096	// engine limit
#define MAX_GLSL_PROGRAMS	128
#define MAX_ELIGHTS		64	// engine limit (studiomodels only)
#define MAX_GLOW_SPRITES	64
#define MAX_MOVIES		16	// max various movies per level
#define MAX_MOVIE_TEXTURES	64	// max # of unique video textures per level
#define MAX_LIGHTMAPS	32	// Xash3D supports up to 256 lightmaps
#define MAX_DLIGHTS 	32	// per one frame. unsigned int limit
#define MAX_SHADOWS		MAX_DLIGHTS
#define MAX_MIRRORS		32	// per one frame!
#define NOISE_SIZE		64

#define WATER_TEXTURES	29
#define WATER_ANIMTIME	29.0f
#define PUDDLE_ANIMTIME	20.0f

#define INVALID_HANDLE	0xFFFF

#define FLASHLIGHT_KEY	-666
#define SUNLIGHT_KEY	-777
#define SKYBOX_ENTITY	70

#define LIGHT_DIRECTIONAL	0
#define LIGHT_OMNIDIRECTIONAL	1
#define LIGHT_SUN		2

// brush model flags (stored in model_t->flags)
#define MODEL_CONVEYOR	(1<<0)
#define MODEL_HAS_ORIGIN	(1<<1)

#define LM_SAMPLE_SIZE	tr.lm_sample_size	// lightmap resoultion

#define BLOCK_SIZE		glConfig.block_size	// lightmap blocksize
#define BLOCK_SIZE_DEFAULT	128		// for keep backward compatibility
#define BLOCK_SIZE_MAX	1024

// VBO offsets
#define OFFSET( type, var )	((const void *)&(((type *)NULL)->var))
#define BUFFER_OFFSET( i )	((void *)(i))

#define R_SurfCmp( a, b )	(( a.hProgram > b.hProgram ) ? true : ( a.hProgram < b.hProgram ))

// refparams
#define RP_NONE		0
#define RP_MIRRORVIEW	(1<<0)	// lock pvs at vieworg
#define RP_ENVVIEW		(1<<1)	// used for cubemapshot
#define RP_OLDVIEWLEAF	(1<<2)
#define RP_CLIPPLANE	(1<<3)	// mirrors used
#define RP_MERGEVISIBILITY	(1<<4)	// merge visibility for additional passes
#define RP_HASDYNLIGHTS	(1<<5)
#define RP_SHADOWVIEW	(1<<6)	// view through light
#define RP_WORLDSURFVISIBLE	(1<<7)	// indicates what we view at least one surface from the current vieworg
#define RP_SKYVISIBLE	(1<<8)	// sky is visible
#define RP_NOSHADOWS	(1<<9)	// disable shadows for this pass
#define RP_SUNSHADOWS	(1<<10)	// we have shadows from sun (studiomodels only)

#define RP_NONVIEWERREF	(RP_MIRRORVIEW|RP_ENVVIEW|RP_SHADOWVIEW)
#define RP_LOCALCLIENT( e )	(gEngfuncs.GetLocalPlayer() && ((e)->index == gEngfuncs.GetLocalPlayer()->index && e->curstate.entityType == ET_PLAYER ))
#define RP_NORMALPASS()	((RI.params & RP_NONVIEWERREF) == 0 )

#define TF_LIGHTMAP		(TF_UNCOMPRESSED|TF_NOMIPMAP|TF_CLAMP)
#define TF_DELUXEMAP	(TF_UNCOMPRESSED|TF_CLAMP|TF_NOMIPMAP|TF_NORMALMAP)
#define TF_IMAGE		(TF_UNCOMPRESSED|TF_NOPICMIP|TF_NOMIPMAP|TF_CLAMP)
#define TF_SCREEN		(TF_UNCOMPRESSED|TF_NOPICMIP|TF_NOMIPMAP|TF_CLAMP)
#define TF_SPOTLIGHT	(TF_UNCOMPRESSED|TF_NOMIPMAP|TF_BORDER)
#define TF_SHADOW		(TF_UNCOMPRESSED|TF_NOMIPMAP|TF_CLAMP|TF_DEPTHMAP)
#define TF_RECTANGLE	(TF_TEXTURE_RECTANGLE|TF_UNCOMPRESSED|TF_NOPICMIP|TF_NOMIPMAP|TF_CLAMP)
#define TF_DEPTH		(TF_IMAGE|TF_NEAREST|TF_DEPTHMAP|TF_LUMINANCE)

#define MAT_ALL_EFFECTS	(BRUSH_HAS_BUMP|BRUSH_HAS_SPECULAR|BRUSH_HAS_PARALLAX|BRUSH_REFLECT|BRUSH_FULLBRIGHT)

#define FBO_MAIN		0

// helpers
#define GetVForward()	(RI.viewMatrix[0])
#define GetVRight()		(RI.viewMatrix[1])
#define GetVUp()		(RI.viewMatrix[2])
#define GetVieworg()	(RI.viewMatrix[3])

enum
{
	BUMP_BASELIGHT_STYLE	= 61,
	BUMP_ADDLIGHT_STYLE		= 62,
	BUMP_LIGHTVECS_STYLE	= 63,
};

enum
{
	ATTR_INDEX_POSITION = 0,
	ATTR_INDEX_TEXCOORD0,	// texture coord
	ATTR_INDEX_TEXCOORD1,	// lightmap coord
	ATTR_INDEX_TANGENT,
	ATTR_INDEX_BINORMAL,
	ATTR_INDEX_NORMAL,
	ATTR_INDEX_BONE_INDEXES,
};

#define SHADER_VERTEX_COMPILED	BIT( 0 )
#define SHADER_FRAGMENT_COMPILED	BIT( 1 )
#define SHADER_PROGRAM_LINKED		BIT( 2 )

typedef struct glsl_program_s
{
	char		name[64];
	GLhandleARB	handle;
	unsigned short	status;

	// sampler parameters
	GLint		u_ColorMap;	// 0-th unit
	GLint		u_DepthMap;	// 1-th unit
	GLint		u_NormalMap;	// 1-th unit
	GLint		u_GlossMap;	// 2-th unit
	GLint		u_DetailMap;	// 6-th unit
	GLint		u_ProjectMap;	// 0-th unit
	GLint		u_ShadowMap;	// 2-th unit
	GLint		u_AttnZMap;	// 1-th unit
	GLint		u_BaseLightMap;	// 1-th unit
	GLint		u_AddLightMap;	// 2-th unit
	GLint		u_DeluxeMap;	// 3-th unit (lightvectors)
	GLint		u_DecalMap;	// 0-th unit
	GLint		u_ScreenMap;	// 2-th unit

	// uniform parameters
	GLint		u_BoneMatrix;	// bonematrix array
	GLint		u_ModelViewMatrix;
	GLint		u_ModelViewProjectionMatrix;
	GLint		u_GenericCondition;	// generic parm
	GLint		u_GenericCondition2;// generic parm
	GLint		u_ClipPlane;	// clipplane is enabled
	GLint		u_WhiteFactor;	// blur factor
	GLint		u_BlurFactor;	// blur factor
	GLint		u_ScreenWidth;
	GLint		u_ScreenHeight;
	GLint		u_zFar;		// global z-far value
	GLint		u_FocalDepth;
	GLint		u_FocalLength;
	GLint		u_FStop;
	GLint		u_LightDir;
	GLint		u_LightAmbient;
	GLint		u_LightDiffuse;
	GLint		u_LambertValue;
	GLint		u_GlossExponent;
	GLint		u_LightOrigin;
	GLint		u_LightRadius;
	GLint		u_ViewOrigin;
	GLint		u_ViewRight;
	GLint		u_FaceFlags;
	GLint		u_ParallaxScale;
	GLint		u_ParallaxSteps;
	GLint		u_ShadowMode;
	GLint		u_ParallaxMode;
	GLint		u_DetailMode;
	GLint		u_DetailScale;
	GLint		u_SpecularMode;	// 0 - no specular, 1 - specular
	GLint		u_LightmapDebug;
	GLint		u_RemapParms;
	GLint		u_RefractScale;
	GLint		u_ReflectScale;
	GLint		u_MirrorMode;	// allow mirrors
	GLint		u_TexOffset;	// e.g. conveyor belt
	GLint		u_RenderColor;	// color + alpha
	GLint		u_RealTime;	// RI.refdef.time
} glsl_program_t;

typedef struct gl_texbuffer_s
{
	int		texture;
	int		texframe;		// this frame texture was used
} gl_texbuffer_t;

typedef struct gl_movie_s
{
	char		name[32];
	void		*state;
	float		length;		// total cinematic length
	long		xres, yres;	// size of cinematic
} gl_movie_t;

typedef struct
{
	cl_entity_t	*ent;
	model_t		*psprite;
	float		width, height;
} cl_glow_sprite_t;

// lightinfo flags
#define LIGHTING_AMBIENT	(1<<0)	// has a lighting ambient info
#define LIGHTING_DIFFUSE	(1<<1)	// has a lighting diffuse info
#define LIGHTING_DIRECTION	(1<<2)	// has a lighting direction info

// extended lightinfo
typedef struct
{
	vec3_t		ambient;
	vec3_t		diffuse;
	vec3_t		direction;
	vec3_t		origin;	// just for debug
	word		flags;
} lightinfo_t;

typedef struct cl_plight_s
{
	Vector		origin;
	Vector		angles;
	float		radius;
	Vector		color;		// ignored for spotlights, they have a texture
	float		die;		// stop lighting after this time
	float		decay;		// drop this each second
	int		key;

	matrix4x4		projectionMatrix;	// light projection matrix
	matrix4x4		modelviewMatrix;	// light modelview
	matrix4x4		textureMatrix;	// result texture matrix	
	matrix4x4		textureMatrix2;	// for bmodels
	matrix4x4		shadowMatrix;	// result texture matrix	
	matrix4x4		shadowMatrix2;	// for bmodels

	Vector		absmin, absmax;	// for frustum culling
	mplane_t		frustum[6];
	int		clipflags;

	CFrustum		frustumTest;

	// spotlight specific:
	int		spotlightTexture;
	int		shadowTexture;	// shadowmap for this light
	int		cinTexturenum;	// not gltexturenum!
	int		lastframe;	// cinematic lastframe
	float		fov;
} DynamicLight;

// 64 bytes here
typedef struct bvert_s
{
	Vector		vertex;		// position
	Vector		normal;		// normal
	Vector		tangent;		// tangent
	Vector		binormal;		// binormal
	float		stcoord[2];	// ST texture coords
	float		lmcoord[2];	// LM texture coords
} bvert_t;

// VBO buffer
struct BrushVertex
{
	Vector		pos;
	float		texcoord[2];
	float		lightmaptexcoord[2];
};

// extradata
struct BrushFace
{
	int		start_vertex;
	Vector		normal;
	Vector		s_tangent;
	Vector		t_tangent;
};

// 14 bytes here
typedef struct
{
	msurface_t	*surface;
	unsigned short	hProgram;	// handle to glsl program
	cl_entity_t	*parent;	// pointer to parent entity
	float		dist;	// for sorting by dist
} gl_bmodelface_t;

// 15 bytes here
typedef struct
{
	struct vbomesh_s	*mesh;
	unsigned short	hProgram;	// handle to glsl program
	cl_entity_t	*parent;	// pointer to parent entity
	model_t		*model;
	bool		trans;	// translucent
} gl_studiomesh_t;

// mirror entity
typedef struct gl_entity_s
{
	cl_entity_t	*ent;
	mextrasurf_t	*chain;
} gl_entity_t;

// 76 bytes
typedef struct
{
	GLfloat		modelviewMatrix[16];		// used for clip-space
	GLfloat		modelviewProjectionMatrix[16];	// used for vertex transform
	float		texofs[2];			// conveyor movement
	Vector		modelorg;				// vieworg in object space
} gl_cachedmatrix_t;

typedef struct
{
	char		name[64];
	float		glossExp;		// gloss exponential fade
	float		parallaxScale;	// height of parallax effect
	byte		parallaxSteps;	// optimization: custom steps count
	float		lightRemap[4];	// remap is used to make soft or hard lighting effect
	float		reflectScale;	// reflection scale for translucent water
	float		refractScale;	// refraction scale for mirrors, windows, water
} matdesc_t;

/*
=======================================================================

 GL STATE MACHINE

=======================================================================
*/
enum
{
	R_OPENGL_110 = 0,		// base
	R_WGL_PROCADDRESS,
	R_ARB_VERTEX_BUFFER_OBJECT_EXT,
	R_ARB_VERTEX_ARRAY_OBJECT_EXT,
	R_ENV_COMBINE_EXT,
	R_NV_COMBINE_EXT,		// NVidia combiners
	R_ARB_MULTITEXTURE,
	R_TEXTURECUBEMAP_EXT,
	R_DOT3_ARB_EXT,
	R_ANISOTROPY_EXT,
	R_TEXTURE_LODBIAS,
	R_OCCLUSION_QUERIES_EXT,
	R_TEXTURE_COMPRESSION_EXT,
	R_SHADER_GLSL100_EXT,
	R_SGIS_MIPMAPS_EXT,
	R_DRAW_RANGEELEMENTS_EXT,
	R_LOCKARRAYS_EXT,
	R_TEXTURE_3D_EXT,
	R_CLAMPTOEDGE_EXT,
	R_COPY_IMAGE_EXT,
	R_STENCILTWOSIDE_EXT,
	R_SHADER_OBJECTS_EXT,
	R_VERTEX_PROGRAM_EXT,	// cg vertex program
	R_FRAGMENT_PROGRAM_EXT,	// cg fragment program
	R_VERTEX_SHADER_EXT,	// glsl vertex program
	R_FRAGMENT_SHADER_EXT,	// glsl fragment program	
	R_EXT_POINTPARAMETERS,
	R_SEPARATESTENCIL_EXT,
	R_ARB_TEXTURE_NPOT_EXT,
	R_CUSTOM_VERTEX_ARRAY_EXT,
	R_TEXTURE_ENV_ADD_EXT,
	R_CLAMP_TEXBORDER_EXT,
	R_DEPTH_TEXTURE,
	R_SHADOW_EXT,
	R_FRAMEBUFFER_OBJECT,
	R_PARANOIA_EXT,		// custom OpenGL32.dll with hacked function glDepthRange
	R_HARDWARE_SKINNING,
	R_EXTCOUNT,		// must be last
};

enum
{
	GL_KEEP_UNIT = -1,		// alternative way - change the unit by GL_SelectTexture
	GL_TEXTURE0 = 0,
	GL_TEXTURE1,
	GL_TEXTURE2,
	GL_TEXTURE3,
	GL_TEXTURE4,
	GL_TEXTURE5,
	GL_TEXTURE6,
	GL_TEXTURE7,
	MAX_TEXTURE_UNITS
};

enum
{
	ENVSTATE_OFF = 0,
	ENVSTATE_REPLACE,
	ENVSTATE_MUL_CONST,
	ENVSTATE_MUL_PREV_CONST,	// ignores texture
	ENVSTATE_MUL,
	ENVSTATE_MUL_X2,
	ENVSTATE_ADD,
	ENVSTATE_DOT,
	ENVSTATE_DOT_CONST,
	ENVSTATE_PREVCOLOR_CURALPHA,
	ENVSTATE_NOSTATE		// uninitialized
};

enum
{
	TC_OFF = 0,
	TC_TEXTURE,
	TC_LIGHTMAP,
	TC_VERTEX_POSITION,		// for specular and dynamic lighting
	TC_NOSTATE		// uninitialized
};

typedef struct
{
	int		params;		// rendering parameters

	qboolean		drawWorld;	// ignore world for drawing PlayerModel
	qboolean		thirdPerson;	// thirdperson camera is enabled
	qboolean		drawOrtho;

	ref_params_t	refdef;		// actual refdef

	matrix3x4		viewMatrix;	// our viewmatrix

	cl_entity_t	*currententity;
	model_t		*currentmodel;
	cl_entity_t	*currentbeam;	// same as above but for beams
	const DynamicLight	*currentlight;
	glsl_program_t	*currentshader;

	int		viewport[4];
	int		scissor[4];
	mplane_t		frustum[6];

	Vector		pvsorigin;
	Vector		vieworg;		// locked vieworigin
	Vector		vforward;
	Vector		vright;
	Vector		vup;

	float		farClip;
	unsigned int	clipFlags;

	float		waveHeight;	// global waveHeight
	float		currentWaveHeight;	// current entity waveHeight

	matrix4x4		objectMatrix;		// currententity matrix
	matrix4x4		worldviewMatrix;		// modelview for world
	matrix4x4		modelviewMatrix;		// worldviewMatrix * objectMatrix

	matrix4x4		projectionMatrix;
	matrix4x4		worldviewProjectionMatrix;	// worldviewMatrix * projectionMatrix

	GLfloat		gl_modelviewMatrix[16];
	GLfloat		gl_modelviewProjectionMatrix[16];// for GLSL transform

	float		viewplanedist;
	mplane_t		clipPlane;

	byte		visbytes[MAX_MAP_LEAFS/8];	// individual visbytes for each pass
} ref_instance_t;

typedef struct
{
	qboolean		modelviewIdentity;

	qboolean		fResetVis;
	qboolean		fCustomRendering;
	int		insideView;	// this is view through portal or mirror

	// entity lists
	gl_entity_t	mirror_entities[MAX_VISIBLE_PACKET];	// an entities that has mirror
	cl_entity_t	*static_entities[MAX_VISIBLE_PACKET];	// opaque non-moved brushes
	cl_entity_t	*solid_entities[MAX_VISIBLE_PACKET];	// opaque moving or alpha brushes
	cl_entity_t	*trans_entities[MAX_VISIBLE_PACKET];	// translucent brushes
	cl_entity_t	*child_entities[MAX_VISIBLE_PACKET];	// entities with MOVETYPE_FOLLOW
	cl_entity_t	*beams_entities[MAX_VISIBLE_PACKET];	// server beams
	cl_entity_t	*water_entities[MAX_VISIBLE_PACKET];	// water brushes
	cl_entity_t	*glass_entities[MAX_VISIBLE_PACKET];	// refracted brushes
	cl_glow_sprite_t	glow_sprites[MAX_GLOW_SPRITES];
	int		num_mirror_entities;
	int		num_static_entities;
	int		num_solid_entities;
	int		num_trans_entities;
	int		num_child_entities;
	int		num_beams_entities;
	int		num_glow_sprites;
	int		num_water_entities;
	int		num_glass_entities;

	cl_entity_t	*solid_studio_ents[MAX_VISIBLE_PACKET];
	int		num_solid_studio_ents;

	cl_entity_t	*solid_bmodel_ents[MAX_VISIBLE_PACKET];
	int		num_solid_bmodel_ents;

	cl_entity_t	*trans_studio_ents[MAX_VISIBLE_PACKET];
	int		num_trans_studio_ents;

	cl_entity_t	*trans_bmodel_ents[MAX_VISIBLE_PACKET];
	int		num_trans_bmodel_ents;

	int		defaultTexture;   	// use for bad textures
	int		skyboxTextures[6];	// skybox sides
	int		normalizeCubemap;
	int		normalmapTexture;	// default normalmap
	int		deluxemapTexture;	// default deluxemap
	int		dlightCubeTexture;

	int		attenuation_1d;	// normal attenuation
	int		atten_point_1d;	// for old cards
	int		atten_point_2d;	// for old cards
	int		atten_point_3d;

	int		noiseTexture;
	int		defaultProjTexture;	// fallback for missed textures
	int		flashlightTexture;	// flashlight projection texture
	int		spotlightTexture[8];// reserve for eight textures
	int		cinTextures[MAX_MOVIE_TEXTURES];
	gl_texbuffer_t	mirrorTextures[MAX_MIRRORS];
	int		shadowTextures[MAX_SHADOWS];
	int		waterTextures[WATER_TEXTURES];
	int		num_shadows_used;	// used shadow textures per full frame
	int		num_cin_used;	// used movie textures per full frame
	int		num_mirrors_used;	// used mirror textures per level

	int		screen_depth;
	int		screen_rgba;	

	int		screen_rgba1;	// downsample for HDRL
	int		baselightmap[MAX_LIGHTMAPS];
	int		addlightmap[MAX_LIGHTMAPS];
	int		lightvecs[MAX_LIGHTMAPS];
	int		lightmapstotal;

	int		grayTexture;
	int		whiteTexture;
	int		blackTexture;
	int		screenTexture;
	int		weightTexture;
	int		refractionTexture;

	// framebuffers
	CFrameBuffer	fbo_mirror;	// used for mirror rendering
	CFrameBuffer	fbo_shadow;	// used for normal shadowmapping
	CFrameBuffer	fbo_sunshadow;	// extra-large shadowmap for sun rendering

	int		lm_sample_size;

	cl_entity_t	*mirror_entity;	
	int		visframecount;	// PVS frame
	int		dlightframecount;	// dynamic light frame
	int		realframecount;	// not including passes
	int		traceframecount;	// to avoid checked each surface multiple times
	int		scrcpywaterframe;
	int		scrcpydecalframe;
	int		scrcpyframe;
	int		framecount;

	Vector		ambientLight;	// at vieworg

	// sky params
	Vector		sky_origin;
	Vector		sky_world_origin;
	float		sky_speed;

	gl_movie_t	cinematics[MAX_MOVIES];	// precached cinematics

	matdesc_t		*materials;
	unsigned int	matcount;

	GLuint		vbo_buffer;
	BrushVertex	*vbo_buffer_data;
	int		use_vertex_array;	// VBO or VA

	// new VBO data
	GLuint		world_vbo;
	GLuint		world_vao;

	material_t	*world_materials;	// single buffer with world materials

	bool		world_has_movies;	// indicate a surfaces with SURF_MOVIE bit set
	bool		world_has_mirrors;	// indicate a surfaces with SURF_REFLECT bit set
	bool		world_has_skybox;	// indicate a surfaces with SURF_DRAWSKY bit set
	bool		local_client_added;	// indicate what a local client already been added into renderlist

	glsl_program_t	glsl_programs[MAX_GLSL_PROGRAMS];
	int		num_glsl_programs;

	gl_cachedmatrix_t	cached_matrices[MAX_CACHED_MATRICES];
	int		num_cached_matrices;

	gl_bmodelface_t	draw_surfaces[MAX_SORTED_FACES];	// 384 kb
	int		num_draw_surfaces;

	gl_bmodelface_t	light_surfaces[MAX_SORTED_FACES];	// 384 kb
	int		num_light_surfaces;

	gl_studiomesh_t	draw_meshes[MAX_SORTED_MESHES];	// 114 kb
	int		num_draw_meshes;

	gl_studiomesh_t	light_meshes[MAX_SORTED_MESHES];	// 114 kb
	int		num_light_meshes;

	const ref_params_t	*cached_refdef;	// pointer to viewer refdef

	// cull info
	Vector		modelorg;		// relative to viewpoint
} ref_globals_t;

typedef struct
{
	unsigned int	c_world_polys;
	unsigned int	c_studio_polys;
	unsigned int	c_sprite_polys;
	unsigned int	c_world_leafs;
	unsigned int	c_grass_polys;
	unsigned int	c_light_polys;

	unsigned int	c_solid_decals;
	unsigned int	c_trans_decals;

	unsigned int	c_view_beams_count;
	unsigned int	c_active_tents_count;
	unsigned int	c_studio_models_drawn;
	unsigned int	c_sprite_models_drawn;
	unsigned int	c_particle_count;

	unsigned int	c_portal_passes;
	unsigned int	c_mirror_passes;
	unsigned int	c_screen_passes;
	unsigned int	c_shadow_passes;
	unsigned int	c_sky_passes;	// drawing through portal or monitor will be increase counter

	unsigned int	c_plights;	// count of actual projected lights
	unsigned int	c_client_ents;	// entities that moved to client

	unsigned int	num_drawed_ents;
	unsigned int	num_passes;

	unsigned int	num_drawed_particles;
	unsigned int	num_particle_systems;
	unsigned int	num_shader_binds;
	unsigned int	num_flushes;

	msurface_t	*debug_surface;
} ref_stats_t;

typedef struct
{
	unsigned int	specular_shader;
	unsigned int	specular_high_shader;
	unsigned int	shadow_shader0;
	unsigned int	shadow_shader1;
	unsigned int	liquid_shader;

	unsigned int	decal0_shader;
	unsigned int	decal1_shader;
	unsigned int	decal2_shader;
	unsigned int	decal3_shader;	// for pointlights
} ref_programs_t;

typedef struct
{
	glsl_program_t	*blurShader;		// e.g. underwater blur	
	glsl_program_t	*dofShader;		// iron sight with dof
	glsl_program_t	*monoShader;		// monochrome effect
	glsl_program_t	*filterShader;		// brightpass for bloom
	glsl_program_t	*tonemapShader;		// tonemapping
	glsl_program_t	*studioDiffuse;		// hardware skinning
	glsl_program_t	*studioGlass;		// hardware skinning with fresnel
	glsl_program_t	*studioRealBump;		// hardware skinning with bumpmapping
	glsl_program_t	*studioParallax;		// hardware skinning with bumpmapping and parallax occlusion mapping
	glsl_program_t	*studioDynLight;		// hardware skinning with dynamic lighting
	glsl_program_t	*studioBumpDynLight;	// hardware skinning with dynamic bump lighting
	glsl_program_t	*studioPOMDynLight;		// hardware skinning with dynamic POM lighting
	glsl_program_t	*studioAdditive;		// hardware skinning with additive drawing
	glsl_program_t	*depthFillGeneric;		// shadow pass
	glsl_program_t	*skyboxShader;		// skybox color

	glsl_program_t	*bmodelAmbient;		// draw world without any effects but detail texture
	glsl_program_t	*bmodelDiffuse;		// draw world without any effects but detail texture
	glsl_program_t	*bmodelFakeBump;		// draw world with fake bumpmapping (specular is optionally)
	glsl_program_t	*bmodelRealBump;		// draw world with real bumpmapping and HQ specular (optionally)
	glsl_program_t	*bmodelParallax;		// draw world with real parallax occlusion mapping (optionally)
	glsl_program_t	*bmodelReflectBump;		// draw world with real bumpmapping and reflecting specular (optionally)
	glsl_program_t	*debugLightmapShader;	// show lightmaps
	glsl_program_t	*bmodelDynLight;		// surfaces with dynamic lighting
	glsl_program_t	*bmodelBumpDynLight;	// surfaces with dynamic lighting and bumpmapping
	glsl_program_t	*bmodelGlass;		// transparent refracted surfaces
	glsl_program_t	*bmodelWater;		// water refracted & reflected
	glsl_program_t	*bmodelAdditive;		// additive surface

	glsl_program_t	*bmodelSingleDecal;		// draw decal surface
	glsl_program_t	*bmodelSingleDecalPOM;	// draw decal surface with POM
	glsl_program_t	*bmodelSingleDecalPuddle;	// draw puddle

	GLSLShader	*pWaterShader;		// reflection & refraction
	GLSLShader	*pGlassShader;		// refraction only
} ref_shaders_t;

typedef struct
{
	int		width, height;
	qboolean		fullScreen;
	qboolean		wideScreen;

	int		faceCull;
	int		frontFace;
	int		frameBuffer;

	qboolean		drawTrans;
	qboolean		drawProjection;
	qboolean		stencilEnabled;
	qboolean		in2DMode;
	int		envStates[MAX_TEXTURE_UNITS];
	int		texpointer[MAX_TEXTURE_UNITS];
} glState_t;

typedef struct
{
	const char	*renderer_string;		// ptrs to OpenGL32.dll, use with caution
	const char	*version_string;
	const char	*vendor_string;

	// list of supported extensions
	const char	*extensions_string;
	bool		extension[R_EXTCOUNT];

	int		block_size;		// lightmap blocksize
	
	int		max_texture_units;
	GLint		max_2d_texture_size;
	GLint		max_2d_rectangle_size;
	GLint		max_3d_texture_size;
	GLint		max_cubemap_size;
	GLint		max_nv_combiners;
	GLint		texRectangle;

	int		max_vertex_uniforms;
	int		max_vertex_attribs;
	int		max_skinning_bones;		// total bones that can be transformed with GLSL

	GLfloat		max_texture_anisotropy;
	GLfloat		max_texture_lodbias;
} glConfig_t;

extern glState_t glState;
extern glConfig_t glConfig;
extern engine_studio_api_t IEngineStudio;
extern DynamicLight cl_dlights[MAX_DLIGHTS];

extern mleaf_t		*r_viewleaf, *r_oldviewleaf;
extern mleaf_t		*r_viewleaf2, *r_oldviewleaf2;
extern float		gldepthmin, gldepthmax;
extern model_t		*worldmodel;
extern ref_params_t		r_lastRefdef;
extern ref_stats_t		r_stats;
extern ref_instance_t	RI;
extern ref_globals_t	tr;
extern ref_programs_t	cg;
extern ref_shaders_t	glsl;

//
// gl_backend.cpp
//
int R_GetSpriteTexture( const model_t *m_pSpriteModel, int frame );
void GL_SetTexEnvs( int env0, int env1 = ENVSTATE_OFF, int env2 = ENVSTATE_OFF, int env3 = ENVSTATE_OFF );
void R_DrawAbsBBox( const Vector &absmin, const Vector &absmax );
void R_BeginDrawProjection( const DynamicLight *pl, bool decalPass = false );
void R_EndDrawProjection( void );
void GL_SetTexPointer( int unitnum, int tc );
void GL_LoadMatrix( const matrix4x4 &source );
void GL_LoadTexMatrix( const matrix4x4 &source );
void GL_DisableAllTexGens( void );
void GL_FrontFace( GLenum front );
void GL_ClipPlane( bool enable );
void GL_BindFBO( GLuint buffer );
void GL_Cull( GLenum cull );
void ResetRenderState();
void ResetCache( void );

//
// gl_bloom.cpp
//
void R_InitBloomTextures( void );
void R_BloomBlend( const ref_params_t *fd );

//
// gl_cull.cpp
//
bool R_CullBoxExt( const mplane_t frustum[6], const Vector &mins, const Vector &maxs, unsigned int clipflags );
bool R_CullSphereExt( const mplane_t frustum[6], const Vector &centre, float radius, unsigned int clipflags );
bool R_CullModel( cl_entity_t *e, const Vector &origin, const Vector &mins, const Vector &maxs, float radius );
bool R_CullSurfaceExt( msurface_t *surf, const mplane_t frustum[6], unsigned int clipflags );
bool R_VisCullBox( const Vector &mins, const Vector &maxs );
bool R_VisCullSphere( const Vector &origin, float radius );

#define R_CullBox( mins, maxs, clipFlags )	R_CullBoxExt( RI.frustum, mins, maxs, clipFlags )
#define R_CullSphere( centre, radius, clipFlags )	R_CullSphereExt( RI.frustum, centre, radius, clipFlags )
#define R_CullSurface( surf, clipFlags )	R_CullSurfaceExt( surf, RI.frustum, clipFlags )

//
// gl_debug.cpp
//
void DrawTextureVecs( void );
void DrawVector( const vec3_t &start, const vec3_t &end, float r, float g, float b, float a );

//
// gl_rbeams.cpp
//
void R_InitViewBeams( void );
void R_AddServerBeam( cl_entity_t *pEnvBeam );
void CL_DrawBeams( int fTrans );

//
// glows.cpp
//
int GlowFilterEntities ( int type, struct cl_entity_s *ent, const char *modelname ); // buz
void InitGlows( void );
void DrawGlows( void );

//
// gl_light_dynamic.cpp
//
void R_RecursiveLightNode( mnode_t *node, const mplane_t frustum[6], unsigned int clipflags, unsigned int lightbits = 0 );
void R_MergeLightProjection( DynamicLight *pl );
qboolean SetupSpotLight( const DynamicLight *pl );
void FinishSpotLight( const DynamicLight *pl );
int SetupPointLight( const DynamicLight *pl );
void FinishPointLight( const DynamicLight *pl );

//
// gl_lightmap.cpp
//
float ApplyGamma( float value );
void UpdateLightmaps( void );
int StyleIndex ( msurface_t *surf, int style );

//
// gl_rlight.cpp
//
DynamicLight *MY_AllocDlight( int key );
void R_GetLightVectors( cl_entity_t *pEnt, Vector &origin, Vector &angles );
void R_SetupLightProjection( DynamicLight *pl, const Vector &origin, const Vector &angles, float radius, float fov, int texture = 0 );
void DrawDynamicLightForEntity( cl_entity_t *e, const Vector &mins, const Vector &maxs );
void R_LightForPoint( const Vector &point, lightinfo_t *lightinfo, bool invLight, bool secondpass = false );
int HasDynamicLights( void );
void DrawDynamicLights(void );
void ResetDynamicLights( void );
void MY_DecayLights( void );

//
// gl_rmain.cpp
//
void R_ClearScene( void );
void R_FindViewLeaf( void );
int R_ComputeFxBlend( cl_entity_t *e );
int R_RankForRenderMode( cl_entity_t *ent );
qboolean R_AddEntity( struct cl_entity_s *clent, int entityType );
qboolean R_WorldToScreen( const Vector &point, Vector &screen );
void R_ScreenToWorld( const Vector &screen, Vector &point );
void R_SetupProjectionMatrix( float fov_x, float fov_y, matrix4x4 &m );
void R_RenderScene( const ref_params_t *pparams );
word GL_RegisterCachedMatrix( const GLfloat MVP[16], const GLfloat MV[16], const Vector &modelorg, const float *texofs = NULL );
void GL_ResetMatrixCache( void );
void R_RotateForEntity( cl_entity_t *e );
void R_TranslateForEntity( cl_entity_t *e );
void R_LoadIdentity( bool force = false );
void R_SetupFrustum( void );

//
// gl_rmisc.cpp
//
void R_NewMap( void );
void R_InitMaterials( void );
matdesc_t *R_FindMaterial( const char *name );

//
// gl_rsurf.cpp
//
void R_MarkLeaves( void );
void R_DrawWorld( void );
void EnableVertexArray( void );
void DisableVertexArray( void );
void GenerateVertexArray( void );
void R_SetupTexMatrix( msurface_t *s, const Vector &origin );
texture_t *R_TextureAnimation( texture_t *base, int surfacenum );
void R_SetupTexMatrix_Reflected( msurface_t *s, const Vector &origin );
void R_DrawBrushModel( cl_entity_t *e, qboolean onlyfirstpass = FALSE );
void R_RecursiveWorldNode( mnode_t *node, const mplane_t frustum[6], unsigned int clipflags );
void R_DrawWaterBrushModel( cl_entity_t *e );
void R_DrawGlassBrushModel( cl_entity_t *e );
void DrawPolyFromArray( msurface_t *s );
void DrawPolyFromArray( glpoly_t *p );
void HUD_BuildLightmaps( void );
void ResetCounters( void );
void FreeBuffer( void );

//
// gl_shader.cpp
//
void GL_BindShader( glsl_program_t *shader );
void GL_InitGPUShaders( void );
void GL_FreeGPUShaders( void );

//
// gl_shadows.cpp
//
void R_RenderShadowmaps( void );

//
// gl_movie.cpp
//
int R_PrecacheCinematic( const char *cinname );
void R_InitCinematics( void );
void R_FreeCinematics( void );
int R_DrawCinematic( msurface_t *surf, texture_t *t );
int R_AllocateCinematicTexture( unsigned int txFlags );
void R_UpdateCinematic( const msurface_t *surf );

//
// gl_mirror.cpp
//
void R_BeginDrawMirror( msurface_t *fa, unsigned int unit );
void R_EndDrawMirror( void );
bool R_FindMirrors( const ref_params_t *fd );
void R_DrawMirrors( cl_entity_t *ignoreent = NULL );

//
// gl_sky.cpp
//
void DrawSky( void );
void InitSky( void );
void ResetSky( void );

//
// gl_postprocess.cpp
//
void InitPostTextures( void );
void InitPostEffects( void );
void RenderDOF( const ref_params_t *fd );
void RenderHDRL( const ref_params_t *fd );
void RenderUnderwaterBlur( void );
void RenderMonochrome( void );

//
// gl_texloader.cpp
//
void UpdateWorldExtradata( const char *worldname );
void DeleteWorldExtradata( void );
void R_ProcessWorldData( model_t *mod, qboolean create );

//
// gl_world.cpp
//
void CreateWorldMeshCache( void );
void DestroyWorldMeshCache( void );
void R_AddBmodelToDrawList( cl_entity_t *e );
void R_AddSurfaceToDrawList( msurface_t *s, unsigned int lightbit = 0 );
void R_AddSolidToDrawList( void );
void R_AddTransToDrawList( void );
void R_RenderSolidBrushList( void );
void R_RenderTransBrushList( void );
void R_RenderShadowBrushList( void );

//
// grass.cpp
//
void GrassInit();
void GrassVidInit();
void GrassDraw();
void GrassCreateEntities();

//
// tri.cpp
//
void RenderFog( void ); //LRC
void BlackFog ( void );

#endif//GL_LOCAL_H