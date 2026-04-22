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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "future_gameplay_hooks.h"
#include "weapon_tuning_core.h"
#include "weapon_debug_logger.h"

namespace
{
const float kGlockFallbackMaxSpeed = 270.0f;
}

enum glock_e {
	GLOCK_IDLE1 = 0,
	GLOCK_IDLE2,
	GLOCK_IDLE3,
	GLOCK_SHOOT,
	GLOCK_SHOOT_EMPTY,
	GLOCK_RELOAD,
	GLOCK_RELOAD_NOT_EMPTY,
	GLOCK_DRAW,
	GLOCK_HOLSTER,
	GLOCK_ADD_SILENCER
};

LINK_ENTITY_TO_CLASS( weapon_glock, CGlock );
LINK_ENTITY_TO_CLASS( weapon_9mmhandgun, CGlock );


void CGlock::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_9mmhandgun"); // hack to allow for old names
	Precache( );
	m_iId = WEAPON_GLOCK;
	SET_MODEL(ENT(pev), "models/w_9mmhandgun.mdl");

	m_iDefaultAmmo = GLOCK_DEFAULT_GIVE;
	m_flLastAcceptedPrimaryShotTime = -1.0f;
	m_flPrimaryCadenceSpreadAccumulator = 0.0f;
	m_iPrimaryCadenceShotCount = 0;
	m_iPrimaryPatternIndex = -1;
	m_fPrimaryHoldBlockLogged = FALSE;

	FallInit();// get ready to fall down.
}


void CGlock::Precache( void )
{
	PRECACHE_MODEL("models/v_9mmhandgun.mdl");
	PRECACHE_MODEL("models/w_9mmhandgun.mdl");
	PRECACHE_MODEL("models/p_9mmhandgun.mdl");

	m_iShell = PRECACHE_MODEL ("models/shell.mdl");// brass shell

	PRECACHE_SOUND("items/9mmclip1.wav");
	PRECACHE_SOUND("items/9mmclip2.wav");

	PRECACHE_SOUND ("weapons/pl_gun1.wav");//silenced handgun
	PRECACHE_SOUND ("weapons/pl_gun2.wav");//silenced handgun
	PRECACHE_SOUND ("weapons/pl_gun3.wav");//handgun

	m_usFireGlock1 = PRECACHE_EVENT( 1, "events/glock1.sc" );
	m_usFireGlock2 = PRECACHE_EVENT( 1, "events/glock2.sc" );
}

int CGlock::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "9mm";
	p->iMaxAmmo1 = _9MM_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = GLOCK_MAX_CLIP;
	p->iSlot = 1;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_GLOCK;
	p->iWeight = GLOCK_WEIGHT;

	return 1;
}

BOOL CGlock::Deploy( )
{
	// pev->body = 1;
	return DefaultDeploy( "models/v_9mmhandgun.mdl", "models/p_9mmhandgun.mdl", GLOCK_DRAW, "onehanded", /*UseDecrement() ? 1 : 0*/ 0 );
}

void CGlock::SecondaryAttack( void )
{
	GlockFire( 0.1, 0.2, FALSE, FALSE );
}

void CGlock::PrimaryAttack( void )
{
	if (ExpPistolTapFireEnabled() && RequiresPrimaryAttackPress() == FALSE)
	{
		if (!m_fPrimaryHoldBlockLogged)
		{
			const BOOL fGrounded = FBitSet(m_pPlayer->pev->flags, FL_ONGROUND);
			const BOOL fDucking = (m_pPlayer->pev->button & IN_DUCK) || FBitSet(m_pPlayer->pev->flags, FL_DUCKING);

			GlockRejectedShotTelemetry rejectedTelemetry = {};
			rejectedTelemetry.horizontalSpeed = m_pPlayer->pev->velocity.Length2D();
			rejectedTelemetry.grounded = fGrounded != FALSE;
			rejectedTelemetry.ducking = fDucking != FALSE;

			LogRejectedGlockPrimaryHold(m_pPlayer, rejectedTelemetry);
			m_fPrimaryHoldBlockLogged = TRUE;
		}

		return;
	}

	m_fPrimaryHoldBlockLogged = FALSE;

	const BOOL fHadAmmo = (m_iClip > 0);
	const SharedWeaponSpreadProfile spreadProfile = BuildGlockPrimarySpreadProfile();
	const BOOL fHasPreviousAcceptedShot = m_flLastAcceptedPrimaryShotTime >= 0.0f;
	const float flTimeSincePreviousAcceptedShot = fHasPreviousAcceptedShot ? (gpGlobals->time - m_flLastAcceptedPrimaryShotTime) : 0.0f;
	const float flRecoveredCadenceAddedSpread = fHasPreviousAcceptedShot
		? RecoverSharedAdditionalSpread(
			m_flPrimaryCadenceSpreadAccumulator,
			flTimeSincePreviousAcceptedShot,
			spreadProfile.additionalSpreadRecoverySeconds,
			spreadProfile.maxSpread)
		: 0.0f;
	const float flCadenceRecoveryApplied = fHasPreviousAcceptedShot
		? (m_flPrimaryCadenceSpreadAccumulator - flRecoveredCadenceAddedSpread)
		: 0.0f;
	const int iCadenceShotIndex = (flRecoveredCadenceAddedSpread > 0.0001f && m_iPrimaryCadenceShotCount > 0)
		? (m_iPrimaryCadenceShotCount + 1)
		: 1;
	const SharedWeaponSpreadState spreadState = BuildPlayerWeaponSpreadState(
		m_pPlayer,
		kGlockFallbackMaxSpeed,
		fHasPreviousAcceptedShot != FALSE,
		flTimeSincePreviousAcceptedShot,
		flRecoveredCadenceAddedSpread);
	const SharedWeaponSpreadResult spreadResult = ComputeSharedWeaponSpread(spreadProfile, spreadState);
	const BOOL fGrounded = spreadState.grounded ? TRUE : FALSE;
	const BOOL fDucking = spreadState.ducking ? TRUE : FALSE;
	const float flHorizontalSpeed = spreadState.horizontalSpeed;
	const float flMaxSpeedForNormalization = spreadResult.normalizedMaxSpeed;
	const BOOL fFirstShotAccuracyApplied = spreadResult.firstShotAccuracyApplied ? TRUE : FALSE;
	const float flMovementPenalty = spreadResult.movementPenalty;
	const float flAdditionalSpread = spreadResult.additionalSpread;
	const float flNextCadenceAddedSpread = GrowSharedAdditionalSpread(
		flRecoveredCadenceAddedSpread,
		ExpGlockPrimaryShotGrowth(),
		spreadProfile.maxSpread);
	const float flSpread = spreadResult.spread;
	const SharedWeaponPatternProfile patternProfile = BuildGlockPrimaryPatternProfile();
	const SharedWeaponPatternResult patternResult = ComputeSharedWeaponPattern(
		patternProfile,
		spreadState,
		spreadResult.speedRatio,
		flSpread,
		m_iPrimaryPatternIndex);

	GlockFire( flSpread, 0.3, TRUE, TRUE, &patternResult );

	if (fHadAmmo)
	{
		GlockAcceptedShotTelemetry acceptedTelemetry = {};
		acceptedTelemetry.experimentalModeActive = ExpGlockExperimentalModeEnabled();
		acceptedTelemetry.tapFireActive = ExpPistolTapFireEnabled();
		acceptedTelemetry.firstShotAccuracyApplied = fFirstShotAccuracyApplied != FALSE;
		acceptedTelemetry.spread = flSpread;
		acceptedTelemetry.baseSpread = spreadProfile.baseSpread;
		acceptedTelemetry.movementPenalty = flMovementPenalty;
		acceptedTelemetry.additionalSpread = flAdditionalSpread;
		acceptedTelemetry.recoveryApplied = flCadenceRecoveryApplied > 0.0f ? flCadenceRecoveryApplied : 0.0f;
		acceptedTelemetry.shotGrowth = ExpGlockPrimaryShotGrowth();
		acceptedTelemetry.nextAdditionalSpread = flNextCadenceAddedSpread;
		acceptedTelemetry.patternModeActive = patternResult.enabled;
		acceptedTelemetry.patternIndex = patternResult.patternIndex;
		acceptedTelemetry.patternOffsetX = patternResult.offsetX;
		acceptedTelemetry.patternOffsetY = patternResult.offsetY;
		acceptedTelemetry.patternResetApplied = patternResult.resetApplied;
		acceptedTelemetry.totalAdditionalSpread = flMovementPenalty + flAdditionalSpread;
		acceptedTelemetry.movementContribution = flMovementPenalty;
		acceptedTelemetry.cadenceGrowthContribution = flAdditionalSpread;
		acceptedTelemetry.speedRatio = spreadResult.speedRatio;
		acceptedTelemetry.horizontalSpeed = flHorizontalSpeed;
		acceptedTelemetry.maxSpeedForNormalization = flMaxSpeedForNormalization;
		acceptedTelemetry.grounded = fGrounded != FALSE;
		acceptedTelemetry.ducking = fDucking != FALSE;
		acceptedTelemetry.hasPreviousAcceptedShot = fHasPreviousAcceptedShot != FALSE;
		acceptedTelemetry.timeSincePreviousAcceptedShot = flTimeSincePreviousAcceptedShot;
		acceptedTelemetry.cadenceShotIndex = iCadenceShotIndex;
		acceptedTelemetry.clipAfterShot = m_iClip;

		LogAcceptedGlockPrimaryShot(m_pPlayer, acceptedTelemetry);
		m_flPrimaryCadenceSpreadAccumulator = flNextCadenceAddedSpread;
		m_iPrimaryCadenceShotCount = iCadenceShotIndex;
		m_iPrimaryPatternIndex = patternResult.patternIndex;
		m_flLastAcceptedPrimaryShotTime = gpGlobals->time;
	}
}

void CGlock::GlockFire( float flSpread , float flCycleTime, BOOL fUseAutoAim, BOOL fExperimentalPrimary, const SharedWeaponPatternResult *pPattern )
{
	if (m_iClip <= 0)
	{
		if (m_fFireOnEmpty)
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = GetNextAttackDelay(0.2);
		}

		return;
	}

	m_iClip--;

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	int flags;

#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	// silenced
	if (pev->body == 1)
	{
		m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;
	}
	else
	{
		// non-silenced
		m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	}

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming;
	
	if ( fUseAutoAim )
	{
		vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );
	}
	else
	{
		vecAiming = gpGlobals->v_forward;
	}

	Vector vecDir;
	if (fExperimentalPrimary)
	{
		BeginGlockPrimaryShotContext(m_pPlayer);
	}
	Vector vecPatternAiming = vecAiming;
	float flRandomSpread = flSpread;
	if (pPattern != NULL && pPattern->enabled)
	{
		vecPatternAiming = (vecAiming + (gpGlobals->v_right * pPattern->offsetX) + (gpGlobals->v_up * pPattern->offsetY)).Normalize();
		flRandomSpread = pPattern->randomSpread;
	}

	vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecPatternAiming, Vector( flRandomSpread, flRandomSpread, flRandomSpread ), 8192, BULLET_PLAYER_9MM, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );
	if (fExperimentalPrimary)
	{
		EndGlockPrimaryShotContext();
	}

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), fUseAutoAim ? m_usFireGlock1 : m_usFireGlock2, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, ( m_iClip == 0 ) ? 1 : 0, 0 );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetNextAttackDelay(flCycleTime);

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}


void CGlock::Reload( void )
{
	if ( m_pPlayer->ammo_9mm <= 0 )
		 return;

	int iResult = DefaultReload( GLOCK_MAX_CLIP, m_iClip > 0 ? GLOCK_RELOAD_NOT_EMPTY : GLOCK_RELOAD, 1.5 );
	if (iResult)
	{
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
	}
}

BOOL CGlock::RequiresPrimaryAttackPress( void ) const
{
	return (m_pPlayer->m_afButtonPressed & IN_ATTACK) != 0;
}

BOOL CGlock::QualifiesForFirstShotAccuracy( void ) const
{
	const BOOL fHasPreviousAcceptedShot = m_flLastAcceptedPrimaryShotTime >= 0.0f;
	const float flTimeSincePreviousAcceptedShot = fHasPreviousAcceptedShot ? (gpGlobals->time - m_flLastAcceptedPrimaryShotTime) : 0.0f;
	const SharedWeaponSpreadProfile spreadProfile = BuildGlockPrimarySpreadProfile();
	const float flRecoveredCadenceAddedSpread = fHasPreviousAcceptedShot
		? RecoverSharedAdditionalSpread(
			m_flPrimaryCadenceSpreadAccumulator,
			flTimeSincePreviousAcceptedShot,
			spreadProfile.additionalSpreadRecoverySeconds,
			spreadProfile.maxSpread)
		: 0.0f;
	const SharedWeaponSpreadResult spreadResult = ComputeSharedWeaponSpread(
		spreadProfile,
		BuildPlayerWeaponSpreadState(
			m_pPlayer,
			kGlockFallbackMaxSpeed,
			fHasPreviousAcceptedShot != FALSE,
			flTimeSincePreviousAcceptedShot,
			flRecoveredCadenceAddedSpread));
	return spreadResult.firstShotAccuracyApplied ? TRUE : FALSE;
}

float CGlock::GetPrimaryFireSpread( void ) const
{
	const BOOL fHasPreviousAcceptedShot = m_flLastAcceptedPrimaryShotTime >= 0.0f;
	const float flTimeSincePreviousAcceptedShot = fHasPreviousAcceptedShot ? (gpGlobals->time - m_flLastAcceptedPrimaryShotTime) : 0.0f;
	const SharedWeaponSpreadProfile spreadProfile = BuildGlockPrimarySpreadProfile();
	const float flRecoveredCadenceAddedSpread = fHasPreviousAcceptedShot
		? RecoverSharedAdditionalSpread(
			m_flPrimaryCadenceSpreadAccumulator,
			flTimeSincePreviousAcceptedShot,
			spreadProfile.additionalSpreadRecoverySeconds,
			spreadProfile.maxSpread)
		: 0.0f;
	const SharedWeaponSpreadResult spreadResult = ComputeSharedWeaponSpread(
		spreadProfile,
		BuildPlayerWeaponSpreadState(
			m_pPlayer,
			kGlockFallbackMaxSpeed,
			fHasPreviousAcceptedShot != FALSE,
			flTimeSincePreviousAcceptedShot,
			flRecoveredCadenceAddedSpread));
	return spreadResult.spread;
}



void CGlock::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	// only idle if the slid isn't back
	if (m_iClip != 0)
	{
		int iAnim;
		float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0.0, 1.0 );

		if (flRand <= 0.3 + 0 * 0.75)
		{
			iAnim = GLOCK_IDLE3;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 49.0 / 16;
		}
		else if (flRand <= 0.6 + 0 * 0.875)
		{
			iAnim = GLOCK_IDLE1;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 60.0 / 16.0;
		}
		else
		{
			iAnim = GLOCK_IDLE2;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 40.0 / 16.0;
		}
		SendWeaponAnim( iAnim, 1 );
	}
}








class CGlockAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_9mmclip.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_9mmclip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		if (pOther->GiveAmmo( AMMO_GLOCKCLIP_GIVE, "9mm", _9MM_MAX_CARRY ) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_glockclip, CGlockAmmo );
LINK_ENTITY_TO_CLASS( ammo_9mmclip, CGlockAmmo );















