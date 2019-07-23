/*
r_studio.h - studio model rendering
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

#ifndef GL_STUDIO_H
#define GL_STUDIO_H

#include "studio.h"
#include <utllinkedlist.h>
#include <utlarray.h>
#include "gl_decals.h"

// "hand" variable values
#define RIGHT_HAND		0.0f
#define LEFT_HAND		1.0f
#define HIDE_WEAPON		2.0f

#define EVENT_CLIENT		5000	// less than this value it's a server-side studio events
#define DECAL_TRANSPARENT_THRESHOLD	230	// transparent decals draw with GL_MODULATE

#define MESH_GLOWSHELL		BIT( 0 )	// scaled mesh by normals
#define MESH_CHROME			BIT( 1 )	// using chrome texcoords instead of mesh texcoords
#define MESH_NOCOLORS		BIT( 2 )	// ignore color buffer
#define MESH_NOTEXCOORDS		BIT( 3 )	// ignore texcoords buffer
#define MESH_NONORMALS		BIT( 4 )	// ignore normals buffer
#define MESH_DRAWARRAY		BIT( 5 )	// using glDrawArrays instead of glBegin method
#define MESH_COLOR_LIGHTING		BIT( 6 )	// get colors from m_lightvalues
#define MESH_COLOR_ENTITY		BIT( 7 )	// get colors from pev->rendercolor
#define MESH_ALPHA_ENTITY		BIT( 8 )	// get alpha from pev->renderamt

enum
{
	DECAL_CLIP_MINUSU	= 0x1,
	DECAL_CLIP_MINUSV	= 0x2,
	DECAL_CLIP_PLUSU	= 0x4,
	DECAL_CLIP_PLUSV	= 0x8,
};

typedef enum
{
	STUDIO_PASS_NORMAL = 0,		// one pass. combine vertex lighting and diffuse texture
	STUDIO_PASS_SHADOW,			// shadow pass. store vertices only (an optional enable texture mask for STUDIO_NF_TRANSPARENT)
	STUDIO_PASS_AMBIENT,		// lighting pass. build ambient map without texture (vertex color only)
	STUDIO_PASS_LIGHT,			// lighting pass. draw projected light, attenuation texture and shadow map
	STUDIO_PASS_DIFFUSE,		// diffuse pass. combine lighting info with diffuse texture
	STUDIO_PASS_GLOWSHELL,		// special case for kRenderFxGlowShell
} StudioPassMode;

struct CStudioLight
{
	Vector		lightVec;			// light vector
	Vector		lightColor;		// ambient light color
	Vector		lightDiffuse;		// lighting diffuse color
	Vector		lightOrigin;		// for debug purpoces

	Vector		blightVec[MAXSTUDIOBONES];	// ambient lightvectors per bone
	Vector		elightVec[MAX_ELIGHTS][MAXSTUDIOBONES];
	Vector		elightColor[MAX_ELIGHTS];	// ambient entity light colors
	int		numElights;
};

// used for build decal projection
struct DecalMesh_t
{
	int	firstvertex;
	int	numvertices;
};

struct DecalVertex_t
{
	mstudiomesh_t *GetMesh( studiohdr_t *pHdr )
	{
		if(( m_Body == 0xFFFF ) || ( m_Model == 0xFFFF ) || ( m_Mesh == 0xFFFF ))
		{
			return NULL;
		}

		mstudiobodyparts_t *pBody = (mstudiobodyparts_t *)((byte *)pHdr + pHdr->bodypartindex) + m_Body;
		mstudiomodel_t *pModel = (mstudiomodel_t *)((byte *)pHdr + pBody->modelindex) + m_Model;
		return (mstudiomesh_t *)((byte *)pHdr + pModel->meshindex) + m_Mesh;
	}

	mstudiomodel_t *GetModel( studiohdr_t *pHdr )
	{
		if(( m_Body == 0xFFFF ) || ( m_Model == 0xFFFF ))
		{
			return NULL;
		}

		mstudiobodyparts_t *pBody = (mstudiobodyparts_t *)((byte *)pHdr + pHdr->bodypartindex) + m_Body;
		return (mstudiomodel_t *)((byte *)pHdr + pBody->modelindex) + m_Model;
	}

	Vector	m_Position;
	Vector	m_Normal;
	Vector2D	m_TexCoord;

	word	m_MeshVertexIndex;	// index into the mesh's vertex list
	word	m_Body;
	word	m_Model;
	word	m_Mesh;
	byte	m_Bone;		// bone that transform this vertex
};

struct DecalClipState_t
{
	// Number of used vertices
	int		m_VertCount;

	// Indices into the clip verts array of the used vertices
	int		m_Indices[2][7];

	// Helps us avoid copying the m_Indices array by using double-buffering
	bool		m_Pass;

	// Add vertices we've started with and had to generate due to clipping
	int		m_ClipVertCount;
	DecalVertex_t	m_ClipVerts[16];

	// Union of the decal triangle clip flags above for each vert
	int		m_ClipFlags[16];
};

// 64 bytes here
typedef struct xvert_s
{
	Vector		vertex;		// position
	Vector		normal;		// normal
	Vector		tangent;		// tangent
	Vector		binormal;		// binormal
	float		stcoord[2];	// ST texture coords
	float		boneid;		
	byte		color[4];		// padding for now
} xvert_t;

/*
====================
CStudioModelRenderer

====================
*/
class CStudioModelRenderer
{
public:
	// Construction/Destruction
	CStudioModelRenderer( void );
	virtual ~CStudioModelRenderer( void );

	// Initialization
	virtual void Init( void );

public:  
	// Public Interfaces
	virtual int StudioDrawModel ( int flags );
	virtual int StudioDrawPlayer ( int flags, struct entity_state_s *pplayer );

public:
	// Local interfaces

	// Look up animation data for sequence
	virtual mstudioanim_t *StudioGetAnim ( model_t *m_pSubModel, mstudioseqdesc_t *pseqdesc );

	// Extract bbox from current sequence
	virtual int StudioExtractBbox ( cl_entity_t *e, studiohdr_t *phdr, int sequence, Vector &mins, Vector &maxs );

	// Compute a full bounding box for current sequence
	virtual int StudioComputeBBox ( cl_entity_t *e, Vector bbox[8] );

	// Interpolate model position and angles and set up matrices
	virtual void StudioSetUpTransform( matrix3x4 &matrix );

	// Set up model bone positions
	virtual void StudioSetupBones( matrix3x4 &transform, matrix3x4 bonetransform[] );	

	// Find final attachment points
	virtual void StudioCalcAttachments( matrix3x4 bones[] );
	
	// Save bone matrices and names
	virtual void StudioSaveBones( void );

	// Merge cached bones with current bones for model
	virtual void StudioMergeBones( matrix3x4 &transform, matrix3x4 bones[], matrix3x4 cached_bones[], model_t *pModel, model_t *pParentModel );

	// Determine interpolation fraction
	virtual float StudioEstimateInterpolant( void );

	// Determine current frame for rendering
	virtual float StudioEstimateFrame ( mstudioseqdesc_t *pseqdesc );

	// Apply special effects to transform matrix
	virtual void StudioFxTransform( cl_entity_t *ent, matrix3x4 &transform );

	// Spherical interpolation of bones
	virtual void StudioSlerpBones ( Vector4D q1[], Vector pos1[], Vector4D q2[], Vector pos2[], float s );

	// Compute bone adjustments ( bone controllers )
	virtual void StudioCalcBoneAdj ( float dadt, float *adj, const byte *pcontroller1, const byte *pcontroller2, byte mouthopen );

	// Get bone quaternions
	virtual void StudioCalcBoneQuaterion ( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, Vector4D &q );

	// Get bone positions
	virtual void StudioCalcBonePosition ( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, Vector &pos );

	// Compute rotations
	virtual void StudioCalcRotations ( Vector pos[], Vector4D q[], mstudioseqdesc_t *pseqdesc, mstudioanim_t *panim, float f );

	// Compute chrome per bone
	virtual void StudioSetupChrome( float *pchrome, int bone, Vector normal );

	// Send bones and verts to renderer
	virtual void StudioRenderModel ( void );

	//calc bodies and get pointers to him
	virtual int StudioSetupModel ( int bodypart, void **ppbodypart, void **ppsubmodel );

	virtual int StudioCheckBBox( void );

	// Finalize rendering
	virtual void StudioRenderFinal( void );

	virtual void StudioSetRenderMode( const int rendermode );

	virtual void StudioGammaCorrection( bool enable );

	inline void QSortStudioMeshes( gl_studiomesh_t *meshes, int Li, int Ri );

	inline void DrawMeshFromBuffer( const vbomesh_t *mesh );
	
	// Player specific data
	// Determine pitch and blending amounts for players
	virtual void StudioPlayerBlend( mstudioseqdesc_t *pseqdesc, int &pBlend, float &pPitch );

	// Estimate gait frame for player
	virtual void StudioEstimateGait( entity_state_t *pplayer );

	// Process movement of player
	virtual void StudioProcessGait( entity_state_t *pplayer, Vector &angles );

	// Process studio client events
	virtual void StudioClientEvents( void );

	virtual void StudioStaticLight( cl_entity_t *ent, matrix3x4 &transform, lightinfo_t *lightinfo );

	virtual void StudioDynamicLight( cl_entity_t *ent, alight_t *lightinfo );

	virtual void StudioEntityLight( alight_t *lightinfo );

	virtual void StudioSetupLighting( alight_t *lightinfo );

	virtual void StudioLighting( Vector &lv, int bone, int flags, const Vector &normal );

	virtual void StudioDrawPoints( void );

	virtual void StudioDrawMeshes( mstudiotexture_t *ptexture, short *pskinref, StudioPassMode drawPass );

	virtual void StudioDrawMesh( short *ptricmds, float s, float t, int iModeFlags );

	virtual void StudioFormatAttachment( int nAttachment, bool bInverse );

	virtual word ChooseStudioProgram( studiohdr_t *phdr, mstudiomaterial_t *mat, unsigned int lightbit = 0 );

	virtual void AddMeshToDrawList( studiohdr_t *phdr, const vbomesh_t *mesh, unsigned int lightbit = 0 );

	virtual void AddBodyPartToDrawList( studiohdr_t *phdr, int bodypart, unsigned int lightbit = 0 );

	// Setup the rendermode, smooth model etc
	virtual void StudioSetupRenderer( int rendermode );

	// Restore renderer state
	virtual void StudioRestoreRenderer( void );

	// Debug drawing
	void StudioDrawHulls( int iHitbox );

	void StudioDrawAbsBBox( void );

	void StudioDrawBones( void );

	void StudioDrawAttachments( void );

	void DrawLightDirection( void );

	int HeadShieldThink( void );

	// intermediate structure. Used only for build unique submodels
	struct TmpModel_t
	{
		char		name[64];
		mstudiomodel_t	*pmodel;
		msubmodel_t	*pout;
	};

	struct ModelInstance_t
	{
		cl_entity_t	*m_pEntity;

		// Need to store off the model. When it changes, we lose all instance data..
		model_t		*m_pModel;
		word		m_DecalHandle;
		int		m_DecalCount;	// just used as timestamp for calculate decal depth

		lightinfo_t	light;		// cached light values
		matrix3x4		m_protationmatrix;
		matrix3x4		m_pbones[MAXSTUDIOBONES];
		matrix3x4		m_pwpnbones[MAXSTUDIOBONES];
		GLfloat		m_glbones[MAXSTUDIOBONES][16];
		GLfloat		m_glwpnbones[MAXSTUDIOBONES][16];	// used for p_models on player or NPC
		unsigned int	cached_frame;	// to avoid compute bones more than once per frame
	};

	struct Decal_t
	{
		int	m_IndexCount;
		int	m_VertexCount;

		// used for decal serialize
		const DecalGroupEntry	*texinfo;
		modelstate_t		state;
		int			depth;
		Vector			vecLocalStart;
		Vector			vecLocalEnd;
		byte			flags;	// decal shared flags
	};

	struct DecalHistory_t
	{
		word	m_Material;
		word	m_Decal;
	};

	typedef CUtlLinkedList<DecalVertex_t, word> DecalVertexList_t;
	typedef CUtlArray<word> DecalIndexList_t;
	typedef CUtlLinkedList<Decal_t, word> DecalList_t;
	typedef CUtlLinkedList<DecalHistory_t, word> DecalHistoryList_t;
	typedef CUtlArray<int> CIntVector;

	struct DecalMaterial_t
	{
		int		decalTexture;
		DecalIndexList_t	m_Indices;
		DecalVertexList_t	m_Vertices;
		DecalList_t	m_Decals;
	};

	struct DecalModelList_t
	{
		word		m_FirstMaterial;
		DecalHistoryList_t	m_DecalHistory;
	};

	struct DecalVertexInfo_t
	{
		Vector2D	m_UV;
		word	m_VertexIndex;	// index into the DecalVertex_t list
		bool	m_FrontFacing;
		bool	m_InValidArea;
	};

	struct DecalBuildInfo_t
	{
		studiohdr_t*		m_pStudioHeader;
		const DecalGroupEntry*	m_pTexInfo;
		mstudiomesh_t*		m_pMesh;
		DecalMesh_t*		m_pDecalMesh;
		DecalMaterial_t*		m_pDecalMaterial;
		float			m_Radius;
		DecalVertexInfo_t*		m_pVertexInfo;
		int			m_Body;
		int			m_Mesh;
		int			m_Model;
		word			m_FirstVertex;
		word			m_VertexCount;
		bool			m_UseClipVert;
	};

	// Stores all decals for a particular material and lod
	CUtlLinkedList< DecalMaterial_t, word >	m_DecalMaterial;

	// Stores all decal lists that have been made
	CUtlLinkedList< DecalModelList_t, word > m_DecalList;

	// keep model instances for each entity
	CUtlLinkedList< ModelInstance_t, word > m_ModelInstances;

	// decal stuff
	virtual bool ComputePoseToDecal( const Vector &vecStart, const Vector &vecEnd );
	virtual void AddDecalToModel( DecalBuildInfo_t& buildInfo );
	virtual void AddDecalToMesh( DecalBuildInfo_t& buildInfo );
	virtual void ProjectDecalOntoMesh( DecalBuildInfo_t& build );
	virtual bool IsFrontFacing( const Vector& norm, byte vertexBone );
	virtual bool TransformToDecalSpace( DecalBuildInfo_t& build, const Vector& pos, byte vertexBone, Vector2D& uv );
	virtual void AddTriangleToDecal( DecalBuildInfo_t& build, int i1, int i2, int i3 );
	virtual int ComputeClipFlags( Vector2D const& uv );
	virtual bool ClipDecal( DecalBuildInfo_t& build, int i1, int i2, int i3, int *pClipFlags );
	virtual void ConvertMeshVertexToDecalVertex( DecalBuildInfo_t& build, int meshIndex, DecalVertex_t& decalVertex );
	virtual void ClipTriangleAgainstPlane( DecalClipState_t& state, int normalInd, int flag, float val );
	virtual int IntersectPlane( DecalClipState_t& state, int start, int end, int normalInd, float val );
	virtual word AddVertexToDecal( DecalBuildInfo_t& build, int meshIndex );
	virtual word AddVertexToDecal( DecalBuildInfo_t& build, DecalVertex_t& vert );
	virtual void AddClippedDecalToTriangle( DecalBuildInfo_t& build, DecalClipState_t& clipState );
	virtual int GetDecalMaterial( DecalModelList_t& decalList, int decalTexture );
	virtual bool ShouldRetireDecal( DecalMaterial_t* pDecalMaterial, DecalHistoryList_t const& decalHistory );
	virtual int AddDecalToMaterialList( DecalMaterial_t* pMaterial );
	virtual void RetireDecal( DecalHistoryList_t& historyList );
	virtual void DestroyDecalList( word handle );
	virtual word CreateDecalList( void );
	virtual bool IsModelInstanceValid( word handle );
	virtual word CreateInstance( cl_entity_t *pEnt );
	virtual void DestroyInstance( word handle );
	virtual void ComputeDecalTransform( DecalMaterial_t& decalMaterial, const matrix3x4 bones[] );
	virtual void DrawDecalMaterial( DecalMaterial_t& decalMaterial, const matrix3x4 bones[] );
	virtual void DrawDecal( cl_entity_t *e );

	virtual void CreateMeshCache( void );
	virtual void DestroyMeshCache( void );

	virtual void LoadStudioMaterials( void );
	virtual void FreeStudioMaterials( void );

	virtual bool StudioLightingIntersect( void );

	inline void MeshCreateBuffer( vbomesh_t *pDst, const mstudiomesh_t *pSrc, const mstudiomodel_t *pSubModel );

	Vector			studio_mins, studio_maxs;
	float			studio_radius;

	// Client clock
	double			m_clTime;
	// Old Client clock
	double			m_clOldTime;			

	// Do interpolation?
	int			m_fDoInterp;			
	// Do gait estimation?
	int			m_fGaitEstimation;		

	// Current render frame #
	int			m_nFrameCount;

	bool			m_fDrawViewModel;
	float			m_flViewmodelFov;

	bool			m_fHasProjectionLighting;

	// Cvars that studio model code needs to reference
	cvar_t			*m_pCvarHiModels;	// Use high quality models?	
	cvar_t			*m_pCvarLerping;	// Use lerping for animation?
	cvar_t			*m_pCvarLambert;	// lambert value for model lighting
	cvar_t			*m_pCvarDrawViewModel;
	cvar_t			*m_pCvarHand;	// handness
	cvar_t			*m_pCvarViewmodelFov;
	cvar_t			*m_pCvarLegsOffset;

	// The entity which we are currently rendering.
	cl_entity_t		*m_pCurrentEntity;		

	// The model for the entity being rendered
	model_t			*m_pRenderModel;

	ModelInstance_t		*m_pModelInstance;

	// Player info for current player, if drawing a player
	player_info_t		*m_pPlayerInfo;

	// The index of the player being drawn
	int			m_nPlayerIndex;

	// Current model rendermode
	int			m_iRenderMode;

	// The player's gait movement
	float			m_flGaitMovement;

	// Pointer to header block for studio model data
	studiohdr_t		*m_pStudioHeader;
	
	// Pointers to current body part and submodel
	mstudiobodyparts_t 		*m_pBodyPart;
	mstudiomodel_t		*m_pSubModel;
	int			m_iBodyPartIndex;

	// Palette substition for top and bottom of model
	int			m_nTopColor;			
	int			m_nBottomColor;

	// set force flags (e.g. chrome)
	int			m_nForceFaceFlags;

	//
	// Sprite model used for drawing studio model chrome
	model_t			*m_pChromeSprite;

	CStudioLight		*m_pLightInfo;

	Vector			m_lightvalues[MAXSTUDIOVERTS];
	float			*m_pvlightvalues;

	int			m_chromeAge[MAXSTUDIOBONES];	// last time chrome vectors were updated
	float			m_chrome[MAXSTUDIOVERTS][2];	// texture coords for surface normals
	Vector			m_chromeRight[MAXSTUDIOBONES];// chrome vector "right" in bone reference frames
	Vector			m_chromeUp[MAXSTUDIOBONES];	// chrome vector "up" in bone reference frames
	GLfloat			m_glbones[MAXSTUDIOBONES][16];

	Vector			m_verts[MAXSTUDIOVERTS];
	Vector			m_norms[MAXSTUDIOVERTS];
	Vector			m_arrayverts[MAXARRAYVERTS];
	Vector			m_arraynorms[MAXARRAYVERTS];
	Vector2D			m_arraycoord[MAXARRAYVERTS];
	byte			m_vertexbone[MAXARRAYVERTS];
	byte			m_arraycolor[MAXARRAYVERTS][4];
	xvert_t			m_arrayxvert[MAXARRAYVERTS];
	unsigned short		m_arrayelems[MAXARRAYVERTS*6];
	unsigned short		m_nNumArrayVerts;
	unsigned short		m_nNumArrayElems;

	// Caching

	// Cached bone & light transformation matrices
	matrix3x4			m_rgCachedBoneTransform [MAXSTUDIOBONES];

	model_t			*m_pParentModel;
	
	// Model render counters ( from engine )
	int			m_nStudioModelCount;
	int			m_nModelsDrawn;

	// Matrices
	// Model to world transformation
	matrix3x4			m_protationmatrix;	

	// Concatenated bone and light transforms
	matrix3x4			m_pbonetransform[MAXSTUDIOBONES];
	matrix3x4			m_pdecaltransform[MAXSTUDIOBONES];

	bool			m_fDrawFaceProtect;	// headshield or gasmask

	bool			m_fDrawPlayerLegs;	// draw player legs
public:
	void	DestroyAllModelInstances( void );

	void	StudioDecalShoot( const Vector &vecSrc, const Vector &vecEnd, const char *name, cl_entity_t *ent, int flags, modelstate_t *state );
	int	StudioDecalList( decallist_t *pList, int count, qboolean changelevel );
	void	StudioClearDecals( void );
	void	RemoveAllDecals( int entityIndex );

	void	ProcessUserData( model_t *mod, qboolean create );

	void	AddStudioModelToDrawList( cl_entity_t *e );

	// Draw generic studiomodel (player too)
	void	DrawStudioModelInternal( cl_entity_t *e, qboolean follow_entity );

	// Process viewmodel events (at start the frame so muzzleflashes will be correct added)
	void	RunViewModelEvents( void );

	// Draw view model (at end the frame)
	void	DrawViewModel( void );

	// draw head shield (after viewmodel)
	void	DrawHeadShield( void );

	void	RenderSolidStudioList( void );

	void	RenderTransStudioList( void );

	void	RenderShadowStudioList( void );

	void	RenderDynLightList( void );

	void	BuildMeshListForLight( DynamicLight *pl );

	void	DrawLightForMeshList( DynamicLight *pl );

	void	AddStudioToLightList( cl_entity_t *e, DynamicLight *pl );

	int	CacheCount( void ) { return m_ModelInstances.Count(); }
};

extern CStudioModelRenderer g_StudioRenderer;

// implementation of drawing funcs
inline void R_DrawStudioModel( cl_entity_t *e ) { g_StudioRenderer.DrawStudioModelInternal( e, false ); }
inline void R_RunViewmodelEvents( void ) { g_StudioRenderer.RunViewModelEvents(); }
inline void R_DrawViewModel( void ) { g_StudioRenderer.DrawViewModel(); }
inline void R_DrawHeadShield( void ) { g_StudioRenderer.DrawHeadShield(); }
inline void R_ProcessStudioData( model_t *mod, qboolean create )
{
	if( mod->type == mod_studio )
		g_StudioRenderer.ProcessUserData( mod, create );
}

inline int R_CreateStudioDecalList( decallist_t *pList, int count, qboolean changelevel )
{
	return g_StudioRenderer.StudioDecalList( pList, count, changelevel );
}

inline void R_ClearStudioDecals( void )
{
	g_StudioRenderer.StudioClearDecals();
}

inline void R_RenderSolidStudioList( void )
{
	g_StudioRenderer.RenderSolidStudioList();
}

inline void R_RenderTransStudioList( void )
{
	g_StudioRenderer.RenderTransStudioList();
}

inline void R_RenderShadowStudioList( void )
{
	g_StudioRenderer.RenderShadowStudioList();
}

inline void R_AddStudioToDrawList( cl_entity_t *e )
{
	g_StudioRenderer.AddStudioModelToDrawList( e );
}

#endif// GL_STUDIO_H