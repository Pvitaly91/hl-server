#pragma once

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
