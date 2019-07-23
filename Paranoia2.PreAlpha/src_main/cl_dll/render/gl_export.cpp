/*
gl_export.cpp - OpenGL dynamically linkage
Copyright (C) 2010 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#define EXTERN

#include "hud.h"
#include "cl_util.h"
#include "gl_local.h"
#include <stringlib.h>
#include "gl_shader.h"
#include "gl_decals.h"
#include "gl_studio.h"
#include "rain.h"

glState_t	glState;
glConfig_t glConfig;

static dllfunc_t opengl_110funcs[] =
{
{ "glClearColor"         	, (void **)&pglClearColor },
{ "glClear"              	, (void **)&pglClear },
{ "glAlphaFunc"          	, (void **)&pglAlphaFunc },
{ "glBlendFunc"          	, (void **)&pglBlendFunc },
{ "glCullFace"           	, (void **)&pglCullFace },
{ "glDrawBuffer"         	, (void **)&pglDrawBuffer },
{ "glReadBuffer"         	, (void **)&pglReadBuffer },
{ "glAccum"         	, (void **)&pglAccum },
{ "glEnable"             	, (void **)&pglEnable },
{ "glDisable"            	, (void **)&pglDisable },
{ "glEnableClientState"  	, (void **)&pglEnableClientState },
{ "glDisableClientState" 	, (void **)&pglDisableClientState },
{ "glGetBooleanv"        	, (void **)&pglGetBooleanv },
{ "glGetDoublev"         	, (void **)&pglGetDoublev },
{ "glGetFloatv"          	, (void **)&pglGetFloatv },
{ "glGetIntegerv"        	, (void **)&pglGetIntegerv },
{ "glGetError"           	, (void **)&pglGetError },
{ "glGetString"          	, (void **)&pglGetString },
{ "glFinish"             	, (void **)&pglFinish },
{ "glFlush"              	, (void **)&pglFlush },
{ "glClearDepth"         	, (void **)&pglClearDepth },
{ "glDepthFunc"          	, (void **)&pglDepthFunc },
{ "glDepthMask"          	, (void **)&pglDepthMask },
{ "glDepthRange"         	, (void **)&pglDepthRange },
{ "glFrontFace"          	, (void **)&pglFrontFace },
{ "glDrawElements"       	, (void **)&pglDrawElements },
{ "glColorMask"          	, (void **)&pglColorMask },
{ "glIndexPointer"       	, (void **)&pglIndexPointer },
{ "glVertexPointer"      	, (void **)&pglVertexPointer },
{ "glNormalPointer"      	, (void **)&pglNormalPointer },
{ "glColorPointer"       	, (void **)&pglColorPointer },
{ "glTexCoordPointer"    	, (void **)&pglTexCoordPointer },
{ "glArrayElement"       	, (void **)&pglArrayElement },
{ "glColor3f"            	, (void **)&pglColor3f },
{ "glColor3fv"           	, (void **)&pglColor3fv },
{ "glColor4f"            	, (void **)&pglColor4f },
{ "glColor4fv"           	, (void **)&pglColor4fv },
{ "glColor3ub"           	, (void **)&pglColor3ub },
{ "glColor4ub"           	, (void **)&pglColor4ub },
{ "glColor4ubv"          	, (void **)&pglColor4ubv },
{ "glTexCoord1f"         	, (void **)&pglTexCoord1f },
{ "glTexCoord2f"         	, (void **)&pglTexCoord2f },
{ "glTexCoord3f"         	, (void **)&pglTexCoord3f },
{ "glTexCoord4f"         	, (void **)&pglTexCoord4f },
{ "glTexCoord1fv"        	, (void **)&pglTexCoord1fv },
{ "glTexCoord2fv"        	, (void **)&pglTexCoord2fv },
{ "glTexCoord3fv"        	, (void **)&pglTexCoord3fv },
{ "glTexCoord4fv"        	, (void **)&pglTexCoord4fv },
{ "glTexGenf"            	, (void **)&pglTexGenf },
{ "glTexGenfv"           	, (void **)&pglTexGenfv },
{ "glTexGeni"            	, (void **)&pglTexGeni },
{ "glVertex2f"           	, (void **)&pglVertex2f },
{ "glVertex3f"           	, (void **)&pglVertex3f },
{ "glVertex3fv"          	, (void **)&pglVertex3fv },
{ "glNormal3f"           	, (void **)&pglNormal3f },
{ "glNormal3fv"          	, (void **)&pglNormal3fv },
{ "glBegin"              	, (void **)&pglBegin },
{ "glEnd"                	, (void **)&pglEnd },
{ "glLineWidth"          	, (void**)&pglLineWidth },
{ "glPointSize"          	, (void**)&pglPointSize },
{ "glMatrixMode"         	, (void **)&pglMatrixMode },
{ "glOrtho"              	, (void **)&pglOrtho },
{ "glRasterPos2f"        	, (void **) &pglRasterPos2f },
{ "glFrustum"            	, (void **)&pglFrustum },
{ "glViewport"           	, (void **)&pglViewport },
{ "glPushMatrix"         	, (void **)&pglPushMatrix },
{ "glPopMatrix"          	, (void **)&pglPopMatrix },
{ "glPushAttrib"         	, (void **)&pglPushAttrib },
{ "glPopAttrib"          	, (void **)&pglPopAttrib },
{ "glLoadIdentity"       	, (void **)&pglLoadIdentity },
{ "glLoadMatrixd"        	, (void **)&pglLoadMatrixd },
{ "glLoadMatrixf"        	, (void **)&pglLoadMatrixf },
{ "glMultMatrixd"        	, (void **)&pglMultMatrixd },
{ "glMultMatrixf"        	, (void **)&pglMultMatrixf },
{ "glRotated"            	, (void **)&pglRotated },
{ "glRotatef"            	, (void **)&pglRotatef },
{ "glScaled"             	, (void **)&pglScaled },
{ "glScalef"             	, (void **)&pglScalef },
{ "glTranslated"         	, (void **)&pglTranslated },
{ "glTranslatef"         	, (void **)&pglTranslatef },
{ "glReadPixels"         	, (void **)&pglReadPixels },
{ "glDrawPixels"         	, (void **)&pglDrawPixels },
{ "glStencilFunc"        	, (void **)&pglStencilFunc },
{ "glStencilMask"        	, (void **)&pglStencilMask },
{ "glStencilOp"          	, (void **)&pglStencilOp },
{ "glClearStencil"       	, (void **)&pglClearStencil },
{ "glIsEnabled"          	, (void **)&pglIsEnabled },
{ "glIsList"             	, (void **)&pglIsList },
{ "glIsTexture"          	, (void **)&pglIsTexture },
{ "glTexEnvf"            	, (void **)&pglTexEnvf },
{ "glTexEnvfv"           	, (void **)&pglTexEnvfv },
{ "glTexEnvi"            	, (void **)&pglTexEnvi },
{ "glTexParameterf"      	, (void **)&pglTexParameterf },
{ "glTexParameterfv"     	, (void **)&pglTexParameterfv },
{ "glTexParameteri"      	, (void **)&pglTexParameteri },
{ "glHint"               	, (void **)&pglHint },
{ "glPixelStoref"        	, (void **)&pglPixelStoref },
{ "glPixelStorei"        	, (void **)&pglPixelStorei },
{ "glGenTextures"        	, (void **)&pglGenTextures },
{ "glDeleteTextures"     	, (void **)&pglDeleteTextures },
{ "glBindTexture"        	, (void **)&pglBindTexture },
{ "glTexImage1D"         	, (void **)&pglTexImage1D },
{ "glTexImage2D"         	, (void **)&pglTexImage2D },
{ "glTexSubImage1D"      	, (void **)&pglTexSubImage1D },
{ "glTexSubImage2D"      	, (void **)&pglTexSubImage2D },
{ "glCopyTexImage1D"     	, (void **)&pglCopyTexImage1D },
{ "glCopyTexImage2D"     	, (void **)&pglCopyTexImage2D },
{ "glCopyTexSubImage1D"  	, (void **)&pglCopyTexSubImage1D },
{ "glCopyTexSubImage2D"  	, (void **)&pglCopyTexSubImage2D },
{ "glScissor"            	, (void **)&pglScissor },
{ "glGetTexImage"		, (void **)&pglGetTexImage },
{ "glGetTexEnviv"        	, (void **)&pglGetTexEnviv },
{ "glPolygonOffset"      	, (void **)&pglPolygonOffset },
{ "glPolygonMode"        	, (void **)&pglPolygonMode },
{ "glPolygonStipple"     	, (void **)&pglPolygonStipple },
{ "glClipPlane"          	, (void **)&pglClipPlane },
{ "glGetClipPlane"       	, (void **)&pglGetClipPlane },
{ "glShadeModel"         	, (void **)&pglShadeModel },
{ "glGetTexLevelParameteriv"	, (void **)&pglGetTexLevelParameteriv },
{ "glGetTexLevelParameterfv"	, (void **)&pglGetTexLevelParameterfv },
{ "glFogfv"              	, (void **)&pglFogfv },
{ "glFogf"               	, (void **)&pglFogf },
{ "glFogi"               	, (void **)&pglFogi },
{ NULL, NULL }
};

static dllfunc_t pointparametersfunc[] =
{
{ "glPointParameterfEXT"  , (void **)&pglPointParameterfEXT },
{ "glPointParameterfvEXT" , (void **)&pglPointParameterfvEXT },
{ NULL, NULL }
};

static dllfunc_t drawrangeelementsfuncs[] =
{
{ "glDrawRangeElements" , (void **)&pglDrawRangeElements },
{ NULL, NULL }
};

static dllfunc_t drawrangeelementsextfuncs[] =
{
{ "glDrawRangeElementsEXT" , (void **)&pglDrawRangeElementsEXT },
{ NULL, NULL }
};

static dllfunc_t sgis_multitexturefuncs[] =
{
{ "glSelectTextureSGIS" , (void **)&pglSelectTextureSGIS },
{ "glMTexCoord2fSGIS"   , (void **)&pglMTexCoord2fSGIS },
{ NULL, NULL }
};

static dllfunc_t multitexturefuncs[] =
{
{ "glMultiTexCoord1fARB"     , (void **)&pglMultiTexCoord1f },
{ "glMultiTexCoord2fARB"     , (void **)&pglMultiTexCoord2f },
{ "glMultiTexCoord3fARB"     , (void **)&pglMultiTexCoord3f },
{ "glMultiTexCoord4fARB"     , (void **)&pglMultiTexCoord4f },
{ "glActiveTextureARB"       , (void **)&pglActiveTexture },
{ "glActiveTextureARB"       , (void **)&pglActiveTextureARB },
{ "glClientActiveTextureARB" , (void **)&pglClientActiveTexture },
{ "glClientActiveTextureARB" , (void **)&pglClientActiveTextureARB },
{ NULL, NULL }
};

static dllfunc_t compiledvertexarrayfuncs[] =
{
{ "glLockArraysEXT"   , (void **)&pglLockArraysEXT },
{ "glUnlockArraysEXT" , (void **)&pglUnlockArraysEXT },
{ "glDrawArrays"      , (void **)&pglDrawArrays },
{ NULL, NULL }
};

static dllfunc_t texture3dextfuncs[] =
{
{ "glTexImage3DEXT"        , (void **)&pglTexImage3D },
{ "glTexSubImage3DEXT"     , (void **)&pglTexSubImage3D },
{ "glCopyTexSubImage3DEXT" , (void **)&pglCopyTexSubImage3D },
{ NULL, NULL }
};

static dllfunc_t atiseparatestencilfuncs[] =
{
{ "glStencilOpSeparateATI"   , (void **)&pglStencilOpSeparate },
{ "glStencilFuncSeparateATI" , (void **)&pglStencilFuncSeparate },
{ NULL, NULL }
};

static dllfunc_t gl2separatestencilfuncs[] =
{
{ "glStencilOpSeparate"   , (void **)&pglStencilOpSeparate },
{ "glStencilFuncSeparate" , (void **)&pglStencilFuncSeparate },
{ NULL, NULL }
};

static dllfunc_t stenciltwosidefuncs[] =
{
{ "glActiveStencilFaceEXT" , (void **)&pglActiveStencilFaceEXT },
{ NULL, NULL }
};

static dllfunc_t copyimagesubdata[] =
{
{ "glCopyImageSubData" , (void **)&pglCopyImageSubData },
{ NULL, NULL }
};

static dllfunc_t nvcombinersfuncs[] =
{
{ "glCombinerParameterfvNV"		, (void **)&pglCombinerParameterfvNV },
{ "glCombinerParameterfNV"		, (void **)&pglCombinerParameterfNV },
{ "glCombinerParameterivNV"		, (void **)&pglCombinerParameterivNV },
{ "glCombinerParameteriNV"		, (void **)&pglCombinerParameteriNV },
{ "glCombinerInputNV"		, (void **)&pglCombinerInputNV },
{ "glCombinerOutputNV"		, (void **)&pglCombinerOutputNV },
{ "glFinalCombinerInputNV"		, (void **)&pglFinalCombinerInputNV },
{ "glGetCombinerInputParameterfvNV"	, (void **)&pglGetCombinerInputParameterfvNV },
{ "glGetCombinerInputParameterivNV"	, (void **)&pglGetCombinerInputParameterivNV },
{ "glGetCombinerOutputParameterfvNV"	, (void **)&pglGetCombinerOutputParameterfvNV },
{ "glGetCombinerOutputParameterivNV"	, (void **)&pglGetCombinerOutputParameterivNV },
{ "glGetFinalCombinerInputParameterfvNV", (void **)&pglGetFinalCombinerInputParameterfvNV },
{ "glGetFinalCombinerInputParameterivNV", (void **)&pglGetFinalCombinerInputParameterivNV },
{ NULL, NULL }
};

static dllfunc_t shaderobjectsfuncs[] =
{
{ "glDeleteObjectARB"             , (void **)&pglDeleteObjectARB },
{ "glGetHandleARB"                , (void **)&pglGetHandleARB },
{ "glDetachObjectARB"             , (void **)&pglDetachObjectARB },
{ "glCreateShaderObjectARB"       , (void **)&pglCreateShaderObjectARB },
{ "glShaderSourceARB"             , (void **)&pglShaderSourceARB },
{ "glCompileShaderARB"            , (void **)&pglCompileShaderARB },
{ "glCreateProgramObjectARB"      , (void **)&pglCreateProgramObjectARB },
{ "glAttachObjectARB"             , (void **)&pglAttachObjectARB },
{ "glLinkProgramARB"              , (void **)&pglLinkProgramARB },
{ "glUseProgramObjectARB"         , (void **)&pglUseProgramObjectARB },
{ "glValidateProgramARB"          , (void **)&pglValidateProgramARB },
{ "glUniform1fARB"                , (void **)&pglUniform1fARB },
{ "glUniform2fARB"                , (void **)&pglUniform2fARB },
{ "glUniform3fARB"                , (void **)&pglUniform3fARB },
{ "glUniform4fARB"                , (void **)&pglUniform4fARB },
{ "glUniform1iARB"                , (void **)&pglUniform1iARB },
{ "glUniform2iARB"                , (void **)&pglUniform2iARB },
{ "glUniform3iARB"                , (void **)&pglUniform3iARB },
{ "glUniform4iARB"                , (void **)&pglUniform4iARB },
{ "glUniform1fvARB"               , (void **)&pglUniform1fvARB },
{ "glUniform2fvARB"               , (void **)&pglUniform2fvARB },
{ "glUniform3fvARB"               , (void **)&pglUniform3fvARB },
{ "glUniform4fvARB"               , (void **)&pglUniform4fvARB },
{ "glUniform1ivARB"               , (void **)&pglUniform1ivARB },
{ "glUniform2ivARB"               , (void **)&pglUniform2ivARB },
{ "glUniform3ivARB"               , (void **)&pglUniform3ivARB },
{ "glUniform4ivARB"               , (void **)&pglUniform4ivARB },
{ "glUniformMatrix2fvARB"         , (void **)&pglUniformMatrix2fvARB },
{ "glUniformMatrix3fvARB"         , (void **)&pglUniformMatrix3fvARB },
{ "glUniformMatrix4fvARB"         , (void **)&pglUniformMatrix4fvARB },
{ "glGetObjectParameterfvARB"     , (void **)&pglGetObjectParameterfvARB },
{ "glGetObjectParameterivARB"     , (void **)&pglGetObjectParameterivARB },
{ "glGetInfoLogARB"               , (void **)&pglGetInfoLogARB },
{ "glGetAttachedObjectsARB"       , (void **)&pglGetAttachedObjectsARB },
{ "glGetUniformLocationARB"       , (void **)&pglGetUniformLocationARB },
{ "glGetActiveUniformARB"         , (void **)&pglGetActiveUniformARB },
{ "glGetUniformfvARB"             , (void **)&pglGetUniformfvARB },
{ "glGetUniformivARB"             , (void **)&pglGetUniformivARB },
{ "glGetShaderSourceARB"          , (void **)&pglGetShaderSourceARB },
{ "glVertexAttribPointerARB"      , (void **)&pglVertexAttribPointerARB },
{ "glEnableVertexAttribArrayARB"  , (void **)&pglEnableVertexAttribArrayARB },
{ "glDisableVertexAttribArrayARB" , (void **)&pglDisableVertexAttribArrayARB },
{ "glBindAttribLocationARB"       , (void **)&pglBindAttribLocationARB },
{ "glGetActiveAttribARB"          , (void **)&pglGetActiveAttribARB },
{ "glGetAttribLocationARB"        , (void **)&pglGetAttribLocationARB },
{ "glVertexAttrib2f"              , (void **)&pglVertexAttrib2fARB },
{ "glVertexAttrib2fv"             , (void **)&pglVertexAttrib2fvARB },
{ "glVertexAttrib3fv"             , (void **)&pglVertexAttrib3fvARB },
{ NULL, NULL }
};

static dllfunc_t vertexshaderfuncs[] =
{
{ "glVertexAttribPointerARB"      , (void **)&pglVertexAttribPointerARB },
{ "glEnableVertexAttribArrayARB"  , (void **)&pglEnableVertexAttribArrayARB },
{ "glDisableVertexAttribArrayARB" , (void **)&pglDisableVertexAttribArrayARB },
{ "glBindAttribLocationARB"       , (void **)&pglBindAttribLocationARB },
{ "glGetActiveAttribARB"          , (void **)&pglGetActiveAttribARB },
{ "glGetAttribLocationARB"        , (void **)&pglGetAttribLocationARB },
{ NULL, NULL }
};

static dllfunc_t cgprogramfuncs[] =
{
{ "glBindProgramARB"              , (void **)&pglBindProgramARB },
{ "glDeleteProgramsARB"           , (void **)&pglDeleteProgramsARB },
{ "glGenProgramsARB"              , (void **)&pglGenProgramsARB },
{ "glProgramStringARB"            , (void **)&pglProgramStringARB },
{ "glGetProgramivARB"             , (void **)&pglGetProgramivARB },
{ "glProgramEnvParameter4fARB"    , (void **)&pglProgramEnvParameter4fARB },
{ "glProgramLocalParameter4fARB"  , (void **)&pglProgramLocalParameter4fARB },
{ NULL, NULL }
};

static dllfunc_t vbofuncs[] =
{
{ "glBindBufferARB"    , (void **)&pglBindBufferARB },
{ "glDeleteBuffersARB" , (void **)&pglDeleteBuffersARB },
{ "glGenBuffersARB"    , (void **)&pglGenBuffersARB },
{ "glIsBufferARB"      , (void **)&pglIsBufferARB },
{ "glMapBufferARB"     , (void **)&pglMapBufferARB },
{ "glUnmapBufferARB"   , (void **)&pglUnmapBufferARB },
{ "glBufferDataARB"    , (void **)&pglBufferDataARB },
{ "glBufferSubDataARB" , (void **)&pglBufferSubDataARB },
{ NULL, NULL }
};

static dllfunc_t vaofuncs[] =
{
{ "glBindVertexArray"    , (void **)&pglBindVertexArray },
{ "glDeleteVertexArrays" , (void **)&pglDeleteVertexArrays },
{ "glGenVertexArrays"    , (void **)&pglGenVertexArrays },
{ "glIsVertexArray"      , (void **)&pglIsVertexArray },
{ NULL, NULL }
};

static dllfunc_t occlusionfunc[] =
{
{ "glGenQueriesARB"        , (void **)&pglGenQueriesARB },
{ "glDeleteQueriesARB"     , (void **)&pglDeleteQueriesARB },
{ "glIsQueryARB"           , (void **)&pglIsQueryARB },
{ "glBeginQueryARB"        , (void **)&pglBeginQueryARB },
{ "glEndQueryARB"          , (void **)&pglEndQueryARB },
{ "glGetQueryivARB"        , (void **)&pglGetQueryivARB },
{ "glGetQueryObjectivARB"  , (void **)&pglGetQueryObjectivARB },
{ "glGetQueryObjectuivARB" , (void **)&pglGetQueryObjectuivARB },
{ NULL, NULL }
};

static dllfunc_t texturecompressionfuncs[] =
{
{ "glCompressedTexImage3DARB"    , (void **)&pglCompressedTexImage3DARB },
{ "glCompressedTexImage2DARB"    , (void **)&pglCompressedTexImage2DARB },
{ "glCompressedTexImage1DARB"    , (void **)&pglCompressedTexImage1DARB },
{ "glCompressedTexSubImage3DARB" , (void **)&pglCompressedTexSubImage3DARB },
{ "glCompressedTexSubImage2DARB" , (void **)&pglCompressedTexSubImage2DARB },
{ "glCompressedTexSubImage1DARB" , (void **)&pglCompressedTexSubImage1DARB },
{ "glGetCompressedTexImageARB"   , (void **)&pglGetCompressedTexImage },
{ NULL, NULL }
};

static dllfunc_t arbfbofuncs[] =
{
{ "glIsRenderbuffer"                      , (void **)&pglIsRenderbuffer },
{ "glBindRenderbuffer"                    , (void **)&pglBindRenderbuffer },
{ "glDeleteRenderbuffers"                 , (void **)&pglDeleteRenderbuffers },
{ "glGenRenderbuffers"                    , (void **)&pglGenRenderbuffers },
{ "glRenderbufferStorage"                 , (void **)&pglRenderbufferStorage },
{ "glGetRenderbufferParameteriv"          , (void **)&pglGetRenderbufferParameteriv },
{ "glIsFramebuffer"                       , (void **)&pglIsFramebuffer },
{ "glBindFramebuffer"                     , (void **)&pglBindFramebuffer },
{ "glDeleteFramebuffers"                  , (void **)&pglDeleteFramebuffers },
{ "glGenFramebuffers"                     , (void **)&pglGenFramebuffers },
{ "glCheckFramebufferStatus"              , (void **)&pglCheckFramebufferStatus },
{ "glFramebufferTexture1D"                , (void **)&pglFramebufferTexture1D },
{ "glFramebufferTexture2D"                , (void **)&pglFramebufferTexture2D },
{ "glFramebufferTexture3D"                , (void **)&pglFramebufferTexture3D },
{ "glFramebufferRenderbuffer"             , (void **)&pglFramebufferRenderbuffer },
{ "glGetFramebufferAttachmentParameteriv" , (void **)&pglGetFramebufferAttachmentParameteriv },
{ "glGenerateMipmap"                      , (void **)&pglGenerateMipmap },
{ NULL, NULL}
};

static dllfunc_t wglproc_funcs[] =
{
{ "wglGetProcAddress"  , (void **)&pwglGetProcAddress },
{ NULL, NULL }
};

static dllhandle_t opengl_dll = NULL;

/*
=================
GL_SetExtension
=================
*/
void GL_SetExtension( int r_ext, int enable )
{
	if( r_ext >= 0 && r_ext < R_EXTCOUNT )
		glConfig.extension[r_ext] = enable ? GL_TRUE : GL_FALSE;
	else ALERT( at_error, "GL_SetExtension: invalid extension %d\n", r_ext );
}

/*
=================
GL_Active
=================
*/
bool GL_Active( void )
{
	return (opengl_dll != NULL) ? true : false;
}

/*
=================
GL_Support
=================
*/
bool GL_Support( int r_ext )
{
	if( r_ext >= 0 && r_ext < R_EXTCOUNT )
		return glConfig.extension[r_ext] ? true : false;
	ALERT( at_error, "GL_Support: invalid extension %d\n", r_ext );
	return false;		
}

/*
=================
GL_GetProcAddress
=================
*/
void *GL_GetProcAddress( const char *name )
{
	void	*p = NULL;

	if( pwglGetProcAddress != NULL )
		p = (void *)pwglGetProcAddress( name );
	if( !p ) p = (void *)Sys_GetProcAddress( opengl_dll, name );

	return p;
}

/*
=================
GL_CheckExtension
=================
*/
void GL_CheckExtension( const char *name, const dllfunc_t *funcs, int r_ext )
{
	const dllfunc_t	*func;

	ALERT( at_aiconsole, "GL_CheckExtension: %s ", name );

	if(( name[2] == '_' || name[3] == '_' ) && !Q_strstr( glConfig.extensions_string, name ))
	{
		GL_SetExtension( r_ext, false );	// update render info
		ALERT( at_aiconsole, "- ^1failed\n" );
		return;
	}

	// clear exports
	for( func = funcs; func && func->name; func++ )
		*func->func = NULL;

	GL_SetExtension( r_ext, true ); // predict extension state
	for( func = funcs; func && func->name != NULL; func++ )
	{
		// functions are cleared before all the extensions are evaluated
		if(!(*func->func = (void *)GL_GetProcAddress( func->name )))
			GL_SetExtension( r_ext, false ); // one or more functions are invalid, extension will be disabled
	}

	if( GL_Support( r_ext ))
		ALERT( at_aiconsole, "- ^2enabled\n" );
}

static void GL_InitExtensions( void )
{
	// initialize gl extensions
	GL_CheckExtension( "OpenGL 1.1.0", opengl_110funcs, R_OPENGL_110 );

	if( !GL_Support( R_OPENGL_110 ))
	{
		ALERT( at_error, "OpenGL 1.0 can't be installed. Custom renderer disabled\n" );
		g_fRenderInitialized = FALSE;
		return;
	}

	// get our various GL strings
	glConfig.vendor_string = pglGetString( GL_VENDOR );
	glConfig.renderer_string = pglGetString( GL_RENDERER );
	glConfig.version_string = pglGetString( GL_VERSION );
	glConfig.extensions_string = pglGetString( GL_EXTENSIONS );

	// initalize until base opengl functions loaded
	GL_CheckExtension( "OpenGL ProcAddress", wglproc_funcs, R_WGL_PROCADDRESS );

	GL_CheckExtension( "glDrawRangeElements", drawrangeelementsfuncs, R_DRAW_RANGEELEMENTS_EXT );

	if( !GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
		GL_CheckExtension( "GL_EXT_draw_range_elements", drawrangeelementsextfuncs, R_DRAW_RANGEELEMENTS_EXT );

	// multitexture
	glConfig.max_texture_units = 1;
	GL_CheckExtension( "GL_ARB_multitexture", multitexturefuncs, R_ARB_MULTITEXTURE );

	if( GL_Support( R_ARB_MULTITEXTURE ))
	{
		pglGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &glConfig.max_texture_units );
		GL_CheckExtension( "GL_ARB_texture_env_combine", NULL, R_ENV_COMBINE_EXT );

		if( !GL_Support( R_ENV_COMBINE_EXT ))
			GL_CheckExtension( "GL_EXT_texture_env_combine", NULL, R_ENV_COMBINE_EXT );

		if( GL_Support( R_ENV_COMBINE_EXT ))
			GL_CheckExtension( "GL_ARB_texture_env_dot3", NULL, R_DOT3_ARB_EXT );
	}
	else
	{
		GL_CheckExtension( "GL_SGIS_multitexture", sgis_multitexturefuncs, R_ARB_MULTITEXTURE );
		if( GL_Support( R_ARB_MULTITEXTURE )) glConfig.max_texture_units = 2;
	}

	if( glConfig.max_texture_units == 1 )
		GL_SetExtension( R_ARB_MULTITEXTURE, false );

	// 3d texture support
	GL_CheckExtension( "GL_EXT_texture3D", texture3dextfuncs, R_TEXTURE_3D_EXT );

	if( GL_Support( R_TEXTURE_3D_EXT ))
	{
		pglGetIntegerv( GL_MAX_3D_TEXTURE_SIZE, &glConfig.max_3d_texture_size );

		if( glConfig.max_3d_texture_size < 32 )
		{
			GL_SetExtension( R_TEXTURE_3D_EXT, false );
			ALERT( at_error, "GL_EXT_texture3D reported bogus GL_MAX_3D_TEXTURE_SIZE, disabled\n" );
		}
	}

	GL_CheckExtension( "GL_SGIS_generate_mipmap", NULL, R_SGIS_MIPMAPS_EXT );

	// hardware cubemaps
	GL_CheckExtension( "GL_ARB_texture_cube_map", NULL, R_TEXTURECUBEMAP_EXT );

	if( GL_Support( R_TEXTURECUBEMAP_EXT ))
		pglGetIntegerv( GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, &glConfig.max_cubemap_size );

	// point particles extension
	GL_CheckExtension( "GL_EXT_point_parameters", pointparametersfunc, R_EXT_POINTPARAMETERS );

	GL_CheckExtension( "GL_ARB_texture_non_power_of_two", NULL, R_ARB_TEXTURE_NPOT_EXT );
	GL_CheckExtension( "GL_ARB_texture_compression", texturecompressionfuncs, R_TEXTURE_COMPRESSION_EXT );
	GL_CheckExtension( "GL_EXT_compiled_vertex_array", compiledvertexarrayfuncs, R_CUSTOM_VERTEX_ARRAY_EXT );

	if( !GL_Support( R_CUSTOM_VERTEX_ARRAY_EXT ))
		GL_CheckExtension( "GL_SGI_compiled_vertex_array", compiledvertexarrayfuncs, R_CUSTOM_VERTEX_ARRAY_EXT );

	GL_CheckExtension( "GL_EXT_texture_edge_clamp", NULL, R_CLAMPTOEDGE_EXT );

	if( !GL_Support( R_CLAMPTOEDGE_EXT ))
		GL_CheckExtension("GL_SGIS_texture_edge_clamp", NULL, R_CLAMPTOEDGE_EXT );

	glConfig.max_texture_anisotropy = 0.0f;
	GL_CheckExtension( "GL_EXT_texture_filter_anisotropic", NULL, R_ANISOTROPY_EXT );

	if( GL_Support( R_ANISOTROPY_EXT ))
		pglGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glConfig.max_texture_anisotropy );

	GL_CheckExtension( "GL_EXT_texture_lod_bias", NULL, R_TEXTURE_LODBIAS );
	if( GL_Support( R_TEXTURE_LODBIAS ))
		pglGetFloatv( GL_MAX_TEXTURE_LOD_BIAS_EXT, &glConfig.max_texture_lodbias );

	GL_CheckExtension( "GL_ARB_texture_border_clamp", NULL, R_CLAMP_TEXBORDER_EXT );

	GL_CheckExtension( "GL_ARB_copy_image", copyimagesubdata, R_COPY_IMAGE_EXT );

	GL_CheckExtension( "glStencilOpSeparate", gl2separatestencilfuncs, R_SEPARATESTENCIL_EXT );
	if( !GL_Support( R_SEPARATESTENCIL_EXT ))
		GL_CheckExtension("GL_ATI_separate_stencil", atiseparatestencilfuncs, R_SEPARATESTENCIL_EXT );

	GL_CheckExtension( "GL_EXT_stencil_two_side", stenciltwosidefuncs, R_STENCILTWOSIDE_EXT );
	GL_CheckExtension( "GL_ARB_vertex_buffer_object", vbofuncs, R_ARB_VERTEX_BUFFER_OBJECT_EXT );
	GL_CheckExtension( "GL_ARB_vertex_array_object", vaofuncs, R_ARB_VERTEX_ARRAY_OBJECT_EXT );

	// we don't care if it's an extension or not, they are identical functions, so keep it simple in the rendering code
	if( pglDrawRangeElementsEXT == NULL ) pglDrawRangeElementsEXT = pglDrawRangeElements;

	GL_CheckExtension( "GL_ARB_vertex_program", cgprogramfuncs, R_VERTEX_PROGRAM_EXT );
	GL_CheckExtension( "GL_ARB_fragment_program", cgprogramfuncs, R_FRAGMENT_PROGRAM_EXT );
	GL_CheckExtension( "GL_ARB_texture_env_add", NULL, R_TEXTURE_ENV_ADD_EXT );

	GL_CheckExtension( "GL_NV_register_combiners", nvcombinersfuncs, R_NV_COMBINE_EXT );

	if( GL_Support( R_NV_COMBINE_EXT ))
		pglGetIntegerv( GL_MAX_GENERAL_COMBINERS_NV, &glConfig.max_nv_combiners );

	// vp and fp shaders
	GL_CheckExtension( "GL_ARB_shader_objects", shaderobjectsfuncs, R_SHADER_OBJECTS_EXT );
	GL_CheckExtension( "GL_ARB_shading_language_100", NULL, R_SHADER_GLSL100_EXT );
	GL_CheckExtension( "GL_ARB_vertex_shader", vertexshaderfuncs, R_VERTEX_SHADER_EXT );
	GL_CheckExtension( "GL_ARB_fragment_shader", NULL, R_FRAGMENT_SHADER_EXT );

	GL_CheckExtension( "GL_ARB_depth_texture", NULL, R_DEPTH_TEXTURE );
	GL_CheckExtension( "GL_ARB_shadow", NULL, R_SHADOW_EXT );

	// occlusion queries
	GL_CheckExtension( "GL_ARB_occlusion_query", occlusionfunc, R_OCCLUSION_QUERIES_EXT );

	// rectangle textures support
	if( Q_strstr( glConfig.extensions_string, "GL_NV_texture_rectangle" ))
	{
		glConfig.texRectangle = GL_TEXTURE_RECTANGLE_NV;
		pglGetIntegerv( GL_MAX_RECTANGLE_TEXTURE_SIZE_NV, &glConfig.max_2d_rectangle_size );
	}
	else if( Q_strstr( glConfig.extensions_string, "GL_EXT_texture_rectangle" ))
	{
		glConfig.texRectangle = GL_TEXTURE_RECTANGLE_EXT;
		pglGetIntegerv( GL_MAX_RECTANGLE_TEXTURE_SIZE_EXT, &glConfig.max_2d_rectangle_size );
	}
	else glConfig.texRectangle = glConfig.max_2d_rectangle_size = 0; // no rectangle

	glConfig.max_2d_texture_size = 0;
	pglGetIntegerv( GL_MAX_TEXTURE_SIZE, &glConfig.max_2d_texture_size );
	if( glConfig.max_2d_texture_size <= 0 ) glConfig.max_2d_texture_size = 256;

	// software mipmap generator does wrong result with NPOT textures ...
	if( !GL_Support( R_SGIS_MIPMAPS_EXT ))
		GL_SetExtension( R_ARB_TEXTURE_NPOT_EXT, false );

	// FBO support
	GL_CheckExtension( "GL_ARB_framebuffer_object", arbfbofuncs, R_FRAMEBUFFER_OBJECT );

	// Paranoia OpenGL32.dll may be eliminate shadows. Run special check for it
	GL_CheckExtension( "PARANOIA_HACKS_V1", NULL, R_PARANOIA_EXT );

	// check for hardware skinning
	if( GL_Support( R_SHADER_OBJECTS_EXT ))
	{
		pglGetIntegerv( GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB, &glConfig.max_vertex_uniforms );
		pglGetIntegerv( GL_MAX_VERTEX_ATTRIBS_ARB, &glConfig.max_vertex_attribs );

		// TODO: tune this
		int reserved = 16 * 10; // approximation how many uniforms we have besides the bone matrices

		glConfig.max_skinning_bones = (int)bound( 0.0f, (Q_max( glConfig.max_vertex_uniforms - reserved, 0 ) / 16 ), MAXSTUDIOBONES );
		GL_SetExtension( R_HARDWARE_SKINNING, glConfig.max_skinning_bones >= 1 ? true : false );
	}
	else glConfig.max_skinning_bones = 0;
}

/*
===============
GL_SetDefaultState
===============
*/
static void GL_SetDefaultState( void )
{
	if( r_stencilbits->value != 0 )
		glState.stencilEnabled = true;
	else glState.stencilEnabled = false;

	pglDisable( GL_SCISSOR_TEST );
	GL_BindFBO( FBO_MAIN );
}

static void GL_CompileFragmentProgram( const char *source, size_t source_size, unsigned int *shader )
{
	pglEnable( GL_FRAGMENT_PROGRAM_ARB );

	pglGenProgramsARB( 1, shader );

	pglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, *shader );
	pglProgramStringARB( GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, source_size, source );

	int isNative = 0;
	pglGetProgramivARB( GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &isNative );

	if( !isNative ) ALERT( at_warning, "Fragment program is above native limits!\n" );
	pglDisable( GL_FRAGMENT_PROGRAM_ARB );
}

static void GL_InitPrograms( void )
{
	memset( &cg, 0, sizeof( cg ));
	memset( &glsl, 0, sizeof( glsl ));

	if( GL_Support( R_FRAGMENT_PROGRAM_EXT ))
	{
		GL_CompileFragmentProgram( fp_specular_source, sizeof( fp_specular_source ) - 1, &cg.specular_shader );
		GL_CompileFragmentProgram( fp_specular_high_source, sizeof( fp_specular_high_source ) - 1, &cg.specular_high_shader );
		GL_CompileFragmentProgram( fp_shadow_source0, sizeof( fp_shadow_source0 ) - 1, &cg.shadow_shader0 );
		GL_CompileFragmentProgram( fp_shadow_source1, sizeof( fp_shadow_source1 ) - 1, &cg.shadow_shader1 );
		GL_CompileFragmentProgram( fp_liquid_source, sizeof( fp_liquid_source ) - 1, &cg.liquid_shader );

		GL_CompileFragmentProgram( fp_decal0_source, sizeof( fp_decal0_source ) - 1, &cg.decal0_shader );
		GL_CompileFragmentProgram( fp_decal1_source, sizeof( fp_decal1_source ) - 1, &cg.decal1_shader );
		GL_CompileFragmentProgram( fp_decal2_source, sizeof( fp_decal2_source ) - 1, &cg.decal2_shader );
		GL_CompileFragmentProgram( fp_decal3_source, sizeof( fp_decal3_source ) - 1, &cg.decal3_shader );
	}

	if( GL_Support( R_SHADER_GLSL100_EXT ))
	{
		glsl.pWaterShader = new GLSLShader( g_szVertexShader_RefractionWater, g_szFragmentShader_RefractionWater );
		glsl.pGlassShader = new GLSLShader( g_szVertexShader_RefractionGlass, g_szFragmentShader_RefractionGlass );
	}
}

static void GL_FreePorgrams( void )
{
	if( cg.specular_shader ) pglDeleteProgramsARB( 1, &cg.specular_shader );
	if( cg.specular_high_shader ) pglDeleteProgramsARB( 1, &cg.specular_high_shader );
	if( cg.shadow_shader0 ) pglDeleteProgramsARB( 1, &cg.shadow_shader0 );
	if( cg.shadow_shader1 ) pglDeleteProgramsARB( 1, &cg.shadow_shader1 );
	if( cg.liquid_shader ) pglDeleteProgramsARB( 1, &cg.liquid_shader );

	if( cg.decal0_shader ) pglDeleteProgramsARB( 1, &cg.decal0_shader );
	if( cg.decal1_shader ) pglDeleteProgramsARB( 1, &cg.decal1_shader );
	if( cg.decal2_shader ) pglDeleteProgramsARB( 1, &cg.decal2_shader );
	if( cg.decal3_shader ) pglDeleteProgramsARB( 1, &cg.decal3_shader );

	if( GL_Support( R_SHADER_GLSL100_EXT ))
	{
		delete glsl.pWaterShader;
		delete glsl.pGlassShader;
	}
}

static void GL_InitTextures( void )
{
	// just get it from engine
	tr.defaultTexture = FIND_TEXTURE( "*default" );   	// use for bad textures
	tr.normalizeCubemap = FIND_TEXTURE( "*normalize" );
	tr.normalmapTexture = FIND_TEXTURE( "*blankbump" );
	tr.deluxemapTexture = FIND_TEXTURE( "*gray" );		// like Vector( 0, 0, 0 )
	tr.dlightCubeTexture = FIND_TEXTURE( "*grayCube" );	// *whiteCube get brightness too much
	tr.attenuation_1d = FIND_TEXTURE( "*atten" );
	tr.whiteTexture = FIND_TEXTURE( "*white" );
	tr.blackTexture = FIND_TEXTURE( "*black" );
	tr.grayTexture = FIND_TEXTURE( "*gray" );
}

/*
==================
GL_Init
==================
*/
bool GL_Init( void )
{
	if( !Sys_LoadLibrary( "*opengl32.dll", &opengl_dll ))
	{
		g_fRenderInitialized = FALSE;
		return false;
	}

	GL_InitExtensions();
	GL_InitPrograms();
	GL_InitGPUShaders();
	GL_InitTextures();
	R_InitMaterials();
	GL_SetDefaultState();

	pglPointSize( 10.0f );

	if( !GL_Support(R_ARB_MULTITEXTURE) || !GL_Support(R_DOT3_ARB_EXT) || !GL_Support(R_TEXTURECUBEMAP_EXT) || glConfig.max_texture_units < 2 )
	{
		ALERT( at_error, "Paranoia custom renderer is not supported by your videocard\n" );
		CVAR_SET_FLOAT( "gl_renderer", 0 );
		g_fRenderInitialized = FALSE;
		GL_Shutdown();
		return false;
	}

	InitSky();
	InitGlows();
	InitPostEffects();
	InitRain();	// rain
	GrassInit();	// buz
	DecalsInit();

	return true;
}

void GL_VidInit( void )
{
	R_NewMap ();
}

/*
===============
GL_Shutdown
===============
*/
void GL_Shutdown( void )
{
	if( !opengl_dll ) return;

	g_StudioRenderer.DestroyAllModelInstances();

	if( tr.materials )
		Mem_Free( tr.materials );

	tr.fbo_mirror.Free();
	tr.fbo_shadow.Free();
	tr.fbo_sunshadow.Free();

	R_FreeCinematics();
	DeleteWorldExtradata();
	DecalsShutdown();
	FreeBuffer();
	GL_FreePorgrams();
	GL_FreeGPUShaders();

	Sys_FreeLibrary( &opengl_dll );

	// now all extensions are disabled
	memset( glConfig.extension, 0, sizeof( glConfig.extension[0] ) * R_EXTCOUNT );
}