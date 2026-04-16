#include "extdll.h"
#include "util.h"

#include "future_gameplay_hooks.h"
#include "weapon_debug_logger.h"

namespace
{
cvar_t sv_exp_pistol_tapfire = {"sv_exp_pistol_tapfire", "0", FCVAR_SERVER};
cvar_t sv_exp_move_spread_scale = {"sv_exp_move_spread_scale", "0.0", FCVAR_SERVER};
cvar_t sv_exp_first_shot_accuracy = {"sv_exp_first_shot_accuracy", "0", FCVAR_SERVER};
cvar_t sv_exp_spread_recovery = {"sv_exp_spread_recovery", "0.0", FCVAR_SERVER};
cvar_t sv_exp_glock_profile_name = {"sv_exp_glock_profile_name", "default", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_glock_primary_base_spread = {"sv_exp_glock_primary_base_spread", "0.01", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_ground_move_penalty = {"sv_exp_glock_primary_ground_move_penalty", "0.08", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_air_move_penalty = {"sv_exp_glock_primary_air_move_penalty", "0.12", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_duck_penalty_scale = {"sv_exp_glock_primary_duck_penalty_scale", "0.75", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_first_shot_speed_threshold = {"sv_exp_glock_primary_first_shot_speed_threshold", "40.0", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_max_spread = {"sv_exp_glock_primary_max_spread", "0.2", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_damage = {"sv_exp_glock_primary_damage", "8.0", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_headshot_scale = {"sv_exp_glock_primary_headshot_scale", "3.0", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_headshot_lethal = {"sv_exp_glock_primary_headshot_lethal", "0", FCVAR_SERVER};
cvar_t sv_exp_debug_weaponlog = {"sv_exp_debug_weaponlog", "0", FCVAR_SERVER};
cvar_t sv_exp_debug_weaponlog_rejections = {"sv_exp_debug_weaponlog_rejections", "0", FCVAR_SERVER};

bool g_futureGameplayCvarsRegistered = false;

float GetNonNegativeCvarValue(const cvar_t &cvar)
{
    return cvar.value >= 0.0f ? cvar.value : 0.0f;
}

const char *GetNonEmptyCvarString(const cvar_t &cvar, const char *fallback)
{
    if (cvar.string == NULL || cvar.string[0] == '\0')
    {
        return fallback;
    }

    return cvar.string;
}
}

void RegisterFutureGameplayCvars()
{
    if (g_futureGameplayCvarsRegistered)
    {
        return;
    }

    g_futureGameplayCvarsRegistered = true;

    CVAR_REGISTER(&sv_exp_pistol_tapfire);
    CVAR_REGISTER(&sv_exp_move_spread_scale);
    CVAR_REGISTER(&sv_exp_first_shot_accuracy);
    CVAR_REGISTER(&sv_exp_spread_recovery);
    CVAR_REGISTER(&sv_exp_glock_profile_name);
    CVAR_REGISTER(&sv_exp_glock_primary_base_spread);
    CVAR_REGISTER(&sv_exp_glock_primary_ground_move_penalty);
    CVAR_REGISTER(&sv_exp_glock_primary_air_move_penalty);
    CVAR_REGISTER(&sv_exp_glock_primary_duck_penalty_scale);
    CVAR_REGISTER(&sv_exp_glock_primary_first_shot_speed_threshold);
    CVAR_REGISTER(&sv_exp_glock_primary_max_spread);
    CVAR_REGISTER(&sv_exp_glock_primary_damage);
    CVAR_REGISTER(&sv_exp_glock_primary_headshot_scale);
    CVAR_REGISTER(&sv_exp_glock_primary_headshot_lethal);
    CVAR_REGISTER(&sv_exp_debug_weaponlog);
    CVAR_REGISTER(&sv_exp_debug_weaponlog_rejections);

    ALERT(at_console, "[hl-server] future gameplay hooks registered\n");
}

void UpdateFutureGameplayHooksFrame()
{
    EnsureWeaponDebugLogReady();
}

bool ExpPistolTapFireEnabled()
{
    return sv_exp_pistol_tapfire.value != 0.0f;
}

float ExpMoveSpreadScale()
{
    return sv_exp_move_spread_scale.value > 0.0f ? sv_exp_move_spread_scale.value : 0.0f;
}

bool ExpFirstShotAccuracyEnabled()
{
    return sv_exp_first_shot_accuracy.value != 0.0f;
}

float ExpSpreadRecoverySeconds()
{
    return sv_exp_spread_recovery.value > 0.0f ? sv_exp_spread_recovery.value : 0.0f;
}

const char *ExpGlockProfileName()
{
    return GetNonEmptyCvarString(sv_exp_glock_profile_name, "default");
}

float ExpGlockPrimaryBaseSpread()
{
    return GetNonNegativeCvarValue(sv_exp_glock_primary_base_spread);
}

float ExpGlockPrimaryGroundMovePenalty()
{
    return GetNonNegativeCvarValue(sv_exp_glock_primary_ground_move_penalty);
}

float ExpGlockPrimaryAirMovePenalty()
{
    return GetNonNegativeCvarValue(sv_exp_glock_primary_air_move_penalty);
}

float ExpGlockPrimaryDuckPenaltyScale()
{
    return GetNonNegativeCvarValue(sv_exp_glock_primary_duck_penalty_scale);
}

float ExpGlockPrimaryFirstShotSpeedThreshold()
{
    return GetNonNegativeCvarValue(sv_exp_glock_primary_first_shot_speed_threshold);
}

float ExpGlockPrimaryMaxSpread()
{
    return GetNonNegativeCvarValue(sv_exp_glock_primary_max_spread);
}

float ExpGlockPrimaryDamage()
{
    return GetNonNegativeCvarValue(sv_exp_glock_primary_damage);
}

float ExpGlockPrimaryHeadshotScale()
{
    return GetNonNegativeCvarValue(sv_exp_glock_primary_headshot_scale);
}

bool ExpGlockPrimaryHeadshotLethal()
{
    return sv_exp_glock_primary_headshot_lethal.value != 0.0f;
}

bool ExpDebugWeaponLogEnabled()
{
    return sv_exp_debug_weaponlog.value != 0.0f;
}

bool ExpDebugWeaponLogRejectionsEnabled()
{
    return sv_exp_debug_weaponlog_rejections.value != 0.0f;
}

bool ExpGlockExperimentalModeEnabled()
{
    return ExpPistolTapFireEnabled() || ExpMoveSpreadScale() > 0.0f || ExpFirstShotAccuracyEnabled();
}
