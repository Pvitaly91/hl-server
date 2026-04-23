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
#include "gamerules.h"
#include "future_gameplay_hooks.h"
#include "weapon_tuning_core.h"
#include "weapon_debug_logger.h"

#include <math.h>

// special deathmatch shotgun spreads
#define VECTOR_CONE_DM_SHOTGUN	Vector( 0.08716, 0.04362, 0.00  )// 10 degrees by 5 degrees
#define VECTOR_CONE_DM_DOUBLESHOTGUN Vector( 0.17365, 0.04362, 0.00 ) // 20 degrees by 5 degrees

namespace
{
const float kShotgunFallbackMaxSpeed = 250.0f;

struct ShotgunPelletPatternPoint
{
	float x;
	float y;
};

const ShotgunPelletPatternPoint kShotgunDeterministicPelletLayout[] =
{
	{ 0.00f,  0.00f },
	{ -0.42f, -0.14f },
	{ 0.42f, -0.14f },
	{ -0.28f, 0.34f },
	{ 0.28f,  0.34f },
	{ 0.00f, -0.48f },
	{ -0.70f, 0.60f },
	{ 0.70f,  0.60f },
	{ -0.58f, -0.62f },
	{ 0.58f,  -0.62f },
	{ -0.12f, 0.80f },
	{ 0.12f,  0.80f }
};

float ClampShotgunPatternScale(float value)
{
	if (value < 0.0f)
	{
		return 0.0f;
	}

	if (value > 4.0f)
	{
		return 4.0f;
	}

	return value;
}

ShotgunPelletPatternPoint ResolveShotgunPelletPatternPoint(int pelletIndex, int patternIndex)
{
	const int pointCount = sizeof(kShotgunDeterministicPelletLayout) / sizeof(kShotgunDeterministicPelletLayout[0]);
	if (pointCount <= 0)
	{
		return { 0.0f, 0.0f };
	}

	const int rotationOffset = ((patternIndex >= 0 ? patternIndex : 0) * 2) % pointCount;
	const int wrappedIndex = (pelletIndex + rotationOffset) % pointCount;
	return kShotgunDeterministicPelletLayout[wrappedIndex];
}

Vector FireDeterministicShotgunPellets(
	CBasePlayer *pPlayer,
	const Vector &vecSrc,
	const Vector &vecAiming,
	int pelletCount,
	float totalSpread,
	int patternIndex,
	float layoutScaleX,
	float layoutScaleY)
{
	TraceResult tr;
	Vector vecRight = gpGlobals->v_right;
	Vector vecUp = gpGlobals->v_up;
	Vector vecLastOffset = g_vecZero;
	const float clampedSpread = totalSpread > 0.0f ? totalSpread : 0.0f;
	const float clampedScaleX = ClampShotgunPatternScale(layoutScaleX);
	const float clampedScaleY = ClampShotgunPatternScale(layoutScaleY);
	const float baseDamage = GetActiveShotgunPrimaryBaseDamage(pPlayer->pev, gSkillData.plrDmgBuckshot);

	ClearMultiDamage();
	gMultiDamage.type = DMG_BULLET | DMG_NEVERGIB;

	for (int pelletIndex = 0; pelletIndex < pelletCount; ++pelletIndex)
	{
		const ShotgunPelletPatternPoint patternPoint = ResolveShotgunPelletPatternPoint(pelletIndex, patternIndex);
		const float pelletOffsetX = clampedSpread * patternPoint.x * clampedScaleX;
		const float pelletOffsetY = clampedSpread * patternPoint.y * clampedScaleY;
		const Vector vecDir = vecAiming +
			(pelletOffsetX * vecRight) +
			(pelletOffsetY * vecUp);
		const Vector vecEnd = vecSrc + vecDir * 2048.0f;

		UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(pPlayer->pev), &tr);
		if (tr.flFraction != 1.0f)
		{
			CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);
			if (pEntity != NULL)
			{
				pEntity->TraceAttack(pPlayer->pev, baseDamage, vecDir, &tr, DMG_BULLET);
			}
		}

		UTIL_BubbleTrail(vecSrc, tr.vecEndPos, (2048.0f * tr.flFraction) / 64.0f);
		vecLastOffset = Vector(pelletOffsetX, pelletOffsetY, 0.0f);
	}

	ApplyMultiDamage(pPlayer->pev, pPlayer->pev);
	FinalizeActiveShotgunPrimaryHitTelemetry();
	return vecLastOffset;
}
}

enum shotgun_e {
	SHOTGUN_IDLE = 0,
	SHOTGUN_FIRE,
	SHOTGUN_FIRE2,
	SHOTGUN_RELOAD,
	SHOTGUN_PUMP,
	SHOTGUN_START_RELOAD,
	SHOTGUN_DRAW,
	SHOTGUN_HOLSTER,
	SHOTGUN_IDLE4,
	SHOTGUN_IDLE_DEEP
};

LINK_ENTITY_TO_CLASS( weapon_shotgun, CShotgun );

void CShotgun::Spawn( )
{
	Precache( );
	m_iId = WEAPON_SHOTGUN;
	SET_MODEL(ENT(pev), "models/w_shotgun.mdl");

	m_iDefaultAmmo = SHOTGUN_DEFAULT_GIVE;
	m_flLastAcceptedPrimaryShotTime = -1.0f;
	m_flPrimarySpreadAccumulator = 0.0f;
	m_iPrimaryShotCount = 0;
	m_iPrimaryPatternIndex = -1;

	FallInit();// get ready to fall
}


void CShotgun::Precache( void )
{
	PRECACHE_MODEL("models/v_shotgun.mdl");
	PRECACHE_MODEL("models/w_shotgun.mdl");
	PRECACHE_MODEL("models/p_shotgun.mdl");

	m_iShell = PRECACHE_MODEL ("models/shotgunshell.mdl");// shotgun shell

	PRECACHE_SOUND("items/9mmclip1.wav");              

	PRECACHE_SOUND ("weapons/dbarrel1.wav");//shotgun
	PRECACHE_SOUND ("weapons/sbarrel1.wav");//shotgun

	PRECACHE_SOUND ("weapons/reload1.wav");	// shotgun reload
	PRECACHE_SOUND ("weapons/reload3.wav");	// shotgun reload

//	PRECACHE_SOUND ("weapons/sshell1.wav");	// shotgun reload - played on client
//	PRECACHE_SOUND ("weapons/sshell3.wav");	// shotgun reload - played on client
	
	PRECACHE_SOUND ("weapons/357_cock1.wav"); // gun empty sound
	PRECACHE_SOUND ("weapons/scock1.wav");	// cock gun

	m_usSingleFire = PRECACHE_EVENT( 1, "events/shotgun1.sc" );
	m_usDoubleFire = PRECACHE_EVENT( 1, "events/shotgun2.sc" );
}

int CShotgun::AddToPlayer( CBasePlayer *pPlayer )
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


int CShotgun::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "buckshot";
	p->iMaxAmmo1 = BUCKSHOT_MAX_CARRY;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = SHOTGUN_MAX_CLIP;
	p->iSlot = 2;
	p->iPosition = 1;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_SHOTGUN;
	p->iWeight = SHOTGUN_WEIGHT;

	return 1;
}



BOOL CShotgun::Deploy( )
{
	return DefaultDeploy( "models/v_shotgun.mdl", "models/p_shotgun.mdl", SHOTGUN_DRAW, "shotgun" );
}

void CShotgun::PrimaryAttack()
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if (m_iClip <= 0)
	{
		Reload( );
		if (m_iClip == 0)
			PlayEmptySound( );
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
	const BOOL fExperimentalPrimary = ExpShotgunPrimaryEnabled();
	const BOOL fHasPreviousAcceptedShot = m_flLastAcceptedPrimaryShotTime >= 0.0f;
	const float flTimeSincePreviousAcceptedShot = fHasPreviousAcceptedShot ? (gpGlobals->time - m_flLastAcceptedPrimaryShotTime) : 0.0f;

	m_iClip--;

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif


	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	Vector vecDir;

	if (fExperimentalPrimary)
	{
		const SharedWeaponSpreadProfile spreadProfile = BuildShotgunPrimarySpreadProfile();
		const SharedWeaponPatternProfile patternProfile = BuildShotgunPrimaryPatternProfile();
		const bool deterministicPelletLayout = ExpShotgunPrimaryPelletSpreadMode() == 1;
		const float flRecoveredAdditionalSpread = fHasPreviousAcceptedShot
			? RecoverSharedAdditionalSpread(
				m_flPrimarySpreadAccumulator,
				flTimeSincePreviousAcceptedShot,
				spreadProfile.additionalSpreadRecoverySeconds,
				spreadProfile.maxSpread)
			: 0.0f;
		const SharedWeaponSpreadState spreadState = BuildPlayerWeaponSpreadState(
			m_pPlayer,
			kShotgunFallbackMaxSpeed,
			fHasPreviousAcceptedShot != FALSE,
			flTimeSincePreviousAcceptedShot,
			flRecoveredAdditionalSpread);
		const SharedWeaponSpreadResult spreadResult = ComputeSharedWeaponSpread(spreadProfile, spreadState);
		const SharedWeaponPatternResult patternResult = ComputeSharedWeaponPattern(
			patternProfile,
			spreadState,
			spreadResult.speedRatio,
			spreadResult.spread,
			m_iPrimaryPatternIndex);
		const float flSpread = spreadResult.spread;
		const float flShotGrowth = ExpShotgunPrimaryShotGrowth();
		const float flNextAdditionalSpread = GrowSharedAdditionalSpread(
			spreadResult.additionalSpread,
			flShotGrowth,
			spreadProfile.maxSpread);
		const int pelletCount = ExpShotgunPrimaryPelletCount();
		const Vector vecPatternAiming = vecAiming +
			(patternResult.offsetX * gpGlobals->v_right) +
			(patternResult.offsetY * gpGlobals->v_up);
		const bool resetShotChain = !fHasPreviousAcceptedShot ||
			patternResult.resetApplied ||
			(spreadProfile.additionalSpreadRecoverySeconds > 0.0f &&
			 flTimeSincePreviousAcceptedShot >= spreadProfile.additionalSpreadRecoverySeconds);

		BeginShotgunPrimaryShotContext(m_pPlayer, pelletCount);
		if (deterministicPelletLayout)
		{
			vecDir = FireDeterministicShotgunPellets(
				m_pPlayer,
				vecSrc,
				vecPatternAiming,
				pelletCount,
				flSpread,
				patternResult.patternIndex,
				ExpShotgunPatternScaleX(),
				ExpShotgunPatternScaleY());
		}
		else
		{
			vecDir = m_pPlayer->FireBulletsPlayer(
				pelletCount,
				vecSrc,
				vecPatternAiming,
				Vector(patternResult.randomSpread, patternResult.randomSpread, 0.0f),
				2048,
				BULLET_PLAYER_BUCKSHOT,
				0,
				0,
				m_pPlayer->pev,
				m_pPlayer->random_seed);
		}

		ShotgunAcceptedShotTelemetry acceptedTelemetry = {};
		acceptedTelemetry.experimentalModeActive = ExpShotgunExperimentalModeEnabled();
		acceptedTelemetry.firstShotAccuracyApplied = spreadResult.firstShotAccuracyApplied;
		acceptedTelemetry.spread = flSpread;
		acceptedTelemetry.baseSpread = spreadProfile.baseSpread;
		acceptedTelemetry.additionalSpread = spreadResult.additionalSpread;
		acceptedTelemetry.recoveryApplied = spreadState.additionalSpread - spreadResult.additionalSpread;
		acceptedTelemetry.shotGrowth = flShotGrowth;
		acceptedTelemetry.patternModeActive = patternProfile.enabled || deterministicPelletLayout;
		acceptedTelemetry.pelletSpreadMode = ExpShotgunPrimaryPelletSpreadMode();
		acceptedTelemetry.patternIndex = patternResult.patternIndex;
		acceptedTelemetry.patternResetApplied = patternResult.resetApplied;
		acceptedTelemetry.patternOffsetX = patternResult.offsetX;
		acceptedTelemetry.patternOffsetY = patternResult.offsetY;
		acceptedTelemetry.patternScaleX = ExpShotgunPatternScaleX();
		acceptedTelemetry.patternScaleY = ExpShotgunPatternScaleY();
		acceptedTelemetry.totalAdditionalSpread = spreadResult.additionalSpread + spreadResult.movementPenalty;
		acceptedTelemetry.movementPenalty = spreadResult.movementPenalty;
		acceptedTelemetry.movementContribution = spreadResult.movementPenalty;
		acceptedTelemetry.patternContribution = sqrtf((patternResult.offsetX * patternResult.offsetX) + (patternResult.offsetY * patternResult.offsetY));
		acceptedTelemetry.cadenceGrowthContribution = flShotGrowth;
		acceptedTelemetry.horizontalSpeed = spreadState.horizontalSpeed;
		acceptedTelemetry.maxSpeedForNormalization = spreadState.maxSpeedForNormalization;
		acceptedTelemetry.grounded = spreadState.grounded;
		acceptedTelemetry.ducking = spreadState.ducking;
		acceptedTelemetry.hasPreviousAcceptedShot = fHasPreviousAcceptedShot != FALSE;
		acceptedTelemetry.timeSincePreviousAcceptedShot = flTimeSincePreviousAcceptedShot;
		acceptedTelemetry.clipAfterShot = m_iClip;
		acceptedTelemetry.pelletCount = pelletCount;

		LogAcceptedShotgunPrimaryShot(m_pPlayer, acceptedTelemetry);
		EndShotgunPrimaryShotContext();
		m_flLastAcceptedPrimaryShotTime = gpGlobals->time;
		m_flPrimarySpreadAccumulator = flNextAdditionalSpread;
		m_iPrimaryShotCount = resetShotChain ? 1 : (m_iPrimaryShotCount + 1);
		m_iPrimaryPatternIndex = patternProfile.enabled ? patternResult.patternIndex : -1;
	}
	else
	{
#ifdef CLIENT_DLL
		if ( bIsMultiplayer() )
#else
		if ( g_pGameRules->IsMultiplayer() )
#endif
		{
			vecDir = m_pPlayer->FireBulletsPlayer( 4, vecSrc, vecAiming, VECTOR_CONE_DM_SHOTGUN, 2048, BULLET_PLAYER_BUCKSHOT, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );
		}
		else
		{
			// regular old, untouched spread. 
			vecDir = m_pPlayer->FireBulletsPlayer( 6, vecSrc, vecAiming, VECTOR_CONE_10DEGREES, 2048, BULLET_PLAYER_BUCKSHOT, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );
		}
	}

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usSingleFire, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );


	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flPumpTime = gpGlobals->time + 0.5;

	m_flNextPrimaryAttack = GetNextAttackDelay(0.75);
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.75;
	if (m_iClip != 0)
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5.0;
	else
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.75;
	m_fInSpecialReload = 0;
}


void CShotgun::SecondaryAttack( void )
{
	// don't fire underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		PlayEmptySound( );
		m_flNextPrimaryAttack = GetNextAttackDelay(0.15);
		return;
	}

	if (m_iClip <= 1)
	{
		Reload( );
		PlayEmptySound( );
		return;
	}

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	m_iClip -= 2;


	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	m_pPlayer->pev->effects = (int)(m_pPlayer->pev->effects) | EF_MUZZLEFLASH;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	Vector vecSrc	 = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	Vector vecDir;
	
#ifdef CLIENT_DLL
	if ( bIsMultiplayer() )
#else
	if ( g_pGameRules->IsMultiplayer() )
#endif
	{
		// tuned for deathmatch
		vecDir = m_pPlayer->FireBulletsPlayer( 8, vecSrc, vecAiming, VECTOR_CONE_DM_DOUBLESHOTGUN, 2048, BULLET_PLAYER_BUCKSHOT, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );
	}
	else
	{
		// untouched default single player
		vecDir = m_pPlayer->FireBulletsPlayer( 12, vecSrc, vecAiming, VECTOR_CONE_10DEGREES, 2048, BULLET_PLAYER_BUCKSHOT, 0, 0, m_pPlayer->pev, m_pPlayer->random_seed );
	}
		
	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usDoubleFire, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, vecDir.x, vecDir.y, 0, 0, 0, 0 );

	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);

	m_flPumpTime = gpGlobals->time + 0.95;

	m_flNextPrimaryAttack = GetNextAttackDelay(1.5);
	m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.5;
	if (m_iClip != 0)
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 6.0;
	else
		m_flTimeWeaponIdle = 1.5;

	m_fInSpecialReload = 0;

}


void CShotgun::Reload( void )
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == SHOTGUN_MAX_CLIP)
		return;

	// don't reload until recoil is done
	if (m_flNextPrimaryAttack > UTIL_WeaponTimeBase())
		return;

	// check to see if we're ready to reload
	if (m_fInSpecialReload == 0)
	{
		SendWeaponAnim( SHOTGUN_START_RELOAD );
		m_fInSpecialReload = 1;
		m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.6;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.6;
		m_flNextPrimaryAttack = GetNextAttackDelay(1.0);
		m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 1.0;
		return;
	}
	else if (m_fInSpecialReload == 1)
	{
		if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
			return;
		// was waiting for gun to move to side
		m_fInSpecialReload = 2;

		if (RANDOM_LONG(0,1))
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/reload1.wav", 1, ATTN_NORM, 0, 85 + RANDOM_LONG(0,0x1f));
		else
			EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/reload3.wav", 1, ATTN_NORM, 0, 85 + RANDOM_LONG(0,0x1f));

		SendWeaponAnim( SHOTGUN_RELOAD );

		m_flNextReload = UTIL_WeaponTimeBase() + 0.5;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
	}
	else
	{
		// Add them to the clip
		m_iClip += 1;
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 1;
		m_fInSpecialReload = 1;
	}
}


void CShotgun::WeaponIdle( void )
{
	ResetEmptySound( );

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if (m_flTimeWeaponIdle <  UTIL_WeaponTimeBase() )
	{
		if (m_iClip == 0 && m_fInSpecialReload == 0 && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		{
			Reload( );
		}
		else if (m_fInSpecialReload != 0)
		{
			if (m_iClip != 8 && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
			{
				Reload( );
			}
			else
			{
				// reload debounce has timed out
				SendWeaponAnim( SHOTGUN_PUMP );
				
				// play cocking sound
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/scock1.wav", 1, ATTN_NORM, 0, 95 + RANDOM_LONG(0,0x1f));
				m_fInSpecialReload = 0;
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.5;
			}
		}
		else
		{
			int iAnim;
			float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
			if (flRand <= 0.8)
			{
				iAnim = SHOTGUN_IDLE_DEEP;
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + (60.0/12.0);// * RANDOM_LONG(2, 5);
			}
			else if (flRand <= 0.95)
			{
				iAnim = SHOTGUN_IDLE;
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + (20.0/9.0);
			}
			else
			{
				iAnim = SHOTGUN_IDLE4;
				m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + (20.0/9.0);
			}
			SendWeaponAnim( iAnim );
		}
	}
}

void CShotgun::ItemPostFrame( void )
{
	if ( m_flPumpTime && m_flPumpTime < gpGlobals->time )
	{
		// play pumping sound
		EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, "weapons/scock1.wav", 1, ATTN_NORM, 0, 95 + RANDOM_LONG(0,0x1f));
		m_flPumpTime = 0;
	}

	CBasePlayerWeapon::ItemPostFrame();
}



class CShotgunAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL(ENT(pev), "models/w_shotbox.mdl");
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL ("models/w_shotbox.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		if (pOther->GiveAmmo( AMMO_BUCKSHOTBOX_GIVE, "buckshot", BUCKSHOT_MAX_CARRY ) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_buckshot, CShotgunAmmo );


