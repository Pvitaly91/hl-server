#include "extdll.h"
#include "util.h"

#include "future_gameplay_hooks.h"

namespace
{
cvar_t sv_exp_pistol_tapfire = {"sv_exp_pistol_tapfire", "0", FCVAR_SERVER};
cvar_t sv_exp_move_spread_scale = {"sv_exp_move_spread_scale", "0.0", FCVAR_SERVER};
cvar_t sv_exp_first_shot_accuracy = {"sv_exp_first_shot_accuracy", "0", FCVAR_SERVER};
cvar_t sv_exp_spread_recovery = {"sv_exp_spread_recovery", "0.0", FCVAR_SERVER};

bool g_futureGameplayCvarsRegistered = false;
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

    ALERT(at_console, "[hl-server] future gameplay hooks registered\n");
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
