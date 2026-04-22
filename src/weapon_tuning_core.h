#pragma once

class CBaseEntity;
class CBasePlayer;

struct SharedWeaponSpreadProfile
{
    float baseSpread;
    float groundMovePenalty;
    float airMovePenalty;
    float duckPenaltyScale;
    float movementPenaltyScale;
    bool firstShotAccuracyEnabled;
    float firstShotSpeedThreshold;
    float firstShotRecoverySeconds;
    float additionalSpreadRecoverySeconds;
    float maxSpread;
    bool duckScalesAdditionalSpread;
};

struct SharedWeaponSpreadState
{
    bool grounded;
    bool ducking;
    bool hasPreviousShot;
    float horizontalSpeed;
    float maxSpeedForNormalization;
    float timeSincePreviousShot;
    float additionalSpread;
};

struct SharedWeaponSpreadResult
{
    bool firstShotAccuracyApplied;
    float normalizedMaxSpeed;
    float speedRatio;
    float movementPenalty;
    float additionalSpread;
    float spread;
};

struct SharedWeaponPatternProfile
{
    bool enabled;
    float scaleX;
    float scaleY;
    float resetTimeSeconds;
    int maxIndex;
};

struct SharedWeaponPatternResult
{
    bool enabled;
    bool resetApplied;
    int patternIndex;
    float offsetX;
    float offsetY;
    float randomSpread;
};

struct SharedWeaponDamageProfile
{
    float baseDamage;
    float headshotScale;
    bool headshotLethal;
};

struct SharedWeaponTraceDamageResult
{
    bool headshot;
    float hitgroupScale;
    float traceDamage;
    float damageToHealth;
    float damageAbsorbed;
    float armorDrain;
    bool headshotLethalApplied;
    bool dummyVictim;
    bool victimArmorKnown;
    float victimArmorBefore;
    bool dummyArmorApplied;
    bool dummyHeadProtected;
};

SharedWeaponSpreadProfile BuildGlockPrimarySpreadProfile();
SharedWeaponSpreadProfile BuildMp5PrimarySpreadProfile();
SharedWeaponSpreadProfile Build357PrimarySpreadProfile();
SharedWeaponSpreadProfile BuildShotgunPrimarySpreadProfile();
SharedWeaponPatternProfile BuildGlockPrimaryPatternProfile();
SharedWeaponPatternProfile BuildMp5PrimaryPatternProfile();
SharedWeaponDamageProfile BuildGlockPrimaryDamageProfile();
SharedWeaponDamageProfile BuildMp5PrimaryDamageProfile();
SharedWeaponDamageProfile Build357PrimaryDamageProfile();
SharedWeaponDamageProfile BuildShotgunPrimaryDamageProfile();
SharedWeaponSpreadState BuildPlayerWeaponSpreadState(
    CBasePlayer *pPlayer,
    float fallbackMaxSpeed,
    bool hasPreviousShot,
    float timeSincePreviousShot,
    float additionalSpread);
SharedWeaponSpreadResult ComputeSharedWeaponSpread(
    const SharedWeaponSpreadProfile &profile,
    const SharedWeaponSpreadState &state);
SharedWeaponPatternResult ComputeSharedWeaponPattern(
    const SharedWeaponPatternProfile &profile,
    const SharedWeaponSpreadState &state,
    float speedRatio,
    float totalSpread,
    int lastPatternIndex);
float RecoverSharedAdditionalSpread(
    float currentSpread,
    float elapsedSeconds,
    float recoverySeconds,
    float maxAdditionalSpread);
float GrowSharedAdditionalSpread(
    float currentSpread,
    float growthPerShot,
    float maxAdditionalSpread);
bool ApplySharedWeaponTraceDamage(
    CBaseEntity *pVictim,
    int hitgroup,
    const SharedWeaponDamageProfile &profile,
    float fallbackDamage,
    SharedWeaponTraceDamageResult *result);
