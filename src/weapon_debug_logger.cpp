#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "future_gameplay_hooks.h"
#include "weapon_tuning_core.h"
#include "weapon_debug_logger.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <string>

namespace
{
FILE *g_weaponDebugLogFile = NULL;
bool g_weaponDebugLogOpenAttempted = false;
bool g_weaponDebugLogPathNoticePrinted = false;
bool g_weaponDebugLogWarningPrinted = false;
std::string g_weaponDebugLogPath;

struct GlockPrimaryShotContext
{
    bool active;
    entvars_t *attacker;
    CBasePlayer *attackerPlayer;
    bool experimentalModeActive;
    float baseDamage;
    float headshotScale;
    bool headshotLethal;
    char profileName[64];
    bool pendingHit;
    CBaseEntity *victim;
    int hitgroup;
    bool headshot;
    float hitgroupScale;
    float traceDamage;
    bool headshotLethalApplied;
    float victimHealthBefore;
    bool victimArmorKnown;
    float victimArmorBefore;
    bool victimIsDummy;
    char targetProfileName[64];
    bool dummyArmorApplied;
    bool dummyHeadProtected;
    float damageRaw;
    float damageToHealth;
    float damageAbsorbed;
    float armorDrain;
};

GlockPrimaryShotContext g_glockPrimaryShotContext = {};

struct Mp5PrimaryShotContext
{
    bool active;
    entvars_t *attacker;
    CBasePlayer *attackerPlayer;
    bool experimentalModeActive;
    float baseDamage;
    float headshotScale;
    bool headshotLethal;
    char profileName[64];
    bool pendingHit;
    CBaseEntity *victim;
    int hitgroup;
    bool headshot;
    float hitgroupScale;
    float traceDamage;
    bool headshotLethalApplied;
    float victimHealthBefore;
    bool victimArmorKnown;
    float victimArmorBefore;
    bool victimIsDummy;
    char targetProfileName[64];
    bool dummyArmorApplied;
    bool dummyHeadProtected;
    float damageRaw;
    float damageToHealth;
    float damageAbsorbed;
    float armorDrain;
};

Mp5PrimaryShotContext g_mp5PrimaryShotContext = {};

struct Weapon357PrimaryShotContext
{
    bool active;
    entvars_t *attacker;
    CBasePlayer *attackerPlayer;
    bool experimentalModeActive;
    float baseDamage;
    float headshotScale;
    bool headshotLethal;
    char profileName[64];
    bool pendingHit;
    CBaseEntity *victim;
    int hitgroup;
    bool headshot;
    float hitgroupScale;
    float traceDamage;
    bool headshotLethalApplied;
    float victimHealthBefore;
    bool victimArmorKnown;
    float victimArmorBefore;
    bool victimIsDummy;
    char targetProfileName[64];
    bool dummyArmorApplied;
    bool dummyHeadProtected;
    float damageRaw;
    float damageToHealth;
    float damageAbsorbed;
    float armorDrain;
};

Weapon357PrimaryShotContext g_357PrimaryShotContext = {};

const int kMaxShotgunVictimsPerShot = 16;

struct ShotgunPrimaryVictimAggregate
{
    bool active;
    CBaseEntity *victim;
    int firstHitgroup;
    bool mixedHitgroups;
    bool anyHeadshot;
    int pelletHits;
    int headshotPellets;
    int headshotLethalPellets;
    float totalTraceDamage;
    float totalDamageToHealth;
    float totalDamageAbsorbed;
    float totalArmorDrain;
    float victimHealthBefore;
    bool victimArmorKnown;
    float victimArmorBefore;
    bool victimIsDummy;
    char targetProfileName[64];
    bool dummyArmorApplied;
    bool dummyHeadProtected;
};

struct ShotgunPrimaryShotContext
{
    bool active;
    entvars_t *attacker;
    CBasePlayer *attackerPlayer;
    bool experimentalModeActive;
    float baseDamagePerPellet;
    float headshotScale;
    bool headshotLethal;
    int pelletCount;
    char profileName[64];
    ShotgunPrimaryVictimAggregate victims[kMaxShotgunVictimsPerShot];
};

ShotgunPrimaryShotContext g_shotgunPrimaryShotContext = {};

void PrintWeaponDebugWarningOnce(const char *message)
{
    if (g_weaponDebugLogWarningPrinted)
    {
        return;
    }

    g_weaponDebugLogWarningPrinted = true;
    ALERT(at_console, "[hl-server] weapon debug logging warning: %s\n", message);
}

void ResetGlockPrimaryShotContext()
{
    memset(&g_glockPrimaryShotContext, 0, sizeof(g_glockPrimaryShotContext));
}

void ClearPendingGlockPrimaryHit()
{
    g_glockPrimaryShotContext.pendingHit = false;
    g_glockPrimaryShotContext.victim = NULL;
    g_glockPrimaryShotContext.hitgroup = HITGROUP_GENERIC;
    g_glockPrimaryShotContext.headshot = false;
    g_glockPrimaryShotContext.hitgroupScale = 0.0f;
    g_glockPrimaryShotContext.traceDamage = 0.0f;
    g_glockPrimaryShotContext.headshotLethalApplied = false;
    g_glockPrimaryShotContext.victimHealthBefore = 0.0f;
    g_glockPrimaryShotContext.victimArmorKnown = false;
    g_glockPrimaryShotContext.victimArmorBefore = 0.0f;
    g_glockPrimaryShotContext.victimIsDummy = false;
    g_glockPrimaryShotContext.targetProfileName[0] = '\0';
    g_glockPrimaryShotContext.dummyArmorApplied = false;
    g_glockPrimaryShotContext.dummyHeadProtected = false;
    g_glockPrimaryShotContext.damageRaw = 0.0f;
    g_glockPrimaryShotContext.damageToHealth = 0.0f;
    g_glockPrimaryShotContext.damageAbsorbed = 0.0f;
    g_glockPrimaryShotContext.armorDrain = 0.0f;
}

bool HasMatchingActiveGlockPrimaryShot(entvars_t *pevAttacker)
{
    return g_glockPrimaryShotContext.active && g_glockPrimaryShotContext.attacker == pevAttacker;
}

void ResetMp5PrimaryShotContext()
{
    memset(&g_mp5PrimaryShotContext, 0, sizeof(g_mp5PrimaryShotContext));
}

void ClearPendingMp5PrimaryHit()
{
    g_mp5PrimaryShotContext.pendingHit = false;
    g_mp5PrimaryShotContext.victim = NULL;
    g_mp5PrimaryShotContext.hitgroup = HITGROUP_GENERIC;
    g_mp5PrimaryShotContext.headshot = false;
    g_mp5PrimaryShotContext.hitgroupScale = 0.0f;
    g_mp5PrimaryShotContext.traceDamage = 0.0f;
    g_mp5PrimaryShotContext.headshotLethalApplied = false;
    g_mp5PrimaryShotContext.victimHealthBefore = 0.0f;
    g_mp5PrimaryShotContext.victimArmorKnown = false;
    g_mp5PrimaryShotContext.victimArmorBefore = 0.0f;
    g_mp5PrimaryShotContext.victimIsDummy = false;
    g_mp5PrimaryShotContext.targetProfileName[0] = '\0';
    g_mp5PrimaryShotContext.dummyArmorApplied = false;
    g_mp5PrimaryShotContext.dummyHeadProtected = false;
    g_mp5PrimaryShotContext.damageRaw = 0.0f;
    g_mp5PrimaryShotContext.damageToHealth = 0.0f;
    g_mp5PrimaryShotContext.damageAbsorbed = 0.0f;
    g_mp5PrimaryShotContext.armorDrain = 0.0f;
}

bool HasMatchingActiveMp5PrimaryShot(entvars_t *pevAttacker)
{
    return g_mp5PrimaryShotContext.active && g_mp5PrimaryShotContext.attacker == pevAttacker;
}

void Reset357PrimaryShotContext()
{
    memset(&g_357PrimaryShotContext, 0, sizeof(g_357PrimaryShotContext));
}

void ClearPending357PrimaryHit()
{
    g_357PrimaryShotContext.pendingHit = false;
    g_357PrimaryShotContext.victim = NULL;
    g_357PrimaryShotContext.hitgroup = HITGROUP_GENERIC;
    g_357PrimaryShotContext.headshot = false;
    g_357PrimaryShotContext.hitgroupScale = 0.0f;
    g_357PrimaryShotContext.traceDamage = 0.0f;
    g_357PrimaryShotContext.headshotLethalApplied = false;
    g_357PrimaryShotContext.victimHealthBefore = 0.0f;
    g_357PrimaryShotContext.victimArmorKnown = false;
    g_357PrimaryShotContext.victimArmorBefore = 0.0f;
    g_357PrimaryShotContext.victimIsDummy = false;
    g_357PrimaryShotContext.targetProfileName[0] = '\0';
    g_357PrimaryShotContext.dummyArmorApplied = false;
    g_357PrimaryShotContext.dummyHeadProtected = false;
    g_357PrimaryShotContext.damageRaw = 0.0f;
    g_357PrimaryShotContext.damageToHealth = 0.0f;
    g_357PrimaryShotContext.damageAbsorbed = 0.0f;
    g_357PrimaryShotContext.armorDrain = 0.0f;
}

bool HasMatchingActive357PrimaryShot(entvars_t *pevAttacker)
{
    return g_357PrimaryShotContext.active && g_357PrimaryShotContext.attacker == pevAttacker;
}

void ResetShotgunPrimaryShotContext()
{
    memset(&g_shotgunPrimaryShotContext, 0, sizeof(g_shotgunPrimaryShotContext));
}

bool HasMatchingActiveShotgunPrimaryShot(entvars_t *pevAttacker)
{
    return g_shotgunPrimaryShotContext.active && g_shotgunPrimaryShotContext.attacker == pevAttacker;
}

ShotgunPrimaryVictimAggregate *FindShotgunVictimAggregate(CBaseEntity *pVictim)
{
    if (pVictim == NULL)
    {
        return NULL;
    }

    for (int index = 0; index < kMaxShotgunVictimsPerShot; ++index)
    {
        ShotgunPrimaryVictimAggregate *aggregate = &g_shotgunPrimaryShotContext.victims[index];
        if (aggregate->active && aggregate->victim == pVictim)
        {
            return aggregate;
        }
    }

    for (int index = 0; index < kMaxShotgunVictimsPerShot; ++index)
    {
        ShotgunPrimaryVictimAggregate *aggregate = &g_shotgunPrimaryShotContext.victims[index];
        if (!aggregate->active)
        {
            aggregate->active = true;
            aggregate->victim = pVictim;
            aggregate->firstHitgroup = HITGROUP_GENERIC;
            return aggregate;
        }
    }

    return NULL;
}

void FormatTimestamp(char *buffer, size_t bufferSize)
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

void FormatFileTimestamp(char *buffer, size_t bufferSize)
{
    SYSTEMTIME localTime;
    GetLocalTime(&localTime);

    _snprintf_s(
        buffer,
        bufferSize,
        _TRUNCATE,
        "%04u%02u%02u-%02u%02u%02u",
        localTime.wYear,
        localTime.wMonth,
        localTime.wDay,
        localTime.wHour,
        localTime.wMinute,
        localTime.wSecond);
}

std::string SanitizeLogValue(const char *value)
{
    if (value == NULL || value[0] == '\0')
    {
        return "unknown";
    }

    std::string sanitized;
    sanitized.reserve(strlen(value));

    for (const char *cursor = value; *cursor != '\0'; ++cursor)
    {
        const char current = *cursor;

        if (current == '\r' || current == '\n' || current == '\t')
        {
            sanitized.push_back(' ');
        }
        else if (current == '"')
        {
            sanitized.push_back('\'');
        }
        else
        {
            sanitized.push_back(current);
        }
    }

    if (sanitized.empty())
    {
        return "unknown";
    }

    return sanitized;
}

void AppendOptionalQuotedTelemetryField(std::string *line, const char *key, const char *value)
{
    if (line == NULL || key == NULL || key[0] == '\0' || value == NULL || value[0] == '\0')
    {
        return;
    }

    *line += " ";
    *line += key;
    *line += "=\"";
    *line += SanitizeLogValue(value);
    *line += "\"";
}

const char *GetHitgroupName(int hitgroup)
{
    switch (hitgroup)
    {
    case HITGROUP_HEAD:
        return "head";
    case HITGROUP_CHEST:
        return "chest";
    case HITGROUP_STOMACH:
        return "stomach";
    case HITGROUP_LEFTARM:
        return "leftarm";
    case HITGROUP_RIGHTARM:
        return "rightarm";
    case HITGROUP_LEFTLEG:
        return "leftleg";
    case HITGROUP_RIGHTLEG:
        return "rightleg";
    case HITGROUP_GENERIC:
    default:
        return "generic";
    }
}

std::string GetDirectoryName(const std::string &path)
{
    const std::string::size_type separator = path.find_last_of("\\/");
    if (separator == std::string::npos)
    {
        return std::string();
    }

    return path.substr(0, separator);
}

std::string JoinPath(const std::string &left, const char *right)
{
    if (left.empty())
    {
        return std::string(right);
    }

    if (left[left.length() - 1] == '\\' || left[left.length() - 1] == '/')
    {
        return left + right;
    }

    return left + "\\" + right;
}

bool EnsureDirectoryTreeExists(const std::string &path)
{
    if (path.empty())
    {
        return false;
    }

    std::string partialPath;
    partialPath.reserve(path.length());

    size_t segmentStart = 0;

    if (path.length() >= 3 && path[1] == ':' && (path[2] == '\\' || path[2] == '/'))
    {
        partialPath = path.substr(0, 3);
        segmentStart = 3;
    }
    else if (path.length() >= 2 && path[0] == '\\' && path[1] == '\\')
    {
        const size_t serverEnd = path.find_first_of("\\/", 2);
        if (serverEnd == std::string::npos)
        {
            return false;
        }

        const size_t shareEnd = path.find_first_of("\\/", serverEnd + 1);
        if (shareEnd == std::string::npos)
        {
            return false;
        }

        partialPath = path.substr(0, shareEnd + 1);
        segmentStart = shareEnd + 1;
    }

    while (segmentStart < path.length())
    {
        const size_t nextSeparator = path.find_first_of("\\/", segmentStart);
        const size_t segmentLength = (nextSeparator == std::string::npos) ? (path.length() - segmentStart) : (nextSeparator - segmentStart);

        if (segmentLength > 0)
        {
            if (!partialPath.empty() && partialPath[partialPath.length() - 1] != '\\' && partialPath[partialPath.length() - 1] != '/')
            {
                partialPath.push_back('\\');
            }

            partialPath.append(path, segmentStart, segmentLength);

            if (!CreateDirectoryA(partialPath.c_str(), NULL))
            {
                const DWORD error = GetLastError();
                if (error != ERROR_ALREADY_EXISTS)
                {
                    return false;
                }
            }
        }

        if (nextSeparator == std::string::npos)
        {
            break;
        }

        segmentStart = nextSeparator + 1;
    }

    return true;
}

bool ResolveLoadedModulePath(std::string *modulePath)
{
    HMODULE moduleHandle = NULL;

    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&ResolveLoadedModulePath),
            &moduleHandle))
    {
        return false;
    }

    char pathBuffer[MAX_PATH];
    const DWORD pathLength = GetModuleFileNameA(moduleHandle, pathBuffer, sizeof(pathBuffer));
    if (pathLength == 0 || pathLength >= sizeof(pathBuffer))
    {
        return false;
    }

    *modulePath = pathBuffer;
    return true;
}

bool ResolveWeaponDebugLogPath(std::string *logPath)
{
    std::string modulePath;
    if (!ResolveLoadedModulePath(&modulePath))
    {
        return false;
    }

    const std::string dllsDirectory = GetDirectoryName(modulePath);
    const std::string valveDirectory = GetDirectoryName(dllsDirectory);
    const std::string runtimeDirectory = GetDirectoryName(valveDirectory);
    const std::string testbedDirectory = GetDirectoryName(runtimeDirectory);
    const std::string logsDirectory = JoinPath(testbedDirectory, "logs");

    if (!EnsureDirectoryTreeExists(logsDirectory))
    {
        return false;
    }

    char timestamp[32];
    FormatFileTimestamp(timestamp, sizeof(timestamp));

    *logPath = JoinPath(logsDirectory, UTIL_VarArgs("weapon-debug-%s.log", timestamp));
    return true;
}

const char *GetSafeMapName()
{
    if (gpGlobals == NULL || gpGlobals->mapname == 0)
    {
        return "unknown";
    }

    return STRING(gpGlobals->mapname);
}

const char *GetSessionProfileName()
{
    const char *weaponUnderTest = ExpWeaponUnderTest();
    if (weaponUnderTest != NULL && weaponUnderTest[0] != '\0' && _stricmp(weaponUnderTest, "shotgun") == 0)
    {
        return ExpShotgunProfileName();
    }

    if (weaponUnderTest != NULL && weaponUnderTest[0] != '\0' && _stricmp(weaponUnderTest, "357") == 0)
    {
        return Exp357ProfileName();
    }

    if (weaponUnderTest != NULL && weaponUnderTest[0] != '\0' && _stricmp(weaponUnderTest, "mp5") == 0)
    {
        return ExpMP5ProfileName();
    }

    return ExpGlockProfileName();
}

std::string NormalizePathForLogging(const std::string &path)
{
    std::string normalized = path;
    for (size_t index = 0; index < normalized.length(); ++index)
    {
        if (normalized[index] == '\\')
        {
            normalized[index] = '/';
        }
    }

    return normalized;
}

void WriteTelemetryLine(const char *line)
{
    ALERT(at_console, "%s\n", line);

    if (g_weaponDebugLogFile != NULL)
    {
        fputs(line, g_weaponDebugLogFile);
        fputc('\n', g_weaponDebugLogFile);
        fflush(g_weaponDebugLogFile);
    }
}

void PrintLogPathNotice()
{
    if (g_weaponDebugLogPathNoticePrinted || g_weaponDebugLogPath.empty())
    {
        return;
    }

    g_weaponDebugLogPathNoticePrinted = true;
    ALERT(at_console, "[hl-server] weapon debug telemetry file: %s\n", g_weaponDebugLogPath.c_str());
}

std::string GetSafePlayerName(CBasePlayer *pPlayer)
{
    if (pPlayer == NULL || pPlayer->pev == NULL || pPlayer->pev->netname == 0)
    {
        return "unknown";
    }

    return SanitizeLogValue(STRING(pPlayer->pev->netname));
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

std::string GetSafeEntityName(CBaseEntity *pEntity)
{
    if (pEntity == NULL)
    {
        return "unknown";
    }

    if (IsExpGlockLabDummyEntity(pEntity))
    {
        if (pEntity->pev != NULL && pEntity->pev->netname != 0)
        {
            return SanitizeLogValue(STRING(pEntity->pev->netname));
        }

        return SanitizeLogValue(ExpGlockLabDummyDisplayName());
    }

    if (pEntity->IsPlayer())
    {
        return GetSafePlayerName((CBasePlayer *)pEntity);
    }

    if (pEntity->pev == NULL || pEntity->pev->classname == 0)
    {
        return "unknown";
    }

    return SanitizeLogValue(STRING(pEntity->pev->classname));
}

std::string GetSafeEntityClassname(CBaseEntity *pEntity)
{
    if (pEntity == NULL || pEntity->pev == NULL || pEntity->pev->classname == 0)
    {
        return "unknown";
    }

    return SanitizeLogValue(STRING(pEntity->pev->classname));
}

std::string GetSafeEntityModel(CBaseEntity *pEntity)
{
    if (pEntity == NULL || pEntity->pev == NULL || pEntity->pev->model == 0)
    {
        return "unknown";
    }

    return SanitizeLogValue(STRING(pEntity->pev->model));
}

const char *GetEntityKind(CBaseEntity *pEntity)
{
    if (pEntity == NULL)
    {
        return "unknown";
    }

    if (IsExpGlockLabDummyEntity(pEntity))
    {
        return "dummy";
    }

    if (pEntity->IsPlayer())
    {
        return "player";
    }

    if (pEntity->pev != NULL && (pEntity->pev->flags & FL_MONSTER))
    {
        return "npc";
    }

    return "entity";
}

int GetEntityIndex(CBaseEntity *pEntity)
{
    if (pEntity == NULL || pEntity->edict() == NULL)
    {
        return -1;
    }

    return ENTINDEX(pEntity->edict());
}

int GetEntityUserId(CBaseEntity *pEntity)
{
    if (pEntity == NULL || !pEntity->IsPlayer() || pEntity->edict() == NULL)
    {
        return -1;
    }

    return GETPLAYERUSERID(pEntity->edict());
}

void FormatOptionalFloat(char *buffer, size_t bufferSize, bool available, float value, int digits)
{
    if (!available)
    {
        strcpy_s(buffer, bufferSize, "na");
        return;
    }

    _snprintf_s(buffer, bufferSize, _TRUNCATE, "%.*f", digits, value);
}

void FormatVector3(char *buffer, size_t bufferSize, const Vector &value)
{
    _snprintf_s(buffer, bufferSize, _TRUNCATE, "%.1f %.1f %.1f", value.x, value.y, value.z);
}

void EnsureWeaponDebugLogOpen()
{
    if (g_weaponDebugLogOpenAttempted)
    {
        return;
    }

    g_weaponDebugLogOpenAttempted = true;

    std::string resolvedLogPath;
    if (!ResolveWeaponDebugLogPath(&resolvedLogPath))
    {
        PrintWeaponDebugWarningOnce("unable to resolve a disposable testbed log path; telemetry will stay console-only");
        return;
    }

    g_weaponDebugLogPath = NormalizePathForLogging(resolvedLogPath);
    g_weaponDebugLogFile = fopen(resolvedLogPath.c_str(), "a");
    if (g_weaponDebugLogFile == NULL)
    {
        PrintWeaponDebugWarningOnce("failed to open the disposable weapon debug log file; telemetry will stay console-only");
        return;
    }

    PrintLogPathNotice();

    char timestamp[64];
    char sessionLine[4096];
    FormatTimestamp(timestamp, sizeof(timestamp));
    _snprintf_s(
        sessionLine,
        sizeof(sessionLine),
        _TRUNCATE,
        "[weaponlog] type=session ts=%s map=%s event=weapon_debug_session status=ready game=valve file=\"%s\" profile=\"%s\" glock_profile=\"%s\" mp5_profile=\"%s\" 357_profile=\"%s\" tapfire=%d move_scale=%.4f firstshot_enabled=%d recovery=%.3f base=%.4f ground_move_penalty=%.4f air_move_penalty=%.4f duck_penalty_scale=%.4f firstshot_speed=%.1f max_spread=%.4f sv_exp_glock_primary_damage=%.4f sv_exp_glock_primary_headshot_scale=%.4f sv_exp_glock_primary_headshot_lethal=%d sv_exp_mp5_primary_enabled=%d sv_exp_mp5_primary_base_spread=%.4f sv_exp_mp5_primary_ground_move_penalty=%.4f sv_exp_mp5_primary_air_move_penalty=%.4f sv_exp_mp5_primary_duck_penalty_scale=%.4f sv_exp_mp5_primary_burst_growth=%.4f sv_exp_mp5_primary_burst_max_additional_spread=%.4f sv_exp_mp5_primary_spread_recovery=%.4f sv_exp_mp5_primary_damage=%.4f sv_exp_mp5_primary_headshot_scale=%.4f sv_exp_mp5_primary_headshot_lethal=%d sv_exp_mp5_primary_first_shot_accuracy=%d sv_exp_mp5_primary_first_shot_speed_threshold=%.1f sv_exp_mp5_primary_max_spread=%.4f sv_exp_mp5_lab_loadout=%d sv_exp_mp5_lab_ammo=%.1f sv_exp_mp5_lab_autoswitch=%d sv_exp_357_primary_enabled=%d sv_exp_357_primary_base_spread=%.4f sv_exp_357_primary_ground_move_penalty=%.4f sv_exp_357_primary_air_move_penalty=%.4f sv_exp_357_primary_duck_penalty_scale=%.4f sv_exp_357_primary_first_shot_accuracy=%d sv_exp_357_primary_first_shot_speed_threshold=%.1f sv_exp_357_primary_spread_recovery=%.4f sv_exp_357_primary_max_spread=%.4f sv_exp_357_primary_damage=%.4f sv_exp_357_primary_headshot_scale=%.4f sv_exp_357_primary_headshot_lethal=%d sv_exp_357_lab_loadout=%d sv_exp_357_lab_ammo=%.1f sv_exp_357_lab_autoswitch=%d sv_exp_glock_lab_dummy=%d sv_exp_glock_lab_target_profile_name=\"%s\" sv_exp_glock_lab_dummy_armor=%.1f sv_exp_glock_lab_dummy_head_protected=%d sv_exp_glock_lab_dummy_armor_health_fraction=%.3f sv_exp_glock_lab_dummy_armor_drain_scale=%.3f cfg_mode=%d",
        timestamp,
        SanitizeLogValue(GetSafeMapName()).c_str(),
        g_weaponDebugLogPath.c_str(),
        SanitizeLogValue(GetSessionProfileName()).c_str(),
        SanitizeLogValue(ExpGlockProfileName()).c_str(),
        SanitizeLogValue(ExpMP5ProfileName()).c_str(),
        SanitizeLogValue(Exp357ProfileName()).c_str(),
        ExpPistolTapFireEnabled() ? 1 : 0,
        ExpMoveSpreadScale(),
        ExpFirstShotAccuracyEnabled() ? 1 : 0,
        ExpSpreadRecoverySeconds(),
        ExpGlockPrimaryBaseSpread(),
        ExpGlockPrimaryGroundMovePenalty(),
        ExpGlockPrimaryAirMovePenalty(),
        ExpGlockPrimaryDuckPenaltyScale(),
        ExpGlockPrimaryFirstShotSpeedThreshold(),
        ExpGlockPrimaryMaxSpread(),
        ExpGlockPrimaryDamage(),
        ExpGlockPrimaryHeadshotScale(),
        ExpGlockPrimaryHeadshotLethal() ? 1 : 0,
        ExpMP5PrimaryEnabled() ? 1 : 0,
        ExpMP5PrimaryBaseSpread(),
        ExpMP5PrimaryGroundMovePenalty(),
        ExpMP5PrimaryAirMovePenalty(),
        ExpMP5PrimaryDuckPenaltyScale(),
        ExpMP5PrimaryBurstGrowth(),
        ExpMP5PrimaryBurstMaxAdditionalSpread(),
        ExpMP5PrimarySpreadRecoverySeconds(),
        ExpMP5PrimaryDamage(),
        ExpMP5PrimaryHeadshotScale(),
        ExpMP5PrimaryHeadshotLethal() ? 1 : 0,
        ExpMP5PrimaryFirstShotAccuracyEnabled() ? 1 : 0,
        ExpMP5PrimaryFirstShotSpeedThreshold(),
        ExpMP5PrimaryMaxSpread(),
        ExpMP5LabLoadoutEnabled() ? 1 : 0,
        ExpMP5LabAmmo(),
        ExpMP5LabAutoswitch() ? 1 : 0,
        Exp357PrimaryEnabled() ? 1 : 0,
        Exp357PrimaryBaseSpread(),
        Exp357PrimaryGroundMovePenalty(),
        Exp357PrimaryAirMovePenalty(),
        Exp357PrimaryDuckPenaltyScale(),
        Exp357PrimaryFirstShotAccuracyEnabled() ? 1 : 0,
        Exp357PrimaryFirstShotSpeedThreshold(),
        Exp357PrimarySpreadRecoverySeconds(),
        Exp357PrimaryMaxSpread(),
        Exp357PrimaryDamage(),
        Exp357PrimaryHeadshotScale(),
        Exp357PrimaryHeadshotLethal() ? 1 : 0,
        Exp357LabLoadoutEnabled() ? 1 : 0,
        Exp357LabAmmo(),
        Exp357LabAutoswitch() ? 1 : 0,
        ExpGlockLabDummyEnabled() ? 1 : 0,
        SanitizeLogValue(ExpGlockLabTargetProfileName()).c_str(),
        ExpGlockLabDummyArmor(),
        ExpGlockLabDummyHeadProtected() ? 1 : 0,
        ExpGlockLabDummyArmorHealthFraction(),
        ExpGlockLabDummyArmorDrainScale(),
        ExpCfgDrivenModeActive() ? 1 : 0);
    std::string sessionTelemetryLine = sessionLine;
    AppendOptionalQuotedTelemetryField(&sessionTelemetryLine, "weapon_under_test", ExpWeaponUnderTest());
    AppendOptionalQuotedTelemetryField(&sessionTelemetryLine, "session_tag", ExpSessionTag());
    AppendOptionalQuotedTelemetryField(&sessionTelemetryLine, "matrix_name", ExpMatrixName());
    AppendOptionalQuotedTelemetryField(&sessionTelemetryLine, "matrix_step", ExpMatrixStep());
    AppendOptionalQuotedTelemetryField(&sessionTelemetryLine, "cfg_active_profile", ExpActiveCfgProfile());
    AppendOptionalQuotedTelemetryField(&sessionTelemetryLine, "cfg_last_successful_profile", ExpLastSuccessfulCfgProfile());
    AppendOptionalQuotedTelemetryField(&sessionTelemetryLine, "cfg_last_action", ExpLastCfgAction());
    AppendOptionalQuotedTelemetryField(&sessionTelemetryLine, "cfg_last_applied_at", ExpLastCfgAppliedAt());
    WriteTelemetryLine(sessionTelemetryLine.c_str());
}
}

void EnsureWeaponDebugLogReady()
{
    if (!ExpDebugWeaponLogEnabled())
    {
        return;
    }

    EnsureWeaponDebugLogOpen();
}

void LogLiveCfgCommand(const char *action, const char *requestedPath, const char *execPath, const char *resolvedPath, bool success, const char *details)
{
    if (!ExpDebugWeaponLogEnabled())
    {
        return;
    }

    EnsureWeaponDebugLogOpen();

    char timestamp[64];
    char line[2048];
    FormatTimestamp(timestamp, sizeof(timestamp));

    _snprintf_s(
        line,
        sizeof(line),
        _TRUNCATE,
        "[weaponlog] type=cfg ts=%s map=%s action=%s status=%s cfg_mode=%d request=\"%s\" exec_path=\"%s\" resolved_path=\"%s\" target_profile=\"%s\"",
        timestamp,
        SanitizeLogValue(GetSafeMapName()).c_str(),
        SanitizeLogValue(action).c_str(),
        success ? "success" : "failure",
        ExpCfgDrivenModeActive() ? 1 : 0,
        SanitizeLogValue(requestedPath).c_str(),
        SanitizeLogValue(execPath).c_str(),
        SanitizeLogValue(resolvedPath).c_str(),
        SanitizeLogValue(ExpGlockLabTargetProfileName()).c_str());

    std::string telemetryLine = line;
    AppendOptionalQuotedTelemetryField(&telemetryLine, "details", details);
    AppendOptionalQuotedTelemetryField(&telemetryLine, "weapon_under_test", ExpWeaponUnderTest());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "session_tag", ExpSessionTag());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "glock_profile", ExpGlockProfileName());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "mp5_profile", ExpMP5ProfileName());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "cfg_active_profile", ExpActiveCfgProfile());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "cfg_last_successful_profile", ExpLastSuccessfulCfgProfile());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "cfg_last_action", ExpLastCfgAction());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "cfg_last_applied_at", ExpLastCfgAppliedAt());
    WriteTelemetryLine(telemetryLine.c_str());
}

void LogRoundEvent(const char *event, const char *state, int roundNumber, int connectedPlayers, int alivePlayers, CBasePlayer *pWinner, const char *reason)
{
    if (!ExpDebugWeaponLogEnabled())
    {
        return;
    }

    EnsureWeaponDebugLogOpen();

    char timestamp[64];
    char line[2048];
    FormatTimestamp(timestamp, sizeof(timestamp));
    const char *winnerTeamName = "none";
    if ((pWinner != NULL || (event != NULL && strcmp(event, "round_end") == 0)) &&
        ExpRoundLastWinnerTeamName()[0] != '\0')
    {
        winnerTeamName = ExpRoundLastWinnerTeamName();
    }

    _snprintf_s(
        line,
        sizeof(line),
        _TRUNCATE,
        "[weaponlog] type=round ts=%s map=%s event=%s state=%s round=%d connected_players=%d alive_players=%d team_mode=%d teamplay=%d team1_name=\"%s\" team2_name=\"%s\" team1_connected=%d team2_connected=%d unassigned_connected=%d team1_alive=%d team2_alive=%d unassigned_alive=%d winner=\"%s\" winner_team=\"%s\" winner_entindex=%d winner_userid=%d no_respawn=%d friendlyfire=%d loadout_mode=\"%s\" weapon_profile=\"%s\"",
        timestamp,
        SanitizeLogValue(GetSafeMapName()).c_str(),
        SanitizeLogValue(event).c_str(),
        SanitizeLogValue(state).c_str(),
        roundNumber,
        connectedPlayers,
        alivePlayers,
        ExpTeamRoundModeEnabled() ? 1 : 0,
        ExpTeamRoundTeamplayEnabled() ? 1 : 0,
        SanitizeLogValue(ExpTeamRoundTeam1Name()).c_str(),
        SanitizeLogValue(ExpTeamRoundTeam2Name()).c_str(),
        ExpRoundConnectedPlayersForTeam(1),
        ExpRoundConnectedPlayersForTeam(2),
        ExpRoundUnassignedConnectedPlayers(),
        ExpRoundAlivePlayersForTeam(1),
        ExpRoundAlivePlayersForTeam(2),
        ExpRoundUnassignedAlivePlayers(),
        GetSafePlayerName(pWinner).c_str(),
        SanitizeLogValue(winnerTeamName).c_str(),
        GetPlayerEntityIndex(pWinner),
        GetPlayerUserId(pWinner),
        ExpRoundNoRespawn() ? 1 : 0,
        ExpRoundFriendlyFireEnabled() ? 1 : 0,
        SanitizeLogValue(ExpRoundLoadoutMode()).c_str(),
        SanitizeLogValue(ExpRoundWeaponProfile()[0] != '\0' ? ExpRoundWeaponProfile() : "none").c_str());

    std::string telemetryLine = line;
    AppendOptionalQuotedTelemetryField(&telemetryLine, "reason", reason);
    AppendOptionalQuotedTelemetryField(&telemetryLine, "weapon_under_test", ExpWeaponUnderTest());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "session_tag", ExpSessionTag());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "cfg_active_profile", ExpActiveCfgProfile());
    WriteTelemetryLine(telemetryLine.c_str());
}

void LogLiveLabConsoleMessage(const char *line)
{
    if (!ExpDebugWeaponLogEnabled() || line == NULL || line[0] == '\0')
    {
        return;
    }

    EnsureWeaponDebugLogOpen();
    if (g_weaponDebugLogFile == NULL)
    {
        return;
    }

    char timestamp[64];
    char entry[1024];
    FormatTimestamp(timestamp, sizeof(timestamp));

    _snprintf_s(
        entry,
        sizeof(entry),
        _TRUNCATE,
        "[weaponlog] type=lab_console ts=%s map=%s cfg_mode=%d target_profile=\"%s\"",
        timestamp,
        SanitizeLogValue(GetSafeMapName()).c_str(),
        ExpCfgDrivenModeActive() ? 1 : 0,
        SanitizeLogValue(ExpGlockLabTargetProfileName()).c_str());

    std::string telemetryLine = entry;
    AppendOptionalQuotedTelemetryField(&telemetryLine, "message", line);
    AppendOptionalQuotedTelemetryField(&telemetryLine, "weapon_under_test", ExpWeaponUnderTest());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "session_tag", ExpSessionTag());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "glock_profile", ExpGlockProfileName());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "mp5_profile", ExpMP5ProfileName());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "cfg_active_profile", ExpActiveCfgProfile());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "cfg_last_successful_profile", ExpLastSuccessfulCfgProfile());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "cfg_last_action", ExpLastCfgAction());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "cfg_last_applied_at", ExpLastCfgAppliedAt());

    fputs(telemetryLine.c_str(), g_weaponDebugLogFile);
    fputc('\n', g_weaponDebugLogFile);
    fflush(g_weaponDebugLogFile);
}

void LogGlockLabDummySpawn(CBaseEntity *pDummy, CBasePlayer *pAnchorPlayer, bool respawn, const Vector &origin, const Vector &angles, const char *source, const char *candidate, const char *spotName, const char *spotStorage)
{
    if (!ExpDebugWeaponLogEnabled())
    {
        return;
    }

    EnsureWeaponDebugLogOpen();

    char timestamp[64];
    char originValue[64];
    char line[2048];
    FormatTimestamp(timestamp, sizeof(timestamp));
    FormatVector3(originValue, sizeof(originValue), origin);

    _snprintf_s(
        line,
        sizeof(line),
        _TRUNCATE,
        "[weaponlog] type=%s ts=%s map=%s dummy=\"%s\" entindex=%d dummy_class=%s dummy_model=\"%s\" health=%.1f autorespawn=%d respawn_delay=%.2f spawn_distance=%.1f anchor=\"%s\" anchor_entindex=%d anchor_userid=%d origin=\"%s\" yaw=%.1f source=\"%s\" candidate=\"%s\" profile=\"%s\" target_profile=\"%s\" spawn_health=%.1f spawn_armor=%.1f head_protected=%d armor_health_fraction=%.3f armor_drain_scale=%.3f",
        respawn ? "dummy_respawn" : "dummy_spawn",
        timestamp,
        SanitizeLogValue(GetSafeMapName()).c_str(),
        GetSafeEntityName(pDummy).c_str(),
        GetEntityIndex(pDummy),
        GetSafeEntityClassname(pDummy).c_str(),
        GetSafeEntityModel(pDummy).c_str(),
        pDummy != NULL && pDummy->pev != NULL ? pDummy->pev->max_health : ExpGlockLabDummyHealth(),
        ExpGlockLabDummyAutoRespawnEnabled() ? 1 : 0,
        ExpGlockLabDummyRespawnDelaySeconds(),
        ExpGlockLabDummySpawnDistance(),
        GetSafePlayerName(pAnchorPlayer).c_str(),
        GetPlayerEntityIndex(pAnchorPlayer),
        GetPlayerUserId(pAnchorPlayer),
        originValue,
        angles.y,
        SanitizeLogValue(source).c_str(),
        SanitizeLogValue(candidate).c_str(),
        SanitizeLogValue(GetSessionProfileName()).c_str(),
        SanitizeLogValue(ExpGlockLabTargetProfileName()).c_str(),
        pDummy != NULL && pDummy->pev != NULL ? pDummy->pev->max_health : ExpGlockLabDummyHealth(),
        pDummy != NULL && pDummy->pev != NULL ? pDummy->pev->armorvalue : ExpGlockLabDummyArmor(),
        ExpGlockLabDummyHeadProtected() ? 1 : 0,
        ExpGlockLabDummyArmorHealthFraction(),
        ExpGlockLabDummyArmorDrainScale());

    std::string telemetryLine = line;
    AppendOptionalQuotedTelemetryField(&telemetryLine, "weapon_under_test", ExpWeaponUnderTest());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "spot_name", spotName);
    AppendOptionalQuotedTelemetryField(&telemetryLine, "spot_storage", spotStorage);
    WriteTelemetryLine(telemetryLine.c_str());
}

void LogGlockLabDummySpawnFailed(CBasePlayer *pAnchorPlayer, const char *source, const char *candidate, const char *code, const char *reason, const Vector *pOrigin, const Vector *pAngles, const char *spotName, const char *spotStorage)
{
    if (!ExpDebugWeaponLogEnabled())
    {
        return;
    }

    EnsureWeaponDebugLogOpen();

    char timestamp[64];
    char originValue[64];
    char line[2048];
    FormatTimestamp(timestamp, sizeof(timestamp));
    FormatVector3(originValue, sizeof(originValue), pOrigin != NULL ? *pOrigin : g_vecZero);

    _snprintf_s(
        line,
        sizeof(line),
        _TRUNCATE,
        "[weaponlog] type=dummy_spawn_failed ts=%s map=%s anchor=\"%s\" anchor_entindex=%d anchor_userid=%d source=\"%s\" candidate=\"%s\" code=\"%s\" reason=\"%s\" origin=\"%s\" yaw=%.1f profile=\"%s\" target_profile=\"%s\"",
        timestamp,
        SanitizeLogValue(GetSafeMapName()).c_str(),
        GetSafePlayerName(pAnchorPlayer).c_str(),
        GetPlayerEntityIndex(pAnchorPlayer),
        GetPlayerUserId(pAnchorPlayer),
        SanitizeLogValue(source).c_str(),
        SanitizeLogValue(candidate).c_str(),
        SanitizeLogValue(code).c_str(),
        SanitizeLogValue(reason).c_str(),
        originValue,
        pAngles != NULL ? pAngles->y : 0.0f,
        SanitizeLogValue(GetSessionProfileName()).c_str(),
        SanitizeLogValue(ExpGlockLabTargetProfileName()).c_str());

    std::string telemetryLine = line;
    AppendOptionalQuotedTelemetryField(&telemetryLine, "weapon_under_test", ExpWeaponUnderTest());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "spot_name", spotName);
    AppendOptionalQuotedTelemetryField(&telemetryLine, "spot_storage", spotStorage);
    WriteTelemetryLine(telemetryLine.c_str());
}

void LogGlockLabDummyMark(const char *action, CBasePlayer *pAnchorPlayer, const Vector &origin, const Vector &angles, const char *details, const char *spotName, const char *spotStorage)
{
    if (!ExpDebugWeaponLogEnabled())
    {
        return;
    }

    EnsureWeaponDebugLogOpen();

    char timestamp[64];
    char originValue[64];
    char line[2048];
    FormatTimestamp(timestamp, sizeof(timestamp));
    FormatVector3(originValue, sizeof(originValue), origin);

    _snprintf_s(
        line,
        sizeof(line),
        _TRUNCATE,
        "[weaponlog] type=%s ts=%s map=%s anchor=\"%s\" anchor_entindex=%d anchor_userid=%d origin=\"%s\" yaw=%.1f details=\"%s\" profile=\"%s\" target_profile=\"%s\"",
        action != NULL && action[0] != '\0' ? action : "target_mark",
        timestamp,
        SanitizeLogValue(GetSafeMapName()).c_str(),
        GetSafePlayerName(pAnchorPlayer).c_str(),
        GetPlayerEntityIndex(pAnchorPlayer),
        GetPlayerUserId(pAnchorPlayer),
        originValue,
        angles.y,
        SanitizeLogValue(details).c_str(),
        SanitizeLogValue(GetSessionProfileName()).c_str(),
        SanitizeLogValue(ExpGlockLabTargetProfileName()).c_str());

    std::string telemetryLine = line;
    AppendOptionalQuotedTelemetryField(&telemetryLine, "weapon_under_test", ExpWeaponUnderTest());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "spot_name", spotName);
    AppendOptionalQuotedTelemetryField(&telemetryLine, "spot_storage", spotStorage);
    WriteTelemetryLine(telemetryLine.c_str());
}

void LogGlockLabDummyReposition(CBaseEntity *pDummy, CBasePlayer *pAnchorPlayer, const Vector &origin, const Vector &angles, const char *reason, const char *source, const char *candidate, const char *spotName, const char *spotStorage)
{
    if (!ExpDebugWeaponLogEnabled())
    {
        return;
    }

    EnsureWeaponDebugLogOpen();

    char timestamp[64];
    char originValue[64];
    char line[2048];
    FormatTimestamp(timestamp, sizeof(timestamp));
    FormatVector3(originValue, sizeof(originValue), origin);

    _snprintf_s(
        line,
        sizeof(line),
        _TRUNCATE,
        "[weaponlog] type=dummy_reposition ts=%s map=%s dummy=\"%s\" entindex=%d dummy_class=%s dummy_model=\"%s\" reason=\"%s\" source=\"%s\" candidate=\"%s\" health=%.1f armor=%.1f head_protected=%d anchor=\"%s\" anchor_entindex=%d anchor_userid=%d origin=\"%s\" yaw=%.1f profile=\"%s\" target_profile=\"%s\"",
        timestamp,
        SanitizeLogValue(GetSafeMapName()).c_str(),
        GetSafeEntityName(pDummy).c_str(),
        GetEntityIndex(pDummy),
        GetSafeEntityClassname(pDummy).c_str(),
        GetSafeEntityModel(pDummy).c_str(),
        SanitizeLogValue(reason).c_str(),
        SanitizeLogValue(source).c_str(),
        SanitizeLogValue(candidate).c_str(),
        pDummy != NULL && pDummy->pev != NULL ? pDummy->pev->health : ExpGlockLabDummyHealth(),
        pDummy != NULL && pDummy->pev != NULL ? pDummy->pev->armorvalue : ExpGlockLabDummyArmor(),
        ExpGlockLabDummyHeadProtected() ? 1 : 0,
        GetSafePlayerName(pAnchorPlayer).c_str(),
        GetPlayerEntityIndex(pAnchorPlayer),
        GetPlayerUserId(pAnchorPlayer),
        originValue,
        angles.y,
        SanitizeLogValue(GetSessionProfileName()).c_str(),
        SanitizeLogValue(ExpGlockLabTargetProfileName()).c_str());

    std::string telemetryLine = line;
    AppendOptionalQuotedTelemetryField(&telemetryLine, "weapon_under_test", ExpWeaponUnderTest());
    AppendOptionalQuotedTelemetryField(&telemetryLine, "spot_name", spotName);
    AppendOptionalQuotedTelemetryField(&telemetryLine, "spot_storage", spotStorage);
    WriteTelemetryLine(telemetryLine.c_str());
}

void LogGlockLabDummyClear(CBaseEntity *pDummy, const char *reason)
{
    if (!ExpDebugWeaponLogEnabled())
    {
        return;
    }

    EnsureWeaponDebugLogOpen();

    char timestamp[64];
    char originValue[64];
    char line[1536];
    FormatTimestamp(timestamp, sizeof(timestamp));
    FormatVector3(originValue, sizeof(originValue), pDummy != NULL && pDummy->pev != NULL ? pDummy->pev->origin : g_vecZero);

    _snprintf_s(
        line,
        sizeof(line),
        _TRUNCATE,
        "[weaponlog] type=dummy_clear ts=%s map=%s dummy=\"%s\" entindex=%d dummy_class=%s dummy_model=\"%s\" reason=\"%s\" health=%.1f armor=%.1f head_protected=%d origin=\"%s\" yaw=%.1f profile=\"%s\" target_profile=\"%s\"",
        timestamp,
        SanitizeLogValue(GetSafeMapName()).c_str(),
        GetSafeEntityName(pDummy).c_str(),
        GetEntityIndex(pDummy),
        GetSafeEntityClassname(pDummy).c_str(),
        GetSafeEntityModel(pDummy).c_str(),
        SanitizeLogValue(reason).c_str(),
        pDummy != NULL && pDummy->pev != NULL ? pDummy->pev->health : 0.0f,
        pDummy != NULL && pDummy->pev != NULL ? pDummy->pev->armorvalue : 0.0f,
        ExpGlockLabDummyHeadProtected() ? 1 : 0,
        originValue,
        pDummy != NULL && pDummy->pev != NULL ? pDummy->pev->angles.y : 0.0f,
        SanitizeLogValue(GetSessionProfileName()).c_str(),
        SanitizeLogValue(ExpGlockLabTargetProfileName()).c_str());

    std::string telemetryLine = line;
    AppendOptionalQuotedTelemetryField(&telemetryLine, "weapon_under_test", ExpWeaponUnderTest());
    WriteTelemetryLine(telemetryLine.c_str());
}

void LogAcceptedGlockPrimaryShot(CBasePlayer *pPlayer, const GlockAcceptedShotTelemetry &telemetry)
{
    if (!ExpDebugWeaponLogEnabled())
    {
        return;
    }

    EnsureWeaponDebugLogOpen();

    char timestamp[64];
    char deltaPreviousShot[32];
    char line[1024];
    FormatTimestamp(timestamp, sizeof(timestamp));

    if (telemetry.hasPreviousAcceptedShot)
    {
        _snprintf_s(deltaPreviousShot, sizeof(deltaPreviousShot), _TRUNCATE, "%.3f", telemetry.timeSincePreviousAcceptedShot);
    }
    else
    {
        strcpy_s(deltaPreviousShot, sizeof(deltaPreviousShot), "na");
    }

    _snprintf_s(
        line,
        sizeof(line),
        _TRUNCATE,
        "[weaponlog] type=accepted ts=%s map=%s player=\"%s\" entindex=%d userid=%d weapon=glock fire=primary experimental=%d tapfire=%d firstshot=%d spread=%.4f base=%.4f move_penalty=%.4f speed2d=%.1f maxspeed=%.1f grounded=%d ducking=%d delta_prev=%s clip=%d",
        timestamp,
        SanitizeLogValue(GetSafeMapName()).c_str(),
        GetSafePlayerName(pPlayer).c_str(),
        GetPlayerEntityIndex(pPlayer),
        GetPlayerUserId(pPlayer),
        telemetry.experimentalModeActive ? 1 : 0,
        telemetry.tapFireActive ? 1 : 0,
        telemetry.firstShotAccuracyApplied ? 1 : 0,
        telemetry.spread,
        telemetry.baseSpread,
        telemetry.movementPenalty,
        telemetry.horizontalSpeed,
        telemetry.maxSpeedForNormalization,
        telemetry.grounded ? 1 : 0,
        telemetry.ducking ? 1 : 0,
        deltaPreviousShot,
        telemetry.clipAfterShot);

    WriteTelemetryLine(line);
}

void LogAcceptedMp5PrimaryShot(CBasePlayer *pPlayer, const Mp5AcceptedShotTelemetry &telemetry)
{
    if (!ExpDebugWeaponLogEnabled())
    {
        return;
    }

    EnsureWeaponDebugLogOpen();

    char timestamp[64];
    char deltaPreviousShot[32];
    char line[1408];
    FormatTimestamp(timestamp, sizeof(timestamp));

    if (telemetry.hasPreviousAcceptedShot)
    {
        _snprintf_s(deltaPreviousShot, sizeof(deltaPreviousShot), _TRUNCATE, "%.3f", telemetry.timeSincePreviousAcceptedShot);
    }
    else
    {
        strcpy_s(deltaPreviousShot, sizeof(deltaPreviousShot), "na");
    }

    _snprintf_s(
        line,
        sizeof(line),
        _TRUNCATE,
        "[weaponlog] type=accepted ts=%s map=%s player=\"%s\" entindex=%d userid=%d weapon=mp5 fire=primary experimental=%d profile=\"%s\" firstshot=%d spread=%.4f base=%.4f move_penalty=%.4f burst_additional_spread=%.4f burst_index=%d speed2d=%.1f maxspeed=%.1f grounded=%d ducking=%d delta_prev=%s clip=%d",
        timestamp,
        SanitizeLogValue(GetSafeMapName()).c_str(),
        GetSafePlayerName(pPlayer).c_str(),
        GetPlayerEntityIndex(pPlayer),
        GetPlayerUserId(pPlayer),
        telemetry.experimentalModeActive ? 1 : 0,
        SanitizeLogValue(ExpMP5ProfileName()).c_str(),
        telemetry.firstShotAccuracyApplied ? 1 : 0,
        telemetry.spread,
        telemetry.baseSpread,
        telemetry.movementPenalty,
        telemetry.burstAddedSpread,
        telemetry.burstShotIndex,
        telemetry.horizontalSpeed,
        telemetry.maxSpeedForNormalization,
        telemetry.grounded ? 1 : 0,
        telemetry.ducking ? 1 : 0,
        deltaPreviousShot,
        telemetry.clipAfterShot);

    WriteTelemetryLine(line);
}

void LogAccepted357PrimaryShot(CBasePlayer *pPlayer, const Weapon357AcceptedShotTelemetry &telemetry)
{
    if (!ExpDebugWeaponLogEnabled())
    {
        return;
    }

    EnsureWeaponDebugLogOpen();

    char timestamp[64];
    char deltaPreviousShot[32];
    char line[1024];
    FormatTimestamp(timestamp, sizeof(timestamp));

    if (telemetry.hasPreviousAcceptedShot)
    {
        _snprintf_s(deltaPreviousShot, sizeof(deltaPreviousShot), _TRUNCATE, "%.3f", telemetry.timeSincePreviousAcceptedShot);
    }
    else
    {
        strcpy_s(deltaPreviousShot, sizeof(deltaPreviousShot), "na");
    }

    _snprintf_s(
        line,
        sizeof(line),
        _TRUNCATE,
        "[weaponlog] type=accepted ts=%s map=%s player=\"%s\" entindex=%d userid=%d weapon=357 fire=primary experimental=%d profile=\"%s\" firstshot=%d spread=%.4f base=%.4f move_penalty=%.4f speed2d=%.1f maxspeed=%.1f grounded=%d ducking=%d delta_prev=%s clip=%d",
        timestamp,
        SanitizeLogValue(GetSafeMapName()).c_str(),
        GetSafePlayerName(pPlayer).c_str(),
        GetPlayerEntityIndex(pPlayer),
        GetPlayerUserId(pPlayer),
        telemetry.experimentalModeActive ? 1 : 0,
        SanitizeLogValue(Exp357ProfileName()).c_str(),
        telemetry.firstShotAccuracyApplied ? 1 : 0,
        telemetry.spread,
        telemetry.baseSpread,
        telemetry.movementPenalty,
        telemetry.horizontalSpeed,
        telemetry.maxSpeedForNormalization,
        telemetry.grounded ? 1 : 0,
        telemetry.ducking ? 1 : 0,
        deltaPreviousShot,
        telemetry.clipAfterShot);

    WriteTelemetryLine(line);
}

void LogAcceptedShotgunPrimaryShot(CBasePlayer *pPlayer, const ShotgunAcceptedShotTelemetry &telemetry)
{
    if (!ExpDebugWeaponLogEnabled())
    {
        return;
    }

    EnsureWeaponDebugLogOpen();

    char timestamp[64];
    char deltaPreviousShot[32];
    char line[1024];
    FormatTimestamp(timestamp, sizeof(timestamp));

    if (telemetry.hasPreviousAcceptedShot)
    {
        _snprintf_s(deltaPreviousShot, sizeof(deltaPreviousShot), _TRUNCATE, "%.3f", telemetry.timeSincePreviousAcceptedShot);
    }
    else
    {
        strcpy_s(deltaPreviousShot, sizeof(deltaPreviousShot), "na");
    }

    _snprintf_s(
        line,
        sizeof(line),
        _TRUNCATE,
        "[weaponlog] type=accepted ts=%s map=%s player=\"%s\" entindex=%d userid=%d weapon=shotgun fire=primary experimental=%d profile=\"%s\" firstshot=%d spread=%.4f base=%.4f move_penalty=%.4f speed2d=%.1f maxspeed=%.1f grounded=%d ducking=%d delta_prev=%s clip=%d pellets=%d",
        timestamp,
        SanitizeLogValue(GetSafeMapName()).c_str(),
        GetSafePlayerName(pPlayer).c_str(),
        GetPlayerEntityIndex(pPlayer),
        GetPlayerUserId(pPlayer),
        telemetry.experimentalModeActive ? 1 : 0,
        SanitizeLogValue(ExpShotgunProfileName()).c_str(),
        telemetry.firstShotAccuracyApplied ? 1 : 0,
        telemetry.spread,
        telemetry.baseSpread,
        telemetry.movementPenalty,
        telemetry.horizontalSpeed,
        telemetry.maxSpeedForNormalization,
        telemetry.grounded ? 1 : 0,
        telemetry.ducking ? 1 : 0,
        deltaPreviousShot,
        telemetry.clipAfterShot,
        telemetry.pelletCount);

    WriteTelemetryLine(line);
}

void LogRejectedGlockPrimaryHold(CBasePlayer *pPlayer, const GlockRejectedShotTelemetry &telemetry)
{
    if (!ExpDebugWeaponLogEnabled() || !ExpDebugWeaponLogRejectionsEnabled())
    {
        return;
    }

    EnsureWeaponDebugLogOpen();

    char timestamp[64];
    char line[768];
    FormatTimestamp(timestamp, sizeof(timestamp));

    _snprintf_s(
        line,
        sizeof(line),
        _TRUNCATE,
        "[weaponlog] type=rejected ts=%s map=%s player=\"%s\" entindex=%d userid=%d weapon=glock fire=primary reason=tapfire_hold_blocked speed2d=%.1f grounded=%d ducking=%d",
        timestamp,
        SanitizeLogValue(GetSafeMapName()).c_str(),
        GetSafePlayerName(pPlayer).c_str(),
        GetPlayerEntityIndex(pPlayer),
        GetPlayerUserId(pPlayer),
        telemetry.horizontalSpeed,
        telemetry.grounded ? 1 : 0,
        telemetry.ducking ? 1 : 0);

    WriteTelemetryLine(line);
}

void BeginGlockPrimaryShotContext(CBasePlayer *pPlayer)
{
    ResetGlockPrimaryShotContext();
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        return;
    }

    const SharedWeaponDamageProfile damageProfile = BuildGlockPrimaryDamageProfile();
    g_glockPrimaryShotContext.active = true;
    g_glockPrimaryShotContext.attacker = pPlayer->pev;
    g_glockPrimaryShotContext.attackerPlayer = pPlayer;
    g_glockPrimaryShotContext.experimentalModeActive = ExpGlockExperimentalModeEnabled();
    g_glockPrimaryShotContext.baseDamage = damageProfile.baseDamage;
    g_glockPrimaryShotContext.headshotScale = damageProfile.headshotScale;
    g_glockPrimaryShotContext.headshotLethal = damageProfile.headshotLethal;
    strncpy_s(g_glockPrimaryShotContext.profileName, sizeof(g_glockPrimaryShotContext.profileName), ExpGlockProfileName(), _TRUNCATE);
    strncpy_s(g_glockPrimaryShotContext.targetProfileName, sizeof(g_glockPrimaryShotContext.targetProfileName), ExpGlockLabTargetProfileName(), _TRUNCATE);
}

void EndGlockPrimaryShotContext()
{
    ResetGlockPrimaryShotContext();
}

float GetActiveGlockPrimaryBaseDamage(entvars_t *pevAttacker, float fallbackDamage)
{
    if (!HasMatchingActiveGlockPrimaryShot(pevAttacker))
    {
        return fallbackDamage;
    }

    return g_glockPrimaryShotContext.baseDamage;
}

bool ApplyActiveGlockPrimaryTraceDamage(CBaseEntity *pVictim, entvars_t *pevAttacker, int hitgroup, float *pDamage)
{
    if (pDamage == NULL || pVictim == NULL || pVictim->pev == NULL || !HasMatchingActiveGlockPrimaryShot(pevAttacker))
    {
        return false;
    }

    SharedWeaponDamageProfile damageProfile = {};
    damageProfile.baseDamage = g_glockPrimaryShotContext.baseDamage;
    damageProfile.headshotScale = g_glockPrimaryShotContext.headshotScale;
    damageProfile.headshotLethal = g_glockPrimaryShotContext.headshotLethal;
    SharedWeaponTraceDamageResult traceResult = {};
    if (!ApplySharedWeaponTraceDamage(
            pVictim,
            hitgroup,
            damageProfile,
            *pDamage,
            &traceResult))
    {
        return false;
    }

    *pDamage = traceResult.damageToHealth;

    g_glockPrimaryShotContext.pendingHit = true;
    g_glockPrimaryShotContext.victim = pVictim;
    g_glockPrimaryShotContext.hitgroup = hitgroup;
    g_glockPrimaryShotContext.headshot = traceResult.headshot;
    g_glockPrimaryShotContext.hitgroupScale = traceResult.hitgroupScale;
    g_glockPrimaryShotContext.traceDamage = traceResult.traceDamage;
    g_glockPrimaryShotContext.headshotLethalApplied = traceResult.headshotLethalApplied;
    g_glockPrimaryShotContext.victimHealthBefore = pVictim->pev->health;
    g_glockPrimaryShotContext.victimArmorKnown = traceResult.victimArmorKnown;
    g_glockPrimaryShotContext.victimArmorBefore = traceResult.victimArmorBefore;
    g_glockPrimaryShotContext.victimIsDummy = traceResult.dummyVictim;
    g_glockPrimaryShotContext.dummyArmorApplied = traceResult.dummyArmorApplied;
    g_glockPrimaryShotContext.dummyHeadProtected = traceResult.dummyHeadProtected;
    g_glockPrimaryShotContext.damageRaw = traceResult.traceDamage;
    g_glockPrimaryShotContext.damageToHealth = traceResult.damageToHealth;
    g_glockPrimaryShotContext.damageAbsorbed = traceResult.damageAbsorbed;
    g_glockPrimaryShotContext.armorDrain = traceResult.armorDrain;

    return true;
}

void FinalizeActiveGlockPrimaryHitTelemetry()
{
    if (!g_glockPrimaryShotContext.active || !g_glockPrimaryShotContext.pendingHit || g_glockPrimaryShotContext.victim == NULL || g_glockPrimaryShotContext.victim->pev == NULL)
    {
        ClearPendingGlockPrimaryHit();
        return;
    }

    CBaseEntity *pVictim = g_glockPrimaryShotContext.victim;
    const float flHealthAfter = pVictim->pev->health;
    const float flAppliedDamage = g_glockPrimaryShotContext.victimHealthBefore - flHealthAfter;
    const bool fArmorKnown = g_glockPrimaryShotContext.victimArmorKnown;
    const float flArmorAfter = fArmorKnown ? pVictim->pev->armorvalue : 0.0f;
    const float flArmorDamage = fArmorKnown ? (g_glockPrimaryShotContext.victimArmorBefore - flArmorAfter) : 0.0f;
    const bool fDummyTelemetry = g_glockPrimaryShotContext.victimIsDummy;

    const bool killedByShot = (g_glockPrimaryShotContext.victimHealthBefore > 0.0f) &&
        (flHealthAfter <= 0.0f || !pVictim->IsAlive());

    if (ExpDebugWeaponLogEnabled())
    {
        EnsureWeaponDebugLogOpen();

        char timestamp[64];
        char armorBefore[32];
        char armorAfter[32];
        char armorDamage[32];
        char dummyArmorBefore[32];
        char dummyArmorAfter[32];
        char damageRaw[32];
        char damageToHealth[32];
        char damageAbsorbed[32];
        char armorDrain[32];
        char line[3072];

        FormatTimestamp(timestamp, sizeof(timestamp));
        FormatOptionalFloat(armorBefore, sizeof(armorBefore), fArmorKnown, g_glockPrimaryShotContext.victimArmorBefore, 1);
        FormatOptionalFloat(armorAfter, sizeof(armorAfter), fArmorKnown, flArmorAfter, 1);
        FormatOptionalFloat(armorDamage, sizeof(armorDamage), fArmorKnown, flArmorDamage, 1);
        FormatOptionalFloat(dummyArmorBefore, sizeof(dummyArmorBefore), fDummyTelemetry, g_glockPrimaryShotContext.victimArmorBefore, 1);
        FormatOptionalFloat(dummyArmorAfter, sizeof(dummyArmorAfter), fDummyTelemetry, flArmorAfter, 1);
        FormatOptionalFloat(damageRaw, sizeof(damageRaw), fDummyTelemetry, g_glockPrimaryShotContext.damageRaw, 4);
        FormatOptionalFloat(damageToHealth, sizeof(damageToHealth), fDummyTelemetry, g_glockPrimaryShotContext.damageToHealth, 4);
        FormatOptionalFloat(damageAbsorbed, sizeof(damageAbsorbed), fDummyTelemetry, g_glockPrimaryShotContext.damageAbsorbed, 4);
        FormatOptionalFloat(armorDrain, sizeof(armorDrain), fDummyTelemetry, g_glockPrimaryShotContext.armorDrain, 4);

        _snprintf_s(
            line,
            sizeof(line),
            _TRUNCATE,
            "[weaponlog] type=hit ts=%s map=%s attacker=\"%s\" attacker_entindex=%d attacker_userid=%d victim=\"%s\" victim_entindex=%d victim_userid=%d victim_kind=%s victim_class=%s victim_model=\"%s\" weapon=glock fire=primary hitgroup=%s hitgroup_id=%d headshot=%d experimental=%d profile=\"%s\" target_profile=\"%s\" base_damage=%.4f hitgroup_scale=%.4f trace_damage=%.4f applied_damage=%.4f health_before=%.1f health_after=%.1f armor_before=%s armor_after=%s armor_damage=%s damage_raw=%s damage_to_health=%s damage_absorbed=%s armor_drain=%s dummy_armor_before=%s dummy_armor_after=%s armor_applied=%d head_protected=%d headshot_lethal_active=%d headshot_lethal_applied=%d",
            timestamp,
            SanitizeLogValue(GetSafeMapName()).c_str(),
            GetSafePlayerName(g_glockPrimaryShotContext.attackerPlayer).c_str(),
            GetPlayerEntityIndex(g_glockPrimaryShotContext.attackerPlayer),
            GetPlayerUserId(g_glockPrimaryShotContext.attackerPlayer),
            GetSafeEntityName(pVictim).c_str(),
            GetEntityIndex(pVictim),
            GetEntityUserId(pVictim),
            GetEntityKind(pVictim),
            GetSafeEntityClassname(pVictim).c_str(),
            GetSafeEntityModel(pVictim).c_str(),
            GetHitgroupName(g_glockPrimaryShotContext.hitgroup),
            g_glockPrimaryShotContext.hitgroup,
            g_glockPrimaryShotContext.headshot ? 1 : 0,
            g_glockPrimaryShotContext.experimentalModeActive ? 1 : 0,
            SanitizeLogValue(g_glockPrimaryShotContext.profileName).c_str(),
            fDummyTelemetry ? SanitizeLogValue(g_glockPrimaryShotContext.targetProfileName).c_str() : "na",
            g_glockPrimaryShotContext.baseDamage,
            g_glockPrimaryShotContext.hitgroupScale,
            g_glockPrimaryShotContext.traceDamage,
            flAppliedDamage,
            g_glockPrimaryShotContext.victimHealthBefore,
            flHealthAfter,
            armorBefore,
            armorAfter,
            armorDamage,
            damageRaw,
            damageToHealth,
            damageAbsorbed,
            armorDrain,
            dummyArmorBefore,
            dummyArmorAfter,
            g_glockPrimaryShotContext.dummyArmorApplied ? 1 : 0,
            g_glockPrimaryShotContext.dummyHeadProtected ? 1 : 0,
            g_glockPrimaryShotContext.headshotLethal ? 1 : 0,
            g_glockPrimaryShotContext.headshotLethalApplied ? 1 : 0);
        WriteTelemetryLine(line);

        if (killedByShot)
        {
            char killLine[3072];
            _snprintf_s(
                killLine,
                sizeof(killLine),
                _TRUNCATE,
                "[weaponlog] type=kill ts=%s map=%s attacker=\"%s\" attacker_entindex=%d attacker_userid=%d victim=\"%s\" victim_entindex=%d victim_userid=%d victim_kind=%s victim_class=%s victim_model=\"%s\" weapon=glock fire=primary hitgroup=%s hitgroup_id=%d headshot=%d experimental=%d profile=\"%s\" target_profile=\"%s\" trace_damage=%.4f applied_damage=%.4f health_before=%.1f health_after=%.1f armor_before=%s armor_after=%s damage_raw=%s damage_to_health=%s damage_absorbed=%s armor_drain=%s dummy_armor_after=%s armor_applied=%d head_protected=%d headshot_lethal_active=%d headshot_lethal_applied=%d",
                timestamp,
                SanitizeLogValue(GetSafeMapName()).c_str(),
                GetSafePlayerName(g_glockPrimaryShotContext.attackerPlayer).c_str(),
                GetPlayerEntityIndex(g_glockPrimaryShotContext.attackerPlayer),
                GetPlayerUserId(g_glockPrimaryShotContext.attackerPlayer),
                GetSafeEntityName(pVictim).c_str(),
                GetEntityIndex(pVictim),
                GetEntityUserId(pVictim),
                GetEntityKind(pVictim),
                GetSafeEntityClassname(pVictim).c_str(),
                GetSafeEntityModel(pVictim).c_str(),
                GetHitgroupName(g_glockPrimaryShotContext.hitgroup),
                g_glockPrimaryShotContext.hitgroup,
                g_glockPrimaryShotContext.headshot ? 1 : 0,
                g_glockPrimaryShotContext.experimentalModeActive ? 1 : 0,
                SanitizeLogValue(g_glockPrimaryShotContext.profileName).c_str(),
                fDummyTelemetry ? SanitizeLogValue(g_glockPrimaryShotContext.targetProfileName).c_str() : "na",
                g_glockPrimaryShotContext.traceDamage,
                flAppliedDamage,
                g_glockPrimaryShotContext.victimHealthBefore,
                flHealthAfter,
                armorBefore,
                armorAfter,
                damageRaw,
                damageToHealth,
                damageAbsorbed,
                armorDrain,
                dummyArmorAfter,
                g_glockPrimaryShotContext.dummyArmorApplied ? 1 : 0,
                g_glockPrimaryShotContext.dummyHeadProtected ? 1 : 0,
                g_glockPrimaryShotContext.headshotLethal ? 1 : 0,
                g_glockPrimaryShotContext.headshotLethalApplied ? 1 : 0);
            WriteTelemetryLine(killLine);
        }
    }

    ClearPendingGlockPrimaryHit();
}

void BeginMp5PrimaryShotContext(CBasePlayer *pPlayer)
{
    ResetMp5PrimaryShotContext();
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        return;
    }

    const SharedWeaponDamageProfile damageProfile = BuildMp5PrimaryDamageProfile();
    g_mp5PrimaryShotContext.active = true;
    g_mp5PrimaryShotContext.attacker = pPlayer->pev;
    g_mp5PrimaryShotContext.attackerPlayer = pPlayer;
    g_mp5PrimaryShotContext.experimentalModeActive = ExpMP5ExperimentalModeEnabled();
    g_mp5PrimaryShotContext.baseDamage = damageProfile.baseDamage;
    g_mp5PrimaryShotContext.headshotScale = damageProfile.headshotScale;
    g_mp5PrimaryShotContext.headshotLethal = damageProfile.headshotLethal;
    strncpy_s(g_mp5PrimaryShotContext.profileName, sizeof(g_mp5PrimaryShotContext.profileName), ExpMP5ProfileName(), _TRUNCATE);
    strncpy_s(g_mp5PrimaryShotContext.targetProfileName, sizeof(g_mp5PrimaryShotContext.targetProfileName), ExpGlockLabTargetProfileName(), _TRUNCATE);
}

void EndMp5PrimaryShotContext()
{
    ResetMp5PrimaryShotContext();
}

float GetActiveMp5PrimaryBaseDamage(entvars_t *pevAttacker, float fallbackDamage)
{
    if (!HasMatchingActiveMp5PrimaryShot(pevAttacker))
    {
        return fallbackDamage;
    }

    return g_mp5PrimaryShotContext.baseDamage;
}

bool ApplyActiveMp5PrimaryTraceDamage(CBaseEntity *pVictim, entvars_t *pevAttacker, int hitgroup, float *pDamage)
{
    if (pDamage == NULL || pVictim == NULL || pVictim->pev == NULL || !HasMatchingActiveMp5PrimaryShot(pevAttacker))
    {
        return false;
    }

    SharedWeaponDamageProfile damageProfile = {};
    damageProfile.baseDamage = g_mp5PrimaryShotContext.baseDamage;
    damageProfile.headshotScale = g_mp5PrimaryShotContext.headshotScale;
    damageProfile.headshotLethal = g_mp5PrimaryShotContext.headshotLethal;
    SharedWeaponTraceDamageResult traceResult = {};
    if (!ApplySharedWeaponTraceDamage(
            pVictim,
            hitgroup,
            damageProfile,
            *pDamage,
            &traceResult))
    {
        return false;
    }

    *pDamage = traceResult.damageToHealth;

    g_mp5PrimaryShotContext.pendingHit = true;
    g_mp5PrimaryShotContext.victim = pVictim;
    g_mp5PrimaryShotContext.hitgroup = hitgroup;
    g_mp5PrimaryShotContext.headshot = traceResult.headshot;
    g_mp5PrimaryShotContext.hitgroupScale = traceResult.hitgroupScale;
    g_mp5PrimaryShotContext.traceDamage = traceResult.traceDamage;
    g_mp5PrimaryShotContext.headshotLethalApplied = traceResult.headshotLethalApplied;
    g_mp5PrimaryShotContext.victimHealthBefore = pVictim->pev->health;
    g_mp5PrimaryShotContext.victimArmorKnown = traceResult.victimArmorKnown;
    g_mp5PrimaryShotContext.victimArmorBefore = traceResult.victimArmorBefore;
    g_mp5PrimaryShotContext.victimIsDummy = traceResult.dummyVictim;
    g_mp5PrimaryShotContext.dummyArmorApplied = traceResult.dummyArmorApplied;
    g_mp5PrimaryShotContext.dummyHeadProtected = traceResult.dummyHeadProtected;
    g_mp5PrimaryShotContext.damageRaw = traceResult.traceDamage;
    g_mp5PrimaryShotContext.damageToHealth = traceResult.damageToHealth;
    g_mp5PrimaryShotContext.damageAbsorbed = traceResult.damageAbsorbed;
    g_mp5PrimaryShotContext.armorDrain = traceResult.armorDrain;

    return true;
}

void FinalizeActiveMp5PrimaryHitTelemetry()
{
    if (!g_mp5PrimaryShotContext.active || !g_mp5PrimaryShotContext.pendingHit || g_mp5PrimaryShotContext.victim == NULL || g_mp5PrimaryShotContext.victim->pev == NULL)
    {
        ClearPendingMp5PrimaryHit();
        return;
    }

    CBaseEntity *pVictim = g_mp5PrimaryShotContext.victim;
    const float healthAfter = pVictim->pev->health;
    const float appliedDamage = g_mp5PrimaryShotContext.victimHealthBefore - healthAfter;
    const bool armorKnown = g_mp5PrimaryShotContext.victimArmorKnown;
    const float armorAfter = armorKnown ? pVictim->pev->armorvalue : 0.0f;
    const float armorDamage = armorKnown ? (g_mp5PrimaryShotContext.victimArmorBefore - armorAfter) : 0.0f;
    const bool dummyTelemetry = g_mp5PrimaryShotContext.victimIsDummy;

    const bool killedByShot = (g_mp5PrimaryShotContext.victimHealthBefore > 0.0f) &&
        (healthAfter <= 0.0f || !pVictim->IsAlive());

    if (ExpDebugWeaponLogEnabled())
    {
        EnsureWeaponDebugLogOpen();

        char timestamp[64];
        char armorBefore[32];
        char armorAfterText[32];
        char armorDamageText[32];
        char dummyArmorBefore[32];
        char dummyArmorAfter[32];
        char damageRaw[32];
        char damageToHealth[32];
        char damageAbsorbed[32];
        char armorDrain[32];
        char line[3072];

        FormatTimestamp(timestamp, sizeof(timestamp));
        FormatOptionalFloat(armorBefore, sizeof(armorBefore), armorKnown, g_mp5PrimaryShotContext.victimArmorBefore, 1);
        FormatOptionalFloat(armorAfterText, sizeof(armorAfterText), armorKnown, armorAfter, 1);
        FormatOptionalFloat(armorDamageText, sizeof(armorDamageText), armorKnown, armorDamage, 1);
        FormatOptionalFloat(dummyArmorBefore, sizeof(dummyArmorBefore), dummyTelemetry, g_mp5PrimaryShotContext.victimArmorBefore, 1);
        FormatOptionalFloat(dummyArmorAfter, sizeof(dummyArmorAfter), dummyTelemetry, armorAfter, 1);
        FormatOptionalFloat(damageRaw, sizeof(damageRaw), dummyTelemetry, g_mp5PrimaryShotContext.damageRaw, 4);
        FormatOptionalFloat(damageToHealth, sizeof(damageToHealth), dummyTelemetry, g_mp5PrimaryShotContext.damageToHealth, 4);
        FormatOptionalFloat(damageAbsorbed, sizeof(damageAbsorbed), dummyTelemetry, g_mp5PrimaryShotContext.damageAbsorbed, 4);
        FormatOptionalFloat(armorDrain, sizeof(armorDrain), dummyTelemetry, g_mp5PrimaryShotContext.armorDrain, 4);

        _snprintf_s(
            line,
            sizeof(line),
            _TRUNCATE,
            "[weaponlog] type=hit ts=%s map=%s attacker=\"%s\" attacker_entindex=%d attacker_userid=%d victim=\"%s\" victim_entindex=%d victim_userid=%d victim_kind=%s victim_class=%s victim_model=\"%s\" weapon=mp5 fire=primary hitgroup=%s hitgroup_id=%d headshot=%d experimental=%d profile=\"%s\" target_profile=\"%s\" base_damage=%.4f hitgroup_scale=%.4f trace_damage=%.4f applied_damage=%.4f health_before=%.1f health_after=%.1f armor_before=%s armor_after=%s armor_damage=%s damage_raw=%s damage_to_health=%s damage_absorbed=%s armor_drain=%s dummy_armor_before=%s dummy_armor_after=%s armor_applied=%d head_protected=%d headshot_lethal_active=%d headshot_lethal_applied=%d",
            timestamp,
            SanitizeLogValue(GetSafeMapName()).c_str(),
            GetSafePlayerName(g_mp5PrimaryShotContext.attackerPlayer).c_str(),
            GetPlayerEntityIndex(g_mp5PrimaryShotContext.attackerPlayer),
            GetPlayerUserId(g_mp5PrimaryShotContext.attackerPlayer),
            GetSafeEntityName(pVictim).c_str(),
            GetEntityIndex(pVictim),
            GetEntityUserId(pVictim),
            GetEntityKind(pVictim),
            GetSafeEntityClassname(pVictim).c_str(),
            GetSafeEntityModel(pVictim).c_str(),
            GetHitgroupName(g_mp5PrimaryShotContext.hitgroup),
            g_mp5PrimaryShotContext.hitgroup,
            g_mp5PrimaryShotContext.headshot ? 1 : 0,
            g_mp5PrimaryShotContext.experimentalModeActive ? 1 : 0,
            SanitizeLogValue(g_mp5PrimaryShotContext.profileName).c_str(),
            dummyTelemetry ? SanitizeLogValue(g_mp5PrimaryShotContext.targetProfileName).c_str() : "na",
            g_mp5PrimaryShotContext.baseDamage,
            g_mp5PrimaryShotContext.hitgroupScale,
            g_mp5PrimaryShotContext.traceDamage,
            appliedDamage,
            g_mp5PrimaryShotContext.victimHealthBefore,
            healthAfter,
            armorBefore,
            armorAfterText,
            armorDamageText,
            damageRaw,
            damageToHealth,
            damageAbsorbed,
            armorDrain,
            dummyArmorBefore,
            dummyArmorAfter,
            g_mp5PrimaryShotContext.dummyArmorApplied ? 1 : 0,
            g_mp5PrimaryShotContext.dummyHeadProtected ? 1 : 0,
            g_mp5PrimaryShotContext.headshotLethal ? 1 : 0,
            g_mp5PrimaryShotContext.headshotLethalApplied ? 1 : 0);
        WriteTelemetryLine(line);

        if (killedByShot)
        {
            char killLine[3072];
            _snprintf_s(
                killLine,
                sizeof(killLine),
                _TRUNCATE,
                "[weaponlog] type=kill ts=%s map=%s attacker=\"%s\" attacker_entindex=%d attacker_userid=%d victim=\"%s\" victim_entindex=%d victim_userid=%d victim_kind=%s victim_class=%s victim_model=\"%s\" weapon=mp5 fire=primary hitgroup=%s hitgroup_id=%d headshot=%d experimental=%d profile=\"%s\" target_profile=\"%s\" trace_damage=%.4f applied_damage=%.4f health_before=%.1f health_after=%.1f armor_before=%s armor_after=%s damage_raw=%s damage_to_health=%s damage_absorbed=%s armor_drain=%s dummy_armor_after=%s armor_applied=%d head_protected=%d headshot_lethal_active=%d headshot_lethal_applied=%d",
                timestamp,
                SanitizeLogValue(GetSafeMapName()).c_str(),
                GetSafePlayerName(g_mp5PrimaryShotContext.attackerPlayer).c_str(),
                GetPlayerEntityIndex(g_mp5PrimaryShotContext.attackerPlayer),
                GetPlayerUserId(g_mp5PrimaryShotContext.attackerPlayer),
                GetSafeEntityName(pVictim).c_str(),
                GetEntityIndex(pVictim),
                GetEntityUserId(pVictim),
                GetEntityKind(pVictim),
                GetSafeEntityClassname(pVictim).c_str(),
                GetSafeEntityModel(pVictim).c_str(),
                GetHitgroupName(g_mp5PrimaryShotContext.hitgroup),
                g_mp5PrimaryShotContext.hitgroup,
                g_mp5PrimaryShotContext.headshot ? 1 : 0,
                g_mp5PrimaryShotContext.experimentalModeActive ? 1 : 0,
                SanitizeLogValue(g_mp5PrimaryShotContext.profileName).c_str(),
                dummyTelemetry ? SanitizeLogValue(g_mp5PrimaryShotContext.targetProfileName).c_str() : "na",
                g_mp5PrimaryShotContext.traceDamage,
                appliedDamage,
                g_mp5PrimaryShotContext.victimHealthBefore,
                healthAfter,
                armorBefore,
                armorAfterText,
                damageRaw,
                damageToHealth,
                damageAbsorbed,
                armorDrain,
                dummyArmorAfter,
                g_mp5PrimaryShotContext.dummyArmorApplied ? 1 : 0,
                g_mp5PrimaryShotContext.dummyHeadProtected ? 1 : 0,
                g_mp5PrimaryShotContext.headshotLethal ? 1 : 0,
                g_mp5PrimaryShotContext.headshotLethalApplied ? 1 : 0);
            WriteTelemetryLine(killLine);
        }
    }

    ClearPendingMp5PrimaryHit();
}

void Begin357PrimaryShotContext(CBasePlayer *pPlayer)
{
    Reset357PrimaryShotContext();
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        return;
    }

    const SharedWeaponDamageProfile damageProfile = Build357PrimaryDamageProfile();
    g_357PrimaryShotContext.active = true;
    g_357PrimaryShotContext.attacker = pPlayer->pev;
    g_357PrimaryShotContext.attackerPlayer = pPlayer;
    g_357PrimaryShotContext.experimentalModeActive = Exp357ExperimentalModeEnabled();
    g_357PrimaryShotContext.baseDamage = damageProfile.baseDamage;
    g_357PrimaryShotContext.headshotScale = damageProfile.headshotScale;
    g_357PrimaryShotContext.headshotLethal = damageProfile.headshotLethal;
    strncpy_s(g_357PrimaryShotContext.profileName, sizeof(g_357PrimaryShotContext.profileName), Exp357ProfileName(), _TRUNCATE);
    strncpy_s(g_357PrimaryShotContext.targetProfileName, sizeof(g_357PrimaryShotContext.targetProfileName), ExpGlockLabTargetProfileName(), _TRUNCATE);
}

void End357PrimaryShotContext()
{
    Reset357PrimaryShotContext();
}

float GetActive357PrimaryBaseDamage(entvars_t *pevAttacker, float fallbackDamage)
{
    if (!HasMatchingActive357PrimaryShot(pevAttacker))
    {
        return fallbackDamage;
    }

    return g_357PrimaryShotContext.baseDamage;
}

bool ApplyActive357PrimaryTraceDamage(CBaseEntity *pVictim, entvars_t *pevAttacker, int hitgroup, float *pDamage)
{
    if (pDamage == NULL || pVictim == NULL || pVictim->pev == NULL || !HasMatchingActive357PrimaryShot(pevAttacker))
    {
        return false;
    }

    SharedWeaponDamageProfile damageProfile = {};
    damageProfile.baseDamage = g_357PrimaryShotContext.baseDamage;
    damageProfile.headshotScale = g_357PrimaryShotContext.headshotScale;
    damageProfile.headshotLethal = g_357PrimaryShotContext.headshotLethal;
    SharedWeaponTraceDamageResult traceResult = {};
    if (!ApplySharedWeaponTraceDamage(
            pVictim,
            hitgroup,
            damageProfile,
            *pDamage,
            &traceResult))
    {
        return false;
    }

    *pDamage = traceResult.damageToHealth;

    g_357PrimaryShotContext.pendingHit = true;
    g_357PrimaryShotContext.victim = pVictim;
    g_357PrimaryShotContext.hitgroup = hitgroup;
    g_357PrimaryShotContext.headshot = traceResult.headshot;
    g_357PrimaryShotContext.hitgroupScale = traceResult.hitgroupScale;
    g_357PrimaryShotContext.traceDamage = traceResult.traceDamage;
    g_357PrimaryShotContext.headshotLethalApplied = traceResult.headshotLethalApplied;
    g_357PrimaryShotContext.victimHealthBefore = pVictim->pev->health;
    g_357PrimaryShotContext.victimArmorKnown = traceResult.victimArmorKnown;
    g_357PrimaryShotContext.victimArmorBefore = traceResult.victimArmorBefore;
    g_357PrimaryShotContext.victimIsDummy = traceResult.dummyVictim;
    g_357PrimaryShotContext.dummyArmorApplied = traceResult.dummyArmorApplied;
    g_357PrimaryShotContext.dummyHeadProtected = traceResult.dummyHeadProtected;
    g_357PrimaryShotContext.damageRaw = traceResult.traceDamage;
    g_357PrimaryShotContext.damageToHealth = traceResult.damageToHealth;
    g_357PrimaryShotContext.damageAbsorbed = traceResult.damageAbsorbed;
    g_357PrimaryShotContext.armorDrain = traceResult.armorDrain;

    return true;
}

void FinalizeActive357PrimaryHitTelemetry()
{
    if (!g_357PrimaryShotContext.active || !g_357PrimaryShotContext.pendingHit || g_357PrimaryShotContext.victim == NULL || g_357PrimaryShotContext.victim->pev == NULL)
    {
        ClearPending357PrimaryHit();
        return;
    }

    CBaseEntity *pVictim = g_357PrimaryShotContext.victim;
    const float healthAfter = pVictim->pev->health;
    const float appliedDamage = g_357PrimaryShotContext.victimHealthBefore - healthAfter;
    const bool armorKnown = g_357PrimaryShotContext.victimArmorKnown;
    const float armorAfter = armorKnown ? pVictim->pev->armorvalue : 0.0f;
    const float armorDamage = armorKnown ? (g_357PrimaryShotContext.victimArmorBefore - armorAfter) : 0.0f;
    const bool dummyTelemetry = g_357PrimaryShotContext.victimIsDummy;

    const bool killedByShot = (g_357PrimaryShotContext.victimHealthBefore > 0.0f) &&
        (healthAfter <= 0.0f || !pVictim->IsAlive());

    if (ExpDebugWeaponLogEnabled())
    {
        EnsureWeaponDebugLogOpen();

        char timestamp[64];
        char armorBefore[32];
        char armorAfterText[32];
        char armorDamageText[32];
        char dummyArmorBefore[32];
        char dummyArmorAfter[32];
        char damageRaw[32];
        char damageToHealth[32];
        char damageAbsorbed[32];
        char armorDrain[32];
        char line[3072];

        FormatTimestamp(timestamp, sizeof(timestamp));
        FormatOptionalFloat(armorBefore, sizeof(armorBefore), armorKnown, g_357PrimaryShotContext.victimArmorBefore, 1);
        FormatOptionalFloat(armorAfterText, sizeof(armorAfterText), armorKnown, armorAfter, 1);
        FormatOptionalFloat(armorDamageText, sizeof(armorDamageText), armorKnown, armorDamage, 1);
        FormatOptionalFloat(dummyArmorBefore, sizeof(dummyArmorBefore), dummyTelemetry, g_357PrimaryShotContext.victimArmorBefore, 1);
        FormatOptionalFloat(dummyArmorAfter, sizeof(dummyArmorAfter), dummyTelemetry, armorAfter, 1);
        FormatOptionalFloat(damageRaw, sizeof(damageRaw), dummyTelemetry, g_357PrimaryShotContext.damageRaw, 4);
        FormatOptionalFloat(damageToHealth, sizeof(damageToHealth), dummyTelemetry, g_357PrimaryShotContext.damageToHealth, 4);
        FormatOptionalFloat(damageAbsorbed, sizeof(damageAbsorbed), dummyTelemetry, g_357PrimaryShotContext.damageAbsorbed, 4);
        FormatOptionalFloat(armorDrain, sizeof(armorDrain), dummyTelemetry, g_357PrimaryShotContext.armorDrain, 4);

        _snprintf_s(
            line,
            sizeof(line),
            _TRUNCATE,
            "[weaponlog] type=hit ts=%s map=%s attacker=\"%s\" attacker_entindex=%d attacker_userid=%d victim=\"%s\" victim_entindex=%d victim_userid=%d victim_kind=%s victim_class=%s victim_model=\"%s\" weapon=357 fire=primary hitgroup=%s hitgroup_id=%d headshot=%d experimental=%d profile=\"%s\" target_profile=\"%s\" base_damage=%.4f hitgroup_scale=%.4f trace_damage=%.4f applied_damage=%.4f health_before=%.1f health_after=%.1f armor_before=%s armor_after=%s armor_damage=%s damage_raw=%s damage_to_health=%s damage_absorbed=%s armor_drain=%s dummy_armor_before=%s dummy_armor_after=%s armor_applied=%d head_protected=%d headshot_lethal_active=%d headshot_lethal_applied=%d",
            timestamp,
            SanitizeLogValue(GetSafeMapName()).c_str(),
            GetSafePlayerName(g_357PrimaryShotContext.attackerPlayer).c_str(),
            GetPlayerEntityIndex(g_357PrimaryShotContext.attackerPlayer),
            GetPlayerUserId(g_357PrimaryShotContext.attackerPlayer),
            GetSafeEntityName(pVictim).c_str(),
            GetEntityIndex(pVictim),
            GetEntityUserId(pVictim),
            GetEntityKind(pVictim),
            GetSafeEntityClassname(pVictim).c_str(),
            GetSafeEntityModel(pVictim).c_str(),
            GetHitgroupName(g_357PrimaryShotContext.hitgroup),
            g_357PrimaryShotContext.hitgroup,
            g_357PrimaryShotContext.headshot ? 1 : 0,
            g_357PrimaryShotContext.experimentalModeActive ? 1 : 0,
            SanitizeLogValue(g_357PrimaryShotContext.profileName).c_str(),
            dummyTelemetry ? SanitizeLogValue(g_357PrimaryShotContext.targetProfileName).c_str() : "na",
            g_357PrimaryShotContext.baseDamage,
            g_357PrimaryShotContext.hitgroupScale,
            g_357PrimaryShotContext.traceDamage,
            appliedDamage,
            g_357PrimaryShotContext.victimHealthBefore,
            healthAfter,
            armorBefore,
            armorAfterText,
            armorDamageText,
            damageRaw,
            damageToHealth,
            damageAbsorbed,
            armorDrain,
            dummyArmorBefore,
            dummyArmorAfter,
            g_357PrimaryShotContext.dummyArmorApplied ? 1 : 0,
            g_357PrimaryShotContext.dummyHeadProtected ? 1 : 0,
            g_357PrimaryShotContext.headshotLethal ? 1 : 0,
            g_357PrimaryShotContext.headshotLethalApplied ? 1 : 0);
        WriteTelemetryLine(line);

        if (killedByShot)
        {
            char killLine[3072];
            _snprintf_s(
                killLine,
                sizeof(killLine),
                _TRUNCATE,
                "[weaponlog] type=kill ts=%s map=%s attacker=\"%s\" attacker_entindex=%d attacker_userid=%d victim=\"%s\" victim_entindex=%d victim_userid=%d victim_kind=%s victim_class=%s victim_model=\"%s\" weapon=357 fire=primary hitgroup=%s hitgroup_id=%d headshot=%d experimental=%d profile=\"%s\" target_profile=\"%s\" trace_damage=%.4f applied_damage=%.4f health_before=%.1f health_after=%.1f armor_before=%s armor_after=%s damage_raw=%s damage_to_health=%s damage_absorbed=%s armor_drain=%s dummy_armor_after=%s armor_applied=%d head_protected=%d headshot_lethal_active=%d headshot_lethal_applied=%d",
                timestamp,
                SanitizeLogValue(GetSafeMapName()).c_str(),
                GetSafePlayerName(g_357PrimaryShotContext.attackerPlayer).c_str(),
                GetPlayerEntityIndex(g_357PrimaryShotContext.attackerPlayer),
                GetPlayerUserId(g_357PrimaryShotContext.attackerPlayer),
                GetSafeEntityName(pVictim).c_str(),
                GetEntityIndex(pVictim),
                GetEntityUserId(pVictim),
                GetEntityKind(pVictim),
                GetSafeEntityClassname(pVictim).c_str(),
                GetSafeEntityModel(pVictim).c_str(),
                GetHitgroupName(g_357PrimaryShotContext.hitgroup),
                g_357PrimaryShotContext.hitgroup,
                g_357PrimaryShotContext.headshot ? 1 : 0,
                g_357PrimaryShotContext.experimentalModeActive ? 1 : 0,
                SanitizeLogValue(g_357PrimaryShotContext.profileName).c_str(),
                dummyTelemetry ? SanitizeLogValue(g_357PrimaryShotContext.targetProfileName).c_str() : "na",
                g_357PrimaryShotContext.traceDamage,
                appliedDamage,
                g_357PrimaryShotContext.victimHealthBefore,
                healthAfter,
                armorBefore,
                armorAfterText,
                damageRaw,
                damageToHealth,
                damageAbsorbed,
                armorDrain,
                dummyArmorAfter,
                g_357PrimaryShotContext.dummyArmorApplied ? 1 : 0,
                g_357PrimaryShotContext.dummyHeadProtected ? 1 : 0,
                g_357PrimaryShotContext.headshotLethal ? 1 : 0,
                g_357PrimaryShotContext.headshotLethalApplied ? 1 : 0);
            WriteTelemetryLine(killLine);
        }
    }

    ClearPending357PrimaryHit();
}

void BeginShotgunPrimaryShotContext(CBasePlayer *pPlayer, int pelletCount)
{
    ResetShotgunPrimaryShotContext();
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        return;
    }

    const SharedWeaponDamageProfile damageProfile = BuildShotgunPrimaryDamageProfile();
    g_shotgunPrimaryShotContext.active = true;
    g_shotgunPrimaryShotContext.attacker = pPlayer->pev;
    g_shotgunPrimaryShotContext.attackerPlayer = pPlayer;
    g_shotgunPrimaryShotContext.experimentalModeActive = ExpShotgunExperimentalModeEnabled();
    g_shotgunPrimaryShotContext.baseDamagePerPellet = damageProfile.baseDamage;
    g_shotgunPrimaryShotContext.headshotScale = damageProfile.headshotScale;
    g_shotgunPrimaryShotContext.headshotLethal = damageProfile.headshotLethal;
    g_shotgunPrimaryShotContext.pelletCount = pelletCount > 0 ? pelletCount : 1;
    strncpy_s(g_shotgunPrimaryShotContext.profileName, sizeof(g_shotgunPrimaryShotContext.profileName), ExpShotgunProfileName(), _TRUNCATE);
}

void EndShotgunPrimaryShotContext()
{
    ResetShotgunPrimaryShotContext();
}

float GetActiveShotgunPrimaryBaseDamage(entvars_t *pevAttacker, float fallbackDamage)
{
    if (!HasMatchingActiveShotgunPrimaryShot(pevAttacker))
    {
        return fallbackDamage;
    }

    return g_shotgunPrimaryShotContext.baseDamagePerPellet;
}

bool ApplyActiveShotgunPrimaryTraceDamage(CBaseEntity *pVictim, entvars_t *pevAttacker, int hitgroup, float *pDamage)
{
    if (pDamage == NULL || pVictim == NULL || pVictim->pev == NULL || !HasMatchingActiveShotgunPrimaryShot(pevAttacker))
    {
        return false;
    }

    SharedWeaponDamageProfile damageProfile = {};
    damageProfile.baseDamage = g_shotgunPrimaryShotContext.baseDamagePerPellet;
    damageProfile.headshotScale = g_shotgunPrimaryShotContext.headshotScale;
    damageProfile.headshotLethal = g_shotgunPrimaryShotContext.headshotLethal;

    SharedWeaponTraceDamageResult traceResult = {};
    if (!ApplySharedWeaponTraceDamage(
            pVictim,
            hitgroup,
            damageProfile,
            *pDamage,
            &traceResult))
    {
        return false;
    }

    *pDamage = traceResult.damageToHealth;

    ShotgunPrimaryVictimAggregate *aggregate = FindShotgunVictimAggregate(pVictim);
    if (aggregate == NULL)
    {
        return true;
    }

    if (aggregate->pelletHits <= 0)
    {
        aggregate->victimHealthBefore = pVictim->pev->health;
        aggregate->victimArmorKnown = traceResult.victimArmorKnown;
        aggregate->victimArmorBefore = traceResult.victimArmorBefore;
        aggregate->victimIsDummy = traceResult.dummyVictim;
        aggregate->dummyHeadProtected = traceResult.dummyHeadProtected;
        strncpy_s(aggregate->targetProfileName, sizeof(aggregate->targetProfileName), ExpGlockLabTargetProfileName(), _TRUNCATE);
        aggregate->firstHitgroup = hitgroup;
    }
    else if (aggregate->firstHitgroup != hitgroup)
    {
        aggregate->mixedHitgroups = true;
    }

    aggregate->anyHeadshot = aggregate->anyHeadshot || traceResult.headshot;
    aggregate->pelletHits += 1;
    aggregate->headshotPellets += traceResult.headshot ? 1 : 0;
    aggregate->headshotLethalPellets += traceResult.headshotLethalApplied ? 1 : 0;
    aggregate->totalTraceDamage += traceResult.traceDamage;
    aggregate->totalDamageToHealth += traceResult.damageToHealth;
    aggregate->totalDamageAbsorbed += traceResult.damageAbsorbed;
    aggregate->totalArmorDrain += traceResult.armorDrain;
    aggregate->dummyArmorApplied = aggregate->dummyArmorApplied || traceResult.dummyArmorApplied;

    return true;
}

void FinalizeActiveShotgunPrimaryHitTelemetry()
{
    if (!g_shotgunPrimaryShotContext.active)
    {
        return;
    }

    if (!ExpDebugWeaponLogEnabled())
    {
        return;
    }

    EnsureWeaponDebugLogOpen();

    for (int index = 0; index < kMaxShotgunVictimsPerShot; ++index)
    {
        ShotgunPrimaryVictimAggregate *aggregate = &g_shotgunPrimaryShotContext.victims[index];
        if (!aggregate->active || aggregate->victim == NULL || aggregate->victim->pev == NULL || aggregate->pelletHits <= 0)
        {
            continue;
        }

        CBaseEntity *pVictim = aggregate->victim;
        const float healthAfter = pVictim->pev->health;
        const float appliedDamage = aggregate->victimHealthBefore - healthAfter;
        const bool armorKnown = aggregate->victimArmorKnown;
        const float armorAfter = armorKnown ? pVictim->pev->armorvalue : 0.0f;
        const float armorDamage = armorKnown ? (aggregate->victimArmorBefore - armorAfter) : 0.0f;
        const bool dummyTelemetry = aggregate->victimIsDummy;
        const bool headshot = aggregate->anyHeadshot;
        const bool headshotLethalApplied = aggregate->headshotLethalPellets > 0;
        const bool killedByShot = (aggregate->victimHealthBefore > 0.0f) &&
            (healthAfter <= 0.0f || !pVictim->IsAlive());
        const char *hitgroupName = aggregate->mixedHitgroups ? "mixed" : GetHitgroupName(aggregate->firstHitgroup);
        const int hitgroupId = aggregate->mixedHitgroups ? HITGROUP_GENERIC : aggregate->firstHitgroup;

        char timestamp[64];
        char armorBefore[32];
        char armorAfterText[32];
        char armorDamageText[32];
        char dummyArmorBefore[32];
        char dummyArmorAfter[32];
        char damageRaw[32];
        char damageToHealth[32];
        char damageAbsorbed[32];
        char armorDrain[32];
        char line[3072];

        FormatTimestamp(timestamp, sizeof(timestamp));
        FormatOptionalFloat(armorBefore, sizeof(armorBefore), armorKnown, aggregate->victimArmorBefore, 1);
        FormatOptionalFloat(armorAfterText, sizeof(armorAfterText), armorKnown, armorAfter, 1);
        FormatOptionalFloat(armorDamageText, sizeof(armorDamageText), armorKnown, armorDamage, 1);
        FormatOptionalFloat(dummyArmorBefore, sizeof(dummyArmorBefore), dummyTelemetry, aggregate->victimArmorBefore, 1);
        FormatOptionalFloat(dummyArmorAfter, sizeof(dummyArmorAfter), dummyTelemetry, armorAfter, 1);
        FormatOptionalFloat(damageRaw, sizeof(damageRaw), dummyTelemetry, aggregate->totalTraceDamage, 4);
        FormatOptionalFloat(damageToHealth, sizeof(damageToHealth), dummyTelemetry, aggregate->totalDamageToHealth, 4);
        FormatOptionalFloat(damageAbsorbed, sizeof(damageAbsorbed), dummyTelemetry, aggregate->totalDamageAbsorbed, 4);
        FormatOptionalFloat(armorDrain, sizeof(armorDrain), dummyTelemetry, aggregate->totalArmorDrain, 4);

        _snprintf_s(
            line,
            sizeof(line),
            _TRUNCATE,
            "[weaponlog] type=hit ts=%s map=%s attacker=\"%s\" attacker_entindex=%d attacker_userid=%d victim=\"%s\" victim_entindex=%d victim_userid=%d victim_kind=%s victim_class=%s victim_model=\"%s\" weapon=shotgun fire=primary hitgroup=%s hitgroup_id=%d headshot=%d experimental=%d profile=\"%s\" target_profile=\"%s\" base_damage=%.4f trace_damage=%.4f applied_damage=%.4f health_before=%.1f health_after=%.1f armor_before=%s armor_after=%s armor_damage=%s damage_raw=%s damage_to_health=%s damage_absorbed=%s armor_drain=%s dummy_armor_before=%s dummy_armor_after=%s armor_applied=%d head_protected=%d headshot_lethal_active=%d headshot_lethal_applied=%d pellets_planned=%d pellets_hit=%d headshot_pellets=%d",
            timestamp,
            SanitizeLogValue(GetSafeMapName()).c_str(),
            GetSafePlayerName(g_shotgunPrimaryShotContext.attackerPlayer).c_str(),
            GetPlayerEntityIndex(g_shotgunPrimaryShotContext.attackerPlayer),
            GetPlayerUserId(g_shotgunPrimaryShotContext.attackerPlayer),
            GetSafeEntityName(pVictim).c_str(),
            GetEntityIndex(pVictim),
            GetEntityUserId(pVictim),
            GetEntityKind(pVictim),
            GetSafeEntityClassname(pVictim).c_str(),
            GetSafeEntityModel(pVictim).c_str(),
            hitgroupName,
            hitgroupId,
            headshot ? 1 : 0,
            g_shotgunPrimaryShotContext.experimentalModeActive ? 1 : 0,
            SanitizeLogValue(g_shotgunPrimaryShotContext.profileName).c_str(),
            dummyTelemetry ? SanitizeLogValue(aggregate->targetProfileName).c_str() : "na",
            g_shotgunPrimaryShotContext.baseDamagePerPellet,
            aggregate->totalTraceDamage,
            appliedDamage,
            aggregate->victimHealthBefore,
            healthAfter,
            armorBefore,
            armorAfterText,
            armorDamageText,
            damageRaw,
            damageToHealth,
            damageAbsorbed,
            armorDrain,
            dummyArmorBefore,
            dummyArmorAfter,
            aggregate->dummyArmorApplied ? 1 : 0,
            aggregate->dummyHeadProtected ? 1 : 0,
            g_shotgunPrimaryShotContext.headshotLethal ? 1 : 0,
            headshotLethalApplied ? 1 : 0,
            g_shotgunPrimaryShotContext.pelletCount,
            aggregate->pelletHits,
            aggregate->headshotPellets);
        WriteTelemetryLine(line);

        if (killedByShot)
        {
            char killLine[3072];
            _snprintf_s(
                killLine,
                sizeof(killLine),
                _TRUNCATE,
                "[weaponlog] type=kill ts=%s map=%s attacker=\"%s\" attacker_entindex=%d attacker_userid=%d victim=\"%s\" victim_entindex=%d victim_userid=%d victim_kind=%s victim_class=%s victim_model=\"%s\" weapon=shotgun fire=primary hitgroup=%s hitgroup_id=%d headshot=%d experimental=%d profile=\"%s\" target_profile=\"%s\" trace_damage=%.4f applied_damage=%.4f health_before=%.1f health_after=%.1f armor_before=%s armor_after=%s damage_raw=%s damage_to_health=%s damage_absorbed=%s armor_drain=%s dummy_armor_after=%s armor_applied=%d head_protected=%d headshot_lethal_active=%d headshot_lethal_applied=%d pellets_planned=%d pellets_hit=%d headshot_pellets=%d",
                timestamp,
                SanitizeLogValue(GetSafeMapName()).c_str(),
                GetSafePlayerName(g_shotgunPrimaryShotContext.attackerPlayer).c_str(),
                GetPlayerEntityIndex(g_shotgunPrimaryShotContext.attackerPlayer),
                GetPlayerUserId(g_shotgunPrimaryShotContext.attackerPlayer),
                GetSafeEntityName(pVictim).c_str(),
                GetEntityIndex(pVictim),
                GetEntityUserId(pVictim),
                GetEntityKind(pVictim),
                GetSafeEntityClassname(pVictim).c_str(),
                GetSafeEntityModel(pVictim).c_str(),
                hitgroupName,
                hitgroupId,
                headshot ? 1 : 0,
                g_shotgunPrimaryShotContext.experimentalModeActive ? 1 : 0,
                SanitizeLogValue(g_shotgunPrimaryShotContext.profileName).c_str(),
                dummyTelemetry ? SanitizeLogValue(aggregate->targetProfileName).c_str() : "na",
                aggregate->totalTraceDamage,
                appliedDamage,
                aggregate->victimHealthBefore,
                healthAfter,
                armorBefore,
                armorAfterText,
                damageRaw,
                damageToHealth,
                damageAbsorbed,
                armorDrain,
                dummyArmorAfter,
                aggregate->dummyArmorApplied ? 1 : 0,
                aggregate->dummyHeadProtected ? 1 : 0,
                g_shotgunPrimaryShotContext.headshotLethal ? 1 : 0,
                headshotLethalApplied ? 1 : 0,
                g_shotgunPrimaryShotContext.pelletCount,
                aggregate->pelletHits,
                aggregate->headshotPellets);
            WriteTelemetryLine(killLine);
        }
    }
}
