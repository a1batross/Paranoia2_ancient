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

#ifndef GL_DECALS_H
#define GL_DECALS_H

class DecalGroup;
class DecalGroupEntry
{
public:
	char		name[64];
	int		gl_diffuse_id;
	int		gl_normalmap_id;
	int		gl_heightmap_id;
	int		xsize, ysize;
	float		overlay;
	const DecalGroup	*group;	// get group name
};

class DecalGroup
{
public:
	DecalGroup( const char *name, int numelems, DecalGroupEntry *source );
	~DecalGroup();

	const DecalGroupEntry *GetRandomDecal( void );
	DecalGroup *GetNext( void ) { return pnext; }
	const char *GetName( void ) { return m_chGroupName; }
	const char *GetName( void ) const { return m_chGroupName; }
	static DecalGroup *FindGroup( const char *name );
	DecalGroupEntry *FindEntry( const char *name );
	DecalGroupEntry *GetEntry( int num );
private:
	char m_chGroupName[16];
	DecalGroupEntry *pEntryArray;
	DecalGroup *pnext;
	int size;
};

extern int gDecalsRendered;

void DrawDecals( void );
void DecalsInit( void );
void ClearDecals( void );
void DecalsShutdown( void );
void DecalsPrintDebugInfo( void );
void R_DrawDecalList( bool opaque );
int SaveDecalList( decallist_t *pBaseList, int count, qboolean changelevel );

#endif//GL_DECALS_H