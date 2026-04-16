#pragma once

class CBaseEntity;

void RegisterFutureGameplayCvars();
void UpdateFutureGameplayHooksFrame();
bool ExpPistolTapFireEnabled();
float ExpMoveSpreadScale();
bool ExpFirstShotAccuracyEnabled();
float ExpSpreadRecoverySeconds();
const char *ExpGlockProfileName();
float ExpGlockPrimaryBaseSpread();
float ExpGlockPrimaryGroundMovePenalty();
float ExpGlockPrimaryAirMovePenalty();
float ExpGlockPrimaryDuckPenaltyScale();
float ExpGlockPrimaryFirstShotSpeedThreshold();
float ExpGlockPrimaryMaxSpread();
float ExpGlockPrimaryDamage();
float ExpGlockPrimaryHeadshotScale();
bool ExpGlockPrimaryHeadshotLethal();
bool ExpDebugWeaponLogEnabled();
bool ExpDebugWeaponLogRejectionsEnabled();
bool ExpGlockExperimentalModeEnabled();
bool ExpGlockLabDummyEnabled();
float ExpGlockLabDummyHealth();
bool ExpGlockLabDummyAutoRespawnEnabled();
float ExpGlockLabDummyRespawnDelaySeconds();
float ExpGlockLabDummySpawnDistance();
const char *ExpGlockLabDummyModel();
bool ExpGlockLabDummyFacePlayer();
float ExpGlockLabDummyOffsetRight();
float ExpGlockLabDummyOffsetUp();
bool IsExpGlockLabDummyEntity(CBaseEntity *pEntity);
