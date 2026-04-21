#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "weapons.h"

#include "future_gameplay_hooks.h"
#include "weapon_debug_logger.h"

#include <io.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <windows.h>

namespace
{
const char *kDefaultGlockLabTargetProfileName = "default";
const float kDefaultGlockLabDummyHealth = 100.0f;
const float kDefaultGlockLabDummyRespawnDelay = 1.0f;
const float kDefaultGlockLabDummySpawnDistance = 256.0f;
const float kGlockLabDummyRetryDelay = 1.0f;
const float kGlockLabDummyPlacementSearchStep = 32.0f;
const float kGlockLabDummyPlacementMinDistance = 64.0f;
const float kGlockLabDummyPlacementSideStep = 16.0f;
const float kGlockLabDummyPlacementForwardStep = 16.0f;
const float kGlockLabDummyPlacementVerticalStep = 18.0f;
const float kGlockLabDummyPlacementWideStep = 64.0f;
const float kGlockLabDummyPlacementLongStep = 96.0f;
const char *kDefaultGlockLabDummyModel = "models/barney.mdl";
const char *kGlockLabDummyDisplayName = "Damage Dummy";
const size_t kMaxLiveCfgRequestLength = 512;
const size_t kMaxLiveCfgExecPathLength = 512;
const size_t kMaxLiveCfgPathLength = 1024;
const size_t kMaxLiveCfgFailureLength = 512;
const size_t kMaxLabDummySpotNameLength = 64;
const size_t kMaxLabDummySpotStorageLength = 16;
const size_t kMaxLabDummySpotTimestampLength = 64;
const size_t kMaxLabDummySpotNoteLength = 128;
const size_t kMaxLabDummySpotFileFailureLength = 512;
const size_t kMaxLabDummySpotListLength = 512;
const size_t kMaxLabDummySpotsPerMap = 32;
const size_t kMaxLabDummySpotFileSize = 64 * 1024;
const char *kAllowedGlockLabDummyModels[] = {
    "models/barney.mdl",
    "models/scientist.mdl"};
const char *kGlockLabDummyClassname = "glock_lab_dummy";
const char *kGlockLabDummyTargetname = "exp_glock_lab_dummy";
const char *kLabDummySourceSavedSpot = "saved_spot";
const char *kLabDummySourceCurrentAnchor = "current_anchor";
const char *kLabDummySourceLastGood = "last_known_good";
const char *kDefaultLabDummySpotName = "default";
const char *kLabDummySpotStorageDisk = "disk";
const char *kLabDummySpotStorageSession = "session";
const char *kLabDummyTargetSpotsDirectoryName = "target_spots";

struct LiveCfgState
{
    bool cfgDrivenModeActive;
    bool hasActiveCfg;
    bool hasLastSuccessfulCfg;
    bool lastCommandSucceeded;
    char activeRequestedPath[kMaxLiveCfgRequestLength];
    char activeExecPath[kMaxLiveCfgExecPathLength];
    char activeResolvedPath[kMaxLiveCfgPathLength];
    char lastSuccessfulExecPath[kMaxLiveCfgExecPathLength];
    char lastSuccessfulResolvedPath[kMaxLiveCfgPathLength];
    char lastAction[32];
    char lastAppliedAt[64];
    float lastApplyServerTime;
    char lastFailureAction[32];
    char lastFailure[kMaxLiveCfgFailureLength];
};

struct ResolvedLiveCfgSelection
{
    char requestedPath[kMaxLiveCfgRequestLength];
    char execPath[kMaxLiveCfgExecPathLength];
    char resolvedPath[kMaxLiveCfgPathLength];
};

struct LabDummyProfileDefinition
{
    const char *name;
    const char *armor;
    const char *headProtected;
    const char *armorHealthFraction;
    const char *armorDrainScale;
    const char *description;
};

struct LabDummyTransformMemory
{
    bool valid;
    Vector origin;
    Vector angles;
    char source[32];
    char candidate[64];
};

struct LabDummySavedSpotRecord
{
    bool valid;
    bool loadedFromDisk;
    Vector origin;
    Vector angles;
    char name[kMaxLabDummySpotNameLength];
    char candidate[64];
    char createdAt[kMaxLabDummySpotTimestampLength];
    char updatedAt[kMaxLabDummySpotTimestampLength];
    char note[kMaxLabDummySpotNoteLength];
};

struct LabDummySpawnSelection
{
    Vector origin;
    Vector angles;
    CBasePlayer *anchorPlayer;
    char source[32];
    char candidate[64];
    char spotName[kMaxLabDummySpotNameLength];
    char spotStorage[kMaxLabDummySpotStorageLength];
};

struct LabDummyFailureInfo
{
    char code[64];
    char source[32];
    char candidate[64];
    char reason[512];
    char spotName[kMaxLabDummySpotNameLength];
    char spotStorage[kMaxLabDummySpotStorageLength];
};

struct LabDummyPlacementCandidate
{
    const char *label;
    float forwardOffset;
    float rightOffset;
    float upOffset;
};

const LabDummyProfileDefinition kBuiltInLabDummyProfiles[] = {
    {"unarmored", "0", "0", "0.5", "1.0", "baseline unarmored target"},
    {"vest", "100", "0", "0.5", "1.0", "torso-armored target"},
    {"vest_headprotected", "100", "1", "0.5", "1.0", "armored target with protected head"}};

const LabDummyPlacementCandidate kLabDummyPlacementCandidates[] = {
    {"center", 0.0f, 0.0f, 0.0f},
    {"right_16", 0.0f, kGlockLabDummyPlacementSideStep, 0.0f},
    {"left_16", 0.0f, -kGlockLabDummyPlacementSideStep, 0.0f},
    {"forward_16", kGlockLabDummyPlacementForwardStep, 0.0f, 0.0f},
    {"back_16", -kGlockLabDummyPlacementForwardStep, 0.0f, 0.0f},
    {"up_18", 0.0f, 0.0f, kGlockLabDummyPlacementVerticalStep},
    {"down_18", 0.0f, 0.0f, -kGlockLabDummyPlacementVerticalStep},
    {"right_32", 0.0f, kGlockLabDummyPlacementSideStep * 2.0f, 0.0f},
    {"left_32", 0.0f, -kGlockLabDummyPlacementSideStep * 2.0f, 0.0f},
    {"forward_32", kGlockLabDummyPlacementForwardStep * 2.0f, 0.0f, 0.0f},
    {"back_32", -kGlockLabDummyPlacementForwardStep * 2.0f, 0.0f, 0.0f},
    {"right_16_up_18", 0.0f, kGlockLabDummyPlacementSideStep, kGlockLabDummyPlacementVerticalStep},
    {"left_16_up_18", 0.0f, -kGlockLabDummyPlacementSideStep, kGlockLabDummyPlacementVerticalStep},
    {"forward_16_up_18", kGlockLabDummyPlacementForwardStep, 0.0f, kGlockLabDummyPlacementVerticalStep},
    {"back_16_up_18", -kGlockLabDummyPlacementForwardStep, 0.0f, kGlockLabDummyPlacementVerticalStep},
    {"right_64", 0.0f, kGlockLabDummyPlacementWideStep, 0.0f},
    {"left_64", 0.0f, -kGlockLabDummyPlacementWideStep, 0.0f},
    {"forward_64", kGlockLabDummyPlacementWideStep, 0.0f, 0.0f},
    {"back_64", -kGlockLabDummyPlacementWideStep, 0.0f, 0.0f},
    {"right_96", 0.0f, kGlockLabDummyPlacementLongStep, 0.0f},
    {"left_96", 0.0f, -kGlockLabDummyPlacementLongStep, 0.0f},
    {"forward_96", kGlockLabDummyPlacementLongStep, 0.0f, 0.0f},
    {"back_96", -kGlockLabDummyPlacementLongStep, 0.0f, 0.0f},
    {"right_64_back_64", -kGlockLabDummyPlacementWideStep, kGlockLabDummyPlacementWideStep, 0.0f},
    {"left_64_back_64", -kGlockLabDummyPlacementWideStep, -kGlockLabDummyPlacementWideStep, 0.0f},
    {"right_64_forward_64", kGlockLabDummyPlacementWideStep, kGlockLabDummyPlacementWideStep, 0.0f},
    {"left_64_forward_64", kGlockLabDummyPlacementWideStep, -kGlockLabDummyPlacementWideStep, 0.0f},
    {"right_96_up_18", 0.0f, kGlockLabDummyPlacementLongStep, kGlockLabDummyPlacementVerticalStep},
    {"left_96_up_18", 0.0f, -kGlockLabDummyPlacementLongStep, kGlockLabDummyPlacementVerticalStep},
    {"back_64_up_18", -kGlockLabDummyPlacementWideStep, 0.0f, kGlockLabDummyPlacementVerticalStep},
    {"back_96_up_18", -kGlockLabDummyPlacementLongStep, 0.0f, kGlockLabDummyPlacementVerticalStep}};

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
LabDummySavedSpotRecord g_glockLabDummySavedSpots[kMaxLabDummySpotsPerMap] = {};
int g_glockLabDummySavedSpotCount = 0;
char g_glockLabDummyActiveSpotName[kMaxLabDummySpotNameLength] = "";
char g_glockLabDummyTargetSpotsPath[kMaxLiveCfgPathLength] = "";
char g_glockLabDummyTargetSpotsLoadFailure[kMaxLabDummySpotFileFailureLength] = "";
LabDummyTransformMemory g_glockLabDummyLastGoodTransform = {};
LabDummyFailureInfo g_glockLabDummyLastSpawnFailure = {};
char g_glockLabDummyLastFailureAt[64] = "";
bool g_glockLabDummyRespawnPending = false;
float g_glockLabDummyRespawnTime = 0.0f;
float g_glockLabDummyRetryTime = 0.0f;
char g_futureHooksMapName[64] = "";
LiveCfgState g_liveCfgState = {};

void PrintLabDummyStatus();
bool EnsureLabDummyMonsterSpawningEnabled(LabDummyFailureInfo *failure, const LabDummySpawnSelection *selection);
bool SaveLabDummySpotsForCurrentMap(char *failureReason, size_t failureReasonSize);
void LoadLabDummySpotsForCurrentMap();

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

void ClearLabDummyFailureInfo(LabDummyFailureInfo *failure)
{
    if (failure == NULL)
    {
        return;
    }

    failure->code[0] = '\0';
    failure->source[0] = '\0';
    failure->candidate[0] = '\0';
    failure->reason[0] = '\0';
    failure->spotName[0] = '\0';
    failure->spotStorage[0] = '\0';
}

void SetLabDummyFailureInfo(
    LabDummyFailureInfo *failure,
    const char *code,
    const char *source,
    const char *candidate,
    const char *reason,
    const char *spotName = NULL,
    const char *spotStorage = NULL)
{
    if (failure == NULL)
    {
        return;
    }

    strncpy_s(failure->code, sizeof(failure->code), code != NULL ? code : "", _TRUNCATE);
    strncpy_s(failure->source, sizeof(failure->source), source != NULL ? source : "", _TRUNCATE);
    strncpy_s(failure->candidate, sizeof(failure->candidate), candidate != NULL ? candidate : "", _TRUNCATE);
    strncpy_s(failure->reason, sizeof(failure->reason), reason != NULL ? reason : "", _TRUNCATE);
    strncpy_s(failure->spotName, sizeof(failure->spotName), spotName != NULL ? spotName : "", _TRUNCATE);
    strncpy_s(failure->spotStorage, sizeof(failure->spotStorage), spotStorage != NULL ? spotStorage : "", _TRUNCATE);
}

void ClearLabDummyTransformMemory(LabDummyTransformMemory *memory)
{
    if (memory == NULL)
    {
        return;
    }

    memory->valid = false;
    memory->origin = g_vecZero;
    memory->angles = g_vecZero;
    memory->source[0] = '\0';
    memory->candidate[0] = '\0';
}

void StoreLabDummyTransformMemory(LabDummyTransformMemory *memory, const Vector &origin, const Vector &angles, const char *source, const char *candidate)
{
    if (memory == NULL)
    {
        return;
    }

    memory->valid = true;
    memory->origin = origin;
    memory->angles = angles;
    strncpy_s(memory->source, sizeof(memory->source), source != NULL ? source : "", _TRUNCATE);
    strncpy_s(memory->candidate, sizeof(memory->candidate), candidate != NULL ? candidate : "", _TRUNCATE);
}

void ClearLabDummySavedSpotRecord(LabDummySavedSpotRecord *spot)
{
    if (spot == NULL)
    {
        return;
    }

    spot->valid = false;
    spot->loadedFromDisk = false;
    spot->origin = g_vecZero;
    spot->angles = g_vecZero;
    spot->name[0] = '\0';
    spot->candidate[0] = '\0';
    spot->createdAt[0] = '\0';
    spot->updatedAt[0] = '\0';
    spot->note[0] = '\0';
}

void StoreLabDummySavedSpotRecord(
    LabDummySavedSpotRecord *spot,
    const char *name,
    const Vector &origin,
    const Vector &angles,
    const char *candidate,
    const char *createdAt,
    const char *updatedAt,
    const char *note,
    bool loadedFromDisk)
{
    if (spot == NULL)
    {
        return;
    }

    spot->valid = true;
    spot->loadedFromDisk = loadedFromDisk;
    spot->origin = origin;
    spot->angles = angles;
    strncpy_s(spot->name, sizeof(spot->name), name != NULL ? name : "", _TRUNCATE);
    strncpy_s(spot->candidate, sizeof(spot->candidate), candidate != NULL ? candidate : "", _TRUNCATE);
    strncpy_s(spot->createdAt, sizeof(spot->createdAt), createdAt != NULL ? createdAt : "", _TRUNCATE);
    strncpy_s(spot->updatedAt, sizeof(spot->updatedAt), updatedAt != NULL ? updatedAt : "", _TRUNCATE);
    strncpy_s(spot->note, sizeof(spot->note), note != NULL ? note : "", _TRUNCATE);
}

void ClearAllLabDummySavedSpots()
{
    for (int spotIndex = 0; spotIndex < ARRAYSIZE(g_glockLabDummySavedSpots); ++spotIndex)
    {
        ClearLabDummySavedSpotRecord(&g_glockLabDummySavedSpots[spotIndex]);
    }

    g_glockLabDummySavedSpotCount = 0;
    g_glockLabDummyActiveSpotName[0] = '\0';
    g_glockLabDummyTargetSpotsPath[0] = '\0';
    g_glockLabDummyTargetSpotsLoadFailure[0] = '\0';
}

void AppendLabDummyFailureAttempt(char *buffer, size_t bufferSize, const LabDummyFailureInfo &failure)
{
    if (buffer == NULL || bufferSize == 0 || failure.reason[0] == '\0')
    {
        return;
    }

    if (buffer[0] != '\0')
    {
        strncat_s(buffer, bufferSize, "; ", _TRUNCATE);
    }

    char attempt[768];
    _snprintf_s(
        attempt,
        sizeof(attempt),
        _TRUNCATE,
        "%s[%s%s%s%s%s]: %s",
        failure.source[0] != '\0' ? failure.source : "target",
        failure.code[0] != '\0' ? failure.code : "failed",
        failure.candidate[0] != '\0' ? "/" : "",
        failure.candidate[0] != '\0' ? failure.candidate : "",
        failure.spotName[0] != '\0' ? " spot=" : "",
        failure.spotName[0] != '\0' ? failure.spotName : "",
        failure.reason);
    strncat_s(buffer, bufferSize, attempt, _TRUNCATE);
}

void PrintLabDummyConsoleLine(const char *format, ...)
{
    char line[512];
    va_list args;
    va_start(args, format);
    _vsnprintf_s(line, sizeof(line), _TRUNCATE, format, args);
    va_end(args);

    ALERT(at_console, "[hl-server] %s\n", line);
    LogLiveLabConsoleMessage(line);
}

void FormatFutureGameplayTimestamp(char *buffer, size_t bufferSize)
{
    SYSTEMTIME localTime;
    GetLocalTime(&localTime);

    _snprintf_s(
        buffer,
        bufferSize,
        _TRUNCATE,
        "%04u-%02u-%02uT%02u:%02u:%02u.%03u",
        localTime.wYear,
        localTime.wMonth,
        localTime.wDay,
        localTime.wHour,
        localTime.wMinute,
        localTime.wSecond,
        localTime.wMilliseconds);
}

const char *GetValueOrFallback(const char *value, const char *fallback)
{
    if (value == NULL || value[0] == '\0')
    {
        return fallback;
    }

    return value;
}

void BuildCommandArgumentString(int firstArgIndex, char *buffer, size_t bufferSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return;
    }

    buffer[0] = '\0';
    const int argc = CMD_ARGC();
    for (int argIndex = firstArgIndex; argIndex < argc; ++argIndex)
    {
        if (argIndex > firstArgIndex)
        {
            strncat_s(buffer, bufferSize, " ", _TRUNCATE);
        }

        strncat_s(buffer, bufferSize, CMD_ARGV(argIndex), _TRUNCATE);
    }
}

void TrimCfgRequestString(const char *input, char *buffer, size_t bufferSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return;
    }

    buffer[0] = '\0';
    if (input == NULL)
    {
        return;
    }

    const char *start = input;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n' || *start == '"')
    {
        ++start;
    }

    const char *end = input + strlen(input);
    while (end > start &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n' || end[-1] == '"'))
    {
        --end;
    }

    const size_t length = (size_t)(end - start);
    strncpy_s(buffer, bufferSize, start, length);
}

bool TryResolveLabDummySpotName(
    const char *requestedName,
    char *buffer,
    size_t bufferSize,
    bool useDefaultIfEmpty,
    char *failureReason,
    size_t failureReasonSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return false;
    }

    buffer[0] = '\0';
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    char trimmedName[kMaxLabDummySpotNameLength];
    TrimCfgRequestString(requestedName, trimmedName, sizeof(trimmedName));
    if (trimmedName[0] == '\0' && useDefaultIfEmpty)
    {
        strncpy_s(trimmedName, sizeof(trimmedName), kDefaultLabDummySpotName, _TRUNCATE);
    }

    if (trimmedName[0] == '\0')
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            strcpy_s(failureReason, failureReasonSize, "target spot name cannot be empty");
        }
        return false;
    }

    if (strlen(trimmedName) >= bufferSize)
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "target spot name \"%s\" is too long (max %u characters)",
                trimmedName,
                (unsigned int)(bufferSize - 1));
        }
        return false;
    }

    for (const char *cursor = trimmedName; *cursor != '\0'; ++cursor)
    {
        const unsigned char ch = (unsigned char)(*cursor);
        if (ch < 32 || *cursor == '"' || *cursor == '\\' || *cursor == '/')
        {
            if (failureReason != NULL && failureReasonSize > 0)
            {
                _snprintf_s(
                    failureReason,
                    failureReasonSize,
                    _TRUNCATE,
                    "target spot name \"%s\" contains unsupported characters",
                    trimmedName);
            }
            return false;
        }
    }

    strncpy_s(buffer, bufferSize, trimmedName, _TRUNCATE);
    return true;
}

void NormalizeSlashes(char *path, char separator)
{
    if (path == NULL)
    {
        return;
    }

    for (char *cursor = path; *cursor != '\0'; ++cursor)
    {
        if (*cursor == '\\' || *cursor == '/')
        {
            *cursor = separator;
        }
    }
}

bool HasAnyPathSeparator(const char *path)
{
    return path != NULL && (strchr(path, '\\') != NULL || strchr(path, '/') != NULL);
}

bool IsAbsoluteCfgPath(const char *path)
{
    if (path == NULL || path[0] == '\0')
    {
        return false;
    }

    return (strlen(path) >= 2 && path[1] == ':') ||
        (path[0] == '\\' && path[1] == '\\');
}

bool EndsWithCfgExtension(const char *path)
{
    if (path == NULL)
    {
        return false;
    }

    const size_t pathLength = strlen(path);
    return pathLength >= 4 && _stricmp(path + pathLength - 4, ".cfg") == 0;
}

bool FileExists(const char *path)
{
    return path != NULL && path[0] != '\0' && _access(path, 0) == 0;
}

bool TryGetFullPathString(const char *path, char *buffer, size_t bufferSize)
{
    if (path == NULL || path[0] == '\0' || buffer == NULL || bufferSize == 0)
    {
        return false;
    }

    DWORD length = GetFullPathNameA(path, (DWORD)bufferSize, buffer, NULL);
    return length > 0 && length < bufferSize;
}

bool IsPathWithinRoot(const char *path, const char *root)
{
    if (path == NULL || root == NULL || path[0] == '\0' || root[0] == '\0')
    {
        return false;
    }

    char normalizedPath[kMaxLiveCfgPathLength];
    char normalizedRoot[kMaxLiveCfgPathLength];
    strncpy_s(normalizedPath, sizeof(normalizedPath), path, _TRUNCATE);
    strncpy_s(normalizedRoot, sizeof(normalizedRoot), root, _TRUNCATE);
    NormalizeSlashes(normalizedPath, '\\');
    NormalizeSlashes(normalizedRoot, '\\');

    size_t rootLength = strlen(normalizedRoot);
    if (rootLength == 0)
    {
        return false;
    }

    if (normalizedRoot[rootLength - 1] != '\\')
    {
        strncat_s(normalizedRoot, sizeof(normalizedRoot), "\\", _TRUNCATE);
        rootLength = strlen(normalizedRoot);
    }

    return _strnicmp(normalizedPath, normalizedRoot, rootLength) == 0;
}

bool TryBuildRelativePathFromRoot(const char *path, const char *root, char *buffer, size_t bufferSize)
{
    if (!IsPathWithinRoot(path, root) || buffer == NULL || bufferSize == 0)
    {
        return false;
    }

    char normalizedPath[kMaxLiveCfgPathLength];
    char normalizedRoot[kMaxLiveCfgPathLength];
    strncpy_s(normalizedPath, sizeof(normalizedPath), path, _TRUNCATE);
    strncpy_s(normalizedRoot, sizeof(normalizedRoot), root, _TRUNCATE);
    NormalizeSlashes(normalizedPath, '\\');
    NormalizeSlashes(normalizedRoot, '\\');

    size_t rootLength = strlen(normalizedRoot);
    if (normalizedRoot[rootLength - 1] != '\\')
    {
        strncat_s(normalizedRoot, sizeof(normalizedRoot), "\\", _TRUNCATE);
        rootLength = strlen(normalizedRoot);
    }

    const char *relativePath = normalizedPath + rootLength;
    while (*relativePath == '\\')
    {
        ++relativePath;
    }

    strncpy_s(buffer, bufferSize, relativePath, _TRUNCATE);
    return buffer[0] != '\0';
}

bool TryGetLiveModRootPath(char *buffer, size_t bufferSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return false;
    }

    HMODULE moduleHandle = NULL;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&RegisterFutureGameplayCvars,
            &moduleHandle))
    {
        return false;
    }

    char modulePath[kMaxLiveCfgPathLength];
    DWORD length = GetModuleFileNameA(moduleHandle, modulePath, ARRAYSIZE(modulePath));
    if (length == 0 || length >= ARRAYSIZE(modulePath))
    {
        return false;
    }

    char *fileName = strrchr(modulePath, '\\');
    if (fileName == NULL)
    {
        return false;
    }

    *fileName = '\0';
    char *dllsDirectory = strrchr(modulePath, '\\');
    if (dllsDirectory == NULL || _stricmp(dllsDirectory + 1, "dlls") != 0)
    {
        return false;
    }

    *dllsDirectory = '\0';
    strncpy_s(buffer, bufferSize, modulePath, _TRUNCATE);
    return true;
}

bool IsSafeLabDummyMapFileName(const char *mapName)
{
    if (mapName == NULL || mapName[0] == '\0')
    {
        return false;
    }

    for (const char *cursor = mapName; *cursor != '\0'; ++cursor)
    {
        const unsigned char ch = (unsigned char)(*cursor);
        if (ch < 32 || *cursor == ':' || *cursor == '\\' || *cursor == '/' || *cursor == '"' || *cursor == '*' || *cursor == '?' || *cursor == '<' || *cursor == '>' || *cursor == '|')
        {
            return false;
        }
    }

    return true;
}

bool TryBuildLabDummyTargetSpotsDirectoryPath(char *buffer, size_t bufferSize, char *failureReason, size_t failureReasonSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return false;
    }

    buffer[0] = '\0';
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    char modRoot[kMaxLiveCfgPathLength];
    if (!TryGetLiveModRootPath(modRoot, sizeof(modRoot)))
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            strcpy_s(failureReason, failureReasonSize, "could not resolve the active hlserver_testbed mod root from hl.dll");
        }
        return false;
    }

    _snprintf_s(buffer, bufferSize, _TRUNCATE, "%s\\%s", modRoot, kLabDummyTargetSpotsDirectoryName);
    return buffer[0] != '\0';
}

bool TryBuildCurrentLabDummyTargetSpotsPath(char *buffer, size_t bufferSize, char *failureReason, size_t failureReasonSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return false;
    }

    buffer[0] = '\0';
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    const char *mapName = GetCurrentMapName();
    if (!IsSafeLabDummyMapFileName(mapName))
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "could not resolve a safe target-spot file name for map \"%s\"",
                GetValueOrFallback(mapName, ""));
        }
        return false;
    }

    char directoryPath[kMaxLiveCfgPathLength];
    if (!TryBuildLabDummyTargetSpotsDirectoryPath(directoryPath, sizeof(directoryPath), failureReason, failureReasonSize))
    {
        return false;
    }

    _snprintf_s(buffer, bufferSize, _TRUNCATE, "%s\\%s.json", directoryPath, mapName);
    return buffer[0] != '\0';
}

bool TryEnsureLabDummyDirectoryExists(const char *path, char *failureReason, size_t failureReasonSize)
{
    if (path == NULL || path[0] == '\0')
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            strcpy_s(failureReason, failureReasonSize, "target spots directory path is empty");
        }
        return false;
    }

    if (CreateDirectoryA(path, NULL))
    {
        return true;
    }

    const DWORD error = GetLastError();
    if (error == ERROR_ALREADY_EXISTS)
    {
        return true;
    }

    if (failureReason != NULL && failureReasonSize > 0)
    {
        _snprintf_s(
            failureReason,
            failureReasonSize,
            _TRUNCATE,
            "could not create target spots directory %s (win32=%lu)",
            path,
            (unsigned long)error);
    }
    return false;
}

int FindLabDummySavedSpotIndex(const char *spotName)
{
    if (spotName == NULL || spotName[0] == '\0')
    {
        return -1;
    }

    for (int spotIndex = 0; spotIndex < g_glockLabDummySavedSpotCount; ++spotIndex)
    {
        if (g_glockLabDummySavedSpots[spotIndex].valid && StringEqualsIgnoreCase(g_glockLabDummySavedSpots[spotIndex].name, spotName))
        {
            return spotIndex;
        }
    }

    return -1;
}

LabDummySavedSpotRecord *FindLabDummySavedSpot(const char *spotName)
{
    const int spotIndex = FindLabDummySavedSpotIndex(spotName);
    return spotIndex >= 0 ? &g_glockLabDummySavedSpots[spotIndex] : NULL;
}

const LabDummySavedSpotRecord *FindLabDummySavedSpotConst(const char *spotName)
{
    const int spotIndex = FindLabDummySavedSpotIndex(spotName);
    return spotIndex >= 0 ? &g_glockLabDummySavedSpots[spotIndex] : NULL;
}

const LabDummySavedSpotRecord *GetLabDummyActiveSavedSpot()
{
    return g_glockLabDummyActiveSpotName[0] != '\0' ? FindLabDummySavedSpotConst(g_glockLabDummyActiveSpotName) : NULL;
}

const LabDummySavedSpotRecord *GetLabDummyDefaultSavedSpot()
{
    return FindLabDummySavedSpotConst(kDefaultLabDummySpotName);
}

const LabDummySavedSpotRecord *GetPreferredLabDummySavedSpot(bool *usedDefaultFallback)
{
    if (usedDefaultFallback != NULL)
    {
        *usedDefaultFallback = false;
    }

    const LabDummySavedSpotRecord *activeSpot = GetLabDummyActiveSavedSpot();
    if (activeSpot != NULL)
    {
        return activeSpot;
    }

    if (g_glockLabDummyActiveSpotName[0] != '\0')
    {
        return NULL;
    }

    const LabDummySavedSpotRecord *defaultSpot = GetLabDummyDefaultSavedSpot();
    if (defaultSpot != NULL && usedDefaultFallback != NULL)
    {
        *usedDefaultFallback = true;
    }

    return defaultSpot;
}

const char *GetLabDummySpotStorageLabel(const LabDummySavedSpotRecord *spot)
{
    if (spot == NULL || !spot->valid)
    {
        return "";
    }

    return spot->loadedFromDisk ? kLabDummySpotStorageDisk : kLabDummySpotStorageSession;
}

void CopyLabDummySavedSpotToSelection(const LabDummySavedSpotRecord &spot, CBasePlayer *pAnchorPlayer, LabDummySpawnSelection *selection)
{
    if (selection == NULL)
    {
        return;
    }

    selection->origin = spot.origin;
    selection->angles = spot.angles;
    selection->anchorPlayer = pAnchorPlayer;
    strncpy_s(selection->source, sizeof(selection->source), kLabDummySourceSavedSpot, _TRUNCATE);
    strncpy_s(selection->candidate, sizeof(selection->candidate), spot.candidate[0] != '\0' ? spot.candidate : "marked_spot", _TRUNCATE);
    strncpy_s(selection->spotName, sizeof(selection->spotName), spot.name, _TRUNCATE);
    strncpy_s(selection->spotStorage, sizeof(selection->spotStorage), GetLabDummySpotStorageLabel(&spot), _TRUNCATE);
}

void BuildLabDummySpotNamesSummary(char *buffer, size_t bufferSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return;
    }

    buffer[0] = '\0';
    for (int spotIndex = 0; spotIndex < g_glockLabDummySavedSpotCount; ++spotIndex)
    {
        const LabDummySavedSpotRecord &spot = g_glockLabDummySavedSpots[spotIndex];
        if (!spot.valid)
        {
            continue;
        }

        if (buffer[0] != '\0')
        {
            strncat_s(buffer, bufferSize, ", ", _TRUNCATE);
        }

        strncat_s(buffer, bufferSize, spot.name, _TRUNCATE);
        if (StringEqualsIgnoreCase(g_glockLabDummyActiveSpotName, spot.name))
        {
            strncat_s(buffer, bufferSize, " [active]", _TRUNCATE);
        }
    }

    if (buffer[0] == '\0')
    {
        strcpy_s(buffer, bufferSize, "none");
    }
}

std::string EscapeLabDummyJsonString(const char *value)
{
    std::string escaped;
    const char *source = value != NULL ? value : "";
    for (const char *cursor = source; *cursor != '\0'; ++cursor)
    {
        switch (*cursor)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += *cursor;
            break;
        }
    }

    return escaped;
}

struct LabDummyJsonCursor
{
    const char *text;
    size_t length;
    size_t position;
};

bool SetLabDummyJsonParseFailure(std::string *failureReason, size_t position, const char *message)
{
    if (failureReason != NULL)
    {
        char buffer[256];
        _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, "json parse error at byte %u: %s", (unsigned int)position, GetValueOrFallback(message, "invalid value"));
        *failureReason = buffer;
    }

    return false;
}

void SkipLabDummyJsonWhitespace(LabDummyJsonCursor *cursor)
{
    if (cursor == NULL || cursor->text == NULL)
    {
        return;
    }

    while (cursor->position < cursor->length && isspace((unsigned char)cursor->text[cursor->position]))
    {
        ++cursor->position;
    }
}

bool TryConsumeLabDummyJsonChar(LabDummyJsonCursor *cursor, char expected)
{
    SkipLabDummyJsonWhitespace(cursor);
    if (cursor == NULL || cursor->text == NULL || cursor->position >= cursor->length || cursor->text[cursor->position] != expected)
    {
        return false;
    }

    ++cursor->position;
    return true;
}

int GetLabDummyJsonHexValue(char ch)
{
    if (ch >= '0' && ch <= '9')
    {
        return ch - '0';
    }

    if (ch >= 'a' && ch <= 'f')
    {
        return 10 + (ch - 'a');
    }

    if (ch >= 'A' && ch <= 'F')
    {
        return 10 + (ch - 'A');
    }

    return -1;
}

bool TryParseLabDummyJsonString(LabDummyJsonCursor *cursor, std::string *value, std::string *failureReason)
{
    SkipLabDummyJsonWhitespace(cursor);
    if (cursor == NULL || cursor->text == NULL || cursor->position >= cursor->length || cursor->text[cursor->position] != '"')
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor != NULL ? cursor->position : 0, "expected a JSON string");
    }

    ++cursor->position;
    std::string parsedValue;
    while (cursor->position < cursor->length)
    {
        char ch = cursor->text[cursor->position++];
        if (ch == '"')
        {
            if (value != NULL)
            {
                *value = parsedValue;
            }
            return true;
        }

        if (ch != '\\')
        {
            parsedValue += ch;
            continue;
        }

        if (cursor->position >= cursor->length)
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor->position, "unterminated JSON escape sequence");
        }

        const char escape = cursor->text[cursor->position++];
        switch (escape)
        {
        case '"':
        case '\\':
        case '/':
            parsedValue += escape;
            break;
        case 'b':
            parsedValue += '\b';
            break;
        case 'f':
            parsedValue += '\f';
            break;
        case 'n':
            parsedValue += '\n';
            break;
        case 'r':
            parsedValue += '\r';
            break;
        case 't':
            parsedValue += '\t';
            break;
        case 'u':
        {
            if (cursor->position + 4 > cursor->length)
            {
                return SetLabDummyJsonParseFailure(failureReason, cursor->position, "incomplete \\u escape sequence");
            }

            int codePoint = 0;
            for (int digitIndex = 0; digitIndex < 4; ++digitIndex)
            {
                const int digitValue = GetLabDummyJsonHexValue(cursor->text[cursor->position++]);
                if (digitValue < 0)
                {
                    return SetLabDummyJsonParseFailure(failureReason, cursor->position, "invalid hex digit in \\u escape sequence");
                }

                codePoint = (codePoint << 4) | digitValue;
            }

            parsedValue += codePoint >= 32 && codePoint <= 126 ? (char)codePoint : '?';
            break;
        }
        default:
            return SetLabDummyJsonParseFailure(failureReason, cursor->position, "unsupported JSON escape sequence");
        }
    }

    return SetLabDummyJsonParseFailure(failureReason, cursor->position, "unterminated JSON string");
}

bool TryParseLabDummyJsonNumber(LabDummyJsonCursor *cursor, double *value, std::string *failureReason)
{
    SkipLabDummyJsonWhitespace(cursor);
    if (cursor == NULL || cursor->text == NULL || cursor->position >= cursor->length)
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor != NULL ? cursor->position : 0, "expected a JSON number");
    }

    errno = 0;
    char *endPointer = NULL;
    const char *startPointer = cursor->text + cursor->position;
    const double parsedValue = strtod(startPointer, &endPointer);
    if (endPointer == startPointer)
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor->position, "expected a JSON number");
    }

    if (errno == ERANGE)
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor->position, "numeric value is out of range");
    }

    cursor->position += (size_t)(endPointer - startPointer);
    if (value != NULL)
    {
        *value = parsedValue;
    }
    return true;
}

bool TryParseLabDummyJsonLiteral(LabDummyJsonCursor *cursor, const char *literal, std::string *failureReason)
{
    SkipLabDummyJsonWhitespace(cursor);
    if (cursor == NULL || cursor->text == NULL || literal == NULL)
    {
        return false;
    }

    const size_t literalLength = strlen(literal);
    if (cursor->position + literalLength > cursor->length || strncmp(cursor->text + cursor->position, literal, literalLength) != 0)
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor->position, "unexpected JSON literal");
    }

    cursor->position += literalLength;
    return true;
}

bool TrySkipLabDummyJsonValue(LabDummyJsonCursor *cursor, std::string *failureReason);

bool TrySkipLabDummyJsonObject(LabDummyJsonCursor *cursor, std::string *failureReason)
{
    if (!TryConsumeLabDummyJsonChar(cursor, '{'))
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor != NULL ? cursor->position : 0, "expected '{'");
    }

    SkipLabDummyJsonWhitespace(cursor);
    if (TryConsumeLabDummyJsonChar(cursor, '}'))
    {
        return true;
    }

    while (true)
    {
        std::string key;
        if (!TryParseLabDummyJsonString(cursor, &key, failureReason))
        {
            return false;
        }

        if (!TryConsumeLabDummyJsonChar(cursor, ':'))
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor->position, "expected ':' after object key");
        }

        if (!TrySkipLabDummyJsonValue(cursor, failureReason))
        {
            return false;
        }

        if (TryConsumeLabDummyJsonChar(cursor, '}'))
        {
            return true;
        }

        if (!TryConsumeLabDummyJsonChar(cursor, ','))
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor->position, "expected ',' or '}' in object");
        }
    }
}

bool TrySkipLabDummyJsonArray(LabDummyJsonCursor *cursor, std::string *failureReason)
{
    if (!TryConsumeLabDummyJsonChar(cursor, '['))
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor != NULL ? cursor->position : 0, "expected '['");
    }

    SkipLabDummyJsonWhitespace(cursor);
    if (TryConsumeLabDummyJsonChar(cursor, ']'))
    {
        return true;
    }

    while (true)
    {
        if (!TrySkipLabDummyJsonValue(cursor, failureReason))
        {
            return false;
        }

        if (TryConsumeLabDummyJsonChar(cursor, ']'))
        {
            return true;
        }

        if (!TryConsumeLabDummyJsonChar(cursor, ','))
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor->position, "expected ',' or ']' in array");
        }
    }
}

bool TrySkipLabDummyJsonValue(LabDummyJsonCursor *cursor, std::string *failureReason)
{
    SkipLabDummyJsonWhitespace(cursor);
    if (cursor == NULL || cursor->text == NULL || cursor->position >= cursor->length)
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor != NULL ? cursor->position : 0, "expected a JSON value");
    }

    const char ch = cursor->text[cursor->position];
    if (ch == '"')
    {
        std::string ignored;
        return TryParseLabDummyJsonString(cursor, &ignored, failureReason);
    }

    if (ch == '{')
    {
        return TrySkipLabDummyJsonObject(cursor, failureReason);
    }

    if (ch == '[')
    {
        return TrySkipLabDummyJsonArray(cursor, failureReason);
    }

    if (ch == '-' || (ch >= '0' && ch <= '9'))
    {
        double ignored = 0.0;
        return TryParseLabDummyJsonNumber(cursor, &ignored, failureReason);
    }

    if (ch == 't')
    {
        return TryParseLabDummyJsonLiteral(cursor, "true", failureReason);
    }

    if (ch == 'f')
    {
        return TryParseLabDummyJsonLiteral(cursor, "false", failureReason);
    }

    if (ch == 'n')
    {
        return TryParseLabDummyJsonLiteral(cursor, "null", failureReason);
    }

    return SetLabDummyJsonParseFailure(failureReason, cursor->position, "unexpected JSON token");
}

bool TryParseLabDummyJsonVector3(LabDummyJsonCursor *cursor, Vector *value, std::string *failureReason)
{
    if (!TryConsumeLabDummyJsonChar(cursor, '['))
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor != NULL ? cursor->position : 0, "expected '[' for origin");
    }

    double coordinates[3] = {};
    for (int coordinateIndex = 0; coordinateIndex < 3; ++coordinateIndex)
    {
        if (!TryParseLabDummyJsonNumber(cursor, &coordinates[coordinateIndex], failureReason))
        {
            return false;
        }

        if (coordinateIndex < 2)
        {
            if (!TryConsumeLabDummyJsonChar(cursor, ','))
            {
                return SetLabDummyJsonParseFailure(failureReason, cursor->position, "expected ',' between origin coordinates");
            }
        }
    }

    if (!TryConsumeLabDummyJsonChar(cursor, ']'))
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor->position, "expected ']'");
    }

    if (value != NULL)
    {
        *value = Vector((float)coordinates[0], (float)coordinates[1], (float)coordinates[2]);
    }

    return true;
}

bool TryParseLabDummySpotObject(LabDummyJsonCursor *cursor, LabDummySavedSpotRecord *spot, std::string *failureReason)
{
    if (spot == NULL)
    {
        return false;
    }

    ClearLabDummySavedSpotRecord(spot);
    if (!TryConsumeLabDummyJsonChar(cursor, '{'))
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor != NULL ? cursor->position : 0, "expected '{' for target spot");
    }

    bool hasName = false;
    bool hasOrigin = false;
    bool hasYaw = false;

    SkipLabDummyJsonWhitespace(cursor);
    if (TryConsumeLabDummyJsonChar(cursor, '}'))
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor->position, "target spot object is missing required fields");
    }

    while (true)
    {
        std::string key;
        if (!TryParseLabDummyJsonString(cursor, &key, failureReason))
        {
            return false;
        }

        if (!TryConsumeLabDummyJsonChar(cursor, ':'))
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor->position, "expected ':' after target spot key");
        }

        if (key == "name")
        {
            std::string name;
            if (!TryParseLabDummyJsonString(cursor, &name, failureReason))
            {
                return false;
            }

            strncpy_s(spot->name, sizeof(spot->name), name.c_str(), _TRUNCATE);
            hasName = true;
        }
        else if (key == "origin")
        {
            if (!TryParseLabDummyJsonVector3(cursor, &spot->origin, failureReason))
            {
                return false;
            }

            hasOrigin = true;
        }
        else if (key == "yaw")
        {
            double yawValue = 0.0;
            if (!TryParseLabDummyJsonNumber(cursor, &yawValue, failureReason))
            {
                return false;
            }

            spot->angles = Vector(0.0f, (float)yawValue, 0.0f);
            hasYaw = true;
        }
        else if (key == "candidate")
        {
            std::string candidate;
            if (!TryParseLabDummyJsonString(cursor, &candidate, failureReason))
            {
                return false;
            }

            strncpy_s(spot->candidate, sizeof(spot->candidate), candidate.c_str(), _TRUNCATE);
        }
        else if (key == "created_at")
        {
            std::string createdAt;
            if (!TryParseLabDummyJsonString(cursor, &createdAt, failureReason))
            {
                return false;
            }

            strncpy_s(spot->createdAt, sizeof(spot->createdAt), createdAt.c_str(), _TRUNCATE);
        }
        else if (key == "updated_at")
        {
            std::string updatedAt;
            if (!TryParseLabDummyJsonString(cursor, &updatedAt, failureReason))
            {
                return false;
            }

            strncpy_s(spot->updatedAt, sizeof(spot->updatedAt), updatedAt.c_str(), _TRUNCATE);
        }
        else if (key == "note")
        {
            std::string note;
            if (!TryParseLabDummyJsonString(cursor, &note, failureReason))
            {
                return false;
            }

            strncpy_s(spot->note, sizeof(spot->note), note.c_str(), _TRUNCATE);
        }
        else
        {
            if (!TrySkipLabDummyJsonValue(cursor, failureReason))
            {
                return false;
            }
        }

        if (TryConsumeLabDummyJsonChar(cursor, '}'))
        {
            break;
        }

        if (!TryConsumeLabDummyJsonChar(cursor, ','))
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor->position, "expected ',' or '}' in target spot object");
        }
    }

    if (!hasName || !hasOrigin || !hasYaw)
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor->position, "target spot object is missing name, origin, or yaw");
    }

    spot->valid = true;
    spot->loadedFromDisk = true;
    if (spot->updatedAt[0] == '\0' && spot->createdAt[0] != '\0')
    {
        strncpy_s(spot->updatedAt, sizeof(spot->updatedAt), spot->createdAt, _TRUNCATE);
    }
    if (spot->createdAt[0] == '\0' && spot->updatedAt[0] != '\0')
    {
        strncpy_s(spot->createdAt, sizeof(spot->createdAt), spot->updatedAt, _TRUNCATE);
    }
    return true;
}

bool TryParseLabDummySpotsFileText(const std::string &jsonText, std::vector<LabDummySavedSpotRecord> *spots, std::string *activeSpotName, std::string *failureReason)
{
    if (spots == NULL)
    {
        return false;
    }

    spots->clear();
    if (activeSpotName != NULL)
    {
        activeSpotName->clear();
    }

    LabDummyJsonCursor cursor = {jsonText.c_str(), jsonText.length(), 0};
    if (!TryConsumeLabDummyJsonChar(&cursor, '{'))
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected a JSON object at the root");
    }

    SkipLabDummyJsonWhitespace(&cursor);
    if (TryConsumeLabDummyJsonChar(&cursor, '}'))
    {
        return true;
    }

    while (true)
    {
        std::string key;
        if (!TryParseLabDummyJsonString(&cursor, &key, failureReason))
        {
            return false;
        }

        if (!TryConsumeLabDummyJsonChar(&cursor, ':'))
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected ':' after root key");
        }

        if (key == "active_spot")
        {
            std::string name;
            if (!TryParseLabDummyJsonString(&cursor, &name, failureReason))
            {
                return false;
            }

            if (activeSpotName != NULL)
            {
                *activeSpotName = name;
            }
        }
        else if (key == "spots")
        {
            if (!TryConsumeLabDummyJsonChar(&cursor, '['))
            {
                return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected '[' for spots array");
            }

            SkipLabDummyJsonWhitespace(&cursor);
            if (!TryConsumeLabDummyJsonChar(&cursor, ']'))
            {
                while (true)
                {
                    LabDummySavedSpotRecord spot = {};
                    if (!TryParseLabDummySpotObject(&cursor, &spot, failureReason))
                    {
                        return false;
                    }

                    for (size_t existingIndex = 0; existingIndex < spots->size(); ++existingIndex)
                    {
                        if (StringEqualsIgnoreCase((*spots)[existingIndex].name, spot.name))
                        {
                            char duplicateMessage[256];
                            _snprintf_s(duplicateMessage, sizeof(duplicateMessage), _TRUNCATE, "duplicate target spot name \"%s\"", spot.name);
                            return SetLabDummyJsonParseFailure(failureReason, cursor.position, duplicateMessage);
                        }
                    }

                    spots->push_back(spot);
                    if (TryConsumeLabDummyJsonChar(&cursor, ']'))
                    {
                        break;
                    }

                    if (!TryConsumeLabDummyJsonChar(&cursor, ','))
                    {
                        return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected ',' or ']' in spots array");
                    }
                }
            }
        }
        else
        {
            if (!TrySkipLabDummyJsonValue(&cursor, failureReason))
            {
                return false;
            }
        }

        if (TryConsumeLabDummyJsonChar(&cursor, '}'))
        {
            break;
        }

        if (!TryConsumeLabDummyJsonChar(&cursor, ','))
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected ',' or '}' in root object");
        }
    }

    SkipLabDummyJsonWhitespace(&cursor);
    if (cursor.position != cursor.length)
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor.position, "unexpected trailing content after JSON object");
    }

    return true;
}

bool TryReadLabDummySpotsFile(const char *path, std::string *fileContents, char *failureReason, size_t failureReasonSize)
{
    if (fileContents == NULL)
    {
        return false;
    }

    fileContents->clear();
    if (path == NULL || path[0] == '\0')
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            strcpy_s(failureReason, failureReasonSize, "target spots file path is empty");
        }
        return false;
    }

    FILE *file = NULL;
    if (fopen_s(&file, path, "rb") != 0 || file == NULL)
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(failureReason, failureReasonSize, _TRUNCATE, "could not open target spots file %s", path);
        }
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(failureReason, failureReasonSize, _TRUNCATE, "could not read the size of target spots file %s", path);
        }
        return false;
    }

    const long fileSize = ftell(file);
    if (fileSize < 0)
    {
        fclose(file);
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(failureReason, failureReasonSize, _TRUNCATE, "could not determine the size of target spots file %s", path);
        }
        return false;
    }

    if ((size_t)fileSize > kMaxLabDummySpotFileSize)
    {
        fclose(file);
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "target spots file %s is too large (%ld bytes, max %u)",
                path,
                fileSize,
                (unsigned int)kMaxLabDummySpotFileSize);
        }
        return false;
    }

    if (fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(failureReason, failureReasonSize, _TRUNCATE, "could not rewind target spots file %s", path);
        }
        return false;
    }

    fileContents->assign((size_t)fileSize, '\0');
    const size_t bytesRead = fileSize > 0 ? fread(&(*fileContents)[0], 1, (size_t)fileSize, file) : 0;
    fclose(file);

    if ((size_t)fileSize != bytesRead)
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "could not read target spots file %s (expected %ld bytes, got %u)",
                path,
                fileSize,
                (unsigned int)bytesRead);
        }
        fileContents->clear();
        return false;
    }

    return true;
}

bool SaveLabDummySpotsForCurrentMap(char *failureReason, size_t failureReasonSize)
{
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    char filePath[kMaxLiveCfgPathLength];
    if (!TryBuildCurrentLabDummyTargetSpotsPath(filePath, sizeof(filePath), failureReason, failureReasonSize))
    {
        return false;
    }

    strncpy_s(g_glockLabDummyTargetSpotsPath, sizeof(g_glockLabDummyTargetSpotsPath), filePath, _TRUNCATE);

    if (g_glockLabDummyActiveSpotName[0] != '\0' && FindLabDummySavedSpotIndex(g_glockLabDummyActiveSpotName) < 0)
    {
        g_glockLabDummyActiveSpotName[0] = '\0';
    }

    if (g_glockLabDummySavedSpotCount <= 0)
    {
        if (DeleteFileA(filePath) || GetLastError() == ERROR_FILE_NOT_FOUND)
        {
            return true;
        }

        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "could not remove empty target-spots file %s (win32=%lu)",
                filePath,
                (unsigned long)GetLastError());
        }
        return false;
    }

    char directoryPath[kMaxLiveCfgPathLength];
    if (!TryBuildLabDummyTargetSpotsDirectoryPath(directoryPath, sizeof(directoryPath), failureReason, failureReasonSize))
    {
        return false;
    }

    if (!TryEnsureLabDummyDirectoryExists(directoryPath, failureReason, failureReasonSize))
    {
        return false;
    }

    std::string jsonText;
    jsonText += "{\n";
    jsonText += "  \"map\": \"";
    jsonText += EscapeLabDummyJsonString(GetCurrentMapName());
    jsonText += "\",\n";
    jsonText += "  \"active_spot\": \"";
    jsonText += EscapeLabDummyJsonString(g_glockLabDummyActiveSpotName);
    jsonText += "\",\n";
    jsonText += "  \"spots\": [\n";

    for (int spotIndex = 0; spotIndex < g_glockLabDummySavedSpotCount; ++spotIndex)
    {
        const LabDummySavedSpotRecord &spot = g_glockLabDummySavedSpots[spotIndex];
        if (!spot.valid)
        {
            continue;
        }

        char numberBuffer[96];
        _snprintf_s(
            numberBuffer,
            sizeof(numberBuffer),
            _TRUNCATE,
            "      \"origin\": [%.3f, %.3f, %.3f],\n      \"yaw\": %.3f,\n",
            spot.origin.x,
            spot.origin.y,
            spot.origin.z,
            spot.angles.y);

        jsonText += "    {\n";
        jsonText += "      \"name\": \"";
        jsonText += EscapeLabDummyJsonString(spot.name);
        jsonText += "\",\n";
        jsonText += numberBuffer;
        jsonText += "      \"candidate\": \"";
        jsonText += EscapeLabDummyJsonString(spot.candidate);
        jsonText += "\",\n";
        jsonText += "      \"created_at\": \"";
        jsonText += EscapeLabDummyJsonString(spot.createdAt);
        jsonText += "\",\n";
        jsonText += "      \"updated_at\": \"";
        jsonText += EscapeLabDummyJsonString(spot.updatedAt);
        jsonText += "\"";
        if (spot.note[0] != '\0')
        {
            jsonText += ",\n      \"note\": \"";
            jsonText += EscapeLabDummyJsonString(spot.note);
            jsonText += "\"";
        }
        jsonText += "\n    }";
        if (spotIndex + 1 < g_glockLabDummySavedSpotCount)
        {
            jsonText += ",";
        }
        jsonText += "\n";
    }

    jsonText += "  ]\n";
    jsonText += "}\n";

    char tempPath[kMaxLiveCfgPathLength];
    _snprintf_s(tempPath, sizeof(tempPath), _TRUNCATE, "%s.tmp", filePath);

    FILE *file = NULL;
    if (fopen_s(&file, tempPath, "wb") != 0 || file == NULL)
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(failureReason, failureReasonSize, _TRUNCATE, "could not write temporary target-spots file %s", tempPath);
        }
        return false;
    }

    const size_t bytesWritten = fwrite(jsonText.data(), 1, jsonText.length(), file);
    fclose(file);
    if (bytesWritten != jsonText.length())
    {
        DeleteFileA(tempPath);
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "could not fully write target-spots file %s (expected %u bytes, wrote %u)",
                tempPath,
                (unsigned int)jsonText.length(),
                (unsigned int)bytesWritten);
        }
        return false;
    }

    if (!MoveFileExA(tempPath, filePath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
    {
        const DWORD error = GetLastError();
        DeleteFileA(tempPath);
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "could not finalize target-spots file %s (win32=%lu)",
                filePath,
                (unsigned long)error);
        }
        return false;
    }

    return true;
}

void LoadLabDummySpotsForCurrentMap()
{
    ClearAllLabDummySavedSpots();

    char filePath[kMaxLiveCfgPathLength];
    char failureReason[kMaxLabDummySpotFileFailureLength];
    if (!TryBuildCurrentLabDummyTargetSpotsPath(filePath, sizeof(filePath), failureReason, sizeof(failureReason)))
    {
        strncpy_s(g_glockLabDummyTargetSpotsLoadFailure, sizeof(g_glockLabDummyTargetSpotsLoadFailure), failureReason, _TRUNCATE);
        PrintLabDummyConsoleLine("target spots load failed: %s", failureReason);
        return;
    }

    strncpy_s(g_glockLabDummyTargetSpotsPath, sizeof(g_glockLabDummyTargetSpotsPath), filePath, _TRUNCATE);
    if (!FileExists(filePath))
    {
        return;
    }

    std::string fileContents;
    if (!TryReadLabDummySpotsFile(filePath, &fileContents, failureReason, sizeof(failureReason)))
    {
        strncpy_s(g_glockLabDummyTargetSpotsLoadFailure, sizeof(g_glockLabDummyTargetSpotsLoadFailure), failureReason, _TRUNCATE);
        PrintLabDummyConsoleLine("target spots load failed for map %s: %s", GetCurrentMapName(), failureReason);
        return;
    }

    std::vector<LabDummySavedSpotRecord> loadedSpots;
    std::string activeSpotName;
    std::string parseFailure;
    if (!TryParseLabDummySpotsFileText(fileContents, &loadedSpots, &activeSpotName, &parseFailure))
    {
        strncpy_s(g_glockLabDummyTargetSpotsLoadFailure, sizeof(g_glockLabDummyTargetSpotsLoadFailure), parseFailure.c_str(), _TRUNCATE);
        PrintLabDummyConsoleLine("target spots load failed for map %s: %s", GetCurrentMapName(), parseFailure.c_str());
        return;
    }

    if (loadedSpots.size() > kMaxLabDummySpotsPerMap)
    {
        _snprintf_s(
            g_glockLabDummyTargetSpotsLoadFailure,
            sizeof(g_glockLabDummyTargetSpotsLoadFailure),
            _TRUNCATE,
            "target spots file has %u entries, but only %u are supported",
            (unsigned int)loadedSpots.size(),
            (unsigned int)kMaxLabDummySpotsPerMap);
        PrintLabDummyConsoleLine("target spots load failed for map %s: %s", GetCurrentMapName(), g_glockLabDummyTargetSpotsLoadFailure);
        return;
    }

    for (size_t spotIndex = 0; spotIndex < loadedSpots.size(); ++spotIndex)
    {
        g_glockLabDummySavedSpots[spotIndex] = loadedSpots[spotIndex];
    }

    g_glockLabDummySavedSpotCount = (int)loadedSpots.size();
    g_glockLabDummyTargetSpotsLoadFailure[0] = '\0';

    if (!activeSpotName.empty())
    {
        if (FindLabDummySavedSpotIndex(activeSpotName.c_str()) >= 0)
        {
            strncpy_s(g_glockLabDummyActiveSpotName, sizeof(g_glockLabDummyActiveSpotName), activeSpotName.c_str(), _TRUNCATE);
        }
        else
        {
            _snprintf_s(
                g_glockLabDummyTargetSpotsLoadFailure,
                sizeof(g_glockLabDummyTargetSpotsLoadFailure),
                _TRUNCATE,
                "active target spot \"%s\" was not found in %s; cleared active selection",
                activeSpotName.c_str(),
                filePath);
            PrintLabDummyConsoleLine("target spots load warning: %s", g_glockLabDummyTargetSpotsLoadFailure);
        }
    }

    if (g_glockLabDummySavedSpotCount > 0)
    {
        PrintLabDummyConsoleLine(
            "loaded %d persisted target spot(s) for map %s from %s%s%s",
            g_glockLabDummySavedSpotCount,
            GetCurrentMapName(),
            filePath,
            g_glockLabDummyActiveSpotName[0] != '\0' ? " active=" : "",
            g_glockLabDummyActiveSpotName[0] != '\0' ? g_glockLabDummyActiveSpotName : "");
    }
}

LabDummySavedSpotRecord *UpsertLabDummySavedSpot(
    const char *spotName,
    const Vector &origin,
    const Vector &angles,
    const char *candidate,
    char *failureReason,
    size_t failureReasonSize)
{
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    int spotIndex = FindLabDummySavedSpotIndex(spotName);
    if (spotIndex < 0)
    {
        if (g_glockLabDummySavedSpotCount >= ARRAYSIZE(g_glockLabDummySavedSpots))
        {
            if (failureReason != NULL && failureReasonSize > 0)
            {
                _snprintf_s(
                    failureReason,
                    failureReasonSize,
                    _TRUNCATE,
                    "cannot store target spot \"%s\": this map already has %u saved spots",
                    GetValueOrFallback(spotName, ""),
                    (unsigned int)ARRAYSIZE(g_glockLabDummySavedSpots));
            }
            return NULL;
        }

        spotIndex = g_glockLabDummySavedSpotCount++;
        ClearLabDummySavedSpotRecord(&g_glockLabDummySavedSpots[spotIndex]);
    }

    LabDummySavedSpotRecord *spot = &g_glockLabDummySavedSpots[spotIndex];
    char timestamp[kMaxLabDummySpotTimestampLength];
    FormatFutureGameplayTimestamp(timestamp, sizeof(timestamp));

    char createdAt[kMaxLabDummySpotTimestampLength];
    strncpy_s(createdAt, sizeof(createdAt), spot->createdAt[0] != '\0' ? spot->createdAt : timestamp, _TRUNCATE);
    char note[kMaxLabDummySpotNoteLength];
    strncpy_s(note, sizeof(note), spot->note, _TRUNCATE);

    StoreLabDummySavedSpotRecord(
        spot,
        spotName,
        origin,
        angles,
        candidate,
        createdAt,
        timestamp,
        note,
        false);
    return spot;
}

bool RemoveLabDummySavedSpot(const char *spotName, LabDummySavedSpotRecord *removedSpot, char *failureReason, size_t failureReasonSize)
{
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    const int spotIndex = FindLabDummySavedSpotIndex(spotName);
    if (spotIndex < 0)
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "target spot \"%s\" does not exist for map %s",
                GetValueOrFallback(spotName, ""),
                GetCurrentMapName());
        }
        return false;
    }

    if (removedSpot != NULL)
    {
        *removedSpot = g_glockLabDummySavedSpots[spotIndex];
    }

    for (int index = spotIndex; index + 1 < g_glockLabDummySavedSpotCount; ++index)
    {
        g_glockLabDummySavedSpots[index] = g_glockLabDummySavedSpots[index + 1];
    }

    if (g_glockLabDummySavedSpotCount > 0)
    {
        --g_glockLabDummySavedSpotCount;
        ClearLabDummySavedSpotRecord(&g_glockLabDummySavedSpots[g_glockLabDummySavedSpotCount]);
    }

    if (StringEqualsIgnoreCase(g_glockLabDummyActiveSpotName, spotName))
    {
        g_glockLabDummyActiveSpotName[0] = '\0';
    }

    return true;
}

bool TryGetLabDummySpotCommandName(int firstArgIndex, char *spotName, size_t spotNameSize, bool useDefaultIfEmpty, char *failureReason, size_t failureReasonSize)
{
    char requestedName[kMaxLabDummySpotNameLength];
    BuildCommandArgumentString(firstArgIndex, requestedName, sizeof(requestedName));
    return TryResolveLabDummySpotName(requestedName, spotName, spotNameSize, useDefaultIfEmpty, failureReason, failureReasonSize);
}

bool TryResolveLiveCfgSelection(const char *requestedPath, ResolvedLiveCfgSelection *pSelection, char *failureReason, size_t failureReasonSize)
{
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    if (pSelection == NULL)
    {
        return false;
    }

    memset(pSelection, 0, sizeof(*pSelection));

    char trimmedRequest[kMaxLiveCfgRequestLength];
    TrimCfgRequestString(requestedPath, trimmedRequest, sizeof(trimmedRequest));
    if (trimmedRequest[0] == '\0')
    {
        strcpy_s(failureReason, failureReasonSize, "cfg path cannot be empty");
        return false;
    }

    if (!EndsWithCfgExtension(trimmedRequest))
    {
        _snprintf_s(
            failureReason,
            failureReasonSize,
            _TRUNCATE,
            "cfg request \"%s\" must point to a .cfg file",
            trimmedRequest);
        return false;
    }

    char modRoot[kMaxLiveCfgPathLength];
    if (!TryGetLiveModRootPath(modRoot, sizeof(modRoot)))
    {
        strcpy_s(failureReason, failureReasonSize, "could not resolve the active hlserver_testbed mod root from hl.dll");
        return false;
    }

    if (IsAbsoluteCfgPath(trimmedRequest))
    {
        char fullPath[kMaxLiveCfgPathLength];
        if (!TryGetFullPathString(trimmedRequest, fullPath, sizeof(fullPath)))
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "could not resolve cfg path \"%s\"",
                trimmedRequest);
            return false;
        }

        if (!IsPathWithinRoot(fullPath, modRoot))
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "cfg path \"%s\" is outside the active mod root %s",
                fullPath,
                modRoot);
            return false;
        }

        if (!FileExists(fullPath))
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "cfg file was not found at %s",
                fullPath);
            return false;
        }

        char relativePath[kMaxLiveCfgExecPathLength];
        if (!TryBuildRelativePathFromRoot(fullPath, modRoot, relativePath, sizeof(relativePath)))
        {
            strcpy_s(failureReason, failureReasonSize, "could not convert the cfg path into a mod-relative exec path");
            return false;
        }

        NormalizeSlashes(relativePath, '/');
        strncpy_s(pSelection->requestedPath, sizeof(pSelection->requestedPath), trimmedRequest, _TRUNCATE);
        strncpy_s(pSelection->execPath, sizeof(pSelection->execPath), relativePath, _TRUNCATE);
        strncpy_s(pSelection->resolvedPath, sizeof(pSelection->resolvedPath), fullPath, _TRUNCATE);
        return true;
    }

    char normalizedRequest[kMaxLiveCfgExecPathLength];
    strncpy_s(normalizedRequest, sizeof(normalizedRequest), trimmedRequest, _TRUNCATE);
    NormalizeSlashes(normalizedRequest, '\\');
    while (normalizedRequest[0] == '\\')
    {
        memmove(normalizedRequest, normalizedRequest + 1, strlen(normalizedRequest));
    }

    while (normalizedRequest[0] == '.' && normalizedRequest[1] == '\\')
    {
        memmove(normalizedRequest, normalizedRequest + 2, strlen(normalizedRequest) - 1);
    }

    if (normalizedRequest[0] == '\0')
    {
        strcpy_s(failureReason, failureReasonSize, "cfg path cannot resolve to the mod root itself");
        return false;
    }

    char candidateRelativePaths[2][kMaxLiveCfgExecPathLength] = {};
    char candidateAbsolutePaths[2][kMaxLiveCfgPathLength] = {};
    int candidateCount = 0;

    strncpy_s(candidateRelativePaths[candidateCount++], sizeof(candidateRelativePaths[0]), normalizedRequest, _TRUNCATE);
    if (!HasAnyPathSeparator(normalizedRequest))
    {
        _snprintf_s(candidateRelativePaths[candidateCount++], sizeof(candidateRelativePaths[1]), _TRUNCATE, "cfg_profiles\\%s", normalizedRequest);
    }

    for (int candidateIndex = 0; candidateIndex < candidateCount; ++candidateIndex)
    {
        char combinedPath[kMaxLiveCfgPathLength];
        _snprintf_s(
            combinedPath,
            sizeof(combinedPath),
            _TRUNCATE,
            "%s\\%s",
            modRoot,
            candidateRelativePaths[candidateIndex]);

        if (!TryGetFullPathString(combinedPath, candidateAbsolutePaths[candidateIndex], sizeof(candidateAbsolutePaths[candidateIndex])))
        {
            continue;
        }

        if (!IsPathWithinRoot(candidateAbsolutePaths[candidateIndex], modRoot))
        {
            continue;
        }

        if (!FileExists(candidateAbsolutePaths[candidateIndex]))
        {
            continue;
        }

        char execPath[kMaxLiveCfgExecPathLength];
        strncpy_s(execPath, sizeof(execPath), candidateRelativePaths[candidateIndex], _TRUNCATE);
        NormalizeSlashes(execPath, '/');

        strncpy_s(pSelection->requestedPath, sizeof(pSelection->requestedPath), trimmedRequest, _TRUNCATE);
        strncpy_s(pSelection->execPath, sizeof(pSelection->execPath), execPath, _TRUNCATE);
        strncpy_s(pSelection->resolvedPath, sizeof(pSelection->resolvedPath), candidateAbsolutePaths[candidateIndex], _TRUNCATE);
        return true;
    }

    if (candidateCount == 1)
    {
        _snprintf_s(
            failureReason,
            failureReasonSize,
            _TRUNCATE,
            "cfg file \"%s\" was not found at %s",
            trimmedRequest,
            candidateAbsolutePaths[0][0] != '\0' ? candidateAbsolutePaths[0] : candidateRelativePaths[0]);
    }
    else
    {
        _snprintf_s(
            failureReason,
            failureReasonSize,
            _TRUNCATE,
            "cfg file \"%s\" was not found. Searched %s and %s",
            trimmedRequest,
            candidateAbsolutePaths[0][0] != '\0' ? candidateAbsolutePaths[0] : candidateRelativePaths[0],
            candidateAbsolutePaths[1][0] != '\0' ? candidateAbsolutePaths[1] : candidateRelativePaths[1]);
    }

    return false;
}

void ClearLiveCfgFailureState()
{
    g_liveCfgState.lastFailureAction[0] = '\0';
    g_liveCfgState.lastFailure[0] = '\0';
}

void RecordLiveCfgFailure(const char *action, const char *reason)
{
    g_liveCfgState.lastCommandSucceeded = false;
    strncpy_s(g_liveCfgState.lastFailureAction, sizeof(g_liveCfgState.lastFailureAction), GetValueOrFallback(action, "apply"), _TRUNCATE);
    strncpy_s(g_liveCfgState.lastFailure, sizeof(g_liveCfgState.lastFailure), GetValueOrFallback(reason, "unknown cfg failure"), _TRUNCATE);
}

void RecordLiveCfgSuccess(const char *action, const ResolvedLiveCfgSelection &selection)
{
    g_liveCfgState.cfgDrivenModeActive = true;
    g_liveCfgState.hasActiveCfg = true;
    g_liveCfgState.hasLastSuccessfulCfg = true;
    g_liveCfgState.lastCommandSucceeded = true;
    strncpy_s(g_liveCfgState.activeRequestedPath, sizeof(g_liveCfgState.activeRequestedPath), selection.requestedPath, _TRUNCATE);
    strncpy_s(g_liveCfgState.activeExecPath, sizeof(g_liveCfgState.activeExecPath), selection.execPath, _TRUNCATE);
    strncpy_s(g_liveCfgState.activeResolvedPath, sizeof(g_liveCfgState.activeResolvedPath), selection.resolvedPath, _TRUNCATE);
    strncpy_s(g_liveCfgState.lastSuccessfulExecPath, sizeof(g_liveCfgState.lastSuccessfulExecPath), selection.execPath, _TRUNCATE);
    strncpy_s(g_liveCfgState.lastSuccessfulResolvedPath, sizeof(g_liveCfgState.lastSuccessfulResolvedPath), selection.resolvedPath, _TRUNCATE);
    strncpy_s(g_liveCfgState.lastAction, sizeof(g_liveCfgState.lastAction), GetValueOrFallback(action, "apply"), _TRUNCATE);
    FormatFutureGameplayTimestamp(g_liveCfgState.lastAppliedAt, sizeof(g_liveCfgState.lastAppliedAt));
    g_liveCfgState.lastApplyServerTime = gpGlobals != NULL ? gpGlobals->time : 0.0f;
    ClearLiveCfgFailureState();
}

void PrintCurrentCfgMetadata()
{
    PrintLabDummyConsoleLine(
        "current cfg metadata: weapon=%s session_tag=%s glock_profile=%s mp5_profile=%s target_profile=%s",
        GetValueOrFallback(ExpWeaponUnderTest(), "none"),
        GetValueOrFallback(ExpSessionTag(), "none"),
        GetValueOrFallback(ExpGlockProfileName(), "default"),
        GetValueOrFallback(ExpMP5ProfileName(), "default"),
        GetValueOrFallback(ExpGlockLabTargetProfileName(), "default"));
}

void PrintLiveCfgStatus()
{
    const char *lastResult = "none";
    if (g_liveCfgState.lastAction[0] != '\0')
    {
        lastResult = g_liveCfgState.lastCommandSucceeded ? "success" : "failure";
    }
    else if (g_liveCfgState.lastFailureAction[0] != '\0')
    {
        lastResult = "failure";
    }

    PrintLabDummyConsoleLine(
        "cfg mode: cfg_driven=%d active_cfg_known=%d last_result=%s",
        g_liveCfgState.cfgDrivenModeActive ? 1 : 0,
        g_liveCfgState.hasActiveCfg ? 1 : 0,
        lastResult);

    if (g_liveCfgState.hasActiveCfg)
    {
        PrintLabDummyConsoleLine(
            "active cfg: request=%s exec=%s source=%s file_present=%d",
            GetValueOrFallback(g_liveCfgState.activeRequestedPath, "unknown"),
            GetValueOrFallback(g_liveCfgState.activeExecPath, "unknown"),
            GetValueOrFallback(g_liveCfgState.activeResolvedPath, "unknown"),
            FileExists(g_liveCfgState.activeResolvedPath) ? 1 : 0);
    }
    else
    {
        PrintLabDummyConsoleLine("active cfg: none");
    }

    if (g_liveCfgState.hasLastSuccessfulCfg)
    {
        PrintLabDummyConsoleLine(
            "last successful cfg: exec=%s source=%s",
            GetValueOrFallback(g_liveCfgState.lastSuccessfulExecPath, "unknown"),
            GetValueOrFallback(g_liveCfgState.lastSuccessfulResolvedPath, "unknown"));
        PrintLabDummyConsoleLine(
            "last successful apply: action=%s at=%s map_time=%.2f",
            GetValueOrFallback(g_liveCfgState.lastAction, "unknown"),
            GetValueOrFallback(g_liveCfgState.lastAppliedAt, "unknown"),
            g_liveCfgState.lastApplyServerTime);
    }
    else
    {
        PrintLabDummyConsoleLine("last successful cfg: none");
    }

    if (g_liveCfgState.lastFailureAction[0] != '\0' && g_liveCfgState.lastFailure[0] != '\0')
    {
        PrintLabDummyConsoleLine(
            "last cfg failure: action=%s reason=%s",
            g_liveCfgState.lastFailureAction,
            g_liveCfgState.lastFailure);
    }

    PrintCurrentCfgMetadata();
    PrintLabDummyConsoleLine("cfg commands: exp_cfg_apply <cfg_name_or_path> | exp_cfg_reload | exp_cfg_status | exp_lab_apply <cfg_name_or_path>");
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
    g_glockLabDummyRespawnPending = false;
    g_glockLabDummyRespawnTime = 0.0f;
    g_glockLabDummyRetryTime = 0.0f;
    ClearAllLabDummySavedSpots();
    ClearLabDummyTransformMemory(&g_glockLabDummyLastGoodTransform);
    ClearLabDummyFailureInfo(&g_glockLabDummyLastSpawnFailure);
    g_glockLabDummyLastFailureAt[0] = '\0';
}

void ClearGlockLabDummyRuntimeState()
{
    g_glockLabDummy = NULL;
    g_glockLabDummyAnchorPlayer = NULL;
    g_glockLabDummyRespawnPending = false;
    g_glockLabDummyRespawnTime = 0.0f;
    g_glockLabDummyRetryTime = 0.0f;
}

void ClearGlockLabDummyFailureState()
{
    ClearLabDummyFailureInfo(&g_glockLabDummyLastSpawnFailure);
    g_glockLabDummyLastFailureAt[0] = '\0';
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
    LoadLabDummySpotsForCurrentMap();
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

void RememberLabDummyLastGoodTransform(CBaseEntity *pDummy, const char *source, const char *candidate)
{
    if (pDummy == NULL || pDummy->pev == NULL)
    {
        return;
    }

    StoreLabDummyTransformMemory(&g_glockLabDummyLastGoodTransform, pDummy->pev->origin, pDummy->pev->angles, source, candidate);
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
        RememberLabDummyLastGoodTransform(pDummy, g_glockLabDummyLastGoodTransform.source, g_glockLabDummyLastGoodTransform.candidate);
        return pDummy;
    }

    g_glockLabDummy = NULL;
    return NULL;
}

void ClearAllLabDummyEntities(const char *reason)
{
    RemoveLabDummyEntities(reason);
    ClearGlockLabDummyRuntimeState();
}

edict_t *GetLabDummyPlacementIgnoreEdict(CBasePlayer *pAnchorPlayer)
{
    CBaseEntity *pDummy = (CBaseEntity *)g_glockLabDummy;
    if (IsLabDummyEntityInternal(pDummy) && pDummy->edict() != NULL)
    {
        return pDummy->edict();
    }

    return pAnchorPlayer != NULL ? pAnchorPlayer->edict() : NULL;
}

bool TryValidateLabDummyExactTransform(const Vector &origin, CBasePlayer *pIgnorePlayer, char *failureCode, size_t failureCodeSize, char *failureReason, size_t failureReasonSize)
{
    edict_t *ignoreEdict = GetLabDummyPlacementIgnoreEdict(pIgnorePlayer);

    TraceResult groundTrace;
    UTIL_TraceLine(
        origin + Vector(0.0f, 0.0f, 36.0f),
        origin - Vector(0.0f, 0.0f, 72.0f),
        ignore_monsters,
        ignoreEdict,
        &groundTrace);

    if (groundTrace.fStartSolid || groundTrace.fAllSolid)
    {
        strcpy_s(failureCode, failureCodeSize, "blocked_ground");
        strcpy_s(failureReason, failureReasonSize, "ground trace started inside solid space");
        return false;
    }

    if (groundTrace.flFraction == 1.0f)
    {
        strcpy_s(failureCode, failureCodeSize, "no_floor");
        strcpy_s(failureReason, failureReasonSize, "no floor was found below the target spot");
        return false;
    }

    TraceResult hullTrace;
    UTIL_TraceHull(origin, origin, dont_ignore_monsters, human_hull, ignoreEdict, &hullTrace);
    if (hullTrace.fStartSolid || hullTrace.fAllSolid)
    {
        strcpy_s(failureCode, failureCodeSize, "blocked_hull");
        strcpy_s(failureReason, failureReasonSize, "the dummy's standing hull is blocked");
        return false;
    }

    failureCode[0] = '\0';
    failureReason[0] = '\0';
    return true;
}

bool TryResolveNamedSavedLabDummySelection(
    const LabDummySavedSpotRecord *spot,
    CBasePlayer *pAnchorPlayer,
    LabDummySpawnSelection *selection,
    LabDummyFailureInfo *failure)
{
    if (spot == NULL || !spot->valid)
    {
        SetLabDummyFailureInfo(failure, "saved_spot_missing", kLabDummySourceSavedSpot, "", "the requested saved target spot is missing");
        return false;
    }

    char failureCode[64];
    char failureReason[192];
    if (!TryValidateLabDummyExactTransform(spot->origin, pAnchorPlayer, failureCode, sizeof(failureCode), failureReason, sizeof(failureReason)))
    {
        char details[512];
        _snprintf_s(
            details,
            sizeof(details),
            _TRUNCATE,
            "saved target spot \"%s\" is no longer valid: %s",
            spot->name,
            failureReason);
        SetLabDummyFailureInfo(
            failure,
            "saved_spot_invalid",
            kLabDummySourceSavedSpot,
            spot->candidate[0] != '\0' ? spot->candidate : "marked_spot",
            details,
            spot->name,
            GetLabDummySpotStorageLabel(spot));
        return false;
    }

    CopyLabDummySavedSpotToSelection(*spot, pAnchorPlayer, selection);
    return true;
}

bool TryResolveSavedLabDummySelection(CBasePlayer *pAnchorPlayer, LabDummySpawnSelection *selection, LabDummyFailureInfo *failure)
{
    const LabDummySavedSpotRecord *activeSpot = GetLabDummyActiveSavedSpot();
    if (g_glockLabDummyActiveSpotName[0] != '\0' && activeSpot == NULL)
    {
        char details[512];
        _snprintf_s(
            details,
            sizeof(details),
            _TRUNCATE,
            "active target spot \"%s\" is selected for map %s but is not available",
            g_glockLabDummyActiveSpotName,
            GetCurrentMapName());
        SetLabDummyFailureInfo(
            failure,
            "saved_spot_missing",
            kLabDummySourceSavedSpot,
            "",
            details,
            g_glockLabDummyActiveSpotName,
            "");
        return false;
    }

    const LabDummySavedSpotRecord *spot = GetPreferredLabDummySavedSpot(NULL);
    if (spot == NULL)
    {
        char details[512];
        if (g_glockLabDummySavedSpotCount > 0)
        {
            _snprintf_s(
                details,
                sizeof(details),
                _TRUNCATE,
                "no active target spot is selected for map %s and no \"%s\" fallback spot is available",
                GetCurrentMapName(),
                kDefaultLabDummySpotName);
        }
        else
        {
            strcpy_s(details, sizeof(details), "no saved target spot is marked for this map");
        }

        SetLabDummyFailureInfo(failure, "saved_spot_missing", kLabDummySourceSavedSpot, "", details);
        return false;
    }

    return TryResolveNamedSavedLabDummySelection(spot, pAnchorPlayer, selection, failure);
}

bool TryResolveLastGoodLabDummySelection(CBasePlayer *pAnchorPlayer, LabDummySpawnSelection *selection, LabDummyFailureInfo *failure)
{
    if (!g_glockLabDummyLastGoodTransform.valid)
    {
        SetLabDummyFailureInfo(failure, "last_good_missing", kLabDummySourceLastGood, "", "no previous successful target transform is available yet");
        return false;
    }

    char failureCode[64];
    char failureReason[192];
    if (!TryValidateLabDummyExactTransform(g_glockLabDummyLastGoodTransform.origin, pAnchorPlayer, failureCode, sizeof(failureCode), failureReason, sizeof(failureReason)))
    {
        char details[512];
        _snprintf_s(details, sizeof(details), _TRUNCATE, "last known good target transform is no longer valid: %s", failureReason);
        SetLabDummyFailureInfo(
            failure,
            "last_good_invalid",
            kLabDummySourceLastGood,
            g_glockLabDummyLastGoodTransform.candidate[0] != '\0' ? g_glockLabDummyLastGoodTransform.candidate : "last_good",
            details);
        return false;
    }

    selection->origin = g_glockLabDummyLastGoodTransform.origin;
    selection->angles = g_glockLabDummyLastGoodTransform.angles;
    selection->anchorPlayer = pAnchorPlayer;
    strncpy_s(selection->source, sizeof(selection->source), kLabDummySourceLastGood, _TRUNCATE);
    strncpy_s(selection->candidate, sizeof(selection->candidate), g_glockLabDummyLastGoodTransform.candidate[0] != '\0' ? g_glockLabDummyLastGoodTransform.candidate : "last_good", _TRUNCATE);
    return true;
}

bool TryBuildLabDummySpawnTransformCandidate(
    CBasePlayer *pPlayer,
    float spawnDistance,
    const LabDummyPlacementCandidate &candidate,
    Vector *pOrigin,
    Vector *pAngles,
    char *candidateLabel,
    size_t candidateLabelSize,
    char *failureCode,
    size_t failureCodeSize,
    char *failureReason,
    size_t failureReasonSize)
{
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        strcpy_s(failureCode, failureCodeSize, "missing_anchor");
        strcpy_s(failureReason, failureReasonSize, "no current live player anchor is available yet");
        return false;
    }

    const float candidateDistance = spawnDistance + candidate.forwardOffset;
    const float clampedDistance = candidateDistance >= 48.0f ? candidateDistance : 48.0f;

    Vector referenceAngles = pPlayer->pev->v_angle;
    referenceAngles.x = 0.0f;
    referenceAngles.z = 0.0f;
    UTIL_MakeVectors(referenceAngles);

    Vector desiredOrigin = pPlayer->pev->origin +
        (gpGlobals->v_forward * clampedDistance) +
        (gpGlobals->v_right * (ExpGlockLabDummyOffsetRight() + candidate.rightOffset));
    desiredOrigin.z += ExpGlockLabDummyOffsetUp() + candidate.upOffset;

    _snprintf_s(candidateLabel, candidateLabelSize, _TRUNCATE, "%s@%.1f", candidate.label, clampedDistance);
    edict_t *ignoreEdict = GetLabDummyPlacementIgnoreEdict(pPlayer);

    TraceResult groundTrace;
    UTIL_TraceLine(
        desiredOrigin + Vector(0.0f, 0.0f, 64.0f),
        desiredOrigin - Vector(0.0f, 0.0f, 1024.0f),
        ignore_monsters,
        ignoreEdict,
        &groundTrace);

    if (groundTrace.fStartSolid || groundTrace.fAllSolid)
    {
        strcpy_s(failureCode, failureCodeSize, "blocked_ground");
        strcpy_s(failureReason, failureReasonSize, "ground trace started inside solid space");
        return false;
    }

    if (groundTrace.flFraction == 1.0f)
    {
        strcpy_s(failureCode, failureCodeSize, "no_floor");
        strcpy_s(failureReason, failureReasonSize, "no floor was found near the requested test position");
        return false;
    }

    Vector spawnOrigin = groundTrace.vecEndPos + Vector(0.0f, 0.0f, 37.0f);

    TraceResult frontTrace;
    UTIL_TraceLine(
        pPlayer->pev->origin + pPlayer->pev->view_ofs,
        spawnOrigin + Vector(0.0f, 0.0f, 36.0f),
        ignore_monsters,
        ignoreEdict,
        &frontTrace);

    if (frontTrace.fStartSolid || frontTrace.fAllSolid || frontTrace.flFraction < 1.0f)
    {
        strcpy_s(failureCode, failureCodeSize, "blocked_path");
        strcpy_s(failureReason, failureReasonSize, "the path from the player to the target spot is blocked");
        return false;
    }

    TraceResult hullTrace;
    UTIL_TraceHull(spawnOrigin, spawnOrigin, dont_ignore_monsters, human_hull, ignoreEdict, &hullTrace);
    if (hullTrace.fStartSolid || hullTrace.fAllSolid)
    {
        strcpy_s(failureCode, failureCodeSize, "blocked_hull");
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
    failureCode[0] = '\0';
    failureReason[0] = '\0';
    return true;
}

bool TryBuildLabDummySpawnTransformFromAnchor(CBasePlayer *pPlayer, LabDummySpawnSelection *selection, LabDummyFailureInfo *failure)
{
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        SetLabDummyFailureInfo(failure, "missing_anchor", kLabDummySourceCurrentAnchor, "", "no current live player anchor is available yet");
        return false;
    }

    const float requestedDistance = ExpGlockLabDummySpawnDistance();
    const float minimumDistance = requestedDistance > kGlockLabDummyPlacementMinDistance ? kGlockLabDummyPlacementMinDistance : requestedDistance;
    LabDummyFailureInfo lastCandidateFailure = {};

    for (float candidateDistance = requestedDistance; candidateDistance > minimumDistance + 0.1f; candidateDistance -= kGlockLabDummyPlacementSearchStep)
    {
        for (int candidateIndex = 0; candidateIndex < ARRAYSIZE(kLabDummyPlacementCandidates); ++candidateIndex)
        {
            char candidateLabel[64];
            char failureCode[64];
            char failureReason[192];
            if (TryBuildLabDummySpawnTransformCandidate(
                    pPlayer,
                    candidateDistance,
                    kLabDummyPlacementCandidates[candidateIndex],
                    &selection->origin,
                    &selection->angles,
                    candidateLabel,
                    sizeof(candidateLabel),
                    failureCode,
                    sizeof(failureCode),
                    failureReason,
                    sizeof(failureReason)))
            {
                selection->anchorPlayer = pPlayer;
                strncpy_s(selection->source, sizeof(selection->source), kLabDummySourceCurrentAnchor, _TRUNCATE);
                strncpy_s(selection->candidate, sizeof(selection->candidate), candidateLabel, _TRUNCATE);
                return true;
            }

            SetLabDummyFailureInfo(&lastCandidateFailure, failureCode, kLabDummySourceCurrentAnchor, candidateLabel, failureReason);
        }
    }

    for (int candidateIndex = 0; candidateIndex < ARRAYSIZE(kLabDummyPlacementCandidates); ++candidateIndex)
    {
        char candidateLabel[64];
        char failureCode[64];
        char failureReason[192];
        if (TryBuildLabDummySpawnTransformCandidate(
                pPlayer,
                minimumDistance,
                kLabDummyPlacementCandidates[candidateIndex],
                &selection->origin,
                &selection->angles,
                candidateLabel,
                sizeof(candidateLabel),
                failureCode,
                sizeof(failureCode),
                failureReason,
                sizeof(failureReason)))
        {
            selection->anchorPlayer = pPlayer;
            strncpy_s(selection->source, sizeof(selection->source), kLabDummySourceCurrentAnchor, _TRUNCATE);
            strncpy_s(selection->candidate, sizeof(selection->candidate), candidateLabel, _TRUNCATE);
            return true;
        }

        SetLabDummyFailureInfo(&lastCandidateFailure, failureCode, kLabDummySourceCurrentAnchor, candidateLabel, failureReason);
    }

    char details[512];
    if (requestedDistance > minimumDistance + 0.1f)
    {
        _snprintf_s(
            details,
            sizeof(details),
            _TRUNCATE,
            "no valid standing target position was found between %.1f and %.1f units; last candidate %s failed with %s: %s",
            requestedDistance,
            minimumDistance,
            lastCandidateFailure.candidate[0] != '\0' ? lastCandidateFailure.candidate : "unknown",
            lastCandidateFailure.code[0] != '\0' ? lastCandidateFailure.code : "unknown",
            lastCandidateFailure.reason[0] != '\0' ? lastCandidateFailure.reason : "unknown placement failure");
    }
    else
    {
        _snprintf_s(
            details,
            sizeof(details),
            _TRUNCATE,
            "no valid standing target position was found at %.1f units; last candidate %s failed with %s: %s",
            minimumDistance,
            lastCandidateFailure.candidate[0] != '\0' ? lastCandidateFailure.candidate : "unknown",
            lastCandidateFailure.code[0] != '\0' ? lastCandidateFailure.code : "unknown",
            lastCandidateFailure.reason[0] != '\0' ? lastCandidateFailure.reason : "unknown placement failure");
    }

    SetLabDummyFailureInfo(
        failure,
        "no_valid_candidate",
        kLabDummySourceCurrentAnchor,
        lastCandidateFailure.candidate,
        details);
    return false;
}

bool TrySelectPreferredLabDummySpawnTransform(CBasePlayer *pAnchorPlayer, LabDummySpawnSelection *selection, LabDummyFailureInfo *failure)
{
    LabDummyFailureInfo savedFailure = {};
    LabDummyFailureInfo anchorFailure = {};
    LabDummyFailureInfo lastGoodFailure = {};
    char attempts[1024] = "";

    if (TryResolveSavedLabDummySelection(pAnchorPlayer, selection, &savedFailure))
    {
        return true;
    }

    AppendLabDummyFailureAttempt(attempts, sizeof(attempts), savedFailure);

    if (TryBuildLabDummySpawnTransformFromAnchor(pAnchorPlayer, selection, &anchorFailure))
    {
        return true;
    }

    AppendLabDummyFailureAttempt(attempts, sizeof(attempts), anchorFailure);

    if (g_glockLabDummyLastGoodTransform.valid)
    {
        if (TryResolveLastGoodLabDummySelection(pAnchorPlayer, selection, &lastGoodFailure))
        {
            return true;
        }

        AppendLabDummyFailureAttempt(attempts, sizeof(attempts), lastGoodFailure);
    }

    if (attempts[0] == '\0')
    {
        strcpy_s(attempts, sizeof(attempts), "no saved target spot, no current live player anchor, and no last known good target transform are available yet");
    }

    LabDummyFailureInfo chosenFailure = {};
    if (savedFailure.code[0] != '\0' && !StringEqualsIgnoreCase(savedFailure.code, "saved_spot_missing"))
    {
        chosenFailure = savedFailure;
    }
    else if (anchorFailure.code[0] != '\0' && !StringEqualsIgnoreCase(anchorFailure.code, "missing_anchor"))
    {
        chosenFailure = anchorFailure;
    }
    else if (lastGoodFailure.code[0] != '\0' && !StringEqualsIgnoreCase(lastGoodFailure.code, "last_good_missing"))
    {
        chosenFailure = lastGoodFailure;
    }
    else if (savedFailure.code[0] != '\0')
    {
        chosenFailure = savedFailure;
    }
    else if (anchorFailure.code[0] != '\0')
    {
        chosenFailure = anchorFailure;
    }
    else
    {
        chosenFailure = lastGoodFailure;
    }

    SetLabDummyFailureInfo(
        failure,
        chosenFailure.code[0] != '\0' ? chosenFailure.code : "missing_anchor",
        chosenFailure.source[0] != '\0' ? chosenFailure.source : kLabDummySourceCurrentAnchor,
        chosenFailure.candidate,
        attempts,
        chosenFailure.spotName,
        chosenFailure.spotStorage);
    return false;
}

void LogLabDummySpawnFailure(const LabDummyFailureInfo &failure, CBasePlayer *pAnchorPlayer)
{
    g_glockLabDummyLastSpawnFailure = failure;
    FormatFutureGameplayTimestamp(g_glockLabDummyLastFailureAt, sizeof(g_glockLabDummyLastFailureAt));

    ALERT(
        at_console,
        "[hl-server] target dummy spawn failed [%s%s%s]: %s\n",
        failure.code[0] != '\0' ? failure.code : "failed",
        failure.candidate[0] != '\0' ? "/" : "",
        failure.candidate[0] != '\0' ? failure.candidate : "",
        failure.reason[0] != '\0' ? failure.reason : "unknown target placement failure");
    LogGlockLabDummySpawnFailed(
        pAnchorPlayer,
        failure.source,
        failure.candidate,
        failure.code,
        failure.reason,
        NULL,
        NULL,
        failure.spotName,
        failure.spotStorage);
}

void StabilizeGlockLabDummyEntity(CBaseEntity *pDummy)
{
    if (pDummy == NULL || pDummy->pev == NULL)
    {
        return;
    }

    if (g_glockLabDummyLastGoodTransform.valid)
    {
        const Vector delta = pDummy->pev->origin - g_glockLabDummyLastGoodTransform.origin;
        if (delta.Length2D() > 1.0f || fabs(delta.z) > 1.0f)
        {
            UTIL_SetOrigin(pDummy->pev, g_glockLabDummyLastGoodTransform.origin);
        }

        pDummy->pev->angles = g_glockLabDummyLastGoodTransform.angles;
    }

    pDummy->pev->ideal_yaw = pDummy->pev->angles.y;
    pDummy->pev->yaw_speed = 0;
    pDummy->pev->velocity = g_vecZero;
    pDummy->pev->avelocity = g_vecZero;
    pDummy->pev->framerate = 0.0f;
    pDummy->SetThink(NULL);
    pDummy->pev->nextthink = 0.0f;
}

void MoveGlockLabDummyToSelection(CBaseEntity *pDummy, CBasePlayer *pAnchorPlayer, const LabDummySpawnSelection &selection, const char *reason)
{
    if (pDummy == NULL || pDummy->pev == NULL)
    {
        return;
    }

    UTIL_SetOrigin(pDummy->pev, selection.origin);
    pDummy->pev->angles = selection.angles;
    g_glockLabDummyAnchorPlayer = pAnchorPlayer;
    StoreLabDummyTransformMemory(&g_glockLabDummyLastGoodTransform, selection.origin, selection.angles, selection.source, selection.candidate);
    StabilizeGlockLabDummyEntity(pDummy);
    ClearGlockLabDummyFailureState();
    LogGlockLabDummyReposition(
        pDummy,
        pAnchorPlayer,
        selection.origin,
        selection.angles,
        reason,
        selection.source,
        selection.candidate,
        selection.spotName,
        selection.spotStorage);
}

CBaseEntity *SpawnGlockLabDummy(const LabDummySpawnSelection &selection, bool logAsRespawn)
{
    LabDummyFailureInfo preflightFailure = {};
    if (!EnsureLabDummyMonsterSpawningEnabled(&preflightFailure, &selection))
    {
        LogLabDummySpawnFailure(preflightFailure, selection.anchorPlayer);
        return NULL;
    }

    edict_t *pent = CREATE_NAMED_ENTITY(MAKE_STRING("monster_generic"));
    if (FNullEnt(pent))
    {
        LabDummyFailureInfo failure = {};
        SetLabDummyFailureInfo(&failure, "entity_alloc_failed", selection.source, selection.candidate, "the engine could not allocate a monster_generic entity");
        LogLabDummySpawnFailure(failure, selection.anchorPlayer);
        return NULL;
    }

    entvars_t *pevDummy = VARS(pent);
    const float dummyHealth = ExpGlockLabDummyHealth();
    const float dummyArmor = ExpGlockLabDummyArmor();

    pevDummy->origin = selection.origin;
    pevDummy->angles = selection.angles;
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
        LabDummyFailureInfo failure = {};
        SetLabDummyFailureInfo(&failure, "entity_spawn_failed", selection.source, selection.candidate, "the dummy entity failed to finish spawning");
        LogLabDummySpawnFailure(failure, selection.anchorPlayer);
        return NULL;
    }

    pDummy->pev->classname = MAKE_STRING("glock_lab_dummy");
    pDummy->pev->targetname = MAKE_STRING("exp_glock_lab_dummy");
    pDummy->pev->netname = ALLOC_STRING(kGlockLabDummyDisplayName);
    pDummy->pev->health = dummyHealth;
    pDummy->pev->max_health = dummyHealth;
    pDummy->pev->armorvalue = dummyArmor;

    StoreLabDummyTransformMemory(&g_glockLabDummyLastGoodTransform, selection.origin, selection.angles, selection.source, selection.candidate);
    StabilizeGlockLabDummyEntity(pDummy);

    g_glockLabDummy = pDummy;
    g_glockLabDummyAnchorPlayer = selection.anchorPlayer;
    g_glockLabDummyRespawnPending = false;
    g_glockLabDummyRespawnTime = 0.0f;
    g_glockLabDummyRetryTime = 0.0f;
    ClearGlockLabDummyFailureState();

    LogGlockLabDummySpawn(
        pDummy,
        selection.anchorPlayer,
        logAsRespawn,
        selection.origin,
        selection.angles,
        selection.source,
        selection.candidate,
        selection.spotName,
        selection.spotStorage);
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
            ClearGlockLabDummyRuntimeState();
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

    LabDummySpawnSelection selection = {};
    LabDummyFailureInfo failure = {};
    if (!TrySelectPreferredLabDummySpawnTransform(pAnchorPlayer, &selection, &failure))
    {
        LogLabDummySpawnFailure(failure, pAnchorPlayer);
        g_glockLabDummyRetryTime = gpGlobals->time + kGlockLabDummyRetryDelay;
        return;
    }

    if (SpawnGlockLabDummy(selection, g_glockLabDummyRespawnPending) != NULL)
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

bool EnsureLabDummyMonsterSpawningEnabled(LabDummyFailureInfo *failure, const LabDummySpawnSelection *selection)
{
    if (CVAR_GET_FLOAT("mp_allowmonsters") != 0.0f)
    {
        return true;
    }

    CVAR_SET_FLOAT("mp_allowmonsters", 1.0f);
    if (CVAR_GET_FLOAT("mp_allowmonsters") != 0.0f)
    {
        return true;
    }

    SetLabDummyFailureInfo(
        failure,
        "monsters_disabled",
        selection != NULL ? selection->source : kLabDummySourceCurrentAnchor,
        selection != NULL ? selection->candidate : "",
        "mp_allowmonsters must be 1 before the server can spawn a monster_generic dummy");
    return false;
}

bool ApplyResolvedLiveCfgSelection(const char *action, const ResolvedLiveCfgSelection &selection)
{
    if (g_engfuncs.pfnServerCommand == NULL || g_engfuncs.pfnServerExecute == NULL)
    {
        const char *reason = "engine server command execution is unavailable";
        RecordLiveCfgFailure(action, reason);
        LogLiveCfgCommand(action, selection.requestedPath, selection.execPath, selection.resolvedPath, false, reason);
        PrintLabDummyConsoleLine("cfg %s failed: %s", GetValueOrFallback(action, "apply"), reason);
        return false;
    }

    char execCommand[kMaxLiveCfgExecPathLength + 16];
    _snprintf_s(execCommand, sizeof(execCommand), _TRUNCATE, "exec %s\n", selection.execPath);
    SERVER_COMMAND(execCommand);
    SERVER_EXECUTE();

    RecordLiveCfgSuccess(action, selection);
    EnsureWeaponDebugLogReady();
    LogLiveCfgCommand(action, selection.requestedPath, selection.execPath, selection.resolvedPath, true, NULL);

    PrintLabDummyConsoleLine(
        "%s cfg: exec=%s source=%s",
        StringEqualsIgnoreCase(action, "reload") ? "reloaded" : "applied",
        selection.execPath,
        selection.resolvedPath);
    PrintCurrentCfgMetadata();
    return true;
}

bool TryRespawnLabDummyInternal(const char *removeReason, char *summary, size_t summarySize, bool printStatusOnFailure)
{
    if (summary == NULL || summarySize == 0)
    {
        return false;
    }

    summary[0] = '\0';
    RefreshFutureHooksMapState();
    SetLabDummyEnabled(true);

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();
    LabDummySpawnSelection selection = {};
    LabDummyFailureInfo failure = {};
    if (!TrySelectPreferredLabDummySpawnTransform(pAnchorPlayer, &selection, &failure))
    {
        LogLabDummySpawnFailure(failure, pAnchorPlayer);
        _snprintf_s(summary, summarySize, _TRUNCATE, "target respawn failed: %s", failure.reason);
        if (printStatusOnFailure)
        {
            PrintLabDummyStatus();
        }
        return false;
    }

    if (pDummy != NULL)
    {
        RemoveLabDummyEntities(removeReason);
        g_glockLabDummy = NULL;
    }

    g_glockLabDummyRespawnPending = false;
    g_glockLabDummyRespawnTime = 0.0f;
    g_glockLabDummyRetryTime = 0.0f;

    if (SpawnGlockLabDummy(selection, true) == NULL)
    {
        _snprintf_s(
            summary,
            summarySize,
            _TRUNCATE,
            "target respawn failed: %s",
            g_glockLabDummyLastSpawnFailure.reason[0] != '\0' ? g_glockLabDummyLastSpawnFailure.reason : "the dummy could not be recreated");
        if (printStatusOnFailure)
        {
            PrintLabDummyStatus();
        }
        return false;
    }

    _snprintf_s(
        summary,
        summarySize,
        _TRUNCATE,
        "respawned \"%s\" using profile %s via %s/%s%s%s.",
        kGlockLabDummyDisplayName,
        ExpGlockLabTargetProfileName(),
        selection.source,
        selection.candidate[0] != '\0' ? selection.candidate : "default",
        selection.spotName[0] != '\0' ? " spot=" : "",
        selection.spotName[0] != '\0' ? selection.spotName : "");
    return true;
}

void PrintLabDummyStatus()
{
    RefreshFutureHooksMapState();

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();
    const LabDummyProfileDefinition *pCurrentProfile = FindBuiltInLabDummyProfile(ExpGlockLabTargetProfileName());
    const LabDummySavedSpotRecord *activeSavedSpot = GetLabDummyActiveSavedSpot();
    bool usingDefaultSavedSpot = false;
    const LabDummySavedSpotRecord *effectiveSavedSpot = GetPreferredLabDummySavedSpot(&usingDefaultSavedSpot);
    LabDummySpawnSelection nextSelection = {};
    LabDummyFailureInfo nextFailure = {};
    const bool respawnPossible = TrySelectPreferredLabDummySpawnTransform(pAnchorPlayer, &nextSelection, &nextFailure);
    char savedSpotNames[kMaxLabDummySpotListLength];
    char savedOrigin[64];
    char lastGoodOrigin[64];
    char nextOrigin[64];
    char currentOrigin[64];
    BuildLabDummySpotNamesSummary(savedSpotNames, sizeof(savedSpotNames));
    strcpy_s(savedOrigin, sizeof(savedOrigin), "n/a");
    strcpy_s(lastGoodOrigin, sizeof(lastGoodOrigin), "n/a");
    strcpy_s(nextOrigin, sizeof(nextOrigin), "n/a");
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
        "target profile detail: %s",
        pCurrentProfile != NULL ? pCurrentProfile->description : "custom dummy coefficients");
    PrintLabDummyConsoleLine(
        "target placement: distance=%.1f right=%.1f up=%.1f face_player=%d",
        ExpGlockLabDummySpawnDistance(),
        ExpGlockLabDummyOffsetRight(),
        ExpGlockLabDummyOffsetUp(),
        ExpGlockLabDummyFacePlayer() ? 1 : 0);
    PrintLabDummyConsoleLine("target monster gate: mp_allowmonsters=%.0f", CVAR_GET_FLOAT("mp_allowmonsters"));

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

    if (g_glockLabDummySavedSpotCount > 0)
    {
        PrintLabDummyConsoleLine(
            "saved target spots: count=%d active=%s names=%s",
            g_glockLabDummySavedSpotCount,
            g_glockLabDummyActiveSpotName[0] != '\0' ? g_glockLabDummyActiveSpotName : "none",
            savedSpotNames);

        if (activeSavedSpot != NULL)
        {
            FormatVector3(savedOrigin, sizeof(savedOrigin), activeSavedSpot->origin);
            PrintLabDummyConsoleLine(
                "active saved spot: name=%s storage=%s origin=%s yaw=%.1f candidate=%s created_at=%s updated_at=%s",
                activeSavedSpot->name,
                GetLabDummySpotStorageLabel(activeSavedSpot),
                savedOrigin,
                activeSavedSpot->angles.y,
                activeSavedSpot->candidate[0] != '\0' ? activeSavedSpot->candidate : "marked_spot",
                activeSavedSpot->createdAt[0] != '\0' ? activeSavedSpot->createdAt : "n/a",
                activeSavedSpot->updatedAt[0] != '\0' ? activeSavedSpot->updatedAt : "n/a");
        }
        else if (g_glockLabDummyActiveSpotName[0] != '\0')
        {
            PrintLabDummyConsoleLine("active saved spot: name=%s state=missing", g_glockLabDummyActiveSpotName);
        }
        else if (usingDefaultSavedSpot && effectiveSavedSpot != NULL)
        {
            FormatVector3(savedOrigin, sizeof(savedOrigin), effectiveSavedSpot->origin);
            PrintLabDummyConsoleLine(
                "active saved spot: none selected, fallback=%s storage=%s origin=%s yaw=%.1f candidate=%s",
                effectiveSavedSpot->name,
                GetLabDummySpotStorageLabel(effectiveSavedSpot),
                savedOrigin,
                effectiveSavedSpot->angles.y,
                effectiveSavedSpot->candidate[0] != '\0' ? effectiveSavedSpot->candidate : "marked_spot");
        }
        else
        {
            PrintLabDummyConsoleLine("active saved spot: none selected");
        }

        if (g_glockLabDummyTargetSpotsPath[0] != '\0')
        {
            PrintLabDummyConsoleLine("target spots file: %s", g_glockLabDummyTargetSpotsPath);
        }
    }
    else
    {
        PrintLabDummyConsoleLine("saved target spots: none");
        if (g_glockLabDummyTargetSpotsPath[0] != '\0')
        {
            PrintLabDummyConsoleLine("target spots file: %s", g_glockLabDummyTargetSpotsPath);
        }
    }

    if (g_glockLabDummyTargetSpotsLoadFailure[0] != '\0')
    {
        PrintLabDummyConsoleLine(
            "target spots load state: problem=%s",
            g_glockLabDummyTargetSpotsLoadFailure);
    }

    if (g_glockLabDummyLastGoodTransform.valid)
    {
        FormatVector3(lastGoodOrigin, sizeof(lastGoodOrigin), g_glockLabDummyLastGoodTransform.origin);
        PrintLabDummyConsoleLine(
            "last known good target transform: origin=%s yaw=%.1f source=%s candidate=%s",
            lastGoodOrigin,
            g_glockLabDummyLastGoodTransform.angles.y,
            g_glockLabDummyLastGoodTransform.source[0] != '\0' ? g_glockLabDummyLastGoodTransform.source : "unknown",
            g_glockLabDummyLastGoodTransform.candidate[0] != '\0' ? g_glockLabDummyLastGoodTransform.candidate : "last_good");
    }
    else
    {
        PrintLabDummyConsoleLine("last known good target transform: none");
    }

    if (g_glockLabDummyLastSpawnFailure.code[0] != '\0')
    {
        PrintLabDummyConsoleLine(
            "last spawn failure: at=%s code=%s source=%s candidate=%s%s%s%s%s reason=%s",
            g_glockLabDummyLastFailureAt[0] != '\0' ? g_glockLabDummyLastFailureAt : "unknown",
            g_glockLabDummyLastSpawnFailure.code,
            g_glockLabDummyLastSpawnFailure.source[0] != '\0' ? g_glockLabDummyLastSpawnFailure.source : "unknown",
            g_glockLabDummyLastSpawnFailure.candidate[0] != '\0' ? g_glockLabDummyLastSpawnFailure.candidate : "n/a",
            g_glockLabDummyLastSpawnFailure.spotName[0] != '\0' ? " spot=" : "",
            g_glockLabDummyLastSpawnFailure.spotName[0] != '\0' ? g_glockLabDummyLastSpawnFailure.spotName : "",
            g_glockLabDummyLastSpawnFailure.spotStorage[0] != '\0' ? " storage=" : "",
            g_glockLabDummyLastSpawnFailure.spotStorage[0] != '\0' ? g_glockLabDummyLastSpawnFailure.spotStorage : "",
            g_glockLabDummyLastSpawnFailure.reason);
    }
    else
    {
        PrintLabDummyConsoleLine("last spawn failure: none");
    }

    if (respawnPossible)
    {
        FormatVector3(nextOrigin, sizeof(nextOrigin), nextSelection.origin);
        PrintLabDummyConsoleLine(
            "target respawn possible: yes source=%s candidate=%s origin=%s yaw=%.1f%s%s%s%s",
            nextSelection.source,
            nextSelection.candidate[0] != '\0' ? nextSelection.candidate : "default",
            nextOrigin,
            nextSelection.angles.y,
            nextSelection.spotName[0] != '\0' ? " spot=" : "",
            nextSelection.spotName[0] != '\0' ? nextSelection.spotName : "",
            nextSelection.spotStorage[0] != '\0' ? " storage=" : "",
            nextSelection.spotStorage[0] != '\0' ? nextSelection.spotStorage : "");
    }
    else
    {
        PrintLabDummyConsoleLine("target respawn possible: no reason=%s", nextFailure.reason[0] != '\0' ? nextFailure.reason : "no valid placement source is available");
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
    PrintLabDummyConsoleLine("commands: exp_target_spawn | exp_target_clear | exp_target_mark [name] | exp_target_unmark [name] | exp_target_list | exp_target_use_saved <name> | exp_target_respawn | exp_target_status | exp_target_tp_front | exp_target_profile <name>");
}

void ExpCfgApplyCommand()
{
    char requestedPath[kMaxLiveCfgRequestLength];
    BuildCommandArgumentString(1, requestedPath, sizeof(requestedPath));
    if (requestedPath[0] == '\0')
    {
        PrintLabDummyConsoleLine("usage: exp_cfg_apply <cfg_name_or_path>");
        PrintLiveCfgStatus();
        return;
    }

    ResolvedLiveCfgSelection selection = {};
    char failureReason[kMaxLiveCfgFailureLength];
    if (!TryResolveLiveCfgSelection(requestedPath, &selection, failureReason, sizeof(failureReason)))
    {
        RecordLiveCfgFailure("apply", failureReason);
        LogLiveCfgCommand("apply", requestedPath, "", "", false, failureReason);
        PrintLabDummyConsoleLine("cfg apply failed: %s", failureReason);
        return;
    }

    ApplyResolvedLiveCfgSelection("apply", selection);
}

void ExpCfgReloadCommand()
{
    if (!g_liveCfgState.hasActiveCfg || g_liveCfgState.activeExecPath[0] == '\0')
    {
        const char *reason = "no active cfg is tracked yet. Use exp_cfg_apply <cfg_name_or_path> first.";
        RecordLiveCfgFailure("reload", reason);
        LogLiveCfgCommand("reload", "", "", "", false, reason);
        PrintLabDummyConsoleLine("cfg reload failed: %s", reason);
        return;
    }

    const char *reloadRequest = g_liveCfgState.activeExecPath;
    ResolvedLiveCfgSelection selection = {};
    char failureReason[kMaxLiveCfgFailureLength];
    if (!TryResolveLiveCfgSelection(reloadRequest, &selection, failureReason, sizeof(failureReason)))
    {
        RecordLiveCfgFailure("reload", failureReason);
        LogLiveCfgCommand("reload", reloadRequest, g_liveCfgState.activeExecPath, g_liveCfgState.activeResolvedPath, false, failureReason);
        PrintLabDummyConsoleLine("cfg reload failed: %s", failureReason);
        return;
    }

    ApplyResolvedLiveCfgSelection("reload", selection);
}

void ExpCfgStatusCommand()
{
    PrintLiveCfgStatus();
}

void ExpLabApplyCommand()
{
    char requestedPath[kMaxLiveCfgRequestLength];
    BuildCommandArgumentString(1, requestedPath, sizeof(requestedPath));
    if (requestedPath[0] == '\0')
    {
        PrintLabDummyConsoleLine("usage: exp_lab_apply <cfg_name_or_path>");
        return;
    }

    ResolvedLiveCfgSelection selection = {};
    char failureReason[kMaxLiveCfgFailureLength];
    if (!TryResolveLiveCfgSelection(requestedPath, &selection, failureReason, sizeof(failureReason)))
    {
        RecordLiveCfgFailure("apply", failureReason);
        LogLiveCfgCommand("apply", requestedPath, "", "", false, failureReason);
        PrintLabDummyConsoleLine("lab apply failed: %s", failureReason);
        return;
    }

    if (!ApplyResolvedLiveCfgSelection("apply", selection))
    {
        return;
    }

    char targetSummary[192];
    const bool targetRespawned = TryRespawnLabDummyInternal("lab_apply", targetSummary, sizeof(targetSummary), true);
    PrintLabDummyConsoleLine("%s", targetSummary);
    PrintLabDummyConsoleLine(
        "lab apply summary: cfg=%s target_refresh=%s target_profile=%s",
        selection.execPath,
        targetRespawned ? "ok" : "failed",
        ExpGlockLabTargetProfileName());
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
    LabDummySpawnSelection targetSelection = {};
    LabDummyFailureInfo failure = {};
    if (!TrySelectPreferredLabDummySpawnTransform(pAnchorPlayer, &targetSelection, &failure))
    {
        LogLabDummySpawnFailure(failure, pAnchorPlayer);
        PrintLabDummyConsoleLine("target spawn failed: %s", failure.reason);
        return;
    }

    g_glockLabDummyRespawnPending = false;
    g_glockLabDummyRespawnTime = 0.0f;
    g_glockLabDummyRetryTime = 0.0f;

    if (SpawnGlockLabDummy(targetSelection, false) == NULL)
    {
        PrintLabDummyStatus();
        return;
    }

    PrintLabDummyConsoleLine(
        "spawned \"%s\" using profile %s via %s/%s%s%s.",
        kGlockLabDummyDisplayName,
        ExpGlockLabTargetProfileName(),
        targetSelection.source,
        targetSelection.candidate[0] != '\0' ? targetSelection.candidate : "default",
        targetSelection.spotName[0] != '\0' ? " spot=" : "",
        targetSelection.spotName[0] != '\0' ? targetSelection.spotName : "");
}

void ExpTargetClearCommand()
{
    RefreshFutureHooksMapState();
    SetLabDummyEnabled(false);

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    if (pDummy == NULL)
    {
        ClearGlockLabDummyRuntimeState();
        PrintLabDummyConsoleLine("target is already clear. Automatic target spawning is now disabled.");
        return;
    }

    ClearAllLabDummyEntities("command_clear");
    PrintLabDummyConsoleLine("cleared the target and disabled automatic target spawning.");
}

void ExpTargetRespawnCommand()
{
    char summary[192];
    const bool success = TryRespawnLabDummyInternal("command_respawn", summary, sizeof(summary), true);
    PrintLabDummyConsoleLine("%s", summary);
    if (!success)
    {
        return;
    }
}

void ExpTargetStatusCommand()
{
    PrintLabDummyStatus();
}

void ExpTargetListCommand()
{
    RefreshFutureHooksMapState();

    PrintLabDummyConsoleLine(
        "saved target spots for map %s: count=%d active=%s file=%s",
        GetCurrentMapName(),
        g_glockLabDummySavedSpotCount,
        g_glockLabDummyActiveSpotName[0] != '\0' ? g_glockLabDummyActiveSpotName : "none",
        g_glockLabDummyTargetSpotsPath[0] != '\0' ? g_glockLabDummyTargetSpotsPath : "n/a");

    if (g_glockLabDummySavedSpotCount <= 0)
    {
        if (g_glockLabDummyTargetSpotsLoadFailure[0] != '\0')
        {
            PrintLabDummyConsoleLine("target spots load state: problem=%s", g_glockLabDummyTargetSpotsLoadFailure);
        }
        return;
    }

    for (int spotIndex = 0; spotIndex < g_glockLabDummySavedSpotCount; ++spotIndex)
    {
        const LabDummySavedSpotRecord &spot = g_glockLabDummySavedSpots[spotIndex];
        char origin[64];
        FormatVector3(origin, sizeof(origin), spot.origin);
        PrintLabDummyConsoleLine(
            "spot[%d]: name=%s%s storage=%s origin=%s yaw=%.1f candidate=%s updated_at=%s",
            spotIndex,
            spot.name,
            StringEqualsIgnoreCase(g_glockLabDummyActiveSpotName, spot.name) ? " [active]" : "",
            GetLabDummySpotStorageLabel(&spot),
            origin,
            spot.angles.y,
            spot.candidate[0] != '\0' ? spot.candidate : "marked_spot",
            spot.updatedAt[0] != '\0' ? spot.updatedAt : "n/a");
    }
}

void ExpTargetMarkCommand()
{
    RefreshFutureHooksMapState();

    char spotName[kMaxLabDummySpotNameLength];
    char failureReason[kMaxLabDummySpotFileFailureLength];
    if (!TryGetLabDummySpotCommandName(1, spotName, sizeof(spotName), true, failureReason, sizeof(failureReason)))
    {
        PrintLabDummyConsoleLine("target mark failed: %s", failureReason);
        return;
    }

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();
    Vector markedOrigin = g_vecZero;
    Vector markedAngles = g_vecZero;
    char markCandidate[64] = "";

    if (pDummy != NULL && pDummy->pev != NULL && pDummy->IsAlive())
    {
        markedOrigin = pDummy->pev->origin;
        markedAngles = pDummy->pev->angles;
        strncpy_s(markCandidate, sizeof(markCandidate), "current_dummy", _TRUNCATE);
    }
    else
    {
        LabDummySpawnSelection selection = {};
        LabDummyFailureInfo failure = {};
        if (!TryBuildLabDummySpawnTransformFromAnchor(pAnchorPlayer, &selection, &failure))
        {
            LogLabDummySpawnFailure(failure, pAnchorPlayer);
            PrintLabDummyConsoleLine("target mark failed: %s", failure.reason);
            return;
        }

        markedOrigin = selection.origin;
        markedAngles = selection.angles;
        strncpy_s(markCandidate, sizeof(markCandidate), selection.candidate, _TRUNCATE);
    }

    LabDummySavedSpotRecord *spot = UpsertLabDummySavedSpot(spotName, markedOrigin, markedAngles, markCandidate, failureReason, sizeof(failureReason));
    if (spot == NULL)
    {
        PrintLabDummyConsoleLine("target mark failed: %s", failureReason);
        return;
    }

    strncpy_s(g_glockLabDummyActiveSpotName, sizeof(g_glockLabDummyActiveSpotName), spot->name, _TRUNCATE);
    const bool savedToDisk = SaveLabDummySpotsForCurrentMap(failureReason, sizeof(failureReason));
    LogGlockLabDummyMark("target_mark", pAnchorPlayer, markedOrigin, markedAngles, markCandidate, spot->name, kLabDummySpotStorageSession);

    char origin[64];
    FormatVector3(origin, sizeof(origin), markedOrigin);
    PrintLabDummyConsoleLine(
        "saved target spot \"%s\" for map %s: origin=%s yaw=%.1f active=%s disk=%s",
        spot->name,
        GetCurrentMapName(),
        origin,
        markedAngles.y,
        g_glockLabDummyActiveSpotName,
        savedToDisk ? g_glockLabDummyTargetSpotsPath : failureReason);
}

void ExpTargetUnmarkCommand()
{
    RefreshFutureHooksMapState();

    char spotName[kMaxLabDummySpotNameLength];
    char failureReason[kMaxLabDummySpotFileFailureLength];
    if (!TryGetLabDummySpotCommandName(1, spotName, sizeof(spotName), true, failureReason, sizeof(failureReason)))
    {
        PrintLabDummyConsoleLine("target unmark failed: %s", failureReason);
        return;
    }

    LabDummySavedSpotRecord removedSpot = {};
    if (!RemoveLabDummySavedSpot(spotName, &removedSpot, failureReason, sizeof(failureReason)))
    {
        PrintLabDummyConsoleLine("target unmark failed: %s", failureReason);
        return;
    }

    const bool savedToDisk = SaveLabDummySpotsForCurrentMap(failureReason, sizeof(failureReason));
    LogGlockLabDummyMark(
        "target_unmark",
        FindCurrentLiveLabDummyAnchorPlayer(),
        removedSpot.origin,
        removedSpot.angles,
        removedSpot.candidate,
        removedSpot.name,
        GetLabDummySpotStorageLabel(&removedSpot));
    PrintLabDummyConsoleLine(
        "cleared target spot \"%s\" for map %s. active=%s disk=%s",
        removedSpot.name,
        GetCurrentMapName(),
        g_glockLabDummyActiveSpotName[0] != '\0' ? g_glockLabDummyActiveSpotName : "none",
        savedToDisk ? g_glockLabDummyTargetSpotsPath : failureReason);
}

void ExpTargetUseSavedCommand()
{
    RefreshFutureHooksMapState();

    char spotName[kMaxLabDummySpotNameLength];
    char failureReason[kMaxLabDummySpotFileFailureLength];
    if (!TryGetLabDummySpotCommandName(1, spotName, sizeof(spotName), true, failureReason, sizeof(failureReason)))
    {
        PrintLabDummyConsoleLine("target use_saved failed: %s", failureReason);
        return;
    }

    const LabDummySavedSpotRecord *spot = FindLabDummySavedSpotConst(spotName);
    if (spot == NULL)
    {
        PrintLabDummyConsoleLine("target use_saved failed: target spot \"%s\" was not found for map %s", spotName, GetCurrentMapName());
        PrintLabDummyStatus();
        return;
    }

    strncpy_s(g_glockLabDummyActiveSpotName, sizeof(g_glockLabDummyActiveSpotName), spot->name, _TRUNCATE);
    const bool savedToDisk = SaveLabDummySpotsForCurrentMap(failureReason, sizeof(failureReason));
    LogGlockLabDummyMark("target_use_saved", FindCurrentLiveLabDummyAnchorPlayer(), spot->origin, spot->angles, "selected_active_spot", spot->name, GetLabDummySpotStorageLabel(spot));
    PrintLabDummyConsoleLine(
        "active target spot is now \"%s\" for map %s. Use exp_target_respawn to rebuild the dummy there. disk=%s",
        spot->name,
        GetCurrentMapName(),
        savedToDisk ? g_glockLabDummyTargetSpotsPath : failureReason);
}

void ExpTargetTpFrontCommand()
{
    RefreshFutureHooksMapState();
    SetLabDummyEnabled(true);

    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();
    LabDummySpawnSelection selection = {};
    LabDummyFailureInfo failure = {};
    if (!TryBuildLabDummySpawnTransformFromAnchor(pAnchorPlayer, &selection, &failure))
    {
        LogLabDummySpawnFailure(failure, pAnchorPlayer);
        PrintLabDummyConsoleLine("target move failed: %s", failure.reason);
        return;
    }

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    if (pDummy != NULL && pDummy->IsAlive())
    {
        MoveGlockLabDummyToSelection(pDummy, pAnchorPlayer, selection, "command_tp_front");
        PrintLabDummyConsoleLine(
            "moved \"%s\" in front of %s using %s/%s.",
            kGlockLabDummyDisplayName,
            GetSafePlayerName(pAnchorPlayer),
            selection.source,
            selection.candidate[0] != '\0' ? selection.candidate : "default");
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

    if (SpawnGlockLabDummy(selection, pDummy != NULL) == NULL)
    {
        PrintLabDummyStatus();
        return;
    }

    PrintLabDummyConsoleLine(
        "spawned \"%s\" in front of %s using %s/%s.",
        kGlockLabDummyDisplayName,
        GetSafePlayerName(pAnchorPlayer),
        selection.source,
        selection.candidate[0] != '\0' ? selection.candidate : "default");
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
    LabDummySpawnSelection selection = {};
    LabDummyFailureInfo failure = {};
    if (!TrySelectPreferredLabDummySpawnTransform(pAnchorPlayer, &selection, &failure))
    {
        LogLabDummySpawnFailure(failure, pAnchorPlayer);
        PrintLabDummyConsoleLine("target profile \"%s\" was stored, but the target could not be refreshed: %s", pProfile->name, failure.reason);
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

    if (SpawnGlockLabDummy(selection, pDummy != NULL) == NULL)
    {
        PrintLabDummyConsoleLine("target profile \"%s\" was stored, but the target could not be refreshed immediately.", pProfile->name);
        return;
    }

    PrintLabDummyConsoleLine(
        "refreshed \"%s\" with profile %s via %s/%s.",
        kGlockLabDummyDisplayName,
        pProfile->name,
        selection.source,
        selection.candidate[0] != '\0' ? selection.candidate : "default");
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
        ALERT(at_console, "[hl-server] live lab commands could not be registered because pfnAddServerCommand is unavailable\n");
        return;
    }

    g_engfuncs.pfnAddServerCommand((char *)"exp_cfg_apply", ExpCfgApplyCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_cfg_reload", ExpCfgReloadCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_cfg_status", ExpCfgStatusCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_lab_apply", ExpLabApplyCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_spawn", ExpTargetSpawnCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_clear", ExpTargetClearCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_mark", ExpTargetMarkCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_unmark", ExpTargetUnmarkCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_list", ExpTargetListCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_use_saved", ExpTargetUseSavedCommand);
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

bool ExpCfgDrivenModeActive()
{
    return g_liveCfgState.cfgDrivenModeActive;
}

const char *ExpActiveCfgProfile()
{
    return g_liveCfgState.hasActiveCfg ? g_liveCfgState.activeExecPath : "";
}

const char *ExpLastSuccessfulCfgProfile()
{
    return g_liveCfgState.hasLastSuccessfulCfg ? g_liveCfgState.lastSuccessfulExecPath : "";
}

const char *ExpLastCfgAction()
{
    return g_liveCfgState.lastAction;
}

const char *ExpLastCfgAppliedAt()
{
    return g_liveCfgState.lastAppliedAt;
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
