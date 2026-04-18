#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "weapons.h"

#include "future_gameplay_hooks.h"
#include "weapon_debug_logger.h"

#include <math.h>
#include <stdarg.h>
#include <string.h>

namespace
{
const char *kDefaultGlockLabTargetProfileName = "default";
const float kDefaultGlockLabDummyHealth = 100.0f;
const float kDefaultGlockLabDummyRespawnDelay = 1.0f;
const float kDefaultGlockLabDummySpawnDistance = 256.0f;
const float kGlockLabDummyRetryDelay = 1.0f;
const float kGlockLabDummyPlacementSearchStep = 32.0f;
const float kGlockLabDummyPlacementMinDistance = 64.0f;
const char *kDefaultGlockLabDummyModel = "models/barney.mdl";
const char *kGlockLabDummyDisplayName = "Damage Dummy";
const char *kAllowedGlockLabDummyModels[] = {
    "models/barney.mdl",
    "models/scientist.mdl"};
const char *kGlockLabDummyClassname = "glock_lab_dummy";
const char *kGlockLabDummyTargetname = "exp_glock_lab_dummy";

struct LabDummyProfileDefinition
{
    const char *name;
    const char *armor;
    const char *headProtected;
    const char *armorHealthFraction;
    const char *armorDrainScale;
    const char *description;
};

const LabDummyProfileDefinition kBuiltInLabDummyProfiles[] = {
    {"unarmored", "0", "0", "0.5", "1.0", "baseline unarmored target"},
    {"vest", "100", "0", "0.5", "1.0", "torso-armored target"},
    {"vest_headprotected", "100", "1", "0.5", "1.0", "armored target with protected head"}};

cvar_t sv_exp_pistol_tapfire = {"sv_exp_pistol_tapfire", "0", FCVAR_SERVER};
cvar_t sv_exp_move_spread_scale = {"sv_exp_move_spread_scale", "0.0", FCVAR_SERVER};
cvar_t sv_exp_first_shot_accuracy = {"sv_exp_first_shot_accuracy", "0", FCVAR_SERVER};
cvar_t sv_exp_spread_recovery = {"sv_exp_spread_recovery", "0.0", FCVAR_SERVER};
cvar_t sv_exp_weapon_under_test = {"sv_exp_weapon_under_test", "", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_glock_profile_name = {"sv_exp_glock_profile_name", "default", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_mp5_profile_name = {"sv_exp_mp5_profile_name", "default", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_session_tag = {"sv_exp_session_tag", "", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_matrix_name = {"sv_exp_matrix_name", "", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_matrix_step = {"sv_exp_matrix_step", "", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_glock_primary_base_spread = {"sv_exp_glock_primary_base_spread", "0.01", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_ground_move_penalty = {"sv_exp_glock_primary_ground_move_penalty", "0.08", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_air_move_penalty = {"sv_exp_glock_primary_air_move_penalty", "0.12", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_duck_penalty_scale = {"sv_exp_glock_primary_duck_penalty_scale", "0.75", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_first_shot_speed_threshold = {"sv_exp_glock_primary_first_shot_speed_threshold", "40.0", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_max_spread = {"sv_exp_glock_primary_max_spread", "0.2", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_damage = {"sv_exp_glock_primary_damage", "8.0", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_headshot_scale = {"sv_exp_glock_primary_headshot_scale", "3.0", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_headshot_lethal = {"sv_exp_glock_primary_headshot_lethal", "0", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_enabled = {"sv_exp_mp5_primary_enabled", "0", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_base_spread = {"sv_exp_mp5_primary_base_spread", "0.0523", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_ground_move_penalty = {"sv_exp_mp5_primary_ground_move_penalty", "0.0200", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_air_move_penalty = {"sv_exp_mp5_primary_air_move_penalty", "0.0400", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_duck_penalty_scale = {"sv_exp_mp5_primary_duck_penalty_scale", "0.7000", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_burst_growth = {"sv_exp_mp5_primary_burst_growth", "0.0060", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_burst_max_additional_spread = {"sv_exp_mp5_primary_burst_max_additional_spread", "0.0600", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_spread_recovery = {"sv_exp_mp5_primary_spread_recovery", "0.3000", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_damage = {"sv_exp_mp5_primary_damage", "12.0", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_headshot_scale = {"sv_exp_mp5_primary_headshot_scale", "3.0", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_headshot_lethal = {"sv_exp_mp5_primary_headshot_lethal", "0", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_first_shot_accuracy = {"sv_exp_mp5_primary_first_shot_accuracy", "0", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_first_shot_speed_threshold = {"sv_exp_mp5_primary_first_shot_speed_threshold", "30.0", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_max_spread = {"sv_exp_mp5_primary_max_spread", "0.1200", FCVAR_SERVER};
cvar_t sv_exp_mp5_lab_loadout = {"sv_exp_mp5_lab_loadout", "0", FCVAR_SERVER};
cvar_t sv_exp_mp5_lab_ammo = {"sv_exp_mp5_lab_ammo", "250", FCVAR_SERVER};
cvar_t sv_exp_mp5_lab_autoswitch = {"sv_exp_mp5_lab_autoswitch", "1", FCVAR_SERVER};
cvar_t sv_exp_debug_weaponlog = {"sv_exp_debug_weaponlog", "0", FCVAR_SERVER};
cvar_t sv_exp_debug_weaponlog_rejections = {"sv_exp_debug_weaponlog_rejections", "0", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy = {"sv_exp_glock_lab_dummy", "0", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_target_profile_name = {"sv_exp_glock_lab_target_profile_name", "default", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_glock_lab_dummy_health = {"sv_exp_glock_lab_dummy_health", "100.0", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_armor = {"sv_exp_glock_lab_dummy_armor", "0.0", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_head_protected = {"sv_exp_glock_lab_dummy_head_protected", "0", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_armor_health_fraction = {"sv_exp_glock_lab_dummy_armor_health_fraction", "0.5", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_armor_drain_scale = {"sv_exp_glock_lab_dummy_armor_drain_scale", "1.0", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_autorespawn = {"sv_exp_glock_lab_dummy_autorespawn", "1", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_respawn_delay = {"sv_exp_glock_lab_dummy_respawn_delay", "1.0", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_spawn_distance = {"sv_exp_glock_lab_dummy_spawn_distance", "256.0", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_model = {"sv_exp_glock_lab_dummy_model", "models/barney.mdl", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_glock_lab_dummy_face_player = {"sv_exp_glock_lab_dummy_face_player", "1", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_offset_right = {"sv_exp_glock_lab_dummy_offset_right", "0.0", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_offset_up = {"sv_exp_glock_lab_dummy_offset_up", "0.0", FCVAR_SERVER};

bool g_futureGameplayCvarsRegistered = false;
bool g_futureGameplayCommandsRegistered = false;
EHANDLE g_glockLabDummy;
EHANDLE g_glockLabDummyAnchorPlayer;
Vector g_glockLabDummySpawnOrigin = g_vecZero;
Vector g_glockLabDummySpawnAngles = g_vecZero;
bool g_glockLabDummyHasSpawnTransform = false;
bool g_glockLabDummyRespawnPending = false;
float g_glockLabDummyRespawnTime = 0.0f;
float g_glockLabDummyRetryTime = 0.0f;
char g_futureHooksMapName[64] = "";

float GetNonNegativeCvarValue(const cvar_t &cvar)
{
    return cvar.value >= 0.0f ? cvar.value : 0.0f;
}

float GetPositiveOrDefaultCvarValue(const cvar_t &cvar, float fallback)
{
    return cvar.value > 0.0f ? cvar.value : fallback;
}

float ClampFloat(float value, float minimum, float maximum)
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

const char *GetNonEmptyCvarString(const cvar_t &cvar, const char *fallback)
{
    if (cvar.string == NULL || cvar.string[0] == '\0')
    {
        return fallback;
    }

    return cvar.string;
}

const char *GetOptionalCvarString(const cvar_t &cvar)
{
    return cvar.string != NULL ? cvar.string : "";
}

const char *GetCurrentMapName()
{
    if (gpGlobals == NULL || gpGlobals->mapname == 0)
    {
        return "";
    }

    return STRING(gpGlobals->mapname);
}

bool StringEqualsIgnoreCase(const char *left, const char *right)
{
    if (left == NULL || right == NULL)
    {
        return false;
    }

    return _stricmp(left, right) == 0;
}

const char *GetSafePlayerName(CBasePlayer *pPlayer)
{
    if (pPlayer == NULL || pPlayer->pev == NULL || pPlayer->pev->netname == 0)
    {
        return "none";
    }

    const char *name = STRING(pPlayer->pev->netname);
    return name != NULL && name[0] != '\0' ? name : "none";
}

int GetPlayerEntityIndex(CBasePlayer *pPlayer)
{
    if (pPlayer == NULL || pPlayer->edict() == NULL)
    {
        return -1;
    }

    return ENTINDEX(pPlayer->edict());
}

int GetPlayerUserId(CBasePlayer *pPlayer)
{
    if (pPlayer == NULL || pPlayer->edict() == NULL)
    {
        return -1;
    }

    return GETPLAYERUSERID(pPlayer->edict());
}

void FormatVector3(char *buffer, size_t bufferSize, const Vector &value)
{
    _snprintf_s(buffer, bufferSize, _TRUNCATE, "%.1f %.1f %.1f", value.x, value.y, value.z);
}

void PrintLabDummyConsoleLine(const char *format, ...)
{
    char line[512];
    va_list args;
    va_start(args, format);
    _vsnprintf_s(line, sizeof(line), _TRUNCATE, format, args);
    va_end(args);

    ALERT(at_console, "[hl-server] %s\n", line);
}

const char *GetLabDummyProfileNames()
{
    return "unarmored, vest, vest_headprotected";
}

const LabDummyProfileDefinition *FindBuiltInLabDummyProfile(const char *profileName)
{
    if (profileName == NULL || profileName[0] == '\0')
    {
        return NULL;
    }

    if (StringEqualsIgnoreCase(profileName, "default"))
    {
        profileName = "unarmored";
    }

    for (int index = 0; index < ARRAYSIZE(kBuiltInLabDummyProfiles); ++index)
    {
        if (StringEqualsIgnoreCase(profileName, kBuiltInLabDummyProfiles[index].name))
        {
            return &kBuiltInLabDummyProfiles[index];
        }
    }

    return NULL;
}

bool ApplyBuiltInLabDummyProfile(const char *profileName)
{
    const LabDummyProfileDefinition *pProfile = FindBuiltInLabDummyProfile(profileName);
    if (pProfile == NULL)
    {
        return false;
    }

    CVAR_SET_STRING("sv_exp_glock_lab_target_profile_name", (char *)pProfile->name);
    CVAR_SET_STRING("sv_exp_glock_lab_dummy_armor", (char *)pProfile->armor);
    CVAR_SET_STRING("sv_exp_glock_lab_dummy_head_protected", (char *)pProfile->headProtected);
    CVAR_SET_STRING("sv_exp_glock_lab_dummy_armor_health_fraction", (char *)pProfile->armorHealthFraction);
    CVAR_SET_STRING("sv_exp_glock_lab_dummy_armor_drain_scale", (char *)pProfile->armorDrainScale);
    return true;
}

const char *ResolveGlockLabDummyModel()
{
    const char *requestedModel = GetNonEmptyCvarString(sv_exp_glock_lab_dummy_model, kDefaultGlockLabDummyModel);

    for (int index = 0; index < ARRAYSIZE(kAllowedGlockLabDummyModels); ++index)
    {
        if (StringEqualsIgnoreCase(requestedModel, kAllowedGlockLabDummyModels[index]))
        {
            return kAllowedGlockLabDummyModels[index];
        }
    }

    return kDefaultGlockLabDummyModel;
}

void ResetGlockLabDummyState()
{
    g_glockLabDummy = NULL;
    g_glockLabDummyAnchorPlayer = NULL;
    g_glockLabDummySpawnOrigin = g_vecZero;
    g_glockLabDummySpawnAngles = g_vecZero;
    g_glockLabDummyHasSpawnTransform = false;
    g_glockLabDummyRespawnPending = false;
    g_glockLabDummyRespawnTime = 0.0f;
    g_glockLabDummyRetryTime = 0.0f;
}

void RefreshFutureHooksMapState()
{
    const char *currentMapName = GetCurrentMapName();
    if (StringEqualsIgnoreCase(g_futureHooksMapName, currentMapName))
    {
        return;
    }

    strncpy_s(g_futureHooksMapName, sizeof(g_futureHooksMapName), currentMapName, _TRUNCATE);
    ResetGlockLabDummyState();
}

bool IsLabDummyClassname(const char *classname)
{
    return classname != NULL && classname[0] != '\0' && FStrEq(classname, kGlockLabDummyClassname);
}

bool IsLabDummyEntityInternal(CBaseEntity *pEntity)
{
    return pEntity != NULL &&
        pEntity->pev != NULL &&
        (pEntity->pev->flags & FL_KILLME) == 0 &&
        pEntity->pev->classname != 0 &&
        IsLabDummyClassname(STRING(pEntity->pev->classname));
}

CBasePlayer *FindFirstLivePlayer()
{
    if (gpGlobals == NULL)
    {
        return NULL;
    }

    for (int playerIndex = 1; playerIndex <= gpGlobals->maxClients; ++playerIndex)
    {
        CBaseEntity *pEntity = UTIL_PlayerByIndex(playerIndex);
        if (pEntity == NULL || !pEntity->IsPlayer() || pEntity->pev == NULL)
        {
            continue;
        }

        CBasePlayer *pPlayer = (CBasePlayer *)pEntity;
        if (pPlayer->IsAlive() && pPlayer->pev->deadflag == DEAD_NO)
        {
            return pPlayer;
        }
    }

    return NULL;
}

CBasePlayer *GetTrackedLabDummyAnchorPlayer(CBasePlayer *fallbackPlayer)
{
    CBaseEntity *pTrackedEntity = (CBaseEntity *)g_glockLabDummyAnchorPlayer;
    if (pTrackedEntity != NULL && pTrackedEntity->IsPlayer() && pTrackedEntity->pev != NULL)
    {
        CBasePlayer *pTrackedPlayer = (CBasePlayer *)pTrackedEntity;
        if (pTrackedPlayer->IsAlive())
        {
            return pTrackedPlayer;
        }
    }

    return fallbackPlayer;
}

CBasePlayer *FindCurrentLiveLabDummyAnchorPlayer()
{
    return GetTrackedLabDummyAnchorPlayer(FindFirstLivePlayer());
}

void RememberLabDummySpawnTransform(CBaseEntity *pDummy)
{
    if (pDummy == NULL || pDummy->pev == NULL)
    {
        return;
    }

    g_glockLabDummySpawnOrigin = pDummy->pev->origin;
    g_glockLabDummySpawnAngles = pDummy->pev->angles;
    g_glockLabDummyHasSpawnTransform = true;
}

CBaseEntity *FindExistingLabDummyEntity()
{
    return UTIL_FindEntityByClassname(NULL, kGlockLabDummyClassname);
}

void RemoveLabDummyEntities(const char *reason)
{
    CBaseEntity *pDummy = UTIL_FindEntityByClassname(NULL, kGlockLabDummyClassname);
    while (pDummy != NULL)
    {
        CBaseEntity *pNext = UTIL_FindEntityByClassname(pDummy, kGlockLabDummyClassname);
        LogGlockLabDummyClear(pDummy, reason);
        UTIL_Remove(pDummy);
        pDummy = pNext;
    }
}

void RemoveExtraLabDummyEntities(CBaseEntity *pDummyToKeep, const char *reason)
{
    CBaseEntity *pDummy = UTIL_FindEntityByClassname(NULL, kGlockLabDummyClassname);
    while (pDummy != NULL)
    {
        CBaseEntity *pNext = UTIL_FindEntityByClassname(pDummy, kGlockLabDummyClassname);
        if (pDummy != pDummyToKeep)
        {
            LogGlockLabDummyClear(pDummy, reason);
            UTIL_Remove(pDummy);
        }

        pDummy = pNext;
    }
}

CBaseEntity *GetTrackedLabDummyEntity()
{
    CBaseEntity *pDummy = (CBaseEntity *)g_glockLabDummy;
    if (IsLabDummyEntityInternal(pDummy))
    {
        RemoveExtraLabDummyEntities(pDummy, "dedupe");
        return pDummy;
    }

    pDummy = FindExistingLabDummyEntity();
    if (IsLabDummyEntityInternal(pDummy))
    {
        RemoveExtraLabDummyEntities(pDummy, "dedupe");
        g_glockLabDummy = pDummy;
        RememberLabDummySpawnTransform(pDummy);
        return pDummy;
    }

    g_glockLabDummy = NULL;
    return NULL;
}

void ClearAllLabDummyEntities(const char *reason)
{
    RemoveLabDummyEntities(reason);
    ResetGlockLabDummyState();
}

bool TryBuildLabDummySpawnTransformAtDistance(CBasePlayer *pPlayer, float spawnDistance, Vector *pOrigin, Vector *pAngles, char *failureReason, size_t failureReasonSize)
{
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        strcpy_s(failureReason, failureReasonSize, "no live player is available");
        return false;
    }

    Vector referenceAngles = pPlayer->pev->v_angle;
    referenceAngles.x = 0.0f;
    referenceAngles.z = 0.0f;
    UTIL_MakeVectors(referenceAngles);

    Vector desiredOrigin = pPlayer->pev->origin +
        (gpGlobals->v_forward * spawnDistance) +
        (gpGlobals->v_right * ExpGlockLabDummyOffsetRight());
    desiredOrigin.z += ExpGlockLabDummyOffsetUp();

    TraceResult groundTrace;
    UTIL_TraceLine(
        desiredOrigin + Vector(0.0f, 0.0f, 64.0f),
        desiredOrigin - Vector(0.0f, 0.0f, 1024.0f),
        ignore_monsters,
        pPlayer->edict(),
        &groundTrace);

    if (groundTrace.fStartSolid || groundTrace.fAllSolid)
    {
        strcpy_s(failureReason, failureReasonSize, "ground trace started inside solid space");
        return false;
    }

    if (groundTrace.flFraction == 1.0f)
    {
        strcpy_s(failureReason, failureReasonSize, "no floor was found near the requested test position");
        return false;
    }

    Vector spawnOrigin = groundTrace.vecEndPos + Vector(0.0f, 0.0f, 1.0f);

    TraceResult frontTrace;
    UTIL_TraceLine(
        pPlayer->pev->origin + pPlayer->pev->view_ofs,
        spawnOrigin + Vector(0.0f, 0.0f, 36.0f),
        ignore_monsters,
        pPlayer->edict(),
        &frontTrace);

    if (frontTrace.fStartSolid || frontTrace.fAllSolid)
    {
        strcpy_s(failureReason, failureReasonSize, "the path from the player to the target spot starts inside solid space");
        return false;
    }

    if (frontTrace.flFraction < 1.0f)
    {
        strcpy_s(failureReason, failureReasonSize, "the path from the player to the target spot is blocked");
        return false;
    }

    TraceResult hullTrace;
    UTIL_TraceHull(spawnOrigin, spawnOrigin, dont_ignore_monsters, human_hull, pPlayer->edict(), &hullTrace);
    if (hullTrace.fStartSolid || hullTrace.fAllSolid)
    {
        strcpy_s(failureReason, failureReasonSize, "the dummy's standing hull is blocked");
        return false;
    }

    Vector spawnAngles = referenceAngles;
    if (ExpGlockLabDummyFacePlayer())
    {
        Vector toPlayer = pPlayer->pev->origin - spawnOrigin;
        toPlayer.z = 0.0f;

        if (toPlayer.Length2D() > 1.0f)
        {
            spawnAngles = UTIL_VecToAngles(toPlayer);
        }
    }

    spawnAngles.x = 0.0f;
    spawnAngles.z = 0.0f;

    *pOrigin = spawnOrigin;
    *pAngles = spawnAngles;
    return true;
}

bool TryBuildLabDummySpawnTransform(CBasePlayer *pPlayer, Vector *pOrigin, Vector *pAngles, char *failureReason, size_t failureReasonSize)
{
    const float requestedDistance = ExpGlockLabDummySpawnDistance();
    const float minimumDistance = requestedDistance > kGlockLabDummyPlacementMinDistance ? kGlockLabDummyPlacementMinDistance : requestedDistance;
    char lastFailureReason[128] = "no valid standing target position was found";

    float candidateDistance = requestedDistance;
    while (candidateDistance > minimumDistance + 0.1f)
    {
        if (TryBuildLabDummySpawnTransformAtDistance(pPlayer, candidateDistance, pOrigin, pAngles, lastFailureReason, sizeof(lastFailureReason)))
        {
            return true;
        }

        candidateDistance -= kGlockLabDummyPlacementSearchStep;
    }

    if (TryBuildLabDummySpawnTransformAtDistance(pPlayer, minimumDistance, pOrigin, pAngles, lastFailureReason, sizeof(lastFailureReason)))
    {
        return true;
    }

    if (requestedDistance > minimumDistance + 0.1f)
    {
        _snprintf_s(
            failureReason,
            failureReasonSize,
            _TRUNCATE,
            "no valid standing target position was found between %.1f and %.1f units: %s",
            requestedDistance,
            minimumDistance,
            lastFailureReason);
    }
    else
    {
        strcpy_s(failureReason, failureReasonSize, lastFailureReason);
    }

    return false;
}

bool TryRememberLabDummySpawnTransformFromPlayer(CBasePlayer *pPlayer, char *failureReason, size_t failureReasonSize)
{
    Vector spawnOrigin = g_vecZero;
    Vector spawnAngles = g_vecZero;
    if (!TryBuildLabDummySpawnTransform(pPlayer, &spawnOrigin, &spawnAngles, failureReason, failureReasonSize))
    {
        return false;
    }

    g_glockLabDummySpawnOrigin = spawnOrigin;
    g_glockLabDummySpawnAngles = spawnAngles;
    g_glockLabDummyHasSpawnTransform = true;
    g_glockLabDummyAnchorPlayer = pPlayer;
    return true;
}

void LogLabDummySpawnFailure(const char *reason)
{
    ALERT(at_console, "[hl-server] target dummy spawn failed: %s\n", reason);
}

void StabilizeGlockLabDummyEntity(CBaseEntity *pDummy)
{
    if (pDummy == NULL || pDummy->pev == NULL)
    {
        return;
    }

    if (g_glockLabDummyHasSpawnTransform)
    {
        const Vector delta = pDummy->pev->origin - g_glockLabDummySpawnOrigin;
        if (delta.Length2D() > 1.0f || fabs(delta.z) > 1.0f)
        {
            UTIL_SetOrigin(pDummy->pev, g_glockLabDummySpawnOrigin);
        }

        pDummy->pev->angles = g_glockLabDummySpawnAngles;
    }

    pDummy->pev->ideal_yaw = pDummy->pev->angles.y;
    pDummy->pev->yaw_speed = 0;
    pDummy->pev->velocity = g_vecZero;
    pDummy->pev->avelocity = g_vecZero;
    pDummy->pev->framerate = 0.0f;
    pDummy->SetThink(NULL);
    pDummy->pev->nextthink = 0.0f;
}

void MoveGlockLabDummyToRememberedTransform(CBaseEntity *pDummy, CBasePlayer *pAnchorPlayer, const char *reason)
{
    if (pDummy == NULL || pDummy->pev == NULL || !g_glockLabDummyHasSpawnTransform)
    {
        return;
    }

    UTIL_SetOrigin(pDummy->pev, g_glockLabDummySpawnOrigin);
    pDummy->pev->angles = g_glockLabDummySpawnAngles;
    g_glockLabDummyAnchorPlayer = pAnchorPlayer;
    StabilizeGlockLabDummyEntity(pDummy);
    RememberLabDummySpawnTransform(pDummy);
    LogGlockLabDummyReposition(pDummy, pAnchorPlayer, g_glockLabDummySpawnOrigin, g_glockLabDummySpawnAngles, reason);
}

CBaseEntity *SpawnGlockLabDummy(CBasePlayer *pAnchorPlayer, bool useRememberedTransform, bool logAsRespawn)
{
    Vector spawnOrigin = g_vecZero;
    Vector spawnAngles = g_vecZero;

    if (useRememberedTransform && g_glockLabDummyHasSpawnTransform)
    {
        spawnOrigin = g_glockLabDummySpawnOrigin;
        spawnAngles = g_glockLabDummySpawnAngles;
    }
    else
    {
        char failureReason[128];
        if (!TryRememberLabDummySpawnTransformFromPlayer(pAnchorPlayer, failureReason, sizeof(failureReason)))
        {
            LogLabDummySpawnFailure(failureReason);
            return NULL;
        }

        spawnOrigin = g_glockLabDummySpawnOrigin;
        spawnAngles = g_glockLabDummySpawnAngles;
    }

    edict_t *pent = CREATE_NAMED_ENTITY(MAKE_STRING("monster_generic"));
    if (FNullEnt(pent))
    {
        LogLabDummySpawnFailure("the engine could not allocate a monster_generic entity");
        return NULL;
    }

    entvars_t *pevDummy = VARS(pent);
    const float dummyHealth = ExpGlockLabDummyHealth();
    const float dummyArmor = ExpGlockLabDummyArmor();

    pevDummy->origin = spawnOrigin;
    pevDummy->angles = spawnAngles;
    pevDummy->model = ALLOC_STRING(ExpGlockLabDummyModel());
    pevDummy->health = dummyHealth;
    pevDummy->max_health = dummyHealth;
    pevDummy->armorvalue = dummyArmor;
    pevDummy->netname = ALLOC_STRING(kGlockLabDummyDisplayName);
    SetBits(pevDummy->spawnflags, SF_MONSTER_GAG | SF_MONSTER_PRISONER);

    DispatchSpawn(pent);

    CBaseEntity *pDummy = CBaseEntity::Instance(pent);
    if (pDummy == NULL || pDummy->pev == NULL || (pDummy->pev->flags & FL_KILLME))
    {
        LogLabDummySpawnFailure("the dummy entity failed to finish spawning");
        return NULL;
    }

    pDummy->pev->classname = MAKE_STRING("glock_lab_dummy");
    pDummy->pev->targetname = MAKE_STRING("exp_glock_lab_dummy");
    pDummy->pev->netname = ALLOC_STRING(kGlockLabDummyDisplayName);
    pDummy->pev->health = dummyHealth;
    pDummy->pev->max_health = dummyHealth;
    pDummy->pev->armorvalue = dummyArmor;
    StabilizeGlockLabDummyEntity(pDummy);

    g_glockLabDummy = pDummy;
    g_glockLabDummyAnchorPlayer = pAnchorPlayer;
    g_glockLabDummyRespawnPending = false;
    g_glockLabDummyRespawnTime = 0.0f;
    g_glockLabDummyRetryTime = 0.0f;
    RememberLabDummySpawnTransform(pDummy);

    LogGlockLabDummySpawn(pDummy, pAnchorPlayer, logAsRespawn, spawnOrigin, spawnAngles);
    return pDummy;
}

void MaintainGlockLabDummy()
{
    CBaseEntity *pDummy = GetTrackedLabDummyEntity();

    if (!ExpGlockLabDummyEnabled())
    {
        if (pDummy != NULL)
        {
            ClearAllLabDummyEntities("disabled");
        }
        else
        {
            ResetGlockLabDummyState();
        }

        return;
    }

    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();
    if (pAnchorPlayer != NULL)
    {
        g_glockLabDummyAnchorPlayer = pAnchorPlayer;
    }

    if (pDummy != NULL && pDummy->IsAlive())
    {
        StabilizeGlockLabDummyEntity(pDummy);
        g_glockLabDummyRespawnPending = false;
        g_glockLabDummyRespawnTime = 0.0f;
        g_glockLabDummyRetryTime = 0.0f;
        return;
    }

    if (pDummy != NULL && !pDummy->IsAlive())
    {
        if (!ExpGlockLabDummyAutoRespawnEnabled())
        {
            g_glockLabDummyRespawnPending = false;
            g_glockLabDummyRespawnTime = 0.0f;
            return;
        }

        if (!g_glockLabDummyRespawnPending)
        {
            g_glockLabDummyRespawnPending = true;
            g_glockLabDummyRespawnTime = gpGlobals->time + ExpGlockLabDummyRespawnDelaySeconds();
            return;
        }

        if (gpGlobals->time < g_glockLabDummyRespawnTime)
        {
            return;
        }

        LogGlockLabDummyClear(pDummy, "respawn_cycle");
        UTIL_Remove(pDummy);
        g_glockLabDummy = NULL;
        pDummy = NULL;
    }
    else if (gpGlobals->time < g_glockLabDummyRetryTime)
    {
        return;
    }

    if (pAnchorPlayer == NULL && !g_glockLabDummyHasSpawnTransform)
    {
        LogLabDummySpawnFailure("waiting for the first live player anchor");
        g_glockLabDummyRetryTime = gpGlobals->time + kGlockLabDummyRetryDelay;
        return;
    }

    const bool useRememberedTransform = g_glockLabDummyRespawnPending || pAnchorPlayer == NULL;
    if (SpawnGlockLabDummy(pAnchorPlayer, useRememberedTransform, g_glockLabDummyRespawnPending) != NULL)
    {
        return;
    }

    if (g_glockLabDummyRespawnPending)
    {
        g_glockLabDummyRespawnTime = gpGlobals->time + kGlockLabDummyRetryDelay;
    }
    else
    {
        g_glockLabDummyRetryTime = gpGlobals->time + kGlockLabDummyRetryDelay;
    }
}

void SetLabDummyEnabled(bool enabled)
{
    CVAR_SET_FLOAT("sv_exp_glock_lab_dummy", enabled ? 1.0f : 0.0f);
}

void PrintLabDummyStatus()
{
    RefreshFutureHooksMapState();

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();
    char savedOrigin[64];
    char currentOrigin[64];
    strcpy_s(savedOrigin, sizeof(savedOrigin), "n/a");
    strcpy_s(currentOrigin, sizeof(currentOrigin), "n/a");

    PrintLabDummyConsoleLine("target type: server-side standing dummy using stock model assets");
    PrintLabDummyConsoleLine(
        "target enabled=%d profile=%s health=%.1f armor=%.1f head_protected=%d autorespawn=%d respawn_delay=%.2f",
        ExpGlockLabDummyEnabled() ? 1 : 0,
        ExpGlockLabTargetProfileName(),
        ExpGlockLabDummyHealth(),
        ExpGlockLabDummyArmor(),
        ExpGlockLabDummyHeadProtected() ? 1 : 0,
        ExpGlockLabDummyAutoRespawnEnabled() ? 1 : 0,
        ExpGlockLabDummyRespawnDelaySeconds());
    PrintLabDummyConsoleLine(
        "target placement: distance=%.1f right=%.1f up=%.1f face_player=%d",
        ExpGlockLabDummySpawnDistance(),
        ExpGlockLabDummyOffsetRight(),
        ExpGlockLabDummyOffsetUp(),
        ExpGlockLabDummyFacePlayer() ? 1 : 0);

    if (pAnchorPlayer != NULL)
    {
        PrintLabDummyConsoleLine(
            "current live anchor: name=%s entindex=%d userid=%d",
            GetSafePlayerName(pAnchorPlayer),
            GetPlayerEntityIndex(pAnchorPlayer),
            GetPlayerUserId(pAnchorPlayer));
    }
    else
    {
        PrintLabDummyConsoleLine("current live anchor: none");
    }

    if (g_glockLabDummyHasSpawnTransform)
    {
        FormatVector3(savedOrigin, sizeof(savedOrigin), g_glockLabDummySpawnOrigin);
        PrintLabDummyConsoleLine("saved target spot: origin=%s yaw=%.1f", savedOrigin, g_glockLabDummySpawnAngles.y);
    }
    else
    {
        PrintLabDummyConsoleLine("saved target spot: none");
    }

    if (pDummy != NULL && pDummy->pev != NULL)
    {
        const char *dummyName = pDummy->pev->netname != 0 ? STRING(pDummy->pev->netname) : kGlockLabDummyDisplayName;
        const char *dummyClass = pDummy->pev->classname != 0 ? STRING(pDummy->pev->classname) : "unknown";
        const char *dummyModel = pDummy->pev->model != 0 ? STRING(pDummy->pev->model) : "unknown";
        FormatVector3(currentOrigin, sizeof(currentOrigin), pDummy->pev->origin);
        PrintLabDummyConsoleLine(
            "target entity: state=%s name=%s entindex=%d class=%s model=%s health=%.1f/%.1f armor=%.1f origin=%s yaw=%.1f",
            pDummy->IsAlive() ? "alive" : "dead",
            dummyName,
            pDummy->edict() != NULL ? ENTINDEX(pDummy->edict()) : -1,
            dummyClass,
            dummyModel,
            pDummy->pev->health,
            pDummy->pev->max_health,
            pDummy->pev->armorvalue,
            currentOrigin,
            pDummy->pev->angles.y);
    }
    else
    {
        PrintLabDummyConsoleLine("target entity: missing");
    }

    PrintLabDummyConsoleLine("built-in target profiles: %s", GetLabDummyProfileNames());
    PrintLabDummyConsoleLine("commands: exp_target_spawn | exp_target_clear | exp_target_respawn | exp_target_status | exp_target_tp_front | exp_target_profile <name>");
}

void ExpTargetSpawnCommand()
{
    RefreshFutureHooksMapState();
    SetLabDummyEnabled(true);

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    if (pDummy != NULL && pDummy->IsAlive())
    {
        PrintLabDummyConsoleLine("target is already active. Use exp_target_tp_front to move it or exp_target_respawn to recreate it.");
        PrintLabDummyStatus();
        return;
    }

    if (pDummy != NULL)
    {
        RemoveLabDummyEntities("command_spawn_cleanup");
        g_glockLabDummy = NULL;
    }

    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();
    if (pAnchorPlayer != NULL)
    {
        char failureReason[192];
        if (!TryRememberLabDummySpawnTransformFromPlayer(pAnchorPlayer, failureReason, sizeof(failureReason)))
        {
            PrintLabDummyConsoleLine("target spawn failed: %s", failureReason);
            return;
        }
    }
    else if (!g_glockLabDummyHasSpawnTransform)
    {
        PrintLabDummyConsoleLine("target spawn failed: no current live player anchor or saved target position is available yet.");
        return;
    }

    g_glockLabDummyRespawnPending = false;
    g_glockLabDummyRespawnTime = 0.0f;
    g_glockLabDummyRetryTime = 0.0f;

    if (SpawnGlockLabDummy(pAnchorPlayer, true, false) == NULL)
    {
        PrintLabDummyStatus();
        return;
    }

    PrintLabDummyConsoleLine("spawned \"%s\" using profile %s.", kGlockLabDummyDisplayName, ExpGlockLabTargetProfileName());
}

void ExpTargetClearCommand()
{
    RefreshFutureHooksMapState();
    SetLabDummyEnabled(false);

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    if (pDummy == NULL)
    {
        ResetGlockLabDummyState();
        PrintLabDummyConsoleLine("target is already clear. Automatic target spawning is now disabled.");
        return;
    }

    ClearAllLabDummyEntities("command_clear");
    PrintLabDummyConsoleLine("cleared the target and disabled automatic target spawning.");
}

void ExpTargetRespawnCommand()
{
    RefreshFutureHooksMapState();
    SetLabDummyEnabled(true);

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();

    if (pAnchorPlayer != NULL)
    {
        char failureReason[192];
        if (!TryRememberLabDummySpawnTransformFromPlayer(pAnchorPlayer, failureReason, sizeof(failureReason)))
        {
            PrintLabDummyConsoleLine("target respawn failed: %s", failureReason);
            return;
        }
    }
    else if (!g_glockLabDummyHasSpawnTransform)
    {
        PrintLabDummyConsoleLine("target respawn failed: no current live player anchor or saved target position is available yet.");
        return;
    }

    if (pDummy != NULL)
    {
        RemoveLabDummyEntities("command_respawn");
        g_glockLabDummy = NULL;
    }

    g_glockLabDummyRespawnPending = false;
    g_glockLabDummyRespawnTime = 0.0f;
    g_glockLabDummyRetryTime = 0.0f;

    if (SpawnGlockLabDummy(pAnchorPlayer, true, true) == NULL)
    {
        PrintLabDummyStatus();
        return;
    }

    PrintLabDummyConsoleLine("respawned \"%s\" using profile %s.", kGlockLabDummyDisplayName, ExpGlockLabTargetProfileName());
}

void ExpTargetStatusCommand()
{
    PrintLabDummyStatus();
}

void ExpTargetTpFrontCommand()
{
    RefreshFutureHooksMapState();
    SetLabDummyEnabled(true);

    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();
    if (pAnchorPlayer == NULL)
    {
        PrintLabDummyConsoleLine("target move failed: no current live player anchor is available yet.");
        return;
    }

    char failureReason[192];
    if (!TryRememberLabDummySpawnTransformFromPlayer(pAnchorPlayer, failureReason, sizeof(failureReason)))
    {
        PrintLabDummyConsoleLine("target move failed: %s", failureReason);
        return;
    }

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    if (pDummy != NULL && pDummy->IsAlive())
    {
        MoveGlockLabDummyToRememberedTransform(pDummy, pAnchorPlayer, "command_tp_front");
        PrintLabDummyConsoleLine("moved \"%s\" in front of %s.", kGlockLabDummyDisplayName, GetSafePlayerName(pAnchorPlayer));
        return;
    }

    if (pDummy != NULL)
    {
        RemoveLabDummyEntities("command_tp_front");
        g_glockLabDummy = NULL;
    }

    g_glockLabDummyRespawnPending = false;
    g_glockLabDummyRespawnTime = 0.0f;
    g_glockLabDummyRetryTime = 0.0f;

    if (SpawnGlockLabDummy(pAnchorPlayer, true, pDummy != NULL) == NULL)
    {
        PrintLabDummyStatus();
        return;
    }

    PrintLabDummyConsoleLine("spawned \"%s\" in front of %s.", kGlockLabDummyDisplayName, GetSafePlayerName(pAnchorPlayer));
}

void ExpTargetProfileCommand()
{
    RefreshFutureHooksMapState();

    if (CMD_ARGC() < 2)
    {
        PrintLabDummyConsoleLine("usage: exp_target_profile <name>");
        PrintLabDummyConsoleLine("built-in target profiles: %s", GetLabDummyProfileNames());
        return;
    }

    const char *requestedProfile = CMD_ARGV(1);
    const LabDummyProfileDefinition *pProfile = FindBuiltInLabDummyProfile(requestedProfile);
    if (pProfile == NULL)
    {
        PrintLabDummyConsoleLine("unknown target profile \"%s\". Built-in target profiles: %s", requestedProfile, GetLabDummyProfileNames());
        return;
    }

    ApplyBuiltInLabDummyProfile(pProfile->name);
    PrintLabDummyConsoleLine("applied target profile \"%s\" (%s).", pProfile->name, pProfile->description);

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    if (pDummy == NULL && !ExpGlockLabDummyEnabled())
    {
        PrintLabDummyConsoleLine("target profile \"%s\" is stored. Use exp_target_spawn to create the target.", pProfile->name);
        return;
    }

    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();
    if (pAnchorPlayer != NULL)
    {
        char failureReason[192];
        if (!TryRememberLabDummySpawnTransformFromPlayer(pAnchorPlayer, failureReason, sizeof(failureReason)))
        {
            PrintLabDummyConsoleLine("target profile \"%s\" was stored, but the target could not be refreshed: %s", pProfile->name, failureReason);
            return;
        }
    }
    else if (!g_glockLabDummyHasSpawnTransform)
    {
        PrintLabDummyConsoleLine("target profile \"%s\" is stored. A live player anchor is required before the target can be refreshed.", pProfile->name);
        return;
    }

    if (pDummy != NULL)
    {
        RemoveLabDummyEntities("command_profile_change");
        g_glockLabDummy = NULL;
    }

    g_glockLabDummyRespawnPending = false;
    g_glockLabDummyRespawnTime = 0.0f;
    g_glockLabDummyRetryTime = 0.0f;

    if (SpawnGlockLabDummy(pAnchorPlayer, true, pDummy != NULL) == NULL)
    {
        PrintLabDummyConsoleLine("target profile \"%s\" was stored, but the target could not be refreshed immediately.", pProfile->name);
        return;
    }

    PrintLabDummyConsoleLine("refreshed \"%s\" with profile %s.", kGlockLabDummyDisplayName, pProfile->name);
}

void RegisterFutureGameplayCommands()
{
    if (g_futureGameplayCommandsRegistered)
    {
        return;
    }

    g_futureGameplayCommandsRegistered = true;
    if (g_engfuncs.pfnAddServerCommand == NULL)
    {
        ALERT(at_console, "[hl-server] target dummy commands could not be registered because pfnAddServerCommand is unavailable\n");
        return;
    }

    g_engfuncs.pfnAddServerCommand((char *)"exp_target_spawn", ExpTargetSpawnCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_clear", ExpTargetClearCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_respawn", ExpTargetRespawnCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_status", ExpTargetStatusCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_tp_front", ExpTargetTpFrontCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_profile", ExpTargetProfileCommand);
}

void MaintainMp5LabLoadout()
{
    if (!ExpMP5LabLoadoutEnabled())
    {
        return;
    }

    CBasePlayer *pPlayer = FindFirstLivePlayer();
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        return;
    }

    const bool hadMp5 = pPlayer->HasPlayerItemFromID(WEAPON_MP5) != FALSE;
    if (!hadMp5)
    {
        pPlayer->GiveNamedItem("weapon_9mmAR");
    }

    const int ammoIndex = CBasePlayer::GetAmmoIndex("9mm");
    const int targetAmmo = (int)ClampFloat(ExpMP5LabAmmo(), 0.0f, (float)_9MM_MAX_CARRY);
    if (ammoIndex >= 0)
    {
        const int currentAmmo = pPlayer->AmmoInventory(ammoIndex);
        const int ammoToGive = targetAmmo - currentAmmo;
        if (ammoToGive > 0)
        {
            pPlayer->GiveAmmo(ammoToGive, "9mm", _9MM_MAX_CARRY);
        }
    }

    if (!hadMp5 && ExpMP5LabAutoswitch())
    {
        pPlayer->SelectItem("weapon_9mmAR");
    }
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
    CVAR_REGISTER(&sv_exp_weapon_under_test);
    CVAR_REGISTER(&sv_exp_glock_profile_name);
    CVAR_REGISTER(&sv_exp_mp5_profile_name);
    CVAR_REGISTER(&sv_exp_session_tag);
    CVAR_REGISTER(&sv_exp_matrix_name);
    CVAR_REGISTER(&sv_exp_matrix_step);
    CVAR_REGISTER(&sv_exp_glock_primary_base_spread);
    CVAR_REGISTER(&sv_exp_glock_primary_ground_move_penalty);
    CVAR_REGISTER(&sv_exp_glock_primary_air_move_penalty);
    CVAR_REGISTER(&sv_exp_glock_primary_duck_penalty_scale);
    CVAR_REGISTER(&sv_exp_glock_primary_first_shot_speed_threshold);
    CVAR_REGISTER(&sv_exp_glock_primary_max_spread);
    CVAR_REGISTER(&sv_exp_glock_primary_damage);
    CVAR_REGISTER(&sv_exp_glock_primary_headshot_scale);
    CVAR_REGISTER(&sv_exp_glock_primary_headshot_lethal);
    CVAR_REGISTER(&sv_exp_mp5_primary_enabled);
    CVAR_REGISTER(&sv_exp_mp5_primary_base_spread);
    CVAR_REGISTER(&sv_exp_mp5_primary_ground_move_penalty);
    CVAR_REGISTER(&sv_exp_mp5_primary_air_move_penalty);
    CVAR_REGISTER(&sv_exp_mp5_primary_duck_penalty_scale);
    CVAR_REGISTER(&sv_exp_mp5_primary_burst_growth);
    CVAR_REGISTER(&sv_exp_mp5_primary_burst_max_additional_spread);
    CVAR_REGISTER(&sv_exp_mp5_primary_spread_recovery);
    CVAR_REGISTER(&sv_exp_mp5_primary_damage);
    CVAR_REGISTER(&sv_exp_mp5_primary_headshot_scale);
    CVAR_REGISTER(&sv_exp_mp5_primary_headshot_lethal);
    CVAR_REGISTER(&sv_exp_mp5_primary_first_shot_accuracy);
    CVAR_REGISTER(&sv_exp_mp5_primary_first_shot_speed_threshold);
    CVAR_REGISTER(&sv_exp_mp5_primary_max_spread);
    CVAR_REGISTER(&sv_exp_mp5_lab_loadout);
    CVAR_REGISTER(&sv_exp_mp5_lab_ammo);
    CVAR_REGISTER(&sv_exp_mp5_lab_autoswitch);
    CVAR_REGISTER(&sv_exp_debug_weaponlog);
    CVAR_REGISTER(&sv_exp_debug_weaponlog_rejections);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy);
    CVAR_REGISTER(&sv_exp_glock_lab_target_profile_name);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_health);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_armor);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_head_protected);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_armor_health_fraction);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_armor_drain_scale);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_autorespawn);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_respawn_delay);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_spawn_distance);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_model);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_face_player);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_offset_right);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_offset_up);

    RegisterFutureGameplayCommands();

    ALERT(at_console, "[hl-server] future gameplay hooks registered\n");
}

void UpdateFutureGameplayHooksFrame()
{
    EnsureWeaponDebugLogReady();
    RefreshFutureHooksMapState();
    MaintainMp5LabLoadout();
    MaintainGlockLabDummy();
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

const char *ExpWeaponUnderTest()
{
    return GetOptionalCvarString(sv_exp_weapon_under_test);
}

const char *ExpGlockProfileName()
{
    return GetNonEmptyCvarString(sv_exp_glock_profile_name, "default");
}

const char *ExpMP5ProfileName()
{
    return GetNonEmptyCvarString(sv_exp_mp5_profile_name, "default");
}

const char *ExpSessionTag()
{
    return GetOptionalCvarString(sv_exp_session_tag);
}

const char *ExpMatrixName()
{
    return GetOptionalCvarString(sv_exp_matrix_name);
}

const char *ExpMatrixStep()
{
    return GetOptionalCvarString(sv_exp_matrix_step);
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

bool ExpMP5ExperimentalModeEnabled()
{
    return ExpMP5PrimaryEnabled();
}

bool ExpMP5PrimaryEnabled()
{
    return sv_exp_mp5_primary_enabled.value != 0.0f;
}

float ExpMP5PrimaryBaseSpread()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_base_spread);
}

float ExpMP5PrimaryGroundMovePenalty()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_ground_move_penalty);
}

float ExpMP5PrimaryAirMovePenalty()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_air_move_penalty);
}

float ExpMP5PrimaryDuckPenaltyScale()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_duck_penalty_scale);
}

float ExpMP5PrimaryBurstGrowth()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_burst_growth);
}

float ExpMP5PrimaryBurstMaxAdditionalSpread()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_burst_max_additional_spread);
}

float ExpMP5PrimarySpreadRecoverySeconds()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_spread_recovery);
}

float ExpMP5PrimaryDamage()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_damage);
}

float ExpMP5PrimaryHeadshotScale()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_headshot_scale);
}

bool ExpMP5PrimaryHeadshotLethal()
{
    return sv_exp_mp5_primary_headshot_lethal.value != 0.0f;
}

bool ExpMP5PrimaryFirstShotAccuracyEnabled()
{
    return sv_exp_mp5_primary_first_shot_accuracy.value != 0.0f;
}

float ExpMP5PrimaryFirstShotSpeedThreshold()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_first_shot_speed_threshold);
}

float ExpMP5PrimaryMaxSpread()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_max_spread);
}

bool ExpMP5LabLoadoutEnabled()
{
    return sv_exp_mp5_lab_loadout.value != 0.0f;
}

float ExpMP5LabAmmo()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_lab_ammo);
}

bool ExpMP5LabAutoswitch()
{
    return sv_exp_mp5_lab_autoswitch.value != 0.0f;
}

bool ExpGlockLabDummyEnabled()
{
    return sv_exp_glock_lab_dummy.value != 0.0f;
}

const char *ExpGlockLabDummyDisplayName()
{
    return kGlockLabDummyDisplayName;
}

const char *ExpGlockLabTargetProfileName()
{
    return GetNonEmptyCvarString(sv_exp_glock_lab_target_profile_name, kDefaultGlockLabTargetProfileName);
}

float ExpGlockLabDummyHealth()
{
    return GetPositiveOrDefaultCvarValue(sv_exp_glock_lab_dummy_health, kDefaultGlockLabDummyHealth);
}

float ExpGlockLabDummyArmor()
{
    return GetNonNegativeCvarValue(sv_exp_glock_lab_dummy_armor);
}

bool ExpGlockLabDummyHeadProtected()
{
    return sv_exp_glock_lab_dummy_head_protected.value != 0.0f;
}

float ExpGlockLabDummyArmorHealthFraction()
{
    return ClampFloat(sv_exp_glock_lab_dummy_armor_health_fraction.value, 0.0f, 1.0f);
}

float ExpGlockLabDummyArmorDrainScale()
{
    return GetNonNegativeCvarValue(sv_exp_glock_lab_dummy_armor_drain_scale);
}

bool ExpGlockLabDummyAutoRespawnEnabled()
{
    return sv_exp_glock_lab_dummy_autorespawn.value != 0.0f;
}

float ExpGlockLabDummyRespawnDelaySeconds()
{
    return GetNonNegativeCvarValue(sv_exp_glock_lab_dummy_respawn_delay);
}

float ExpGlockLabDummySpawnDistance()
{
    return GetPositiveOrDefaultCvarValue(sv_exp_glock_lab_dummy_spawn_distance, kDefaultGlockLabDummySpawnDistance);
}

const char *ExpGlockLabDummyModel()
{
    return ResolveGlockLabDummyModel();
}

bool ExpGlockLabDummyFacePlayer()
{
    return sv_exp_glock_lab_dummy_face_player.value != 0.0f;
}

float ExpGlockLabDummyOffsetRight()
{
    return sv_exp_glock_lab_dummy_offset_right.value;
}

float ExpGlockLabDummyOffsetUp()
{
    return sv_exp_glock_lab_dummy_offset_up.value;
}

bool IsExpGlockLabDummyEntity(CBaseEntity *pEntity)
{
    return IsLabDummyEntityInternal(pEntity);
}
