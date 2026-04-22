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

int ClampPatternMaxIndex(int maxIndex)
{
    return maxIndex > 1 ? maxIndex : 1;
}

int ResolveNextPatternIndex(
    const SharedWeaponPatternProfile &profile,
    const SharedWeaponSpreadState &state,
    int lastPatternIndex,
    bool *resetApplied)
{
    if (resetApplied != NULL)
    {
        *resetApplied = false;
    }

    if (!profile.enabled || !state.hasPreviousShot)
    {
        return 0;
    }

    if (profile.resetTimeSeconds > 0.0f && state.timeSincePreviousShot >= profile.resetTimeSeconds)
    {
        if (resetApplied != NULL)
        {
            *resetApplied = true;
        }

        return 0;
    }

    const int clampedMaxIndex = ClampPatternMaxIndex(profile.maxIndex);
    const int nextPatternIndex = lastPatternIndex >= 0 ? (lastPatternIndex + 1) : 0;
    return nextPatternIndex < clampedMaxIndex ? nextPatternIndex : (clampedMaxIndex - 1);
}

float ResolvePatternControlScale(const SharedWeaponSpreadState &state, float speedRatio)
{
    float controlScale = 1.0f;

    if (!state.grounded)
    {
        controlScale = 1.40f;
    }
    else
    {
        controlScale += ClampSharedValue(speedRatio, 0.0f, 1.0f) * 0.60f;
        if (state.ducking)
        {
            controlScale *= 0.80f;
        }
    }

    return controlScale;
}

const Vector2D kDeterministicPatternPoints[] =
{
    Vector2D(0.00f, 0.00f),
    Vector2D(0.18f, -0.12f),
    Vector2D(-0.16f, -0.24f),
    Vector2D(0.22f, -0.38f),
    Vector2D(-0.20f, -0.52f),
    Vector2D(0.26f, -0.68f),
    Vector2D(-0.24f, -0.84f),
    Vector2D(0.30f, -1.00f),
    Vector2D(-0.28f, -1.16f),
    Vector2D(0.34f, -1.32f)
};

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

bool PlayerArmorProtectsHitgroup(int hitgroup, bool headProtected)
{
    switch (hitgroup)
    {
    case HITGROUP_CHEST:
    case HITGROUP_STOMACH:
    case HITGROUP_LEFTARM:
    case HITGROUP_RIGHTARM:
        return true;
    case HITGROUP_HEAD:
        return headProtected;
    default:
        return false;
    }
}

bool ApplyCustomArmorAbsorption(
    float traceDamage,
    float armorBefore,
    float armorHealthFraction,
    float armorDrainScale,
    float *damageToHealth,
    float *damageAbsorbed,
    float *armorDrain,
    bool *armorApplied)
{
    if (damageToHealth == NULL || damageAbsorbed == NULL || armorDrain == NULL || armorApplied == NULL)
    {
        return false;
    }

    *damageToHealth = traceDamage;
    *damageAbsorbed = 0.0f;
    *armorDrain = 0.0f;
    *armorApplied = false;

    if (traceDamage <= 0.0f || armorBefore <= 0.0f)
    {
        return true;
    }

    const float clampedArmorHealthFraction = ClampSharedValue(armorHealthFraction, 0.0f, 1.0f);
    const float desiredDamageToHealth = traceDamage * clampedArmorHealthFraction;
    const float desiredDamageAbsorbed = traceDamage - desiredDamageToHealth;
    if (desiredDamageAbsorbed <= 0.0f)
    {
        return true;
    }

    if (armorDrainScale <= 0.0f)
    {
        *damageToHealth = desiredDamageToHealth;
        *damageAbsorbed = desiredDamageAbsorbed;
        *armorApplied = true;
        return true;
    }

    const float desiredArmorDrain = desiredDamageAbsorbed * armorDrainScale;
    if (desiredArmorDrain <= armorBefore)
    {
        *damageToHealth = desiredDamageToHealth;
        *damageAbsorbed = desiredDamageAbsorbed;
        *armorDrain = desiredArmorDrain;
        *armorApplied = true;
        return true;
    }

    *armorDrain = armorBefore;
    *damageAbsorbed = *armorDrain / armorDrainScale;
    *damageToHealth = traceDamage - *damageAbsorbed;
    *armorApplied = *damageAbsorbed > 0.0f;
    return true;
}

float ComputeCustomArmoredLethalDamage(float health, float armor, float armorHealthFraction, float armorDrainScale)
{
    if (health <= 0.0f)
    {
        return 0.0f;
    }

    if (armor <= 0.0f)
    {
        return (float)ceil(health);
    }

    const float clampedArmorHealthFraction = ClampSharedValue(armorHealthFraction, 0.0f, 1.0f);
    const float absorbedFraction = 1.0f - clampedArmorHealthFraction;
    if (absorbedFraction <= 0.0f)
    {
        return (float)ceil(health);
    }

    if (armorDrainScale <= 0.0f)
    {
        if (clampedArmorHealthFraction <= 0.0f)
        {
            return 0.0f;
        }

        return (float)ceil(health / clampedArmorHealthFraction);
    }

    if (clampedArmorHealthFraction > 0.0f)
    {
        const float transitionDamage = armor / (absorbedFraction * armorDrainScale);
        const float lethalWhileArmored = health / clampedArmorHealthFraction;
        if (lethalWhileArmored <= transitionDamage)
        {
            return (float)ceil(lethalWhileArmored);
        }
    }

    return (float)ceil(health + (armor / armorDrainScale));
}

float ComputePlayerHeadshotLethalDamage(CBasePlayer *pPlayer)
{
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        return 0.0f;
    }

    const float health = pPlayer->pev->health > 0.0f ? pPlayer->pev->health : 0.0f;
    const float armor = pPlayer->pev->armorvalue > 0.0f ? pPlayer->pev->armorvalue : 0.0f;
    if (ExpArmorModeEnabled())
    {
        if (!FutureGameplayPlayerHeadProtectionActive(pPlayer))
        {
            return (float)ceil(health);
        }

        return ComputeCustomArmoredLethalDamage(
            health,
            armor,
            ExpArmorHealthFraction(),
            ExpArmorDrainScale());
    }

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
    profile.additionalSpreadRecoverySeconds = ExpSpreadRecoverySeconds();
    profile.maxSpread = ExpGlockPrimaryMaxSpread();
    profile.duckScalesAdditionalSpread = true;
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
    profile.firstShotRecoverySeconds = ExpMP5PrimarySpreadRecoverySeconds();
    profile.additionalSpreadRecoverySeconds = ExpMP5PrimarySpreadRecoverySeconds();
    profile.maxSpread = ExpMP5PrimaryMaxSpread();
    profile.duckScalesAdditionalSpread = true;
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
    profile.additionalSpreadRecoverySeconds = Exp357PrimarySpreadRecoverySeconds();
    profile.maxSpread = Exp357PrimaryMaxSpread();
    profile.duckScalesAdditionalSpread = false;
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
    profile.additionalSpreadRecoverySeconds = ExpShotgunPrimarySpreadRecoverySeconds();
    profile.maxSpread = ExpShotgunPrimaryMaxSpread();
    profile.duckScalesAdditionalSpread = false;
    return profile;
}

SharedWeaponPatternProfile BuildGlockPrimaryPatternProfile()
{
    SharedWeaponPatternProfile profile = {};
    profile.enabled = ExpGlockPatternModeEnabled();
    profile.scaleX = ExpGlockPatternScaleX();
    profile.scaleY = ExpGlockPatternScaleY();
    profile.resetTimeSeconds = ExpGlockPatternResetTime();
    profile.maxIndex = ExpGlockPatternMaxIndex();
    return profile;
}

SharedWeaponPatternProfile BuildMp5PrimaryPatternProfile()
{
    SharedWeaponPatternProfile profile = {};
    profile.enabled = ExpMP5PatternModeEnabled();
    profile.scaleX = ExpMP5PatternScaleX();
    profile.scaleY = ExpMP5PatternScaleY();
    profile.resetTimeSeconds = ExpMP5PatternResetTime();
    profile.maxIndex = ExpMP5PatternMaxIndex();
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

        if (state.grounded && state.ducking && profile.duckScalesAdditionalSpread)
        {
            result.additionalSpread *= profile.duckPenaltyScale;
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

SharedWeaponPatternResult ComputeSharedWeaponPattern(
    const SharedWeaponPatternProfile &profile,
    const SharedWeaponSpreadState &state,
    float speedRatio,
    float totalSpread,
    int lastPatternIndex)
{
    SharedWeaponPatternResult result = {};
    const float clampedTotalSpread = totalSpread > 0.0f ? totalSpread : 0.0f;
    result.enabled = profile.enabled;
    result.randomSpread = clampedTotalSpread;

    if (!profile.enabled)
    {
        return result;
    }

    result.patternIndex = ResolveNextPatternIndex(profile, state, lastPatternIndex, &result.resetApplied);

    const int patternPointCount = sizeof(kDeterministicPatternPoints) / sizeof(kDeterministicPatternPoints[0]);
    const int cappedPatternIndex = result.patternIndex < patternPointCount
        ? result.patternIndex
        : (patternPointCount - 1);
    const Vector2D patternPoint = kDeterministicPatternPoints[cappedPatternIndex];
    const float controlScale = ResolvePatternControlScale(state, speedRatio);
    const float scaleX = ClampSharedValue(profile.scaleX, 0.0f, 4.0f);
    const float scaleY = ClampSharedValue(profile.scaleY, 0.0f, 4.0f);

    result.offsetX = clampedTotalSpread * patternPoint.x * scaleX * controlScale;
    result.offsetY = clampedTotalSpread * patternPoint.y * scaleY * controlScale;

    if (clampedTotalSpread <= 0.0001f)
    {
        result.randomSpread = 0.0f;
        return result;
    }

    const float offsetMagnitude = sqrtf((result.offsetX * result.offsetX) + (result.offsetY * result.offsetY));
    const float offsetRatio = ClampSharedValue(offsetMagnitude / clampedTotalSpread, 0.0f, 0.75f);
    const float residualScale = ClampSharedValue(0.70f - (offsetRatio * 0.30f), 0.35f, 0.70f);
    result.randomSpread = clampedTotalSpread * residualScale;
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

float GrowSharedAdditionalSpread(
    float currentSpread,
    float growthPerShot,
    float maxAdditionalSpread)
{
    if (maxAdditionalSpread <= 0.0f)
    {
        return 0.0f;
    }

    const float clampedCurrentSpread = ClampSharedValue(currentSpread, 0.0f, maxAdditionalSpread);
    if (growthPerShot <= 0.0f)
    {
        return clampedCurrentSpread;
    }

    return ClampSharedValue(clampedCurrentSpread + growthPerShot, 0.0f, maxAdditionalSpread);
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
            ApplyCustomArmorAbsorption(
                result->traceDamage,
                dummyArmorBefore,
                ExpGlockLabDummyArmorHealthFraction(),
                ExpGlockLabDummyArmorDrainScale(),
                &result->damageToHealth,
                &result->damageAbsorbed,
                &result->armorDrain,
                &result->dummyArmorApplied);

            pVictim->pev->armorvalue = dummyArmorBefore - result->armorDrain;
            if (pVictim->pev->armorvalue < 0.0f)
            {
                pVictim->pev->armorvalue = 0.0f;
            }
        }
    }
    else if (pVictim->IsPlayer() && ExpArmorModeEnabled())
    {
        CBasePlayer *pPlayerVictim = (CBasePlayer *)pVictim;
        const float playerArmorBefore = pVictim->pev->armorvalue > 0.0f ? pVictim->pev->armorvalue : 0.0f;
        result->victimArmorBefore = playerArmorBefore;
        result->dummyHeadProtected = FutureGameplayPlayerHeadProtectionActive(pPlayerVictim);
        FutureGameplayMarkPlayerBulletArmorHandled(pPlayerVictim);

        if (playerArmorBefore > 0.0f && PlayerArmorProtectsHitgroup(hitgroup, result->dummyHeadProtected))
        {
            ApplyCustomArmorAbsorption(
                result->traceDamage,
                playerArmorBefore,
                ExpArmorHealthFraction(),
                ExpArmorDrainScale(),
                &result->damageToHealth,
                &result->damageAbsorbed,
                &result->armorDrain,
                &result->dummyArmorApplied);

            pVictim->pev->armorvalue = playerArmorBefore - result->armorDrain;
            if (pVictim->pev->armorvalue < 0.0f)
            {
                pVictim->pev->armorvalue = 0.0f;
            }
        }
    }

    return true;
}
