#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

#include "future_gameplay_hooks.h"
#include "weapon_debug_logger.h"

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

void PrintWeaponDebugWarningOnce(const char *message)
{
    if (g_weaponDebugLogWarningPrinted)
    {
        return;
    }

    g_weaponDebugLogWarningPrinted = true;
    ALERT(at_console, "[hl-server] weapon debug logging warning: %s\n", message);
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
    char sessionLine[1024];
    FormatTimestamp(timestamp, sizeof(timestamp));
    _snprintf_s(
        sessionLine,
        sizeof(sessionLine),
        _TRUNCATE,
        "[weaponlog] type=session ts=%s map=%s event=weapon_debug_session status=ready game=valve file=\"%s\" profile=\"%s\" tapfire=%d move_scale=%.4f firstshot_enabled=%d recovery=%.3f base=%.4f ground_move_penalty=%.4f air_move_penalty=%.4f duck_penalty_scale=%.4f firstshot_speed=%.1f max_spread=%.4f",
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
        ExpGlockPrimaryMaxSpread());
    WriteTelemetryLine(sessionLine);
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
}

void EnsureWeaponDebugLogReady()
{
    if (!ExpDebugWeaponLogEnabled())
    {
        return;
    }

    EnsureWeaponDebugLogOpen();
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
