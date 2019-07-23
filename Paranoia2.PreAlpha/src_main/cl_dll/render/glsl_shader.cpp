/*
glsl_shader.cpp - glsl shaders
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

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "ref_params.h"
#include "gl_local.h"
#include <mathlib.h>
#include <stringlib.h>

GLSLShader :: GLSLShader( const char *szVertexProgram, const char *szFragmentProgram )
{
	GLint objectStatus;
	char nameBuffer[16];
	int c;

	m_bValid = false;

	m_hVShader = 0;
	m_hFShader = 0;
	m_hProgram = 0;

	if( !szVertexProgram && !szFragmentProgram )
		return;

	if( szVertexProgram )
	{
		pglGetError();
		m_hVShader = pglCreateShaderObjectARB( GL_VERTEX_SHADER_ARB );
		GLint srcLen = Q_strlen( szVertexProgram );
		pglShaderSourceARB( m_hVShader, 1, (const char**)&szVertexProgram, &srcLen );
		pglCompileShaderARB( m_hVShader );

		if( pglGetError() != GL_NO_ERROR )
		{
			ALERT( at_error, "Failed to compile vertex shader:\n\n%s\n", szVertexProgram );
			PrintInfoLog( m_hVShader );
			return;
		}
		
		pglGetObjectParameterivARB( m_hVShader, GL_OBJECT_COMPILE_STATUS_ARB, &objectStatus );
		if( !objectStatus )
		{
			ALERT( at_error, "Failed to compile vertex shader:\n\n%s\n", szVertexProgram );
			PrintInfoLog( m_hVShader );
			return;
		}
	}

	if( szFragmentProgram )
	{
		pglGetError();
		m_hFShader = pglCreateShaderObjectARB( GL_FRAGMENT_SHADER_ARB );
		GLint srcLen = Q_strlen( szFragmentProgram );
		pglShaderSourceARB( m_hFShader, 1, (const char**)&szFragmentProgram, &srcLen );
		pglCompileShaderARB( m_hFShader );

		if( pglGetError() != GL_NO_ERROR )
		{
			ALERT( at_error, "Failed to compile fragment shader:\n\n%s\n", szFragmentProgram );
			PrintInfoLog( m_hFShader );
			return;
		}
		
		pglGetObjectParameterivARB( m_hFShader, GL_OBJECT_COMPILE_STATUS_ARB, &objectStatus );

		if( !objectStatus )
		{
			ALERT( at_error, "Failed to compile fragment shader:\n\n%s\n", szFragmentProgram );
			PrintInfoLog( m_hFShader );
			return;
		}
	}

	m_hProgram = pglCreateProgramObjectARB();
	if( szVertexProgram ) pglAttachObjectARB( m_hProgram, m_hVShader );
	if( szFragmentProgram ) pglAttachObjectARB( m_hProgram, m_hFShader );

	pglGetError();
	pglLinkProgramARB( m_hProgram );

	if( pglGetError() != GL_NO_ERROR )
	{
		ALERT( at_error, "Failed to link shaders!\n\nVertex:\n%s\n\nFragment:\n%s\n",
		szVertexProgram ? szVertexProgram : "none", szFragmentProgram ? szFragmentProgram : "none" );
		PrintInfoLog( m_hProgram );
		return;
	}
		
	pglGetObjectParameterivARB( m_hProgram, GL_OBJECT_LINK_STATUS_ARB, &objectStatus );
	if( !objectStatus )
	{
		ALERT( at_error, "Failed to link shaders!\n\nVertex:\n%s\n\nFragment:\n%s\n",
		szVertexProgram ? szVertexProgram : "none", szFragmentProgram ? szFragmentProgram : "none" );
		PrintInfoLog( m_hProgram );
		return;
	}

	pglUseProgramObjectARB( m_hProgram );

	// Get uniforms
	for( c = 0; c < MAX_SHADER_UNIFORMS; c++ )
	{
		Q_snprintf( nameBuffer, sizeof( nameBuffer ), "Local%i", c );
		m_iUniforms[c] = pglGetUniformLocationARB( m_hProgram, nameBuffer );
	}

	// Attach uniform textures
	for( c = 0; c < MAX_SHADER_TEXTURES; c++ )
	{
		Q_snprintf( nameBuffer, sizeof( nameBuffer ), "Texture%i", c );
		GLint texunitloc = pglGetUniformLocationARB( m_hProgram, nameBuffer );
		if( texunitloc != -1 ) pglUniform1iARB( texunitloc, c );
	}

	pglValidateProgramARB( m_hProgram );
	pglGetObjectParameterivARB( m_hProgram, GL_OBJECT_VALIDATE_STATUS_ARB, &objectStatus );

	if( !objectStatus )
	{
		ALERT( at_error, "Failed to validate shaders!\n\nVertex:\n%s\n\nFragment:\n%s\n",
		szVertexProgram ? szVertexProgram : "none", szFragmentProgram ? szFragmentProgram : "none" );
		PrintInfoLog( m_hProgram );
		return;
	}

	pglUseProgramObjectARB( 0 );
	m_bValid = true;
}

GLSLShader :: ~GLSLShader()
{
	if( m_hVShader )
	{
		pglDetachObjectARB( m_hProgram, m_hVShader );
		pglDeleteObjectARB( m_hVShader );
	}

	if( m_hFShader )
	{
		pglDetachObjectARB( m_hProgram, m_hFShader );
		pglDeleteObjectARB( m_hFShader );
	}

	pglDeleteObjectARB( m_hProgram );
}

void GLSLShader :: Bind( void )
{
	pglUseProgramObjectARB( m_hProgram );
}

void GLSLShader :: Unbind( void )
{
	pglUseProgramObjectARB( 0 );
}

void GLSLShader :: SetParameter4f( int param, float x, float y, float z, float w )
{
	if(( unsigned int)param >= MAX_SHADER_UNIFORMS )
	{
		ALERT( at_warning, "GLSLShader::SetParameter4f: parameter index out of range (%i)\n", param );
		return;
	}

	pglUniform4fARB( m_iUniforms[param], x, y, z, w );
}

void GLSLShader :: SetParameter4fv( int param, float* v )
{
	if(( unsigned int)param >= MAX_SHADER_UNIFORMS )
	{
		ALERT( at_warning, "GLSLShader::SetParameter4fv: parameter index out of range (%i)\n", param );
		return;
	}

	pglUniform4fvARB( m_iUniforms[param], 4, v );
}

void GLSLShader :: PrintInfoLog( GLhandleARB object )
{
	int logMaxLen;

	pglGetObjectParameterivARB( object, GL_OBJECT_INFO_LOG_LENGTH_ARB, &logMaxLen );
	if( logMaxLen <= 0 ) return;

	char *plog = new char[logMaxLen+1];
	pglGetInfoLogARB( object, logMaxLen, NULL, plog );

	if( Q_strlen( plog ))
		ALERT( at_console, "Compile log: %s\n", plog );

	delete [] plog;
}