/*
gl_framebuffer.h - framebuffer implementation class
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

#ifndef GL_FRAMEBUFFER_H
#define GL_FRAMEBUFFER_H

typedef enum
{
	FBO_COLOR = 0,
	FBO_DEPTH,
	FBO_CUBE,
} FBO_TYPE;

#define FBO_MAKEPOW		(1<<0)		// round buffer size to nearest pow
#define FBO_NOTEXTURE	(1<<1)		// don't create texture on initialization
#define FBO_FLOAT		(1<<2)		// use float texture
#define FBO_RECTANGLE	(1<<3)		// use rectangle texture

class CFrameBuffer
{
public:
	CFrameBuffer();
	~CFrameBuffer();

	bool Init( FBO_TYPE type, GLuint width, GLuint height, GLuint flags = 0 );
	void Bind( GLuint texture = 0 );
	bool ValidateFBO( void );
	void Free( void );

	unsigned int GetWidth( void ) const { return m_iFrameWidth; }
	unsigned int GetHeight( void ) const { return m_iFrameHeight; }
	unsigned int GetTexture( void ) const { return m_iTexture; }
	bool Active( void ) const { return m_bAllowFBO; }
protected:
	static int	m_iBufferNum;	// single object for all instances
private:
	GLuint		m_iFrameWidth;
	GLuint		m_iFrameHeight;
	GLuint		m_iTexture;

	GLuint		m_iFrameBuffer;
	GLuint		m_iDepthBuffer;
	GLenum		m_iAttachment;	// attachment type
	bool		m_bAllowFBO;	// FBO is valid
	GLuint		m_iFlags;		// member FBO flags
};

#endif//GL_FRAMEBUFFER_H