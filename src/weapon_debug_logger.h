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
    float horizontalSpeed;
    float maxSpeedForNormalization;
    bool grounded;
    bool ducking;
    bool hasPreviousAcceptedShot;
    float timeSincePreviousAcceptedShot;
    int clipAfterShot;
};

struct GlockRejectedShotTelemetry
{
    float horizontalSpeed;
    bool grounded;
    bool ducking;
};

void EnsureWeaponDebugLogReady();
void LogAcceptedGlockPrimaryShot(CBasePlayer *pPlayer, const GlockAcceptedShotTelemetry &telemetry);
void LogRejectedGlockPrimaryHold(CBasePlayer *pPlayer, const GlockRejectedShotTelemetry &telemetry);
void LogGlockLabDummySpawn(CBaseEntity *pDummy, CBasePlayer *pAnchorPlayer, bool respawn, const Vector &origin, const Vector &angles);
void LogGlockLabDummyClear(CBaseEntity *pDummy, const char *reason);
void BeginGlockPrimaryShotContext(CBasePlayer *pPlayer);
void EndGlockPrimaryShotContext();
float GetActiveGlockPrimaryBaseDamage(entvars_t *pevAttacker, float fallbackDamage);
bool ApplyActiveGlockPrimaryTraceDamage(CBaseEntity *pVictim, entvars_t *pevAttacker, int hitgroup, float *pDamage);
void FinalizeActiveGlockPrimaryHitTelemetry();
