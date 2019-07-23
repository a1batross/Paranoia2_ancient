/*
gl_shader.cpp - glsl shaders
Copyright (C) 2014 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "ref_params.h"
#include "gl_local.h"
#include <mathlib.h>
#include <stringlib.h>

static char *GL_PrintInfoLog( GLhandleARB object )
{
	static char	msg[4096];
	int		maxLength = 0;

	pglGetObjectParameterivARB( object, GL_OBJECT_INFO_LOG_LENGTH_ARB, &maxLength );

	if( maxLength >= sizeof( msg ))
	{
		ALERT( at_warning, "GL_PrintInfoLog: message exceeds %i symbols\n", sizeof( msg ));
		maxLength = sizeof( msg ) - 1;
	}

	pglGetInfoLogARB( object, maxLength, &maxLength, msg );

	return msg;
}

static char *GL_PrintShaderSource( GLhandleARB object )
{
	static char	msg[4096];
	int		maxLength = 0;
	
	pglGetObjectParameterivARB( object, GL_OBJECT_SHADER_SOURCE_LENGTH_ARB, &maxLength );

	if( maxLength >= sizeof( msg ))
	{
		ALERT( at_warning, "GL_PrintShaderSource: message exceeds %i symbols\n", sizeof( msg ));
		maxLength = sizeof( msg ) - 1;
	}

	pglGetShaderSourceARB( object, maxLength, &maxLength, msg );

	return msg;
}

/*
==============
R_LoadIncludes

Search shader texts for '#include' directives
and insert included file contents.
==============
*/
static char *GL_LoadIncludes( char *buffer )
{
	char	filename[128], token[1024];
	char	*pfile, *afile, *oldbuffer;
	int	size1, limit = 64; // limit for prevent infinity recursion

	// calculate size of glsl with includes
	size1 = Q_strlen( buffer );
	pfile = buffer;

	while( 1 )
	{
		afile = pfile;
		pfile = COM_ParseFile( pfile, token );
		if( !token[0] ) break;

		if( !Q_strcmp( token, "#include" ))
		{
			char	*incfile;
			int	size2;

			if( limit < 0 )
				HOST_ERROR( "GL_LoadIncludes: 64 includes reached (probably infinite loop)\n" );

			pfile = COM_ParseLine( pfile, token );
			Q_snprintf( filename, sizeof( filename ), "glsl/%s", token );
			incfile = (char *)gEngfuncs.COM_LoadFile( filename, 5, &size2 );
			if( !incfile )
			{
				ALERT( at_error, "couldn't load %s", filename );
				continue;
			}

			oldbuffer = buffer;
			int bufferSize = size1 + size2 + 2;
			buffer = (char *)Mem_Alloc( bufferSize );
			memcpy( buffer, oldbuffer, afile - oldbuffer );
			Q_strncat( buffer, "\n", bufferSize - 1 );
			Q_strncat( buffer, incfile, bufferSize - 1 );
			Q_strncat( buffer, pfile, bufferSize - 1 );
			pfile = afile - oldbuffer + buffer;
			size1 = Q_strlen( buffer );
			gEngfuncs.COM_FreeFile( incfile );
			limit--;
		}
	}

	return buffer;
}

static void GL_LoadGPUShader( glsl_program_t *shader, const char *name, GLenum shaderType )
{
	char		filename[64];
	static char	params[1024];
	GLcharARB		*buffer = NULL;
	GLcharARB		*file = NULL;
	GLhandleARB	object;
	GLint		compiled;
	int		size;

	ASSERT( shader != NULL );

	if( shaderType == GL_VERTEX_SHADER_ARB )
	{
		Q_snprintf( filename, sizeof( filename ), "glsl/%s_vp.glsl", name );
	}
	else
	{
		Q_snprintf( filename, sizeof( filename ), "glsl/%s_fp.glsl", name );
	}

	file = (GLcharARB *)gEngfuncs.COM_LoadFile( filename, 5, &size );

	if( !file )
	{
		ALERT( at_error, "Couldn't load %s\n", filename );
		return;
	}

	memset( params, 0, sizeof( params ));

	// add internal defines
	Q_strncat( params, "#ifndef M_PI\n#define M_PI 3.14159265358979323846\n#endif\n", sizeof( params ));
	Q_strncat( params, va( "#ifndef MAXSTUDIOBONES\n#define MAXSTUDIOBONES %i\n#endif\n", glConfig.max_skinning_bones ), sizeof( params ));
	Q_strncat( params, "#extension GL_EXT_gpu_shader4 : enable\n", sizeof( params )); // support bitwise ops
	Q_strncat( params, "#line 0\n", sizeof( params )); // reset line counting

	int bufferSize = Q_strlen( params ) + size + 2;
	buffer = (GLcharARB *)Mem_Alloc( bufferSize );
	Q_strncpy( buffer, params, bufferSize );
	Q_strncat( buffer, file, bufferSize );
	gEngfuncs.COM_FreeFile( file );

	// load possibility includes
	buffer = GL_LoadIncludes( buffer );
	bufferSize = Q_strlen( buffer );

	ALERT( at_aiconsole, "loading '%s'\n", filename );
	object = pglCreateShaderObjectARB( shaderType );
	pglShaderSourceARB( object, 1, (const GLcharARB **)&buffer, &bufferSize );

	// compile shader
	pglCompileShaderARB( object );

	// check if shader compiled
	pglGetObjectParameterivARB( object, GL_OBJECT_COMPILE_STATUS_ARB, &compiled );

	if( !compiled )
	{
		ALERT( at_error, "Couldn't compile %s\n%s", filename, GL_PrintInfoLog( object ));
		Mem_Free( buffer );
		return;
	}

	if( shaderType == GL_VERTEX_SHADER_ARB )
		shader->status |= SHADER_VERTEX_COMPILED;
	else shader->status |= SHADER_FRAGMENT_COMPILED;

	// attach shader to program
	pglAttachObjectARB( shader->handle, object );

	// delete shader, no longer needed
	pglDeleteObjectARB( object );

	Mem_Free( buffer );
}

static void GL_LinkProgram( glsl_program_t *shader )
{
	GLint	linked = 0;

	if( !shader ) return;

	pglLinkProgramARB( shader->handle );

	pglGetObjectParameterivARB( shader->handle, GL_OBJECT_LINK_STATUS_ARB, &linked );
	if( !linked ) ALERT( at_error, "%s\n%s shader failed to link\n", GL_PrintInfoLog( shader->handle ), shader->name );
	else shader->status |= SHADER_PROGRAM_LINKED;
}

static void GL_ValidateProgram( glsl_program_t *shader )
{
	GLint	validated = 0;

	if( !shader ) return;

	pglValidateProgramARB( shader->handle );

	pglGetObjectParameterivARB( shader->handle, GL_OBJECT_VALIDATE_STATUS_ARB, &validated );
	if( !validated ) ALERT( at_error, "%s\n%s shader failed to validate\n", GL_PrintInfoLog( shader->handle ), shader->name );
}

void GL_ShowProgramUniforms( glsl_program_t *shader )
{
	int	count, size;
	char	uniformName[256];
	GLuint	type;

	if( !shader || developer_level < at_aiconsole )
		return;
	
	// install the executables in the program object as part of current state.
	pglUseProgramObjectARB( shader->handle );

	// query the number of active uniforms
	pglGetObjectParameterivARB( shader->handle, GL_OBJECT_ACTIVE_UNIFORMS_ARB, &count );

	// Loop over each of the active uniforms, and set their value
	for( int i = 0; i < count; i++ )
	{
		pglGetActiveUniformARB( shader->handle, i, sizeof( uniformName ), NULL, &size, &type, uniformName );
		ALERT( at_aiconsole, "active uniform: '%s'\n", uniformName );
	}
	
	pglUseProgramObjectARB( GL_NONE );
}

void GL_BindShader( glsl_program_t *shader )
{
	if( !shader && RI.currentshader )
	{
		pglUseProgramObjectARB( GL_NONE );
		r_stats.num_shader_binds++;
		RI.currentshader = NULL;
	}
	else if( shader != RI.currentshader && shader->handle )
	{
		pglUseProgramObjectARB( shader->handle );
		r_stats.num_shader_binds++;
		RI.currentshader = shader;
	}
}

static glsl_program_t *GL_InitGPUShader( const char *glname, const char *vpname, const char *fpname )
{
	if( !GL_Support( R_SHADER_GLSL100_EXT ))
		return NULL;

	ASSERT( glname != NULL );

	// first check for coexist
	for( int i = 0; i < tr.num_glsl_programs; i++ )
	{
		if( !Q_stricmp( tr.glsl_programs[i].name, glname ))
			return &tr.glsl_programs[i];
	}

	if( tr.num_glsl_programs >= MAX_GLSL_PROGRAMS )
	{
		ALERT( at_error, "GL_InitGPUShader: GLSL shaders limit exceeded (%i max)\n", MAX_GLSL_PROGRAMS );
		return NULL;
	}

	// alloc new shader
	glsl_program_t *shader = &tr.glsl_programs[i];
	tr.num_glsl_programs++;

	Q_strncpy( shader->name, glname, sizeof( shader->name ));
	shader->handle = pglCreateProgramObjectARB();

	if( vpname ) GL_LoadGPUShader( shader, vpname, GL_VERTEX_SHADER_ARB );
	if( fpname ) GL_LoadGPUShader( shader, fpname, GL_FRAGMENT_SHADER_ARB );

	pglBindAttribLocationARB( shader->handle, ATTR_INDEX_POSITION, "attr_Position" );
	pglBindAttribLocationARB( shader->handle, ATTR_INDEX_TEXCOORD0, "attr_TexCoord0" );
	pglBindAttribLocationARB( shader->handle, ATTR_INDEX_TEXCOORD1, "attr_TexCoord1" );
	pglBindAttribLocationARB( shader->handle, ATTR_INDEX_TANGENT, "attr_Tangent" );
	pglBindAttribLocationARB( shader->handle, ATTR_INDEX_BINORMAL, "attr_Binormal" );
	pglBindAttribLocationARB( shader->handle, ATTR_INDEX_NORMAL, "attr_Normal" );
	pglBindAttribLocationARB( shader->handle, ATTR_INDEX_BONE_INDEXES, "attr_BoneIndexes" );

	GL_LinkProgram( shader );

	return shader;
}

static void GL_FreeGPUShader( glsl_program_t *shader )
{
	if( shader && shader->handle )
	{
		pglDeleteObjectARB( shader->handle );
		memset( shader, 0, sizeof( *shader ));
	}
}

static void GL_InitBmodelUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_BaseLightMap = pglGetUniformLocationARB( shader->handle, "u_BaseLightMap" );
	shader->u_AddLightMap = pglGetUniformLocationARB( shader->handle, "u_AddLightMap" );
	shader->u_DeluxeMap = pglGetUniformLocationARB( shader->handle, "u_DeluxeMap" );
	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_DetailMap = pglGetUniformLocationARB( shader->handle, "u_DetailMap" );
	shader->u_NormalMap = pglGetUniformLocationARB( shader->handle, "u_NormalMap" );
	shader->u_GlossMap = pglGetUniformLocationARB( shader->handle, "u_GlossMap" );
	shader->u_DepthMap = pglGetUniformLocationARB( shader->handle, "u_DepthMap" );

	shader->u_DetailMode = pglGetUniformLocationARB( shader->handle, "u_DetailMode" );
	shader->u_ParallaxMode = pglGetUniformLocationARB( shader->handle, "u_ParallaxMode" );
	shader->u_LightmapDebug = pglGetUniformLocationARB( shader->handle, "u_DebugMode" );
	shader->u_SpecularMode = pglGetUniformLocationARB( shader->handle, "u_SpecularMode" );
	shader->u_ModelViewProjectionMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelViewProjectionMatrix" );
	shader->u_ModelViewMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelViewMatrix" );
	shader->u_ViewOrigin = pglGetUniformLocationARB( shader->handle, "u_ViewOrigin" );
	shader->u_FaceFlags = pglGetUniformLocationARB( shader->handle, "u_FaceFlags" );
	shader->u_DetailScale = pglGetUniformLocationARB( shader->handle, "u_DetailScale" );
	shader->u_GlossExponent = pglGetUniformLocationARB( shader->handle, "u_GlossExponent" );
	shader->u_ParallaxScale = pglGetUniformLocationARB( shader->handle, "u_ParallaxScale" );
	shader->u_ParallaxSteps = pglGetUniformLocationARB( shader->handle, "u_ParallaxSteps" );
	shader->u_ClipPlane = pglGetUniformLocationARB( shader->handle, "u_ClipPlane" );
	shader->u_RemapParms = pglGetUniformLocationARB( shader->handle, "u_LightRemap" );
	shader->u_ReflectScale = pglGetUniformLocationARB( shader->handle, "u_ReflectScale" );
	shader->u_RefractScale = pglGetUniformLocationARB( shader->handle, "u_RefractScale" );
	shader->u_MirrorMode = pglGetUniformLocationARB( shader->handle, "u_MirrorMode" );
	shader->u_TexOffset = pglGetUniformLocationARB( shader->handle, "u_TexOffset" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_BaseLightMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_AddLightMap, GL_TEXTURE1 );
	pglUniform1iARB( shader->u_DeluxeMap, GL_TEXTURE2 );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE3 );
	pglUniform1iARB( shader->u_DetailMap, GL_TEXTURE4 );
	pglUniform1iARB( shader->u_NormalMap, GL_TEXTURE5 );
	pglUniform1iARB( shader->u_GlossMap, GL_TEXTURE6 );
	pglUniform1iARB( shader->u_DepthMap, GL_TEXTURE7 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitTransBmodelUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_DetailMap = pglGetUniformLocationARB( shader->handle, "u_DetailMap" );
	shader->u_NormalMap = pglGetUniformLocationARB( shader->handle, "u_NormalMap" );
	shader->u_DepthMap = pglGetUniformLocationARB( shader->handle, "u_DepthMap" );

	shader->u_DetailMode = pglGetUniformLocationARB( shader->handle, "u_DetailMode" );
	shader->u_ModelViewProjectionMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelViewProjectionMatrix" );
	shader->u_ModelViewMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelViewMatrix" );
	shader->u_ViewOrigin = pglGetUniformLocationARB( shader->handle, "u_ViewOrigin" );
	shader->u_FaceFlags = pglGetUniformLocationARB( shader->handle, "u_FaceFlags" );
	shader->u_DetailScale = pglGetUniformLocationARB( shader->handle, "u_DetailScale" );
	shader->u_ClipPlane = pglGetUniformLocationARB( shader->handle, "u_ClipPlane" );
	shader->u_ReflectScale = pglGetUniformLocationARB( shader->handle, "u_ReflectScale" );
	shader->u_RefractScale = pglGetUniformLocationARB( shader->handle, "u_RefractScale" );
	shader->u_TexOffset = pglGetUniformLocationARB( shader->handle, "u_TexOffset" );
	shader->u_RenderColor = pglGetUniformLocationARB( shader->handle, "u_RenderColor" );
	shader->u_MirrorMode = pglGetUniformLocationARB( shader->handle, "u_MirrorMode" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_DetailMap, GL_TEXTURE1 );
	pglUniform1iARB( shader->u_NormalMap, GL_TEXTURE2 );
	pglUniform1iARB( shader->u_DepthMap, GL_TEXTURE3 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitBmodelDecalUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_DecalMap = pglGetUniformLocationARB( shader->handle, "u_DecalMap" );
	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_DepthMap = pglGetUniformLocationARB( shader->handle, "u_DepthMap" );
	shader->u_NormalMap = pglGetUniformLocationARB( shader->handle, "u_NormalMap" );
	shader->u_ScreenMap = pglGetUniformLocationARB( shader->handle, "u_ScreenMap" );
	shader->u_ModelViewProjectionMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelViewProjectionMatrix" );
	shader->u_ModelViewMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelViewMatrix" );
	shader->u_FaceFlags = pglGetUniformLocationARB( shader->handle, "u_FaceFlags" );
	shader->u_ParallaxScale = pglGetUniformLocationARB( shader->handle, "u_ParallaxScale" );
	shader->u_ParallaxSteps = pglGetUniformLocationARB( shader->handle, "u_ParallaxSteps" );
	shader->u_ParallaxMode = pglGetUniformLocationARB( shader->handle, "u_ParallaxMode" );
	shader->u_ClipPlane = pglGetUniformLocationARB( shader->handle, "u_ClipPlane" );
	shader->u_RealTime = pglGetUniformLocationARB( shader->handle, "u_RealTime" );
	shader->u_LightDir = pglGetUniformLocationARB( shader->handle, "u_LightDir" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_DecalMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE1 );
	pglUniform1iARB( shader->u_DepthMap, GL_TEXTURE2 );
	pglUniform1iARB( shader->u_NormalMap, GL_TEXTURE3 );
	pglUniform1iARB( shader->u_ScreenMap, GL_TEXTURE4 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitBmodelDynLightUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ProjectMap = pglGetUniformLocationARB( shader->handle, "u_ProjectMap" );
	shader->u_AttnZMap = pglGetUniformLocationARB( shader->handle, "u_AttnZMap" );
	shader->u_ShadowMap = pglGetUniformLocationARB( shader->handle, "u_ShadowMap" );
	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_DetailMap = pglGetUniformLocationARB( shader->handle, "u_DetailMap" );
	shader->u_NormalMap = pglGetUniformLocationARB( shader->handle, "u_NormalMap" );
	shader->u_GlossMap = pglGetUniformLocationARB( shader->handle, "u_GlossMap" );
	shader->u_DepthMap = pglGetUniformLocationARB( shader->handle, "u_DepthMap" );
	shader->u_LightDir = pglGetUniformLocationARB( shader->handle, "u_LightDir" );
	shader->u_LightDiffuse = pglGetUniformLocationARB( shader->handle, "u_LightDiffuse" );
	shader->u_GlossExponent = pglGetUniformLocationARB( shader->handle, "u_GlossExponent" );
	shader->u_DetailMode = pglGetUniformLocationARB( shader->handle, "u_DetailMode" );
	shader->u_DetailScale = pglGetUniformLocationARB( shader->handle, "u_DetailScale" );
	shader->u_LightOrigin = pglGetUniformLocationARB( shader->handle, "u_LightOrigin" );
	shader->u_LightRadius = pglGetUniformLocationARB( shader->handle, "u_LightRadius" );
	shader->u_ViewOrigin = pglGetUniformLocationARB( shader->handle, "u_ViewOrigin" );
	shader->u_FaceFlags = pglGetUniformLocationARB( shader->handle, "u_FaceFlags" );
	shader->u_ModelViewProjectionMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelViewProjectionMatrix" );
	shader->u_ModelViewMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelViewMatrix" );
	shader->u_GenericCondition = pglGetUniformLocationARB( shader->handle, "u_LightMode" );
	shader->u_ScreenWidth = pglGetUniformLocationARB( shader->handle, "u_ScreenWidth" );
	shader->u_ScreenHeight = pglGetUniformLocationARB( shader->handle, "u_ScreenHeight" );
	shader->u_ParallaxScale = pglGetUniformLocationARB( shader->handle, "u_ParallaxScale" );
	shader->u_ParallaxSteps = pglGetUniformLocationARB( shader->handle, "u_ParallaxSteps" );
	shader->u_ParallaxMode = pglGetUniformLocationARB( shader->handle, "u_ParallaxMode" );
	shader->u_SpecularMode = pglGetUniformLocationARB( shader->handle, "u_SpecularMode" );
	shader->u_ShadowMode = pglGetUniformLocationARB( shader->handle, "u_ShadowMode" );
	shader->u_ClipPlane = pglGetUniformLocationARB( shader->handle, "u_ClipPlane" );
	shader->u_RemapParms = pglGetUniformLocationARB( shader->handle, "u_LightRemap" );
	shader->u_ReflectScale = pglGetUniformLocationARB( shader->handle, "u_ReflectScale" );
	shader->u_RefractScale = pglGetUniformLocationARB( shader->handle, "u_RefractScale" );
	shader->u_MirrorMode = pglGetUniformLocationARB( shader->handle, "u_MirrorMode" );
	shader->u_TexOffset = pglGetUniformLocationARB( shader->handle, "u_TexOffset" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ProjectMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_AttnZMap, GL_TEXTURE1 );
	pglUniform1iARB( shader->u_ShadowMap, GL_TEXTURE2 ); // optional
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE3 );
	pglUniform1iARB( shader->u_DetailMap, GL_TEXTURE4 );
	pglUniform1iARB( shader->u_NormalMap, GL_TEXTURE5 );
	pglUniform1iARB( shader->u_GlossMap, GL_TEXTURE6 );
	pglUniform1iARB( shader->u_DepthMap, GL_TEXTURE7 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitStudioUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_NormalMap = pglGetUniformLocationARB( shader->handle, "u_NormalMap" );
	shader->u_GlossMap = pglGetUniformLocationARB( shader->handle, "u_GlossMap" );
	shader->u_DepthMap = pglGetUniformLocationARB( shader->handle, "u_DepthMap" );
	shader->u_BoneMatrix = pglGetUniformLocationARB( shader->handle, "u_BoneMatrix" );
	shader->u_LightDir = pglGetUniformLocationARB( shader->handle, "u_LightDir" );
	shader->u_LightAmbient = pglGetUniformLocationARB( shader->handle, "u_LightAmbient" );
	shader->u_LightDiffuse = pglGetUniformLocationARB( shader->handle, "u_LightDiffuse" );
	shader->u_LambertValue = pglGetUniformLocationARB( shader->handle, "u_LambertValue" );
	shader->u_GlossExponent = pglGetUniformLocationARB( shader->handle, "u_GlossExponent" );
	shader->u_ViewOrigin = pglGetUniformLocationARB( shader->handle, "u_ViewOrigin" );
	shader->u_ViewRight = pglGetUniformLocationARB( shader->handle, "u_ViewRight" );
	shader->u_FaceFlags = pglGetUniformLocationARB( shader->handle, "u_FaceFlags" );
	shader->u_ModelViewMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelViewMatrix" );
	shader->u_ModelViewProjectionMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelViewProjectionMatrix" );
	shader->u_ParallaxMode = pglGetUniformLocationARB( shader->handle, "u_ParallaxMode" );
	shader->u_ParallaxScale = pglGetUniformLocationARB( shader->handle, "u_ParallaxScale" );
	shader->u_ParallaxSteps = pglGetUniformLocationARB( shader->handle, "u_ParallaxSteps" );
	shader->u_ClipPlane = pglGetUniformLocationARB( shader->handle, "u_ClipPlane" );
	shader->u_RemapParms = pglGetUniformLocationARB( shader->handle, "u_LightRemap" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_NormalMap, GL_TEXTURE1 );
	pglUniform1iARB( shader->u_GlossMap, GL_TEXTURE2 );
	pglUniform1iARB( shader->u_DepthMap, GL_TEXTURE3 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitTransStudioUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_NormalMap = pglGetUniformLocationARB( shader->handle, "u_NormalMap" );
	shader->u_GlossMap = pglGetUniformLocationARB( shader->handle, "u_GlossMap" );
	shader->u_DepthMap = pglGetUniformLocationARB( shader->handle, "u_DepthMap" );

	shader->u_BoneMatrix = pglGetUniformLocationARB( shader->handle, "u_BoneMatrix" );
	shader->u_GlossExponent = pglGetUniformLocationARB( shader->handle, "u_GlossExponent" );
	shader->u_ViewOrigin = pglGetUniformLocationARB( shader->handle, "u_ViewOrigin" );
	shader->u_ViewRight = pglGetUniformLocationARB( shader->handle, "u_ViewRight" );
	shader->u_FaceFlags = pglGetUniformLocationARB( shader->handle, "u_FaceFlags" );
	shader->u_ReflectScale = pglGetUniformLocationARB( shader->handle, "u_ReflectScale" );
	shader->u_RefractScale = pglGetUniformLocationARB( shader->handle, "u_RefractScale" );
	shader->u_ModelViewMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelViewMatrix" );
	shader->u_ModelViewProjectionMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelViewProjectionMatrix" );
	shader->u_ClipPlane = pglGetUniformLocationARB( shader->handle, "u_ClipPlane" );
	shader->u_RenderColor = pglGetUniformLocationARB( shader->handle, "u_RenderColor" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_NormalMap, GL_TEXTURE1 );
	pglUniform1iARB( shader->u_GlossMap, GL_TEXTURE2 );
	pglUniform1iARB( shader->u_DepthMap, GL_TEXTURE3 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

static void GL_InitStudioDynLightUniforms( glsl_program_t *shader )
{
	ASSERT( shader != NULL );

	shader->u_ProjectMap = pglGetUniformLocationARB( shader->handle, "u_ProjectMap" );
	shader->u_AttnZMap = pglGetUniformLocationARB( shader->handle, "u_AttnZMap" );
	shader->u_ShadowMap = pglGetUniformLocationARB( shader->handle, "u_ShadowMap" );
	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_NormalMap = pglGetUniformLocationARB( shader->handle, "u_NormalMap" );
	shader->u_GlossMap = pglGetUniformLocationARB( shader->handle, "u_GlossMap" );
	shader->u_DepthMap = pglGetUniformLocationARB( shader->handle, "u_DepthMap" );
	shader->u_BoneMatrix = pglGetUniformLocationARB( shader->handle, "u_BoneMatrix" );
	shader->u_LightDir = pglGetUniformLocationARB( shader->handle, "u_LightDir" );
	shader->u_LightDiffuse = pglGetUniformLocationARB( shader->handle, "u_LightDiffuse" );
	shader->u_GlossExponent = pglGetUniformLocationARB( shader->handle, "u_GlossExponent" );
	shader->u_LightOrigin = pglGetUniformLocationARB( shader->handle, "u_LightOrigin" );
	shader->u_LightRadius = pglGetUniformLocationARB( shader->handle, "u_LightRadius" );
	shader->u_ViewOrigin = pglGetUniformLocationARB( shader->handle, "u_ViewOrigin" );
	shader->u_ViewRight = pglGetUniformLocationARB( shader->handle, "u_ViewRight" );
	shader->u_FaceFlags = pglGetUniformLocationARB( shader->handle, "u_FaceFlags" );
	shader->u_ModelViewProjectionMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelViewProjectionMatrix" );
	shader->u_ModelViewMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelViewMatrix" );
	shader->u_GenericCondition = pglGetUniformLocationARB( shader->handle, "u_LightMode" );
	shader->u_ScreenWidth = pglGetUniformLocationARB( shader->handle, "u_ScreenWidth" );
	shader->u_ScreenHeight = pglGetUniformLocationARB( shader->handle, "u_ScreenHeight" );
	shader->u_ParallaxScale = pglGetUniformLocationARB( shader->handle, "u_ParallaxScale" );
	shader->u_ParallaxSteps = pglGetUniformLocationARB( shader->handle, "u_ParallaxSteps" );
	shader->u_ShadowMode = pglGetUniformLocationARB( shader->handle, "u_ShadowMode" );
	shader->u_ParallaxMode = pglGetUniformLocationARB( shader->handle, "u_ParallaxMode" );
	shader->u_ClipPlane = pglGetUniformLocationARB( shader->handle, "u_ClipPlane" );
	shader->u_RemapParms = pglGetUniformLocationARB( shader->handle, "u_LightRemap" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ProjectMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_AttnZMap, GL_TEXTURE1 );
	pglUniform1iARB( shader->u_ShadowMap, GL_TEXTURE2 ); // optional
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE3 );
	pglUniform1iARB( shader->u_NormalMap, GL_TEXTURE4 );
	pglUniform1iARB( shader->u_GlossMap, GL_TEXTURE5 );
	pglUniform1iARB( shader->u_DepthMap, GL_TEXTURE6 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

void GL_InitGPUShaders( void )
{
	if( !GL_Support( R_SHADER_GLSL100_EXT ))
		return;

	glsl_program_t *shader; // generic pointer. help to initialize uniforms

	// monochrome effect
	glsl.monoShader = shader = GL_InitGPUShader( "HW_MonoChrome", "generic", "monochrome" );

	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_BlurFactor = pglGetUniformLocationARB( shader->handle, "u_BlurFactor" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );

	// gaussian blur
	glsl.blurShader = shader = GL_InitGPUShader( "HW_GaussBlur", "generic", "gaussblur" );

	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_BlurFactor = pglGetUniformLocationARB( shader->handle, "u_BlurFactor" );
	shader->u_GenericCondition = pglGetUniformLocationARB( shader->handle, "u_GenericCondition" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );

	// bright filter
	glsl.filterShader = shader = GL_InitGPUShader( "HW_BrightFilter", "generic", "filter" );

	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );

	glsl.tonemapShader = shader = GL_InitGPUShader( "HW_ToneMapping", "generic", "tonemap" );

	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_AddLightMap = pglGetUniformLocationARB( shader->handle, "u_AddLightMap" );
	shader->u_BlurFactor = pglGetUniformLocationARB( shader->handle, "u_BlurFactor" );
	shader->u_WhiteFactor = pglGetUniformLocationARB( shader->handle, "u_WhiteFactor" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_AddLightMap, GL_TEXTURE1 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );

	// DOF with bokeh
	glsl.dofShader = shader = GL_InitGPUShader( "HW_DOF", "generic", "dofbokeh" );

	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_DepthMap = pglGetUniformLocationARB( shader->handle, "u_DepthMap" );
	shader->u_ScreenWidth = pglGetUniformLocationARB( shader->handle, "u_ScreenWidth" );
	shader->u_ScreenHeight = pglGetUniformLocationARB( shader->handle, "u_ScreenHeight" );
	shader->u_GenericCondition = pglGetUniformLocationARB( shader->handle, "u_GenericCondition" );	// for debug
	shader->u_FocalDepth = pglGetUniformLocationARB( shader->handle, "u_FocalDepth" );
	shader->u_FocalLength = pglGetUniformLocationARB( shader->handle, "u_FocalLength" );
	shader->u_FStop = pglGetUniformLocationARB( shader->handle, "u_FStop" );
	shader->u_zFar = pglGetUniformLocationARB( shader->handle, "u_zFar" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	pglUniform1iARB( shader->u_DepthMap, GL_TEXTURE1 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );

	// hardware skinning
	glsl.studioDiffuse = shader = GL_InitGPUShader( "HW_StudioDiffuse", "StudioDiffuse", "StudioDiffuse" );
	GL_InitStudioUniforms( shader );

	// hardware skinning with fresnel
	glsl.studioGlass = shader = GL_InitGPUShader( "HW_StudioGlass", "StudioDiffuse", "StudioGlass" );
	GL_InitTransStudioUniforms( shader );

	// hardware skinning with fresnel
	glsl.studioAdditive = shader = GL_InitGPUShader( "HW_StudioTransAdd", "StudioDiffuse", "StudioAdditive" );
	GL_InitTransStudioUniforms( shader );

	// hardware skinning with bumpmapping
	glsl.studioRealBump = shader = GL_InitGPUShader( "HW_StudioBump", "StudioBump", "StudioBump" );
	GL_InitStudioUniforms( shader );

	// hardware skinning with bumpmapping and parallax occlusion mapping
	glsl.studioParallax = shader = GL_InitGPUShader( "HW_StudioParallax", "StudioBump", "StudioPOM" );
	GL_InitStudioUniforms( shader );

	// hardware skinning with dynamic lighting
	glsl.studioDynLight = shader = GL_InitGPUShader( "HW_StudioDiffuseLighting", "StudioDynLight", "StudioDynLight" );
	GL_InitStudioDynLightUniforms( shader );

	// hardware skinning with dynamic bump lighting
	glsl.studioBumpDynLight = shader = GL_InitGPUShader( "HW_StudioBumpLighting", "StudioBumpDynLight", "StudioBumpDynLight" );
	GL_InitStudioDynLightUniforms( shader );

	// hardware skinning with dynamic POM lighting
	glsl.studioPOMDynLight = shader = GL_InitGPUShader( "HW_StudioParallaxLighting", "StudioBumpDynLight", "StudioPOMDynLight" );
	GL_InitStudioDynLightUniforms( shader );

	// hardware skinning
	glsl.depthFillGeneric = shader = GL_InitGPUShader( "HW_DepthPass", "DepthFill", "DepthFill" );

	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_BoneMatrix = pglGetUniformLocationARB( shader->handle, "u_BoneMatrix" );
	shader->u_ModelViewProjectionMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelViewProjectionMatrix" );
	shader->u_ModelViewMatrix = pglGetUniformLocationARB( shader->handle, "u_ModelViewMatrix" );
	shader->u_GenericCondition = pglGetUniformLocationARB( shader->handle, "u_StudioModel" );
	shader->u_GenericCondition2 = pglGetUniformLocationARB( shader->handle, "u_AlphaTest" );
	shader->u_ClipPlane = pglGetUniformLocationARB( shader->handle, "u_ClipPlane" );
	shader->u_TexOffset = pglGetUniformLocationARB( shader->handle, "u_TexOffset" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );

	// bmodel ambient shader
	glsl.bmodelAmbient = shader = GL_InitGPUShader( "HW_BrushAmbient", "BrushGeneric", "BrushAmbient" );
	GL_InitBmodelUniforms( shader );

	// bmodel diffuse shader
	glsl.bmodelDiffuse = shader = GL_InitGPUShader( "HW_BrushDiffuse", "BrushGeneric", "BrushDiffuse" );
	GL_InitBmodelUniforms( shader );

	// bmodel fakebump shader
	glsl.bmodelFakeBump = shader = GL_InitGPUShader( "HW_BrushFakeBump", "BrushGeneric", "BrushFakeBump" );
	GL_InitBmodelUniforms( shader );

	// bmodel bumpmap shader
	glsl.bmodelRealBump = shader = GL_InitGPUShader( "HW_BrushRealBump", "BrushGeneric", "BrushRealBump" );
	GL_InitBmodelUniforms( shader );

	// bmodel parallax shader
	glsl.bmodelParallax = shader = GL_InitGPUShader( "HW_BrushParallax", "BrushGeneric", "BrushPOM" );
	GL_InitBmodelUniforms( shader );

	// bmodel bumpmap shader
	glsl.bmodelReflectBump = shader = GL_InitGPUShader( "HW_BrushReflectBump", "BrushGeneric", "BrushReflectBump" );
	GL_InitBmodelUniforms( shader );

	// lightmap debug shader
	glsl.debugLightmapShader = shader = GL_InitGPUShader( "DBG_ShowLightmap", "debugLightmap", "debugLightmap" );
	GL_InitBmodelUniforms( shader );

	// bmodel glass shader
	glsl.bmodelGlass = shader = GL_InitGPUShader( "HW_BrushGlass", "BrushGeneric", "BrushGlass" );
	GL_InitTransBmodelUniforms( shader );

	// bmodel water shader
	glsl.bmodelWater = shader = GL_InitGPUShader( "HW_BrushWater", "BrushGeneric", "BrushWater" );
	GL_InitTransBmodelUniforms( shader );

	// bmodel additive
	glsl.bmodelAdditive = shader = GL_InitGPUShader( "HW_BrushTransAdd", "BrushGeneric", "BrushAdditive" );
	GL_InitTransBmodelUniforms( shader );

	// surface decals
	glsl.bmodelSingleDecal = shader = GL_InitGPUShader( "HW_SurfaceDecal", "BrushDecal", "BrushDecal" );
	GL_InitBmodelDecalUniforms( shader );

	// surface decals
	glsl.bmodelSingleDecalPOM = shader = GL_InitGPUShader( "HW_SurfaceDecalPOM", "BrushDecal", "BrushDecalPOM" );
	GL_InitBmodelDecalUniforms( shader );

	// puddles
	glsl.bmodelSingleDecalPuddle = shader = GL_InitGPUShader( "HW_Puddle", "BrushDecal", "Puddle" );
	GL_InitBmodelDecalUniforms( shader );

	// bmodel dynamic lighting
	glsl.bmodelDynLight = shader = GL_InitGPUShader( "HW_BrushDynLight", "BrushDynLight", "BrushDynLight" );
	GL_InitBmodelDynLightUniforms( shader );

	// bmodel dynamic lighting
	glsl.bmodelBumpDynLight = shader = GL_InitGPUShader( "HW_BrushBumpDynLight", "BrushBumpDynLight", "BrushBumpDynLight" );
	GL_InitBmodelDynLightUniforms( shader );

	glsl.skyboxShader = shader = GL_InitGPUShader( "HW_SkyBox", "generic", "skybox" );

	ASSERT( shader != NULL );

	shader->u_ColorMap = pglGetUniformLocationARB( shader->handle, "u_ColorMap" );
	shader->u_LightDir = pglGetUniformLocationARB( shader->handle, "u_LightDir" );
	shader->u_LightDiffuse = pglGetUniformLocationARB( shader->handle, "u_LightDiffuse" );
	shader->u_ViewOrigin = pglGetUniformLocationARB( shader->handle, "u_ViewOrigin" );

	GL_BindShader( shader );
	pglUniform1iARB( shader->u_ColorMap, GL_TEXTURE0 );
	GL_BindShader( GL_NONE );

	GL_ValidateProgram( shader );
	GL_ShowProgramUniforms( shader );
}

void GL_FreeGPUShaders( void )
{
	if( !GL_Support( R_SHADER_GLSL100_EXT ))
		return;

	for( int i = 0; i < tr.num_glsl_programs; i++ )
		GL_FreeGPUShader( &tr.glsl_programs[i] );

	GL_BindShader( GL_NONE );
}