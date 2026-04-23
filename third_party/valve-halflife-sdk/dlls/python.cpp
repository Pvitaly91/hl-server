/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "monsters.h"
#include "player.h"
#include "gamerules.h"
#include "future_gameplay_hooks.h"
#include "weapon_tuning_core.h"
#include "weapon_debug_logger.h"
#include <math.h>

namespace
{
const float k357FallbackMaxSpeed = 250.0f;
}


enum python_e {
	PYTHON_IDLE1 = 0,
	PYTHON_FIDGET,
	PYTHON_FIRE1,
	PYTHON_RELOAD,
	PYTHON_HOLSTER,
	PYTHON_DRAW,
	PYTHON_IDLE2,
	PYTHON_IDLE3
};

LINK_ENTITY_TO_CLASS( weapon_python, CPython );
LINK_ENTITY_TO_CLASS( weapon_357, CPython );

int CPython::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "357";
	p->iMaxAmmo1 = _357_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = PYTHON_MAX_CLIP;
	p->iFlags = 0;
	p->iSlot = 1;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_PYTHON;
	p->iWeight = PYTHON_WEIGHT;

	return 1;
}

int CPython::AddToPlayer( CBasePlayer *pPlayer )
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

void CPython::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_357"); // hack to allow for old names
	Precache( );
	m_iId = WEAPON_PYTHON;
	SET_MODEL(ENT(pev), "models/w_357.mdl");

	m_iDefaultAmmo = PYTHON_DEFAULT_GIVE;
	m_flLastAcceptedPrimaryShotTime = -1.0f;
	m_flPrimaryCadenceSpreadAccumulator = 0.0f;
	m_iPrimaryCadenceShotCount = 0;
	m_iPrimaryPatternIndex = -1;

	FallInit();// get ready to fall down.
}


void CPython::Precache( void )
{
	PRECACHE_MODEL("models/v_357.mdl");
	PRECACHE_MODEL("models/w_357.mdl");
	PRECACHE_MODEL("models/p_357.mdl");

	PRECACHE_MODEL("models/w_357ammobox.mdl");
	PRECACHE_SOUND("items/9mmclip1.wav");              

	PRECACHE_SOUND ("weapons/357_reload1.wav");
	PRECACHE_SOUND ("weapons/357_cock1.wav");
	PRECACHE_SOUND ("weapons/357_shot1.wav");
	PRECACHE_SOUND ("weapons/357_shot2.wav");

	m_usFirePython = PRECACHE_EVENT( 1, "events/python.sc" );
}

BOOL CPython::Deploy( )
{
#ifdef CLIENT_DLL
	if ( bIsMultiplayer() )
#else
	if ( g_pGameRules->IsMultiplayer() )
#endif
	{
		// enable laser sight geometry.
		pev->body = 1;
	}
	else
	{
		pev->body = 0;
	}

	return DefaultDeploy( "models/v_357.mdl", "models/p_357.mdl", PYTHON_DRAW, "python", UseDecrement(), pev->body );
}


void CPython::Holster( int skiplocal /* = 0 */ )
{
	m_fInReload = FALSE;// cancel any reload in progress.

	if ( m_fInZoom )
	{
		SecondaryAttack();
	}

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 1.0;
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	SendWeaponAnim( PYTHON_HOLSTER );
}

void CPython::SecondaryAttack( void )
{
#ifdef CLIENT_DLL
	if ( !bIsMultiplayer() )
#else
	if ( !g_pGameRules->IsMultiplayer() )
#endif
	{
		return;
	}

	if ( m_pPlayer->pev->fov != 0 )
	{
		m_fInZoom = FALSE;
		m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;  // 0 means reset to default fov
	}
	else if ( m_pPlayer->pev->fov != 40 )
	{
		m_fInZoom = TRUE;
		m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 40;
	}

	m_flNextSecondaryAttack = 0.5;
}

void CPython::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = 0.15;
		return;
	}

	if (m_iClip <= 0)
	{
		if (!m_fFireOnEmpty)
			Reload( );
		else
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = 0.15;
		}

		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
	const BOOL fHadAmmo = (m_iClip > 0);
	const BOOL fExperimentalPrimary = Exp357ExperimentalModeEnabled() ? TRUE : FALSE;
	const BOOL fHasPreviousAcceptedShot = m_flLastAcceptedPrimaryShotTime >= 0.0f;
	const BOOL fHoldingAttack = (m_pPlayer->m_afButtonPressed & IN_ATTACK) == 0;
	const float flTimeSincePreviousAcceptedShot = fHasPreviousAcceptedShot ? (gpGlobals->time - m_flLastAcceptedPrimaryShotTime) : 0.0f;

	m_iClip--;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );


	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	Vector vecDir;
	if (fExperimentalPrimary)
	{
		const SharedWeaponSpreadProfile spreadProfile = Build357PrimarySpreadProfile();
		const SharedWeaponCadenceProfile cadenceProfile = Build357PrimaryCadenceProfile();
		const float flRecoveredCadenceAddedSpread = fHasPreviousAcceptedShot
			? RecoverSharedAdditionalSpread(
				m_flPrimaryCadenceSpreadAccumulator,
				flTimeSincePreviousAcceptedShot,
				spreadProfile.additionalSpreadRecoverySeconds,
				spreadProfile.maxSpread)
			: 0.0f;
		const SharedWeaponCadenceResult cadenceResult = ComputeSharedWeaponCadence(
			cadenceProfile,
			fHasPreviousAcceptedShot != FALSE,
			flTimeSincePreviousAcceptedShot,
			fHoldingAttack != FALSE);
		const float flRecoveredSpreadAfterCadenceReset = cadenceResult.resetApplied ? 0.0f : flRecoveredCadenceAddedSpread;
		const float flCadenceRecoveryApplied = fHasPreviousAcceptedShot
			? (m_flPrimaryCadenceSpreadAccumulator - flRecoveredCadenceAddedSpread)
			: 0.0f;
		const float flCadenceAdditionalSpread = min(
			spreadProfile.maxSpread,
			flRecoveredSpreadAfterCadenceReset + cadenceResult.cadencePenalty);
		const int iCadenceShotIndex = (flRecoveredSpreadAfterCadenceReset > 0.0001f && m_iPrimaryCadenceShotCount > 0 && !cadenceResult.resetApplied)
			? (m_iPrimaryCadenceShotCount + 1)
			: 1;
		const SharedWeaponSpreadState spreadState = BuildPlayerWeaponSpreadState(
			m_pPlayer,
			k357FallbackMaxSpeed,
			fHasPreviousAcceptedShot != FALSE,
			flTimeSincePreviousAcceptedShot,
			flCadenceAdditionalSpread);
		const SharedWeaponSpreadResult spreadResult = ComputeSharedWeaponSpread(spreadProfile, spreadState);
		const float flSpread = spreadResult.spread;
		const float flAdditionalSpread = spreadResult.additionalSpread;
		const float flMovementPenalty = spreadResult.movementPenalty;
		const SharedWeaponPatternProfile patternProfile = Build357PrimaryPatternProfile();
		const SharedWeaponPatternResult patternResult = ComputeSharedWeaponPattern(
			patternProfile,
			spreadState,
			spreadResult.speedRatio,
			flSpread,
			m_iPrimaryPatternIndex);
		const float flNextCadenceAddedSpread = GrowSharedAdditionalSpread(
			flCadenceAdditionalSpread,
			0.0f,
			spreadProfile.maxSpread);
		Vector vecPatternAiming = vecAiming;
		float flRandomSpread = flSpread;
		if (patternResult.enabled)
		{
			vecPatternAiming = (vecAiming + (gpGlobals->v_right * patternResult.offsetX) + (gpGlobals->v_up * patternResult.offsetY)).Normalize();
			flRandomSpread = patternResult.randomSpread;
		}

		Begin357PrimaryShotContext(m_pPlayer);
		vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecPatternAiming, Vector( flRandomSpread, flRandomSpread, flRandomSpread ), 8192, BULLET_PLAYER_357, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );
		End357PrimaryShotContext();

		if (fHadAmmo)
		{
			Weapon357AcceptedShotTelemetry acceptedTelemetry = {};
			acceptedTelemetry.experimentalModeActive = Exp357ExperimentalModeEnabled() != FALSE;
			acceptedTelemetry.firstShotAccuracyApplied = spreadResult.firstShotAccuracyApplied ? TRUE : FALSE;
			acceptedTelemetry.spread = flSpread;
			acceptedTelemetry.baseSpread = spreadProfile.baseSpread;
			acceptedTelemetry.movementPenalty = flMovementPenalty;
			acceptedTelemetry.additionalSpread = flAdditionalSpread;
			acceptedTelemetry.recoveryApplied = flCadenceRecoveryApplied > 0.0f ? flCadenceRecoveryApplied : 0.0f;
			acceptedTelemetry.cadenceModeActive = cadenceResult.enabled;
			acceptedTelemetry.cadenceInterval = cadenceResult.intervalSeconds;
			acceptedTelemetry.cadencePenalty = cadenceResult.cadencePenalty;
			acceptedTelemetry.cadenceResetApplied = cadenceResult.resetApplied;
			acceptedTelemetry.holdPenalty = cadenceResult.holdPenalty;
			acceptedTelemetry.nextAdditionalSpread = flNextCadenceAddedSpread;
			acceptedTelemetry.patternModeActive = patternResult.enabled;
			acceptedTelemetry.patternIndex = patternResult.enabled ? patternResult.patternIndex : -1;
			acceptedTelemetry.patternOffsetX = patternResult.offsetX;
			acceptedTelemetry.patternOffsetY = patternResult.offsetY;
			acceptedTelemetry.patternResetApplied = patternResult.resetApplied;
			acceptedTelemetry.totalAdditionalSpread = flMovementPenalty + flAdditionalSpread;
			acceptedTelemetry.movementContribution = flMovementPenalty;
			acceptedTelemetry.patternContribution = patternResult.enabled ? sqrtf((patternResult.offsetX * patternResult.offsetX) + (patternResult.offsetY * patternResult.offsetY)) : 0.0f;
			acceptedTelemetry.cadenceContribution = cadenceResult.cadencePenalty;
			acceptedTelemetry.horizontalSpeed = spreadState.horizontalSpeed;
			acceptedTelemetry.maxSpeedForNormalization = spreadResult.normalizedMaxSpeed;
			acceptedTelemetry.grounded = spreadState.grounded ? TRUE : FALSE;
			acceptedTelemetry.ducking = spreadState.ducking ? TRUE : FALSE;
			acceptedTelemetry.hasPreviousAcceptedShot = fHasPreviousAcceptedShot != FALSE;
			acceptedTelemetry.timeSincePreviousAcceptedShot = flTimeSincePreviousAcceptedShot;
			acceptedTelemetry.cadenceShotIndex = iCadenceShotIndex;
			acceptedTelemetry.clipAfterShot = m_iClip;

			LogAccepted357PrimaryShot(m_pPlayer, acceptedTelemetry);
			m_flPrimaryCadenceSpreadAccumulator = flNextCadenceAddedSpread;
			m_iPrimaryCadenceShotCount = iCadenceShotIndex;
			m_iPrimaryPatternIndex = patternResult.enabled ? patternResult.patternIndex : -1;
			m_flLastAcceptedPrimaryShotTime = gpGlobals->time;
		}
	}
	else
	{
		vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, VECTOR_CONE_1DEGREES, 8192, BULLET_PLAYER_357, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );
	}

    int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usFirePython, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flNextPrimaryAttack = 0.75;
	m_flTimeWeaponIdle = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}


void CPython::Reload( void )
{
	if ( m_pPlayer->ammo_357 <= 0 )
		return;

	if ( m_pPlayer->pev->fov != 0 )
	{
		m_fInZoom = FALSE;
		m_pPlayer->pev->fov = m_pPlayer->m_iFOV = 0;  // 0 means reset to default fov
	}

	int bUseScope = FALSE;
#ifdef CLIENT_DLL
	bUseScope = bIsMultiplayer();
#else
	bUseScope = g_pGameRules->IsMultiplayer();
#endif

	DefaultReload( 6, PYTHON_RELOAD, 2.0, bUseScope );
}


void CPython::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	int iAnim;
	float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
	if (flRand <= 0.5)
	{
		iAnim = PYTHON_IDLE1;
		m_flTimeWeaponIdle = (70.0/30.0);
	}
	else if (flRand <= 0.7)
	{
		iAnim = PYTHON_IDLE2;
		m_flTimeWeaponIdle = (60.0/30.0);
	}
	else if (flRand <= 0.9)
	{
		iAnim = PYTHON_IDLE3;
		m_flTimeWeaponIdle = (88.0/30.0);
	}
	else
	{
		iAnim = PYTHON_FIDGET;
		m_flTimeWeaponIdle = (170.0/30.0);
	}
	
	int bUseScope = FALSE;
#ifdef CLIENT_DLL
	bUseScope = bIsMultiplayer();
#else
	bUseScope = g_pGameRules->IsMultiplayer();
#endif
	
	SendWeaponAnim( iAnim, UseDecrement() ? 1 : 0, bUseScope );
}


class CPythonAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_357ammobox.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_357ammobox.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		if (pOther->GiveAmmo( AMMO_357BOX_GIVE, "357", _357_MAX_CARRY ) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_357, CPythonAmmo );


#endif
