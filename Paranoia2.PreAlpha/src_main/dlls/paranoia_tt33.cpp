
/*******************************************************
*	weapon_tt33 class
*
*	(Пистолет Тульский Токарева)
*	recoded by Lev for Half-Life:Paranoia modification
*******************************************************/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "gamerules.h"

#include "paranoia_wpn.h"

enum tt33_e
{
	TT33_IDLE_A = 0,
	TT33_RELOAD_A,
	TT33_DRAW,
	TT33_SHOOT1_A,
	TT33_SHOOT2_A,
	TT33_SHOOTLAST_A,
	TT33_IDLE_B,
	TT33_SHOOT_B,
	TT33_SHOOTLAST_B,
	TT33_CHANGETO_B,
	TT33_CHANGETO_A,	
	TT33_RELOAD_B,
};

class CTT33 : public CBaseToggleWeapon
{
public:
	void	Spawn( void );
	void	Precache( void );
	int		iItemSlot( void ) { return 2; }
	int		GetItemInfo(ItemInfo *p);
	int		AddToPlayer( CBasePlayer *pPlayer );
	BOOL	Deploy( void );
        
	void Attack1( void );
	void Attack2( void );    
	void Reload1( void );
	void Reload2( void );
	void Idle1( void );
	void Idle2( void );
	int ChangeModeTo1( void );
	int ChangeModeTo2( void );
//	Vector GetSpreadVec1( void );
//	Vector GetSpreadVec2( void );

	int m_iShell;

private:
	unsigned short m_usTT33;
};

LINK_ENTITY_TO_CLASS( weapon_tt33, CTT33 );


void CTT33::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_tt33");
	Precache( );
	SET_MODEL(ENT(pev), "models/w_tt33.mdl");
	m_iId = WEAPON_TT33;

	m_iDefaultAmmo = TT33_DEFAULT_GIVE;

	FallInit();
}

void CTT33::Precache( void )
{ 
	PRECACHE_MODEL("models/v_tt33.mdl");
	PRECACHE_MODEL("models/w_tt33.mdl");
	PRECACHE_MODEL("models/p_tt33.mdl");

	m_iShell = PRECACHE_MODEL ("models/tt33_shell.mdl");

	PRECACHE_SOUND ("weapons/tt33-1.wav");
	PRECACHE_SOUND ("weapons/357_cock1.wav");

	m_usTT33 = PRECACHE_EVENT( 1, "events/tt33.sc" );

	m_flTimeWeaponIdle = 0; // fix to resend idle animation 
}

int CTT33::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "tt33";
	p->iMaxAmmo1 = TT33_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = TT33_MAX_CLIP;
	p->iSlot = 1;
	p->iPosition = 2;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_TT33;
	p->iWeight = GLOCK_WEIGHT;    

	return 1;
}

int CTT33::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev );
			WRITE_BYTE( m_iId );
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}


BOOL CTT33::Deploy()
{
	InitToggling();
	return DefaultDeploy( "models/v_tt33.mdl", "models/p_tt33.mdl", TT33_DRAW, "onehanded" );
}

void CTT33::Attack1()
{
    if(!(m_pPlayer->m_afButtonPressed & IN_ATTACK))
    return;

	// don't fire underwater
	if ((m_iClip <= 0) || (m_pPlayer->pev->waterlevel == 3 && m_pPlayer->pev->watertype > CONTENT_FLYFIELD))
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = 0.15;
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	m_iClip--;
	
	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	float spread = ExpandSpread( m_pMySpread->pri_expand );
	EqualizeSpread( &spread, m_pMySpread->pri_equalize );
	Vector vecSpread = AdvanceSpread( m_pMySpread->pri_minspread, m_pMySpread->pri_addspread, spread);
	Vector vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, vecSpread, 8192, BULLET_PLAYER_TT33, 2, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	int iAnim = m_iClip ? TT33_SHOOT1_A : TT33_SHOOTLAST_A;
	PLAYBACK_EVENT_FULL( 0, m_pPlayer->edict(), m_usTT33, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, iAnim, (int)(spread * 255), 0, 0 );

	DefPrimPunch();

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
   
    if (m_iWeaponFireMode == 0)
    {
	    m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.12;
    	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
    }
	       	
}

void CTT33::Attack2()
{
    if(!(m_pPlayer->m_afButtonPressed & IN_ATTACK))
    return;

	// don't fire underwater
	if ((m_iClip <= 0) || (m_pPlayer->pev->waterlevel == 3 && m_pPlayer->pev->watertype > CONTENT_FLYFIELD))
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = 0.15;
		return;
	}

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	m_iClip--;
	
	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	float spread = ExpandSpread( m_pMySpread->sec_expand );
	EqualizeSpread( &spread, m_pMySpread->sec_equalize );
	Vector vecSpread = AdvanceSpread( m_pMySpread->sec_minspread, m_pMySpread->sec_addspread, spread);
	Vector vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, vecSpread, 8192, BULLET_PLAYER_TT33, 2, 0, m_pPlayer->pev, m_pPlayer->random_seed );

	int iAnim = m_iClip ? TT33_SHOOT_B : TT33_SHOOTLAST_B;
	PLAYBACK_EVENT_FULL( 0, m_pPlayer->edict(), m_usTT33, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, iAnim, (int)(spread * 255), 0, 0 );

	DefSecPunch();

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	    m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.12;
    	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}

void CTT33::Reload1()
{
	if ( m_pPlayer->ammo_tt33 <= 0 )
		return;

	DefaultReload( TT33_MAX_CLIP, TT33_RELOAD_A, 2.3 );   
}

void CTT33::Reload2()
{
	if ( m_pPlayer->ammo_tt33 <= 0 )
		return;

	DefaultReload( TT33_MAX_CLIP, TT33_RELOAD_B, 2.3 );
}

void CTT33::Idle1( void )
{
	SendWeaponAnim( TT33_IDLE_A );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5;
} 

void CTT33::Idle2( void )
{
	SendWeaponAnim( TT33_IDLE_B );
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5;
}

extern int firemode;

int CTT33::ChangeModeTo1()
{
	SendWeaponAnim( TT33_CHANGETO_A );
	WeaponDelay(TT33_CHANGETO_A); 
	return 1;
}

int CTT33::ChangeModeTo2()
{    
	SendWeaponAnim( TT33_CHANGETO_B );
	WeaponDelay(TT33_CHANGETO_B);
	return 1;
}




/**************************** Ammoboxes and ammoclips *********************/

class CTT33AmmoClip : public CBasePlayerAmmo
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_tt33ammo.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_tt33ammo.mdl");
		PRECACHE_SOUND ("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		int bResult = (pOther->GiveAmmo( 8, "tt33", TT33_MAX_CARRY) != -1);
		if (bResult)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
		}
		return bResult;
	}
};

LINK_ENTITY_TO_CLASS( ammo_tt33, CTT33AmmoClip );


class CTT33AmmoBox : public CBasePlayerAmmo
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_tt33ammobox.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_tt33ammobox.mdl");
		PRECACHE_SOUND ("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		int bResult = (pOther->GiveAmmo( 32, "tt33", TT33_MAX_CARRY) != -1);
		if (bResult)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
		}
		return bResult;
	}
};

LINK_ENTITY_TO_CLASS( ammo_tt33box, CTT33AmmoBox );