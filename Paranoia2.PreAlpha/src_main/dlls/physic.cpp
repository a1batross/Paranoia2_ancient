/*
physic.cpp - common physics code
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

#include	"extdll.h"
#include  "util.h"
#include	"cbase.h"
#include	"saverestore.h"
#include	"client.h"
#include  "nodes.h"
#include	"decals.h"
#include	"gamerules.h"
#include	"game.h"
#include	"com_model.h"
#include	"features.h"
#include  "triangleapi.h"
#include  "render_api.h"
#include  "pm_defs.h"
#include  "trace.h"

unsigned int EngineSetFeatures( void )
{
	unsigned int flags = (ENGINE_WRITE_LARGE_COORD|ENGINE_BUILD_SURFMESHES|ENGINE_COMPUTE_STUDIO_LERP);

	if( g_iXashEngineBuildNumber >= 2148 )
		flags |= ENGINE_LARGE_LIGHTMAPS;

	return flags;
}

void Physic_SweepTest( CBaseEntity *pTouch, const Vector &start, const Vector &mins, const Vector &maxs, const Vector &end, trace_t *tr )
{
	if( !pTouch->pev->modelindex || !GET_MODEL_PTR( pTouch->edict() ))
	{
		// bad model?
		tr->allsolid = false;
		return;
	}

	Vector trace_mins, trace_maxs;
	UTIL_MoveBounds( start, mins, maxs, end, trace_mins, trace_maxs );

	// NOTE: pmove code completely ignore a bounds checking. So we need to do it here
	if( !BoundsIntersect( trace_mins, trace_maxs, pTouch->pev->absmin, pTouch->pev->absmax ))
	{
		tr->allsolid = false;
		return;
	}

	mmesh_t *pMesh = pTouch->m_BodyMesh.CheckMesh( pTouch->pev->origin, pTouch->pev->angles );
	areanode_t *pHeadNode = pTouch->m_BodyMesh.GetHeadNode();

	if( !pMesh )
	{
		// update cache or build from scratch
		if( !pTouch->m_BodyMesh.StudioConstructMesh( pTouch ))
		{
			// failed to build mesh for some reasons, so skip them
			tr->allsolid = false;
			return;
		}

		// NOTE: don't care about validity this pointer
		// just trace failed if it's happens
		pMesh = pTouch->m_BodyMesh.GetMesh();
		pHeadNode = pTouch->m_BodyMesh.GetHeadNode();
	}

	TraceMesh	trm;	// a name like Doom3 :-)

	trm.SetTraceMesh( pMesh, pHeadNode );
	trm.SetupTrace( start, mins, maxs, end, tr );

	if( trm.DoTrace())
	{
		if( tr->fraction < 1.0f || tr->startsolid )
			tr->ent = pTouch->edict();
	}
}

void SV_ClipMoveToEntity( edict_t *ent, const float *start, float *mins, float *maxs, const float *end, trace_t *trace )
{
	// convert edict_t to base entity
	CBaseEntity *pTouch = CBaseEntity::Instance( ent );

	if( !pTouch )
	{
		// removed entity?
		trace->allsolid = false;
		return;
	}

	Physic_SweepTest( pTouch, start, mins, maxs, end, trace );
}

void SV_ClipPMoveToEntity( physent_t *pe, const float *start, float *mins, float *maxs, const float *end, pmtrace_t *tr )
{
	// convert physent_t to base entity
	CBaseEntity *pTouch = CBaseEntity::Instance( INDEXENT( pe->info ));
	trace_t trace;

	if( !pTouch )
	{
		// removed entity?
		tr->allsolid = false;
		return;
	}

	// make trace default
	memset( &trace, 0, sizeof( trace ));
	trace.allsolid = true;
	trace.fraction = 1.0f;
	trace.endpos = end;

	Physic_SweepTest( pTouch, start, mins, maxs, end, &trace );

	// convert trace_t into pmtrace_t
	memcpy( tr, &trace, 48 );
	tr->hitgroup = trace.hitgroup;

	if( trace.ent != NULL )
		tr->ent = pe->info;
	else tr->ent = -1;
}

int SV_RestoreDecal( decallist_t *entry, edict_t *pEdict, qboolean adjacent )
{
	int	flags = entry->flags;
	int	entityIndex = ENTINDEX( pEdict );
	int	modelIndex = 0;

	if( flags & FDECAL_STUDIO )
	{
		UTIL_RestoreStudioDecal( entry->position, entry->impactPlaneNormal, entityIndex, pEdict->v.modelindex, entry->name, flags, &entry->studio_state );
		return TRUE;
          }

	if( adjacent && entityIndex != 0 && ( !pEdict || pEdict->free ))
	{
		TraceResult tr;

		// these entities might not exist over transitions,
		// so we'll use the saved plane and do a traceline instead
		flags |= FDECAL_DONTSAVE;

		ALERT( at_error, "couldn't restore entity index %i, do trace for decal\n", entityIndex );

		Vector testspot = entry->position + entry->impactPlaneNormal * 5.0f;
		Vector testend = entry->position + entry->impactPlaneNormal * -5.0f;

		UTIL_TraceLine( testspot, testend, ignore_monsters, NULL, &tr );

		// NOTE: this code may does wrong result on moving brushes e.g. func_tracktrain
		if( tr.flFraction != 1.0f && !tr.fAllSolid )
		{
			// check impact plane normal
			float	dot = DotProduct( entry->impactPlaneNormal, tr.vecPlaneNormal );

			if( dot >= 0.95f )
			{
				entityIndex = ENTINDEX( tr.pHit );
				if( entityIndex > 0 ) modelIndex = tr.pHit->v.modelindex;
				UTIL_RestoreCustomDecal( tr.vecEndPos, entry->impactPlaneNormal, entityIndex, modelIndex, entry->name, flags );
			}
		}
	}
	else
	{
		// global entity is exist on new level so we can apply decal in local space
		// NOTE: this case also used for transition world decals
		UTIL_RestoreCustomDecal( entry->position, entry->impactPlaneNormal, entityIndex, pEdict->v.modelindex, entry->name, flags );
	}

	return TRUE;
}

//
// Xash3D physics interface
//
static physics_interface_t gPhysicsInterface = 
{
	SV_PHYSICS_INTERFACE_VERSION,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,	// not needs
	NULL,	// not needs
	EngineSetFeatures,
	NULL,
	NULL,
	NULL,
	SV_ClipMoveToEntity,
	SV_ClipPMoveToEntity,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	SV_RestoreDecal,
};

int Server_GetPhysicsInterface( int iVersion, server_physics_api_t *pfuncsFromEngine, physics_interface_t *pFunctionTable )
{
	if ( !pFunctionTable || !pfuncsFromEngine || iVersion != SV_PHYSICS_INTERFACE_VERSION )
	{
		return FALSE;
	}

	size_t iExportSize = sizeof( server_physics_api_t );
	size_t iImportSize = sizeof( physics_interface_t );

	// NOTE: the very old versions NOT have information about current build in any case
	if( g_iXashEngineBuildNumber <= 1910 )
	{
		if( g_fXashEngine )
			ALERT( at_console, "old version of Xash3D was detected. Engine features was disabled.\n" );

		// interface sizes for build 1905 and older
		iExportSize = 28;
		iImportSize = 24;
	}

	// copy new physics interface
	memcpy( &g_physfuncs, pfuncsFromEngine, iExportSize );

	// fill engine callbacks
	memcpy( pFunctionTable, &gPhysicsInterface, iImportSize );

	g_fPhysicInitialized = TRUE;

	return TRUE;
}