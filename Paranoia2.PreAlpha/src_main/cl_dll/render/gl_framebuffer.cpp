/*
gl_framebuffer.cpp - framebuffer implementation class
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
#include "com_model.h"
#include <pm_movevars.h>
#include "camera.h"
#include "gl_local.h"
#include <mathlib.h>
#include <stringlib.h>

CFrameBuffer :: CFrameBuffer( void )
{
	m_iFrameWidth = m_iFrameHeight = 0;
	m_iFrameBuffer = m_iDepthBuffer = 0;
	m_iTexture = m_iAttachment = 0;

	m_bAllowFBO = false;
}

CFrameBuffer :: ~CFrameBuffer( void )
{
	// NOTE: static case will be failed
	Free();
}

int CFrameBuffer :: m_iBufferNum = 0;
	
bool CFrameBuffer :: Init( FBO_TYPE type, GLuint width, GLuint height, GLuint flags )
{
	Free(); // release old buffer

	m_iFlags = flags;

	if( !GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
		SetBits( m_iFlags, FBO_MAKEPOW );

	if( FBitSet( m_iFlags, FBO_MAKEPOW ))
	{
		width = NearestPOW( width, true );
		height = NearestPOW( height, true );
	}

	// clamp size to hardware limits
	if( type == FBO_CUBE )
	{
		m_iFrameWidth = bound( 0, width, glConfig.max_cubemap_size );
		m_iFrameHeight = bound( 0, height, glConfig.max_cubemap_size );
	}
	else
	{
		m_iFrameWidth = bound( 0, width, glConfig.max_2d_texture_size );
		m_iFrameHeight = bound( 0, height, glConfig.max_2d_texture_size );
	}

	if( !m_iFrameWidth || !m_iFrameHeight )
	{
		ALERT( at_error, "CFrameBuffer( %i x %i ) invalid size\n", m_iFrameWidth, m_iFrameHeight );
		return false;
	}

	// create FBO texture
	if( !FBitSet( m_iFlags, FBO_NOTEXTURE ))
	{
		int texFlags = (TF_NOPICMIP|TF_NOMIPMAP|TF_CLAMP);

		if( type == FBO_CUBE )
			SetBits( texFlags, TF_CUBEMAP );
		else if( type == FBO_DEPTH ) 
			SetBits( texFlags, TF_DEPTHMAP );

		if( FBitSet( m_iFlags, FBO_FLOAT ))
			SetBits( texFlags, TF_FLOAT );

		if( FBitSet( m_iFlags, FBO_RECTANGLE ))
			SetBits( texFlags, TF_TEXTURE_RECTANGLE );

		m_iTexture = CREATE_TEXTURE( va( "*framebuffer#%i", m_iBufferNum++ ), m_iFrameWidth, m_iFrameHeight, NULL, texFlags ); 
	}

	m_bAllowFBO = (GL_Support( R_FRAMEBUFFER_OBJECT )) ? true : false;

	if( m_bAllowFBO )
	{
		// frame buffer
		pglGenFramebuffers( 1, &m_iFrameBuffer );
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, m_iFrameBuffer );

		// depth buffer
		pglGenRenderbuffers( 1, &m_iDepthBuffer );
		pglBindRenderbuffer( GL_RENDERBUFFER_EXT, m_iDepthBuffer );
		pglRenderbufferStorage( GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24, m_iFrameWidth, m_iFrameHeight );

		// attach depthbuffer to framebuffer
		pglFramebufferRenderbuffer( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_iDepthBuffer );

		//  attach the framebuffer to our texture, which may be a depth texture
		if( type == FBO_DEPTH )
		{
			m_iAttachment = GL_DEPTH_ATTACHMENT_EXT;
			pglDrawBuffer( GL_NONE );
			pglReadBuffer( GL_NONE );
		}
		else
		{
			m_iAttachment = GL_COLOR_ATTACHMENT0_EXT;
			pglDrawBuffer( m_iAttachment );
			pglReadBuffer( m_iAttachment );
		}

		if( m_iTexture != 0 )
		{
			GLuint target = RENDER_GET_PARM( PARM_TEX_TARGET, m_iTexture );
			GLuint texnum = RENDER_GET_PARM( PARM_TEX_TEXNUM, m_iTexture );
			pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, m_iAttachment, target, texnum, 0 );
		}

		m_bAllowFBO = ValidateFBO();
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
	}

	return true;
}

void CFrameBuffer :: Free( void )
{
	if( !GL_Active() || !m_bAllowFBO )
		return;

	if( !FBitSet( m_iFlags, FBO_NOTEXTURE ) && m_iTexture != 0 )
		FREE_TEXTURE( m_iTexture );

	pglDeleteRenderbuffers( 1, &m_iDepthBuffer );
	pglDeleteFramebuffers( 1, &m_iFrameBuffer );

	m_iFrameWidth = m_iFrameHeight = 0;
	m_iFrameBuffer = m_iDepthBuffer = 0;
	m_iTexture = m_iAttachment = 0;

	m_bAllowFBO = false;
}

bool CFrameBuffer :: ValidateFBO( void )
{
	if( !GL_Support( R_FRAMEBUFFER_OBJECT ))
		return false;

	// check FBO status
	GLenum status = pglCheckFramebufferStatus( GL_FRAMEBUFFER_EXT );

	switch( status )
	{
	case GL_FRAMEBUFFER_COMPLETE_EXT:
		return true;
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
		ALERT( at_error, "CFrameBuffer: attachment is NOT complete\n" );
		return false;
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
		ALERT( at_error, "CFrameBuffer: no image is attached to FBO\n" );
		return false;
	case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
		ALERT( at_error, "CFrameBuffer: attached images have different dimensions\n" );
		return false;
	case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
		ALERT( at_error, "CFrameBuffer: color attached images have different internal formats\n" );
		return false;
	case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
		ALERT( at_error, "CFrameBuffer: draw buffer incomplete\n" );
		return false;
	case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
		ALERT( at_error, "CFrameBuffere: read buffer incomplete\n" );
		return false;
	case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
		ALERT( at_error, "CFrameBuffer: unsupported by current FBO implementation\n" );
		return false;
	default:
		ALERT( at_error, "CFrameBuffer: unknown error\n" );
		return false;
	}
}

void CFrameBuffer :: Bind( GLuint texture )
{
	if( !m_bAllowFBO ) return;

	if( glState.frameBuffer != m_iFrameBuffer )
	{
		pglBindFramebuffer( GL_FRAMEBUFFER_EXT, m_iFrameBuffer );
		glState.frameBuffer = m_iFrameBuffer;
	}

	// change texture if needs
	if( FBitSet( m_iFlags, FBO_NOTEXTURE ) && texture != 0 )
	{
		m_iTexture = texture;

		GLuint target = RENDER_GET_PARM( PARM_TEX_TARGET, m_iTexture );
		GLuint texnum = RENDER_GET_PARM( PARM_TEX_TEXNUM, m_iTexture );
		pglFramebufferTexture2D( GL_FRAMEBUFFER_EXT, m_iAttachment, target, texnum, 0 );
	}	

	ValidateFBO();
}