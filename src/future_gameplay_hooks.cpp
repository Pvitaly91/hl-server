#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "weapons.h"

#include "future_gameplay_hooks.h"
#include "weapon_debug_logger.h"

#include <io.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <string>
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
const char *kAllowedGlockLabDummyModels[] = {
    "models/barney.mdl",
    "models/scientist.mdl"};
const char *kGlockLabDummyClassname = "glock_lab_dummy";
const char *kGlockLabDummyTargetname = "exp_glock_lab_dummy";
const char *kLabDummySourceSavedSpot = "saved_spot";
const char *kLabDummySourceCurrentAnchor = "current_anchor";
const char *kLabDummySourceLastGood = "last_known_good";

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

struct LabDummySpawnSelection
{
    Vector origin;
    Vector angles;
    CBasePlayer *anchorPlayer;
    char source[32];
    char candidate[64];
};

struct LabDummyFailureInfo
{
    char code[64];
    char source[32];
    char candidate[64];
    char reason[512];
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
LabDummyTransformMemory g_glockLabDummySavedSpot = {};
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
}

void SetLabDummyFailureInfo(LabDummyFailureInfo *failure, const char *code, const char *source, const char *candidate, const char *reason)
{
    if (failure == NULL)
    {
        return;
    }

    strncpy_s(failure->code, sizeof(failure->code), code != NULL ? code : "", _TRUNCATE);
    strncpy_s(failure->source, sizeof(failure->source), source != NULL ? source : "", _TRUNCATE);
    strncpy_s(failure->candidate, sizeof(failure->candidate), candidate != NULL ? candidate : "", _TRUNCATE);
    strncpy_s(failure->reason, sizeof(failure->reason), reason != NULL ? reason : "", _TRUNCATE);
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
        "%s[%s%s%s]: %s",
        failure.source[0] != '\0' ? failure.source : "target",
        failure.code[0] != '\0' ? failure.code : "failed",
        failure.candidate[0] != '\0' ? "/" : "",
        failure.candidate[0] != '\0' ? failure.candidate : "",
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
    ClearLabDummyTransformMemory(&g_glockLabDummySavedSpot);
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

bool TryResolveSavedLabDummySelection(CBasePlayer *pAnchorPlayer, LabDummySpawnSelection *selection, LabDummyFailureInfo *failure)
{
    if (!g_glockLabDummySavedSpot.valid)
    {
        SetLabDummyFailureInfo(failure, "saved_spot_missing", kLabDummySourceSavedSpot, "", "no saved target spot is marked for this map");
        return false;
    }

    char failureCode[64];
    char failureReason[192];
    if (!TryValidateLabDummyExactTransform(g_glockLabDummySavedSpot.origin, pAnchorPlayer, failureCode, sizeof(failureCode), failureReason, sizeof(failureReason)))
    {
        char details[512];
        _snprintf_s(details, sizeof(details), _TRUNCATE, "saved target spot is no longer valid: %s", failureReason);
        SetLabDummyFailureInfo(
            failure,
            "saved_spot_invalid",
            kLabDummySourceSavedSpot,
            g_glockLabDummySavedSpot.candidate[0] != '\0' ? g_glockLabDummySavedSpot.candidate : "marked_spot",
            details);
        return false;
    }

    selection->origin = g_glockLabDummySavedSpot.origin;
    selection->angles = g_glockLabDummySavedSpot.angles;
    selection->anchorPlayer = pAnchorPlayer;
    strncpy_s(selection->source, sizeof(selection->source), kLabDummySourceSavedSpot, _TRUNCATE);
    strncpy_s(selection->candidate, sizeof(selection->candidate), g_glockLabDummySavedSpot.candidate[0] != '\0' ? g_glockLabDummySavedSpot.candidate : "marked_spot", _TRUNCATE);
    return true;
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

    if (g_glockLabDummySavedSpot.valid)
    {
        if (TryResolveSavedLabDummySelection(pAnchorPlayer, selection, &savedFailure))
        {
            return true;
        }

        AppendLabDummyFailureAttempt(attempts, sizeof(attempts), savedFailure);
    }

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
    if (anchorFailure.code[0] != '\0' && !StringEqualsIgnoreCase(anchorFailure.code, "missing_anchor"))
    {
        chosenFailure = anchorFailure;
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
        attempts);
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
        NULL);
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
    LogGlockLabDummyReposition(pDummy, pAnchorPlayer, selection.origin, selection.angles, reason, selection.source, selection.candidate);
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

    LogGlockLabDummySpawn(pDummy, selection.anchorPlayer, logAsRespawn, selection.origin, selection.angles, selection.source, selection.candidate);
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
        "respawned \"%s\" using profile %s via %s/%s.",
        kGlockLabDummyDisplayName,
        ExpGlockLabTargetProfileName(),
        selection.source,
        selection.candidate[0] != '\0' ? selection.candidate : "default");
    return true;
}

void PrintLabDummyStatus()
{
    RefreshFutureHooksMapState();

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();
    const LabDummyProfileDefinition *pCurrentProfile = FindBuiltInLabDummyProfile(ExpGlockLabTargetProfileName());
    LabDummySpawnSelection nextSelection = {};
    LabDummyFailureInfo nextFailure = {};
    const bool respawnPossible = TrySelectPreferredLabDummySpawnTransform(pAnchorPlayer, &nextSelection, &nextFailure);
    char savedOrigin[64];
    char lastGoodOrigin[64];
    char nextOrigin[64];
    char currentOrigin[64];
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

    if (g_glockLabDummySavedSpot.valid)
    {
        FormatVector3(savedOrigin, sizeof(savedOrigin), g_glockLabDummySavedSpot.origin);
        PrintLabDummyConsoleLine(
            "saved target spot: present origin=%s yaw=%.1f candidate=%s",
            savedOrigin,
            g_glockLabDummySavedSpot.angles.y,
            g_glockLabDummySavedSpot.candidate[0] != '\0' ? g_glockLabDummySavedSpot.candidate : "marked_spot");
    }
    else
    {
        PrintLabDummyConsoleLine("saved target spot: none");
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
            "last spawn failure: at=%s code=%s source=%s candidate=%s reason=%s",
            g_glockLabDummyLastFailureAt[0] != '\0' ? g_glockLabDummyLastFailureAt : "unknown",
            g_glockLabDummyLastSpawnFailure.code,
            g_glockLabDummyLastSpawnFailure.source[0] != '\0' ? g_glockLabDummyLastSpawnFailure.source : "unknown",
            g_glockLabDummyLastSpawnFailure.candidate[0] != '\0' ? g_glockLabDummyLastSpawnFailure.candidate : "n/a",
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
            "target respawn possible: yes source=%s candidate=%s origin=%s yaw=%.1f",
            nextSelection.source,
            nextSelection.candidate[0] != '\0' ? nextSelection.candidate : "default",
            nextOrigin,
            nextSelection.angles.y);
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
    PrintLabDummyConsoleLine("commands: exp_target_spawn | exp_target_clear | exp_target_mark | exp_target_unmark | exp_target_use_saved | exp_target_respawn | exp_target_status | exp_target_tp_front | exp_target_profile <name>");
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
        "spawned \"%s\" using profile %s via %s/%s.",
        kGlockLabDummyDisplayName,
        ExpGlockLabTargetProfileName(),
        targetSelection.source,
        targetSelection.candidate[0] != '\0' ? targetSelection.candidate : "default");
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

void ExpTargetMarkCommand()
{
    RefreshFutureHooksMapState();

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();

    if (pDummy != NULL && pDummy->pev != NULL && pDummy->IsAlive())
    {
        StoreLabDummyTransformMemory(&g_glockLabDummySavedSpot, pDummy->pev->origin, pDummy->pev->angles, kLabDummySourceSavedSpot, "current_dummy");
        LogGlockLabDummyMark("target_mark", pAnchorPlayer, pDummy->pev->origin, pDummy->pev->angles, "current_dummy");

        char origin[64];
        FormatVector3(origin, sizeof(origin), pDummy->pev->origin);
        PrintLabDummyConsoleLine("saved current dummy position for this map: origin=%s yaw=%.1f", origin, pDummy->pev->angles.y);
        return;
    }

    LabDummySpawnSelection selection = {};
    LabDummyFailureInfo failure = {};
    if (!TryBuildLabDummySpawnTransformFromAnchor(pAnchorPlayer, &selection, &failure))
    {
        LogLabDummySpawnFailure(failure, pAnchorPlayer);
        PrintLabDummyConsoleLine("target mark failed: %s", failure.reason);
        return;
    }

    StoreLabDummyTransformMemory(&g_glockLabDummySavedSpot, selection.origin, selection.angles, kLabDummySourceSavedSpot, selection.candidate);
    LogGlockLabDummyMark("target_mark", pAnchorPlayer, selection.origin, selection.angles, selection.candidate);

    char origin[64];
    FormatVector3(origin, sizeof(origin), selection.origin);
    PrintLabDummyConsoleLine(
        "saved target spot for this map: origin=%s yaw=%.1f source=%s/%s",
        origin,
        selection.angles.y,
        selection.source,
        selection.candidate);
}

void ExpTargetUnmarkCommand()
{
    RefreshFutureHooksMapState();

    if (!g_glockLabDummySavedSpot.valid)
    {
        PrintLabDummyConsoleLine("saved target spot is already clear for this map.");
        return;
    }

    LogGlockLabDummyMark("target_unmark", FindCurrentLiveLabDummyAnchorPlayer(), g_glockLabDummySavedSpot.origin, g_glockLabDummySavedSpot.angles, g_glockLabDummySavedSpot.candidate);
    ClearLabDummyTransformMemory(&g_glockLabDummySavedSpot);
    PrintLabDummyConsoleLine("cleared the saved target spot for this map.");
}

void ExpTargetUseSavedCommand()
{
    RefreshFutureHooksMapState();
    SetLabDummyEnabled(true);

    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();
    LabDummySpawnSelection selection = {};
    LabDummyFailureInfo failure = {};
    if (!TryResolveSavedLabDummySelection(pAnchorPlayer, &selection, &failure))
    {
        LogLabDummySpawnFailure(failure, pAnchorPlayer);
        PrintLabDummyConsoleLine("target use_saved failed: %s", failure.reason);
        PrintLabDummyStatus();
        return;
    }

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    if (pDummy != NULL && pDummy->IsAlive())
    {
        MoveGlockLabDummyToSelection(pDummy, pAnchorPlayer, selection, "command_use_saved");
        PrintLabDummyConsoleLine(
            "moved \"%s\" to the saved target spot via %s/%s.",
            kGlockLabDummyDisplayName,
            selection.source,
            selection.candidate[0] != '\0' ? selection.candidate : "marked_spot");
        return;
    }

    if (pDummy != NULL)
    {
        RemoveLabDummyEntities("command_use_saved");
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
        "spawned \"%s\" at the saved target spot via %s/%s.",
        kGlockLabDummyDisplayName,
        selection.source,
        selection.candidate[0] != '\0' ? selection.candidate : "marked_spot");
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
