#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "future_gameplay_hooks.h"
#include "weapon_tuning_core.h"

#include <math.h>
#include <string.h>

namespace
{
float ClampSharedValue(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }

    if (value > maximum)
    {
        return maximum;
    }

    return value;
}

float ResolveNormalizedMaxSpeed(float maxSpeed, float fallbackMaxSpeed)
{
    return maxSpeed > 1.0f ? maxSpeed : fallbackMaxSpeed;
}

float GetFallbackHitgroupScale(CBaseEntity *pVictim, int hitgroup)
{
    const bool playerVictim = pVictim != NULL && pVictim->IsPlayer();

    switch (hitgroup)
    {
    case HITGROUP_HEAD:
        return playerVictim ? gSkillData.plrHead : gSkillData.monHead;
    case HITGROUP_CHEST:
        return playerVictim ? gSkillData.plrChest : gSkillData.monChest;
    case HITGROUP_STOMACH:
        return playerVictim ? gSkillData.plrStomach : gSkillData.monStomach;
    case HITGROUP_LEFTARM:
    case HITGROUP_RIGHTARM:
        return playerVictim ? gSkillData.plrArm : gSkillData.monArm;
    case HITGROUP_LEFTLEG:
    case HITGROUP_RIGHTLEG:
        return playerVictim ? gSkillData.plrLeg : gSkillData.monLeg;
    case HITGROUP_GENERIC:
    default:
        return 1.0f;
    }
}

bool DummyArmorProtectsHitgroup(int hitgroup, bool headProtected)
{
    switch (hitgroup)
    {
    case HITGROUP_CHEST:
    case HITGROUP_STOMACH:
        return true;
    case HITGROUP_HEAD:
        return headProtected;
    default:
        return false;
    }
}

float ComputePlayerHeadshotLethalDamage(CBasePlayer *pPlayer)
{
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        return 0.0f;
    }

    const float health = pPlayer->pev->health > 0.0f ? pPlayer->pev->health : 0.0f;
    const float armor = pPlayer->pev->armorvalue > 0.0f ? pPlayer->pev->armorvalue : 0.0f;
    const float requiredDamage = (armor >= (2.0f * health))
        ? (5.0f * health)
        : (health + (2.0f * armor));

    return (float)ceil(requiredDamage);
}

float ComputeDummyHeadshotLethalDamage(CBaseEntity *pVictim)
{
    if (pVictim == NULL || pVictim->pev == NULL)
    {
        return 0.0f;
    }

    const float health = pVictim->pev->health > 0.0f ? pVictim->pev->health : 0.0f;
    if (health <= 0.0f)
    {
        return 0.0f;
    }

    if (!ExpGlockLabDummyHeadProtected())
    {
        return (float)ceil(health);
    }

    const float armor = pVictim->pev->armorvalue > 0.0f ? pVictim->pev->armorvalue : 0.0f;
    if (armor <= 0.0f)
    {
        return (float)ceil(health);
    }

    const float armorHealthFraction = ClampSharedValue(ExpGlockLabDummyArmorHealthFraction(), 0.0f, 1.0f);
    const float armorDrainScale = ExpGlockLabDummyArmorDrainScale();
    const float absorbedFraction = 1.0f - armorHealthFraction;

    if (absorbedFraction <= 0.0f)
    {
        return (float)ceil(health);
    }

    if (armorDrainScale <= 0.0f)
    {
        if (armorHealthFraction <= 0.0f)
        {
            return 0.0f;
        }

        return (float)ceil(health / armorHealthFraction);
    }

    if (armorHealthFraction > 0.0f)
    {
        const float transitionDamage = armor / (absorbedFraction * armorDrainScale);
        const float lethalWhileArmored = health / armorHealthFraction;
        if (lethalWhileArmored <= transitionDamage)
        {
            return (float)ceil(lethalWhileArmored);
        }
    }

    return (float)ceil(health + (armor / armorDrainScale));
}

float ComputeHeadshotLethalDamage(CBaseEntity *pVictim)
{
    if (pVictim == NULL || pVictim->pev == NULL)
    {
        return 0.0f;
    }

    if (IsExpGlockLabDummyEntity(pVictim))
    {
        const float dummyLethalDamage = ComputeDummyHeadshotLethalDamage(pVictim);
        if (dummyLethalDamage > 0.0f)
        {
            return dummyLethalDamage;
        }
    }

    if (pVictim->IsPlayer())
    {
        return ComputePlayerHeadshotLethalDamage((CBasePlayer *)pVictim);
    }

    return (float)ceil(pVictim->pev->health > 0.0f ? pVictim->pev->health : 0.0f);
}
}

SharedWeaponSpreadProfile BuildGlockPrimarySpreadProfile()
{
    SharedWeaponSpreadProfile profile = {};
    profile.baseSpread = ExpGlockPrimaryBaseSpread();
    profile.groundMovePenalty = ExpGlockPrimaryGroundMovePenalty();
    profile.airMovePenalty = ExpGlockPrimaryAirMovePenalty();
    profile.duckPenaltyScale = ExpGlockPrimaryDuckPenaltyScale();
    profile.movementPenaltyScale = ExpMoveSpreadScale();
    profile.firstShotAccuracyEnabled = ExpFirstShotAccuracyEnabled();
    profile.firstShotSpeedThreshold = ExpGlockPrimaryFirstShotSpeedThreshold();
    profile.firstShotRecoverySeconds = ExpSpreadRecoverySeconds();
    profile.maxSpread = ExpGlockPrimaryMaxSpread();
    return profile;
}

SharedWeaponSpreadProfile BuildMp5PrimarySpreadProfile()
{
    SharedWeaponSpreadProfile profile = {};
    profile.baseSpread = ExpMP5PrimaryBaseSpread();
    profile.groundMovePenalty = ExpMP5PrimaryGroundMovePenalty();
    profile.airMovePenalty = ExpMP5PrimaryAirMovePenalty();
    profile.duckPenaltyScale = ExpMP5PrimaryDuckPenaltyScale();
    profile.movementPenaltyScale = 1.0f;
    profile.firstShotAccuracyEnabled = ExpMP5PrimaryFirstShotAccuracyEnabled();
    profile.firstShotSpeedThreshold = ExpMP5PrimaryFirstShotSpeedThreshold();
    profile.firstShotRecoverySeconds = 0.0f;
    profile.maxSpread = ExpMP5PrimaryMaxSpread();
    return profile;
}

SharedWeaponSpreadProfile Build357PrimarySpreadProfile()
{
    SharedWeaponSpreadProfile profile = {};
    profile.baseSpread = Exp357PrimaryBaseSpread();
    profile.groundMovePenalty = Exp357PrimaryGroundMovePenalty();
    profile.airMovePenalty = Exp357PrimaryAirMovePenalty();
    profile.duckPenaltyScale = Exp357PrimaryDuckPenaltyScale();
    profile.movementPenaltyScale = 1.0f;
    profile.firstShotAccuracyEnabled = Exp357PrimaryFirstShotAccuracyEnabled();
    profile.firstShotSpeedThreshold = Exp357PrimaryFirstShotSpeedThreshold();
    profile.firstShotRecoverySeconds = Exp357PrimarySpreadRecoverySeconds();
    profile.maxSpread = Exp357PrimaryMaxSpread();
    return profile;
}

SharedWeaponSpreadProfile BuildShotgunPrimarySpreadProfile()
{
    SharedWeaponSpreadProfile profile = {};
    profile.baseSpread = ExpShotgunPrimaryBaseSpread();
    profile.groundMovePenalty = ExpShotgunPrimaryGroundMovePenalty();
    profile.airMovePenalty = ExpShotgunPrimaryAirMovePenalty();
    profile.duckPenaltyScale = ExpShotgunPrimaryDuckPenaltyScale();
    profile.movementPenaltyScale = 1.0f;
    profile.firstShotAccuracyEnabled = ExpShotgunPrimaryFirstShotAccuracyEnabled();
    profile.firstShotSpeedThreshold = ExpShotgunPrimaryFirstShotSpeedThreshold();
    profile.firstShotRecoverySeconds = ExpShotgunPrimarySpreadRecoverySeconds();
    profile.maxSpread = ExpShotgunPrimaryMaxSpread();
    return profile;
}

SharedWeaponDamageProfile BuildGlockPrimaryDamageProfile()
{
    SharedWeaponDamageProfile profile = {};
    profile.baseDamage = ExpGlockPrimaryDamage();
    profile.headshotScale = ExpGlockPrimaryHeadshotScale();
    profile.headshotLethal = ExpGlockPrimaryHeadshotLethal();
    return profile;
}

SharedWeaponDamageProfile BuildMp5PrimaryDamageProfile()
{
    SharedWeaponDamageProfile profile = {};
    profile.baseDamage = ExpMP5PrimaryDamage();
    profile.headshotScale = ExpMP5PrimaryHeadshotScale();
    profile.headshotLethal = ExpMP5PrimaryHeadshotLethal();
    return profile;
}

SharedWeaponDamageProfile Build357PrimaryDamageProfile()
{
    SharedWeaponDamageProfile profile = {};
    profile.baseDamage = Exp357PrimaryDamage();
    profile.headshotScale = Exp357PrimaryHeadshotScale();
    profile.headshotLethal = Exp357PrimaryHeadshotLethal();
    return profile;
}

SharedWeaponDamageProfile BuildShotgunPrimaryDamageProfile()
{
    SharedWeaponDamageProfile profile = {};
    profile.baseDamage = ExpShotgunPrimaryDamagePerPellet();
    profile.headshotScale = ExpShotgunPrimaryHeadshotScale();
    profile.headshotLethal = ExpShotgunPrimaryHeadshotLethal();
    return profile;
}

SharedWeaponSpreadState BuildPlayerWeaponSpreadState(
    CBasePlayer *pPlayer,
    float fallbackMaxSpeed,
    bool hasPreviousShot,
    float timeSincePreviousShot,
    float additionalSpread)
{
    SharedWeaponSpreadState state = {};
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        state.maxSpeedForNormalization = fallbackMaxSpeed;
        return state;
    }

    state.grounded = FBitSet(pPlayer->pev->flags, FL_ONGROUND) != FALSE;
    state.ducking = ((pPlayer->pev->button & IN_DUCK) != 0) || FBitSet(pPlayer->pev->flags, FL_DUCKING);
    state.hasPreviousShot = hasPreviousShot;
    state.horizontalSpeed = pPlayer->pev->velocity.Length2D();
    state.maxSpeedForNormalization = ResolveNormalizedMaxSpeed(pPlayer->pev->maxspeed, fallbackMaxSpeed);
    state.timeSincePreviousShot = timeSincePreviousShot;
    state.additionalSpread = additionalSpread > 0.0f ? additionalSpread : 0.0f;
    return state;
}

SharedWeaponSpreadResult ComputeSharedWeaponSpread(
    const SharedWeaponSpreadProfile &profile,
    const SharedWeaponSpreadState &state)
{
    SharedWeaponSpreadResult result = {};
    result.normalizedMaxSpeed = ResolveNormalizedMaxSpeed(state.maxSpeedForNormalization, 1.0f);
    result.speedRatio = ClampSharedValue(
        result.normalizedMaxSpeed > 0.0f ? (state.horizontalSpeed / result.normalizedMaxSpeed) : 0.0f,
        0.0f,
        1.0f);
    result.additionalSpread = state.additionalSpread > 0.0f ? state.additionalSpread : 0.0f;

    const bool firstShotRecoverySatisfied = !state.hasPreviousShot ||
        profile.firstShotRecoverySeconds <= 0.0f ||
        state.timeSincePreviousShot >= profile.firstShotRecoverySeconds;
    result.firstShotAccuracyApplied = profile.firstShotAccuracyEnabled &&
        state.grounded &&
        state.horizontalSpeed <= profile.firstShotSpeedThreshold &&
        result.additionalSpread <= 0.0001f &&
        firstShotRecoverySatisfied;

    if (!result.firstShotAccuracyApplied)
    {
        if (profile.movementPenaltyScale > 0.0f)
        {
            result.movementPenalty = state.grounded
                ? (profile.groundMovePenalty * result.speedRatio * profile.movementPenaltyScale)
                : (profile.airMovePenalty * profile.movementPenaltyScale);

            if (state.ducking)
            {
                result.movementPenalty *= profile.duckPenaltyScale;
            }
        }

        result.spread = ClampSharedValue(
            profile.baseSpread + result.movementPenalty + result.additionalSpread,
            0.0f,
            profile.maxSpread);
    }
    else
    {
        result.spread = 0.0f;
    }

    return result;
}

float RecoverSharedAdditionalSpread(
    float currentSpread,
    float elapsedSeconds,
    float recoverySeconds,
    float maxAdditionalSpread)
{
    if (currentSpread <= 0.0f || recoverySeconds <= 0.0f || maxAdditionalSpread <= 0.0f)
    {
        return 0.0f;
    }

    const float recoveredSpread = currentSpread - ((maxAdditionalSpread / recoverySeconds) * elapsedSeconds);
    return recoveredSpread > 0.0f ? recoveredSpread : 0.0f;
}

bool ApplySharedWeaponTraceDamage(
    CBaseEntity *pVictim,
    int hitgroup,
    const SharedWeaponDamageProfile &profile,
    float fallbackDamage,
    SharedWeaponTraceDamageResult *result)
{
    if (pVictim == NULL || pVictim->pev == NULL || result == NULL)
    {
        return false;
    }

    memset(result, 0, sizeof(*result));
    result->headshot = hitgroup == HITGROUP_HEAD;
    result->hitgroupScale = result->headshot ? profile.headshotScale : GetFallbackHitgroupScale(pVictim, hitgroup);
    result->traceDamage = profile.baseDamage >= 0.0f
        ? (profile.baseDamage * result->hitgroupScale)
        : fallbackDamage;

    result->dummyVictim = IsExpGlockLabDummyEntity(pVictim);

    if (result->headshot && profile.headshotLethal)
    {
        const float lethalDamage = ComputeHeadshotLethalDamage(pVictim);
        if (lethalDamage > result->traceDamage)
        {
            result->traceDamage = lethalDamage;
            result->headshotLethalApplied = true;
        }
    }

    result->damageToHealth = result->traceDamage;
    result->victimArmorKnown = pVictim->IsPlayer() || result->dummyVictim;
    result->victimArmorBefore = result->victimArmorKnown ? pVictim->pev->armorvalue : 0.0f;

    if (result->dummyVictim)
    {
        result->dummyHeadProtected = ExpGlockLabDummyHeadProtected();
        const float dummyArmorBefore = pVictim->pev->armorvalue > 0.0f ? pVictim->pev->armorvalue : 0.0f;
        result->victimArmorBefore = dummyArmorBefore;

        if (dummyArmorBefore > 0.0f && DummyArmorProtectsHitgroup(hitgroup, result->dummyHeadProtected))
        {
            const float armorHealthFraction = ClampSharedValue(ExpGlockLabDummyArmorHealthFraction(), 0.0f, 1.0f);
            const float desiredDamageToHealth = result->traceDamage * armorHealthFraction;
            const float desiredDamageAbsorbed = result->traceDamage - desiredDamageToHealth;
            const float armorDrainScale = ExpGlockLabDummyArmorDrainScale();

            if (desiredDamageAbsorbed > 0.0f)
            {
                if (armorDrainScale <= 0.0f)
                {
                    result->damageToHealth = desiredDamageToHealth;
                    result->damageAbsorbed = desiredDamageAbsorbed;
                    result->armorDrain = 0.0f;
                    result->dummyArmorApplied = true;
                }
                else
                {
                    const float desiredArmorDrain = desiredDamageAbsorbed * armorDrainScale;

                    if (desiredArmorDrain <= dummyArmorBefore)
                    {
                        result->damageToHealth = desiredDamageToHealth;
                        result->damageAbsorbed = desiredDamageAbsorbed;
                        result->armorDrain = desiredArmorDrain;
                        result->dummyArmorApplied = true;
                    }
                    else
                    {
                        result->armorDrain = dummyArmorBefore;
                        result->damageAbsorbed = result->armorDrain / armorDrainScale;
                        result->damageToHealth = result->traceDamage - result->damageAbsorbed;
                        result->dummyArmorApplied = result->damageAbsorbed > 0.0f;
                    }
                }

                pVictim->pev->armorvalue = dummyArmorBefore - result->armorDrain;
                if (pVictim->pev->armorvalue < 0.0f)
                {
                    pVictim->pev->armorvalue = 0.0f;
                }
            }
        }
    }

    return true;
}
