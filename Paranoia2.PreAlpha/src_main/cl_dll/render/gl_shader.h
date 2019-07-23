/*
r_programs.h - compiled CG programs
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/
#ifndef GL_SHADER_H
#define GL_SHADER_H

// shader version for ATI cards. NV will use register combiners
const char fp_specular_source[] =
"!!ARBfp1.0\n"
"OPTION ARB_precision_hint_fastest;\n"
"ATTRIB tex0 = fragment.texcoord[0];\n"
"ATTRIB tex1 = fragment.texcoord[1];\n"
"ATTRIB tex2 = fragment.texcoord[2];\n"
"ATTRIB tex3 = fragment.texcoord[3];\n"
"PARAM scaler = { 4, 4, 2, -1 };\n"
"OUTPUT outColor = result.color;\n"

"TEMP eyevec, lightvec;\n"
"TEMP specdot, col, res;\n"

"TEX eyevec, tex0, texture[0], CUBE;\n"
"MAD eyevec.rgb, eyevec, scaler.b, scaler.a;\n"

"TEX lightvec, tex1, texture[1], 2D;\n"
"MAD lightvec.rgb, lightvec, scaler.b, scaler.a;\n"

"DP3_SAT specdot.a, eyevec, lightvec;\n"
"POW specdot.a, specdot.a, scaler.r;\n"

"TEX col, tex2, texture[2], 2D;\n" // get specular color from lightmap
"TEX res, tex3, texture[3], 2D;\n" // get color from glossmap
"MUL res.rgb, col, res;\n"

"MUL_SAT outColor, res, specdot.a;\n"
"END";

// High quality specular shader
const char fp_specular_high_source[] =
"!!ARBfp1.0\n"
"OPTION ARB_precision_hint_fastest;\n"
"ATTRIB tex_camdir = fragment.texcoord[0];\n" // camera dir
"ATTRIB tex_lightmap = fragment.texcoord[1];\n" // lightmap texcoord
"ATTRIB tex_face = fragment.texcoord[3];\n" // face texture texcoord
"PARAM scaler = { 16, 8, 2, -1 };\n"
"OUTPUT outColor = result.color;\n"

"TEMP vec, halfvec;\n"
"TEMP specdot, color;\n"

// expand light vector
"TEX vec, tex_lightmap, texture[1], 2D;\n"
"MAD vec.rgb, vec, scaler.b, scaler.a;\n"

// normalize vector to camera
"DP3 halfvec.x, tex_camdir, tex_camdir;\n"
"RSQ halfvec.x, halfvec.x;\n"
"MUL halfvec, tex_camdir, halfvec.x;\n"

// add light vector and renormalize
"ADD halfvec, halfvec, vec;\n"

"DP3 halfvec.w, halfvec, halfvec;\n"
"RSQ halfvec.w, halfvec.w;\n"
"MUL halfvec.xyz, halfvec, halfvec.w;\n"

// expand normal vector
"TEX vec, tex_face, texture[0], 2D;\n"
"MAD vec.rgb, vec, scaler.b, scaler.a;\n"

// calc specular factor
"DP3_SAT specdot.a, vec, halfvec;\n"
"POW specdot.a, specdot.a, scaler.r;\n"

// multiply by specular color
"TEX color, tex_lightmap, texture[2], 2D;\n"
"MUL_SAT color, color, specdot.a;\n"

// multiply by gloss map
"TEX vec, tex_face, texture[3], 2D;\n"
"MUL outColor, color, vec;\n"
"END";

const char fp_shadow_source0[] =
"!!ARBfp1.0"
"OPTION ARB_fragment_program_shadow;"
"OPTION ARB_precision_hint_fastest;"
"PARAM c[5] = {"
"{0, -0.00390625},"
"{-0.00390625, 0},"
"{0.00390625, 0},"
"{0, 0.00390625},"
"{5, 1}};"
"TEMP R0;"
"TEMP R1;"
"RCP R0.x, fragment.texcoord[2].w;"
"MUL R0.xyz, fragment.texcoord[2], R0.x;"
"TEX R0.w, R0, texture[2], SHADOW2D;"
"ADD R1.xyz, R0, c[0];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"ADD R1.xyz, R0, c[1];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"ADD R1.xyz, R0, c[2];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"ADD R1.xyz, R0, c[3];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"RCP R1.w, c[4].x;"
"MUL R1.w, R0.w, R1.w;"
"TXP R0, fragment.texcoord[0], texture[0], 2D;"
"MUL R1, R0, R1.w;"
"TXP R0, fragment.texcoord[1], texture[1], 1D;"
"MUL R1, R1, R0;"
"MUL result.color.xyz, fragment.color.primary, R1;"
"MOV result.color.w, c[4].y;"
"END";

const char fp_shadow_source1[] =
"!!ARBfp1.0"
"OPTION ARB_fragment_program_shadow;"
"OPTION ARB_precision_hint_fastest;"
"PARAM c[5] = {"
"{0, -0.00390625},"
"{-0.00390625, 0},"
"{0.00390625, 0},"
"{0, 0.00390625},"
"{5, 1}};"
"TEMP R0;"
"TEMP R1;"
"RCP R0.x, fragment.texcoord[2].w;"
"MUL R0.xyz, fragment.texcoord[2], R0.x;"
"TEX R0.w, R0, texture[2], SHADOW2D;"
"ADD R1.xyz, R0, c[0];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"ADD R1.xyz, R0, c[1];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"ADD R1.xyz, R0, c[2];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"ADD R1.xyz, R0, c[3];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"RCP R1.w, c[4].x;"
"MUL R1.w, R0.w, R1.w;"
"TXP R0, fragment.texcoord[1], texture[1], 2D;"
"MUL R1, R0, R1.w;"
"TXP R0, fragment.texcoord[0], texture[0], 1D;"
"MUL R1, R1, R0;"
"MUL result.color.xyz, fragment.color.primary, R1;"
"MOV result.color.w, c[4].y;"
"END";

const char fp_decal0_source[] =
"!!ARBfp1.0"
"TEMP R0;"
"TEMP R1;"
"TXP R0, fragment.texcoord[0], texture[0], 2D;"
"TXP R1, fragment.texcoord[1], texture[1], 1D;"
"MUL R1, R0, R1;"
"MUL R1, fragment.color.primary, R1;"
"TEX R0, fragment.texcoord[3], texture[3], 2D;"
"MUL R0.xyz, R0, R0.w;"
"MUL result.color.xyz, R0, R1;"
"MOV result.color.w, 1.0;"
"END";

const char fp_decal1_source[] =
"!!ARBfp1.0"
"OPTION ARB_fragment_program_shadow;"
"OPTION ARB_precision_hint_fastest;"
"TEMP R0;"
"TEMP R1;"
"TEMP R3;"
"TEMP R4;"
"RCP R0.x, fragment.texcoord[2].w;"
"MUL R0.xyz, fragment.texcoord[2], R0.x;"
"TEX R1.w, R0, texture[2], SHADOW2D;"
"TXP R0, fragment.texcoord[0], texture[0], 2D;"
"MUL R1, R0, R1.w;"
"TXP R0, fragment.texcoord[1], texture[1], 1D;"
"MUL R1, R1, R0;"
"MUL R3.xyz, fragment.color.primary, R1;"
"TEX R4, fragment.texcoord[3], texture[3], 2D;"
"MUL R4.xyz, R4, R4.w;"
"MUL result.color.xyz, R3, R4;"
"MOV result.color.w, 1.0;"
"END";

const char fp_decal2_source[] =
"!!ARBfp1.0"
"OPTION ARB_fragment_program_shadow;"
"OPTION ARB_precision_hint_fastest;"
"PARAM c[5] = {"
"{0, -0.00390625},"
"{-0.00390625, 0},"
"{0.00390625, 0},"
"{0, 0.00390625},"
"{5, 1}};"
"TEMP R0;"
"TEMP R1;"
"TEMP R3;"
"TEMP R4;"
"RCP R0.x, fragment.texcoord[2].w;"
"MUL R0.xyz, fragment.texcoord[2], R0.x;"
"TEX R0.w, R0, texture[2], SHADOW2D;"
"ADD R1.xyz, R0, c[0];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"ADD R1.xyz, R0, c[1];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"ADD R1.xyz, R0, c[2];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"ADD R1.xyz, R0, c[3];"
"TEX R1.w, R1, texture[2], SHADOW2D;"
"ADD R0.w, R1.w, R0.w;"
"RCP R1.w, c[4].x;"
"MUL R1.w, R0.w, R1.w;"
"TXP R0, fragment.texcoord[0], texture[0], 2D;"
"MUL R1, R0, R1.w;"
"TXP R0, fragment.texcoord[1], texture[1], 1D;"
"MUL R1, R1, R0;"
"MUL R3.xyz, fragment.color.primary, R1;"
"TEX R4, fragment.texcoord[3], texture[3], 2D;"
"MUL R4.xyz, R4, R4.w;"
"MUL result.color.xyz, R3, R4;"
"MOV result.color.w, c[4].y;"
"END";

const char fp_decal3_source[] =
"!!ARBfp1.0"
"TEMP R0;"
"TEMP R1;"
"TXP R0, fragment.texcoord[0], texture[0], CUBE;"
"TXP R1, fragment.texcoord[1], texture[1], 3D;"
"MUL R1, R0, R1;"
"MUL R1, fragment.color.primary, R1;"
"TEX R0, fragment.texcoord[3], texture[3], 2D;"
"MUL R0.xyz, R0, R0.w;"
"MUL result.color.xyz, R0, R1;"
"MOV result.color.w, 1.0;"
"END";

const char fp_liquid_source[ ] =
"!!ARBfp1.0\n"
"TEMP R0;\n"
"TEX R0, fragment.texcoord[1], texture[1], 3D;\n"
"MAD R0, R0, 2.0, fragment.texcoord[0];\n"
"TXP result.color.xyz, R0, texture[0], 2D;\n"
"DP3 R0.w, fragment.texcoord[2], fragment.texcoord[2];\n"
"RSQ R0.w, R0.w;\n"
"MUL R0.xyz, R0.w, fragment.texcoord[2];\n"
"DP3 R0.w, R0, fragment.texcoord[3];\n"
"ADD R0.w, R0.w, 1.0;\n"
"MAD result.color.w, R0.w, 0.8, 0.1;\n"
"END\n";

static const char g_szVertexShader_RefractionWater[] =
"uniform vec4 Local0;\n"
"\n"
"void main(void)\n"
"{\n"
"	vec4 v1 = ftransform();\n"
"	gl_Position = v1;\n"
"	gl_TexCoord[0] = v1;\n"
"	gl_TexCoord[1].xy = gl_MultiTexCoord0.xy;\n"
"	gl_TexCoord[2].xyz = Local0.xyz - gl_Vertex.xyz;\n"
"}";

static const char g_szFragmentShader_RefractionWater[] =
"#extension GL_ARB_texture_rectangle : enable\n"
"uniform vec4 Local1;\n"
"uniform vec4 Local2;\n"
"uniform sampler2D Texture0;\n"
"uniform sampler2D Texture1;\n"
"uniform sampler2DRect Texture2;\n"
"\n"
"void main(void)\n"
"{\n"
"	vec3 v1 = normalize( gl_TexCoord[2] ).xyz;\n"
"	vec3 v2 = normalize( texture2D( Texture1, gl_TexCoord[1].xy ) * 2.0 - 1.0 ).xyz;\n"
"  	vec2 v3 = (gl_TexCoord[0].xy / gl_TexCoord[0].w).xy * vec2( -0.5, 0.5 ) + 0.5;\n"
"	vec3 v4 = texture2D( Texture0, v3 + v2.xy / Local1.xy * Local1.z ).xyz;\n"
"	vec3 v5 = texture2DRect( Texture2, gl_FragCoord.xy + v2.xy * Local1.z ).xyz * Local2.xyz;\n"
"	float v6 = clamp( dot( v1, v2 ) + Local2.w, 0.0, 1.0 );\n"
"	gl_FragColor = vec4( mix( v4, v5, v6 ), 1.0 );\n"
"}\n";

static const char g_szVertexShader_RefractionGlass[] =
"void main(void)\n"
"{\n"
"	gl_TexCoord[0]=gl_MultiTexCoord0;\n"
"	gl_Position=ftransform();\n"
"}\n";

static const char g_szFragmentShader_RefractionGlass[] =
"#extension GL_ARB_texture_rectangle : enable\n"
"uniform vec4 Local0;\n"
"uniform sampler2D Texture0;\n"
"uniform sampler2D Texture1;\n"
"uniform sampler2DRect Texture2;\n"
"\n"
"void main(void)\n"
"{\n"
"	vec3 v1 = texture2D( Texture0, gl_TexCoord[0].xy ).xyz;\n"
"	vec3 v2 = normalize( texture2D( Texture1, gl_TexCoord[0].xy ) * 2.0 - 1.0 ).xyz;\n"
"	vec3 v3 = texture2DRect( Texture2, gl_FragCoord.xy + v2.xy * Local0.x ).xyz;\n"
"	gl_FragColor = vec4( v3 * v1 * 2.0, 1.0 );\n"
"}\n";

#endif//GL_SHADER_H