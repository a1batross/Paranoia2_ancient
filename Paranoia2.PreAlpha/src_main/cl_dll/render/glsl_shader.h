/*
glsl_shader.h - glsl shaders
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
#ifndef GLSL_SHADER_H
#define GLSL_SHADER_H

#define MAX_SHADER_UNIFORMS		8
#define MAX_SHADER_TEXTURES		8

class GLSLShader
{
public:
	GLSLShader( const char *szVertexProgram, const char *szFragmentProgram );
	~GLSLShader();

	bool IsValid( void ) { return m_bValid; }

	void Bind( void );
	void Unbind( void );
	void SetParameter4f( int param, float x, float y, float z, float w );
	void SetParameter4fv( int param, float* v );

private:
	void PrintInfoLog( GLhandleARB object );

private:
	bool		m_bValid;
	GLhandleARB	m_hProgram;
	GLhandleARB	m_hVShader;
	GLhandleARB	m_hFShader;
	GLint		m_iUniforms[MAX_SHADER_UNIFORMS];
};

#endif//GLSL_SHADER_H