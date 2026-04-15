#pragma once

class CBasePlayer;

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
