#pragma once

#include "extdll.h"

class CBasePlayer;
class CBaseEntity;

struct GlockAcceptedShotTelemetry
{
    bool experimentalModeActive;
    bool tapFireActive;
    bool firstShotAccuracyApplied;
    float spread;
    float baseSpread;
    float movementPenalty;
    float additionalSpread;
    float recoveryApplied;
    float shotGrowth;
    float nextAdditionalSpread;
    bool cadenceModeActive;
    float cadenceInterval;
    float cadencePenalty;
    bool cadenceResetApplied;
    float holdPenalty;
    bool patternModeActive;
    int patternIndex;
    float patternOffsetX;
    float patternOffsetY;
    bool patternResetApplied;
    float totalAdditionalSpread;
    float movementContribution;
    float patternContribution;
    float cadenceContribution;
    float cadenceGrowthContribution;
    float speedRatio;
    float horizontalSpeed;
    float maxSpeedForNormalization;
    bool grounded;
    bool ducking;
    bool hasPreviousAcceptedShot;
    float timeSincePreviousAcceptedShot;
    int cadenceShotIndex;
    int clipAfterShot;
};

struct GlockRejectedShotTelemetry
{
    float horizontalSpeed;
    bool grounded;
    bool ducking;
};

struct Mp5AcceptedShotTelemetry
{
    bool experimentalModeActive;
    bool firstShotAccuracyApplied;
    float spread;
    float baseSpread;
    float movementPenalty;
    float burstAddedSpread;
    float recoveryApplied;
    bool burstResetApplied;
    float shotGrowth;
    float holdPenalty;
    float nextAdditionalSpread;
    bool patternModeActive;
    int patternIndex;
    float patternOffsetX;
    float patternOffsetY;
    bool patternResetApplied;
    float totalAdditionalSpread;
    float movementContribution;
    float airContribution;
    float crouchBonus;
    float cadenceGrowthContribution;
    float speedRatio;
    int burstShotIndex;
    float horizontalSpeed;
    float maxSpeedForNormalization;
    bool grounded;
    bool ducking;
    bool hasPreviousAcceptedShot;
    float timeSincePreviousAcceptedShot;
    int clipAfterShot;
};

struct Weapon357AcceptedShotTelemetry
{
    bool experimentalModeActive;
    bool firstShotAccuracyApplied;
    float spread;
    float baseSpread;
    float movementPenalty;
    float additionalSpread;
    float recoveryApplied;
    bool cadenceModeActive;
    float cadenceInterval;
    float cadencePenalty;
    bool cadenceResetApplied;
    float holdPenalty;
    float nextAdditionalSpread;
    bool patternModeActive;
    int patternIndex;
    float patternOffsetX;
    float patternOffsetY;
    bool patternResetApplied;
    float totalAdditionalSpread;
    float movementContribution;
    float patternContribution;
    float cadenceContribution;
    float horizontalSpeed;
    float maxSpeedForNormalization;
    bool grounded;
    bool ducking;
    bool hasPreviousAcceptedShot;
    float timeSincePreviousAcceptedShot;
    int cadenceShotIndex;
    int clipAfterShot;
};

struct ShotgunAcceptedShotTelemetry
{
    bool experimentalModeActive;
    bool firstShotAccuracyApplied;
    float spread;
    float baseSpread;
    float additionalSpread;
    float recoveryApplied;
    float shotGrowth;
    bool patternModeActive;
    int pelletSpreadMode;
    int patternIndex;
    bool patternResetApplied;
    float patternOffsetX;
    float patternOffsetY;
    float patternScaleX;
    float patternScaleY;
    float totalAdditionalSpread;
    float movementPenalty;
    float movementContribution;
    float patternContribution;
    float cadenceGrowthContribution;
    float horizontalSpeed;
    float maxSpeedForNormalization;
    bool grounded;
    bool ducking;
    bool hasPreviousAcceptedShot;
    float timeSincePreviousAcceptedShot;
    int clipAfterShot;
    int pelletCount;
};

void EnsureWeaponDebugLogReady();
void LogLiveCfgCommand(const char *action, const char *requestedPath, const char *execPath, const char *resolvedPath, bool success, const char *details);
void LogRoundEvent(const char *event, const char *state, int roundNumber, int connectedPlayers, int alivePlayers, CBasePlayer *pWinner, const char *reason);
void LogMatchEvent(const char *event, const char *reason);
void LogBuyEvent(const char *event, CBasePlayer *pPlayer, const char *teamName, const char *roundState, bool phaseOpen, const char *weapon, int cost, int moneyBefore, int moneyAfter, bool success, const char *reason);
void LogAcceptedGlockPrimaryShot(CBasePlayer *pPlayer, const GlockAcceptedShotTelemetry &telemetry);
void LogAcceptedMp5PrimaryShot(CBasePlayer *pPlayer, const Mp5AcceptedShotTelemetry &telemetry);
void LogAccepted357PrimaryShot(CBasePlayer *pPlayer, const Weapon357AcceptedShotTelemetry &telemetry);
void LogAcceptedShotgunPrimaryShot(CBasePlayer *pPlayer, const ShotgunAcceptedShotTelemetry &telemetry);
void LogRejectedGlockPrimaryHold(CBasePlayer *pPlayer, const GlockRejectedShotTelemetry &telemetry);
void LogLiveLabConsoleMessage(const char *line);
void LogGlockLabDummySpawn(CBaseEntity *pDummy, CBasePlayer *pAnchorPlayer, bool respawn, const Vector &origin, const Vector &angles, const char *source, const char *candidate, const char *spotName, const char *spotStorage);
void LogGlockLabDummySpawnFailed(CBasePlayer *pAnchorPlayer, const char *source, const char *candidate, const char *code, const char *reason, const Vector *pOrigin, const Vector *pAngles, const char *spotName, const char *spotStorage);
void LogGlockLabDummyMark(const char *action, CBasePlayer *pAnchorPlayer, const Vector &origin, const Vector &angles, const char *details, const char *spotName, const char *spotStorage);
void LogGlockLabDummyReposition(CBaseEntity *pDummy, CBasePlayer *pAnchorPlayer, const Vector &origin, const Vector &angles, const char *reason, const char *source, const char *candidate, const char *spotName, const char *spotStorage);
void LogGlockLabDummyClear(CBaseEntity *pDummy, const char *reason);
void LogTeamSpawnEvent(const char *event, CBasePlayer *pPlayer, int teamId, const char *teamName, const Vector &origin, const Vector &angles, const char *source, const char *candidate, const char *spotName, const char *spotStorage, const char *reason);
void BeginGlockPrimaryShotContext(CBasePlayer *pPlayer, bool verificationMode = false);
void BeginMp5PrimaryShotContext(CBasePlayer *pPlayer, bool verificationMode = false);
void Begin357PrimaryShotContext(CBasePlayer *pPlayer, bool verificationMode = false);
void BeginShotgunPrimaryShotContext(CBasePlayer *pPlayer, int pelletCount, bool verificationMode = false);
void EndGlockPrimaryShotContext();
void EndMp5PrimaryShotContext();
void End357PrimaryShotContext();
void EndShotgunPrimaryShotContext();
float GetActiveGlockPrimaryBaseDamage(entvars_t *pevAttacker, float fallbackDamage);
float GetActiveMp5PrimaryBaseDamage(entvars_t *pevAttacker, float fallbackDamage);
float GetActive357PrimaryBaseDamage(entvars_t *pevAttacker, float fallbackDamage);
float GetActiveShotgunPrimaryBaseDamage(entvars_t *pevAttacker, float fallbackDamage);
bool ApplyActiveGlockPrimaryTraceDamage(CBaseEntity *pVictim, entvars_t *pevAttacker, int hitgroup, float *pDamage);
bool ApplyActiveMp5PrimaryTraceDamage(CBaseEntity *pVictim, entvars_t *pevAttacker, int hitgroup, float *pDamage);
bool ApplyActive357PrimaryTraceDamage(CBaseEntity *pVictim, entvars_t *pevAttacker, int hitgroup, float *pDamage);
bool ApplyActiveShotgunPrimaryTraceDamage(CBaseEntity *pVictim, entvars_t *pevAttacker, int hitgroup, float *pDamage);
void FinalizeActiveGlockPrimaryHitTelemetry();
void FinalizeActiveMp5PrimaryHitTelemetry();
void FinalizeActive357PrimaryHitTelemetry();
void FinalizeActiveShotgunPrimaryHitTelemetry();
