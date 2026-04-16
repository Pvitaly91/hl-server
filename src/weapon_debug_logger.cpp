#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "future_gameplay_hooks.h"
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

float GetFallbackHitgroupScale(CBaseEntity *pVictim, int hitgroup)
{
    const bool fPlayerVictim = pVictim != NULL && pVictim->IsPlayer();

    switch (hitgroup)
    {
    case HITGROUP_HEAD:
        return fPlayerVictim ? gSkillData.plrHead : gSkillData.monHead;
    case HITGROUP_CHEST:
        return fPlayerVictim ? gSkillData.plrChest : gSkillData.monChest;
    case HITGROUP_STOMACH:
        return fPlayerVictim ? gSkillData.plrStomach : gSkillData.monStomach;
    case HITGROUP_LEFTARM:
    case HITGROUP_RIGHTARM:
        return fPlayerVictim ? gSkillData.plrArm : gSkillData.monArm;
    case HITGROUP_LEFTLEG:
    case HITGROUP_RIGHTLEG:
        return fPlayerVictim ? gSkillData.plrLeg : gSkillData.monLeg;
    case HITGROUP_GENERIC:
    default:
        return 1.0f;
    }
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

float ComputePlayerHeadshotLethalDamage(CBasePlayer *pPlayer)
{
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        return 0.0f;
    }

    const float flHealth = pPlayer->pev->health > 0.0f ? pPlayer->pev->health : 0.0f;
    const float flArmor = pPlayer->pev->armorvalue > 0.0f ? pPlayer->pev->armorvalue : 0.0f;
    const float flRequiredDamage = (flArmor >= (2.0f * flHealth))
        ? (5.0f * flHealth)
        : (flHealth + (2.0f * flArmor));

    return (float)ceil(flRequiredDamage);
}

float ComputeDummyHeadshotLethalDamage(CBaseEntity *pVictim)
{
    if (pVictim == NULL || pVictim->pev == NULL)
    {
        return 0.0f;
    }

    const float health = pVictim->pev->health > 0.0f ? pVictim->pev->health : 0.0f;
    if (health <= 0.0f)
    {
        return 0.0f;
    }

    if (!ExpGlockLabDummyHeadProtected())
    {
        return (float)ceil(health);
    }

    const float armor = pVictim->pev->armorvalue > 0.0f ? pVictim->pev->armorvalue : 0.0f;
    if (armor <= 0.0f)
    {
        return (float)ceil(health);
    }

    const float armorHealthFraction = ClampFloat(ExpGlockLabDummyArmorHealthFraction(), 0.0f, 1.0f);
    const float armorDrainScale = ExpGlockLabDummyArmorDrainScale();
    const float absorbedFraction = 1.0f - armorHealthFraction;

    if (absorbedFraction <= 0.0f)
    {
        return (float)ceil(health);
    }

    if (armorDrainScale <= 0.0f)
    {
        if (armorHealthFraction <= 0.0f)
        {
            return 0.0f;
        }

        return (float)ceil(health / armorHealthFraction);
    }

    if (armorHealthFraction > 0.0f)
    {
        const float transitionDamage = armor / (absorbedFraction * armorDrainScale);
        const float lethalWhileArmored = health / armorHealthFraction;
        if (lethalWhileArmored <= transitionDamage)
        {
            return (float)ceil(lethalWhileArmored);
        }
    }

    return (float)ceil(health + (armor / armorDrainScale));
}

float ComputeHeadshotLethalDamage(CBaseEntity *pVictim)
{
    if (pVictim == NULL || pVictim->pev == NULL)
    {
        return 0.0f;
    }

    if (IsExpGlockLabDummyEntity(pVictim))
    {
        const float dummyLethalDamage = ComputeDummyHeadshotLethalDamage(pVictim);
        if (dummyLethalDamage > 0.0f)
        {
            return dummyLethalDamage;
        }
    }

    if (pVictim->IsPlayer())
    {
        return ComputePlayerHeadshotLethalDamage((CBasePlayer *)pVictim);
    }

    return (float)ceil(pVictim->pev->health > 0.0f ? pVictim->pev->health : 0.0f);
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
        return "Glock Lab Dummy";
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

bool DummyArmorProtectsHitgroup(int hitgroup, bool headProtected)
{
    switch (hitgroup)
    {
    case HITGROUP_CHEST:
    case HITGROUP_STOMACH:
        return true;
    case HITGROUP_HEAD:
        return headProtected;
    default:
        return false;
    }
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
    char sessionLine[2048];
    FormatTimestamp(timestamp, sizeof(timestamp));
    _snprintf_s(
        sessionLine,
        sizeof(sessionLine),
        _TRUNCATE,
        "[weaponlog] type=session ts=%s map=%s event=weapon_debug_session status=ready game=valve file=\"%s\" profile=\"%s\" tapfire=%d move_scale=%.4f firstshot_enabled=%d recovery=%.3f base=%.4f ground_move_penalty=%.4f air_move_penalty=%.4f duck_penalty_scale=%.4f firstshot_speed=%.1f max_spread=%.4f sv_exp_glock_primary_damage=%.4f sv_exp_glock_primary_headshot_scale=%.4f sv_exp_glock_primary_headshot_lethal=%d sv_exp_glock_lab_dummy=%d sv_exp_glock_lab_target_profile_name=\"%s\" sv_exp_glock_lab_dummy_armor=%.1f sv_exp_glock_lab_dummy_head_protected=%d sv_exp_glock_lab_dummy_armor_health_fraction=%.3f sv_exp_glock_lab_dummy_armor_drain_scale=%.3f",
        timestamp,
        SanitizeLogValue(GetSafeMapName()).c_str(),
        g_weaponDebugLogPath.c_str(),
        SanitizeLogValue(ExpGlockProfileName()).c_str(),
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
        ExpGlockLabDummyEnabled() ? 1 : 0,
        SanitizeLogValue(ExpGlockLabTargetProfileName()).c_str(),
        ExpGlockLabDummyArmor(),
        ExpGlockLabDummyHeadProtected() ? 1 : 0,
        ExpGlockLabDummyArmorHealthFraction(),
        ExpGlockLabDummyArmorDrainScale());
    WriteTelemetryLine(sessionLine);
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

void LogGlockLabDummySpawn(CBaseEntity *pDummy, CBasePlayer *pAnchorPlayer, bool respawn, const Vector &origin, const Vector &angles)
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
        "[weaponlog] type=%s ts=%s map=%s dummy=\"%s\" entindex=%d dummy_class=%s dummy_model=\"%s\" health=%.1f autorespawn=%d respawn_delay=%.2f spawn_distance=%.1f anchor=\"%s\" anchor_entindex=%d anchor_userid=%d origin=\"%s\" yaw=%.1f profile=\"%s\" target_profile=\"%s\" spawn_health=%.1f spawn_armor=%.1f head_protected=%d armor_health_fraction=%.3f armor_drain_scale=%.3f",
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
        SanitizeLogValue(ExpGlockProfileName()).c_str(),
        SanitizeLogValue(ExpGlockLabTargetProfileName()).c_str(),
        pDummy != NULL && pDummy->pev != NULL ? pDummy->pev->max_health : ExpGlockLabDummyHealth(),
        pDummy != NULL && pDummy->pev != NULL ? pDummy->pev->armorvalue : ExpGlockLabDummyArmor(),
        ExpGlockLabDummyHeadProtected() ? 1 : 0,
        ExpGlockLabDummyArmorHealthFraction(),
        ExpGlockLabDummyArmorDrainScale());

    WriteTelemetryLine(line);
}

void LogGlockLabDummyClear(CBaseEntity *pDummy, const char *reason)
{
    if (!ExpDebugWeaponLogEnabled())
    {
        return;
    }

    EnsureWeaponDebugLogOpen();

    char timestamp[64];
    char line[1536];
    FormatTimestamp(timestamp, sizeof(timestamp));

    _snprintf_s(
        line,
        sizeof(line),
        _TRUNCATE,
        "[weaponlog] type=dummy_clear ts=%s map=%s dummy=\"%s\" entindex=%d dummy_class=%s dummy_model=\"%s\" reason=\"%s\" profile=\"%s\" target_profile=\"%s\"",
        timestamp,
        SanitizeLogValue(GetSafeMapName()).c_str(),
        GetSafeEntityName(pDummy).c_str(),
        GetEntityIndex(pDummy),
        GetSafeEntityClassname(pDummy).c_str(),
        GetSafeEntityModel(pDummy).c_str(),
        SanitizeLogValue(reason).c_str(),
        SanitizeLogValue(ExpGlockProfileName()).c_str(),
        SanitizeLogValue(ExpGlockLabTargetProfileName()).c_str());

    WriteTelemetryLine(line);
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

    g_glockPrimaryShotContext.active = true;
    g_glockPrimaryShotContext.attacker = pPlayer->pev;
    g_glockPrimaryShotContext.attackerPlayer = pPlayer;
    g_glockPrimaryShotContext.experimentalModeActive = ExpGlockExperimentalModeEnabled();
    g_glockPrimaryShotContext.baseDamage = ExpGlockPrimaryDamage();
    g_glockPrimaryShotContext.headshotScale = ExpGlockPrimaryHeadshotScale();
    g_glockPrimaryShotContext.headshotLethal = ExpGlockPrimaryHeadshotLethal();
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

    const bool fHeadshot = hitgroup == HITGROUP_HEAD;
    float flHitgroupScale = GetFallbackHitgroupScale(pVictim, hitgroup);

    if (fHeadshot)
    {
        flHitgroupScale = g_glockPrimaryShotContext.headshotScale;
    }

    float flDamage = GetActiveGlockPrimaryBaseDamage(pevAttacker, *pDamage) * flHitgroupScale;
    bool fHeadshotLethalApplied = false;
    const bool fDummyVictim = IsExpGlockLabDummyEntity(pVictim);

    if (fHeadshot && g_glockPrimaryShotContext.headshotLethal)
    {
        const float flLethalDamage = ComputeHeadshotLethalDamage(pVictim);
        if (flLethalDamage > flDamage)
        {
            flDamage = flLethalDamage;
            fHeadshotLethalApplied = true;
        }
    }

    float flDamageToHealth = flDamage;
    float flDamageAbsorbed = 0.0f;
    float flArmorDrain = 0.0f;
    bool fDummyArmorApplied = false;
    const bool fDummyHeadProtected = fDummyVictim && ExpGlockLabDummyHeadProtected();
    const float flDummyArmorBefore = fDummyVictim && pVictim->pev->armorvalue > 0.0f ? pVictim->pev->armorvalue : 0.0f;

    if (fDummyVictim && flDummyArmorBefore > 0.0f && DummyArmorProtectsHitgroup(hitgroup, fDummyHeadProtected))
    {
        const float flArmorHealthFraction = ClampFloat(ExpGlockLabDummyArmorHealthFraction(), 0.0f, 1.0f);
        const float flDesiredDamageToHealth = flDamage * flArmorHealthFraction;
        const float flDesiredDamageAbsorbed = flDamage - flDesiredDamageToHealth;
        const float flArmorDrainScale = ExpGlockLabDummyArmorDrainScale();

        if (flDesiredDamageAbsorbed > 0.0f)
        {
            if (flArmorDrainScale <= 0.0f)
            {
                flDamageToHealth = flDesiredDamageToHealth;
                flDamageAbsorbed = flDesiredDamageAbsorbed;
                flArmorDrain = 0.0f;
                fDummyArmorApplied = true;
            }
            else
            {
                const float flDesiredArmorDrain = flDesiredDamageAbsorbed * flArmorDrainScale;

                if (flDesiredArmorDrain <= flDummyArmorBefore)
                {
                    flDamageToHealth = flDesiredDamageToHealth;
                    flDamageAbsorbed = flDesiredDamageAbsorbed;
                    flArmorDrain = flDesiredArmorDrain;
                    fDummyArmorApplied = true;
                }
                else
                {
                    flArmorDrain = flDummyArmorBefore;
                    flDamageAbsorbed = flArmorDrain / flArmorDrainScale;
                    flDamageToHealth = flDamage - flDamageAbsorbed;
                    fDummyArmorApplied = flDamageAbsorbed > 0.0f;
                }
            }

            pVictim->pev->armorvalue = flDummyArmorBefore - flArmorDrain;
            if (pVictim->pev->armorvalue < 0.0f)
            {
                pVictim->pev->armorvalue = 0.0f;
            }
        }
    }

    *pDamage = flDamageToHealth;

    g_glockPrimaryShotContext.pendingHit = true;
    g_glockPrimaryShotContext.victim = pVictim;
    g_glockPrimaryShotContext.hitgroup = hitgroup;
    g_glockPrimaryShotContext.headshot = fHeadshot;
    g_glockPrimaryShotContext.hitgroupScale = flHitgroupScale;
    g_glockPrimaryShotContext.traceDamage = flDamage;
    g_glockPrimaryShotContext.headshotLethalApplied = fHeadshotLethalApplied;
    g_glockPrimaryShotContext.victimHealthBefore = pVictim->pev->health;
    g_glockPrimaryShotContext.victimArmorKnown = pVictim->IsPlayer() || fDummyVictim;
    g_glockPrimaryShotContext.victimArmorBefore = g_glockPrimaryShotContext.victimArmorKnown
        ? (fDummyVictim ? flDummyArmorBefore : pVictim->pev->armorvalue)
        : 0.0f;
    g_glockPrimaryShotContext.victimIsDummy = fDummyVictim;
    g_glockPrimaryShotContext.dummyArmorApplied = fDummyArmorApplied;
    g_glockPrimaryShotContext.dummyHeadProtected = fDummyHeadProtected;
    g_glockPrimaryShotContext.damageRaw = flDamage;
    g_glockPrimaryShotContext.damageToHealth = flDamageToHealth;
    g_glockPrimaryShotContext.damageAbsorbed = flDamageAbsorbed;
    g_glockPrimaryShotContext.armorDrain = flArmorDrain;

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

        if (!pVictim->IsAlive())
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
