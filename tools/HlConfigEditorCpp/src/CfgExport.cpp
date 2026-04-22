#include "CfgExport.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>
#include <windows.h>

namespace hlcfg {

namespace {

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const int required = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) {
        return {};
    }

    std::wstring result(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), required);
    return result;
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }

    std::string result(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), required, nullptr, nullptr);
    return result;
}

bool WriteUtf8TextFile(const std::wstring& path, const std::wstring& content, std::wstring& errorMessage) {
    const std::filesystem::path filePath(path);
    std::error_code createDirectoriesError;
    std::filesystem::create_directories(filePath.parent_path(), createDirectoriesError);
    if (createDirectoriesError) {
        errorMessage = L"Unable to create the export folder.";
        return false;
    }

    std::ofstream stream(filePath, std::ios::binary | std::ios::trunc);
    if (!stream) {
        errorMessage = L"Unable to create the exported cfg file.";
        return false;
    }

    const std::string bytes = WideToUtf8(content);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!stream.good()) {
        errorMessage = L"Unable to finish writing the exported cfg file.";
        return false;
    }

    return true;
}

bool FileExists(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::exists(path, error);
}

std::wstring ToLower(const std::wstring& value) {
    std::wstring lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return lowered;
}

std::wstring Trim(const std::wstring& value) {
    return Trimmed(value);
}

std::wstring ReadEnv(const std::map<std::wstring, std::wstring>& envFileValues, const std::wstring& key) {
    wchar_t buffer[32767];
    const DWORD length = GetEnvironmentVariableW(key.c_str(), buffer, static_cast<DWORD>(std::size(buffer)));
    if (length > 0 && length < std::size(buffer)) {
        return buffer;
    }

    const auto it = envFileValues.find(key);
    return it == envFileValues.end() ? std::wstring() : it->second;
}

std::map<std::wstring, std::wstring> LoadEnvFile(const std::wstring& repoRoot) {
    std::map<std::wstring, std::wstring> values;
    if (repoRoot.empty()) {
        return values;
    }

    const std::filesystem::path envPath = std::filesystem::path(repoRoot) / L".env";
    if (!FileExists(envPath)) {
        return values;
    }

    std::ifstream stream(envPath, std::ios::binary);
    if (!stream) {
        return values;
    }

    const std::string bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    std::wstringstream lines(Utf8ToWide(bytes));
    std::wstring line;
    while (std::getline(lines, line)) {
        const std::wstring trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == L'#') {
            continue;
        }

        const std::size_t equalsIndex = trimmed.find(L'=');
        if (equalsIndex == std::wstring::npos || equalsIndex == 0) {
            continue;
        }

        std::wstring key = Trim(trimmed.substr(0, equalsIndex));
        std::wstring value = Trim(trimmed.substr(equalsIndex + 1));
        if (value.size() >= 2 &&
            ((value.front() == L'"' && value.back() == L'"') || (value.front() == L'\'' && value.back() == L'\''))) {
            value = value.substr(1, value.size() - 2);
        }

        values[key] = value;
    }

    return values;
}

std::wstring FindRepoRoot(const std::wstring& moduleFilePath) {
    std::filesystem::path current = std::filesystem::path(moduleFilePath).parent_path();
    std::error_code error;

    while (!current.empty()) {
        if (std::filesystem::exists(current / L".git", error) ||
            (std::filesystem::exists(current / L"README.md", error) && std::filesystem::exists(current / L"scripts", error))) {
            return current.wstring();
        }

        const auto parent = current.parent_path();
        if (parent == current) {
            break;
        }

        current = parent;
    }

    return {};
}

std::wstring RootFromExePath(const std::wstring& exePath) {
    const std::wstring trimmed = Trim(exePath);
    if (trimmed.empty()) {
        return {};
    }

    const std::filesystem::path path(trimmed);
    if (!FileExists(path)) {
        return {};
    }

    return path.parent_path().wstring();
}

std::wstring DetectHalfLifeRoot(const std::wstring& repoRoot) {
    const auto envValues = LoadEnvFile(repoRoot);

    std::vector<std::wstring> candidates;
    candidates.push_back(RootFromExePath(ReadEnv(envValues, L"HL_EXE")));
    candidates.push_back(RootFromExePath(ReadEnv(envValues, L"HLDS_EXE")));
    candidates.push_back(L"D:\\Steam\\steamapps\\common\\Half-Life");
    candidates.push_back(L"D:\\SteamLibrary\\steamapps\\common\\Half-Life");
    candidates.push_back(L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Half-Life");
    candidates.push_back(L"C:\\Steam\\steamapps\\common\\Half-Life");
    candidates.push_back(L"E:\\SteamLibrary\\steamapps\\common\\Half-Life");

    for (const std::wstring& candidate : candidates) {
        if (candidate.empty()) {
            continue;
        }

        const std::filesystem::path root(candidate);
        if (FileExists(root / L"hl.exe") || FileExists(root / L"hlds.exe")) {
            return root.wstring();
        }
    }

    return {};
}

std::wstring QuoteCfgString(const std::wstring& value) {
    std::wstring quoted;
    quoted.reserve(value.size() + 4);
    quoted.push_back(L'"');
    for (const wchar_t ch : value) {
        if (ch == L'\\' || ch == L'"') {
            quoted.push_back(L'\\');
        }

        quoted.push_back(ch);
    }

    quoted.push_back(L'"');
    return quoted;
}

bool NormalizeFloatValue(const std::wstring& input, std::wstring& normalized, std::wstring& errorMessage, const std::wstring& fieldName) {
    const std::wstring trimmed = Trim(input);
    if (trimmed.empty()) {
        errorMessage = fieldName + L" is required.";
        return false;
    }

    wchar_t* end = nullptr;
    const double parsed = std::wcstod(trimmed.c_str(), &end);
    if (end == trimmed.c_str() || (end != nullptr && *end != L'\0')) {
        errorMessage = fieldName + L" must be a valid number.";
        return false;
    }

    std::wostringstream stream;
    stream.precision(12);
    stream << parsed;
    normalized = stream.str();

    if (normalized.find(L'.') != std::wstring::npos) {
        while (!normalized.empty() && normalized.back() == L'0') {
            normalized.pop_back();
        }
        if (!normalized.empty() && normalized.back() == L'.') {
            normalized.push_back(L'0');
        }
    }

    return true;
}

bool NormalizeIntegerValue(const std::wstring& input, std::wstring& normalized, std::wstring& errorMessage, const std::wstring& fieldName) {
    const std::wstring trimmed = Trim(input);
    if (trimmed.empty()) {
        errorMessage = fieldName + L" is required.";
        return false;
    }

    wchar_t* end = nullptr;
    const long long parsed = std::wcstoll(trimmed.c_str(), &end, 10);
    if (end == trimmed.c_str() || (end != nullptr && *end != L'\0')) {
        errorMessage = fieldName + L" must be a whole number.";
        return false;
    }

    normalized = std::to_wstring(parsed);
    return true;
}

void AddLine(std::vector<std::wstring>& lines, const std::wstring& name, const std::wstring& value) {
    lines.push_back(name + L" " + value);
}

void AddBlankLine(std::vector<std::wstring>& lines) {
    if (!lines.empty() && !lines.back().empty()) {
        lines.emplace_back();
    }
}

bool EqualTrimmed(const std::wstring& left, const std::wstring& right) {
    return Trim(left) == Trim(right);
}

bool IsGlockRelevant(const ProjectDocument& document) {
    const ProjectDocument defaults = CreateDefaultProject();
    if (ToLower(Trim(document.general.weaponUnderTest)) == L"glock") {
        return true;
    }

    return document.glock.tapFire != defaults.glock.tapFire ||
           document.glock.firstShotAccuracy != defaults.glock.firstShotAccuracy ||
           !EqualTrimmed(document.glock.spreadRecovery, defaults.glock.spreadRecovery) ||
           !EqualTrimmed(document.glock.moveSpreadScale, defaults.glock.moveSpreadScale) ||
           !EqualTrimmed(document.glock.profileName, defaults.glock.profileName) ||
           !EqualTrimmed(document.glock.primaryBaseSpread, defaults.glock.primaryBaseSpread) ||
           !EqualTrimmed(document.glock.primaryGroundMovePenalty, defaults.glock.primaryGroundMovePenalty) ||
           !EqualTrimmed(document.glock.primaryAirMovePenalty, defaults.glock.primaryAirMovePenalty) ||
           !EqualTrimmed(document.glock.primaryDuckPenaltyScale, defaults.glock.primaryDuckPenaltyScale) ||
           !EqualTrimmed(document.glock.primaryFirstShotSpeedThreshold, defaults.glock.primaryFirstShotSpeedThreshold) ||
           !EqualTrimmed(document.glock.primaryMaxSpread, defaults.glock.primaryMaxSpread) ||
           !EqualTrimmed(document.glock.primaryDamage, defaults.glock.primaryDamage) ||
           !EqualTrimmed(document.glock.primaryHeadshotScale, defaults.glock.primaryHeadshotScale) ||
           document.glock.primaryHeadshotLethal != defaults.glock.primaryHeadshotLethal;
}

bool IsMp5Relevant(const ProjectDocument& document) {
    const ProjectDocument defaults = CreateDefaultProject();
    if (ToLower(Trim(document.general.weaponUnderTest)) == L"mp5") {
        return true;
    }

    return document.mp5.primaryEnabled != defaults.mp5.primaryEnabled ||
           !EqualTrimmed(document.mp5.profileName, defaults.mp5.profileName) ||
           !EqualTrimmed(document.mp5.primaryBaseSpread, defaults.mp5.primaryBaseSpread) ||
           !EqualTrimmed(document.mp5.primaryGroundMovePenalty, defaults.mp5.primaryGroundMovePenalty) ||
           !EqualTrimmed(document.mp5.primaryAirMovePenalty, defaults.mp5.primaryAirMovePenalty) ||
           !EqualTrimmed(document.mp5.primaryDuckPenaltyScale, defaults.mp5.primaryDuckPenaltyScale) ||
           !EqualTrimmed(document.mp5.primaryBurstGrowth, defaults.mp5.primaryBurstGrowth) ||
           !EqualTrimmed(document.mp5.primaryBurstMaxAdditionalSpread, defaults.mp5.primaryBurstMaxAdditionalSpread) ||
           !EqualTrimmed(document.mp5.primarySpreadRecovery, defaults.mp5.primarySpreadRecovery) ||
           document.mp5.primaryFirstShotAccuracy != defaults.mp5.primaryFirstShotAccuracy ||
           !EqualTrimmed(document.mp5.primaryFirstShotSpeedThreshold, defaults.mp5.primaryFirstShotSpeedThreshold) ||
           !EqualTrimmed(document.mp5.primaryMaxSpread, defaults.mp5.primaryMaxSpread) ||
           !EqualTrimmed(document.mp5.primaryDamage, defaults.mp5.primaryDamage) ||
           !EqualTrimmed(document.mp5.primaryHeadshotScale, defaults.mp5.primaryHeadshotScale) ||
           document.mp5.primaryHeadshotLethal != defaults.mp5.primaryHeadshotLethal ||
           document.mp5.labLoadout != defaults.mp5.labLoadout ||
           !EqualTrimmed(document.mp5.labAmmo, defaults.mp5.labAmmo) ||
           document.mp5.labAutoswitch != defaults.mp5.labAutoswitch;
}

bool Is357Relevant(const ProjectDocument& document) {
    const ProjectDocument defaults = CreateDefaultProject();
    if (ToLower(Trim(document.general.weaponUnderTest)) == L"357") {
        return true;
    }

    return document.weapon357.primaryEnabled != defaults.weapon357.primaryEnabled ||
           !EqualTrimmed(document.weapon357.profileName, defaults.weapon357.profileName) ||
           !EqualTrimmed(document.weapon357.primaryBaseSpread, defaults.weapon357.primaryBaseSpread) ||
           !EqualTrimmed(document.weapon357.primaryGroundMovePenalty, defaults.weapon357.primaryGroundMovePenalty) ||
           !EqualTrimmed(document.weapon357.primaryAirMovePenalty, defaults.weapon357.primaryAirMovePenalty) ||
           !EqualTrimmed(document.weapon357.primaryDuckPenaltyScale, defaults.weapon357.primaryDuckPenaltyScale) ||
           document.weapon357.primaryFirstShotAccuracy != defaults.weapon357.primaryFirstShotAccuracy ||
           !EqualTrimmed(document.weapon357.primaryFirstShotSpeedThreshold, defaults.weapon357.primaryFirstShotSpeedThreshold) ||
           !EqualTrimmed(document.weapon357.primarySpreadRecovery, defaults.weapon357.primarySpreadRecovery) ||
           !EqualTrimmed(document.weapon357.primaryMaxSpread, defaults.weapon357.primaryMaxSpread) ||
           !EqualTrimmed(document.weapon357.primaryDamage, defaults.weapon357.primaryDamage) ||
           !EqualTrimmed(document.weapon357.primaryHeadshotScale, defaults.weapon357.primaryHeadshotScale) ||
           document.weapon357.primaryHeadshotLethal != defaults.weapon357.primaryHeadshotLethal ||
           document.weapon357.labLoadout != defaults.weapon357.labLoadout ||
           !EqualTrimmed(document.weapon357.labAmmo, defaults.weapon357.labAmmo) ||
           document.weapon357.labAutoswitch != defaults.weapon357.labAutoswitch;
}

bool IsShotgunRelevant(const ProjectDocument& document) {
    const ProjectDocument defaults = CreateDefaultProject();
    if (ToLower(Trim(document.general.weaponUnderTest)) == L"shotgun") {
        return true;
    }

    return document.shotgun.primaryEnabled != defaults.shotgun.primaryEnabled ||
           !EqualTrimmed(document.shotgun.profileName, defaults.shotgun.profileName) ||
           !EqualTrimmed(document.shotgun.primaryBaseSpread, defaults.shotgun.primaryBaseSpread) ||
           !EqualTrimmed(document.shotgun.primaryGroundMovePenalty, defaults.shotgun.primaryGroundMovePenalty) ||
           !EqualTrimmed(document.shotgun.primaryAirMovePenalty, defaults.shotgun.primaryAirMovePenalty) ||
           !EqualTrimmed(document.shotgun.primaryDuckPenaltyScale, defaults.shotgun.primaryDuckPenaltyScale) ||
           document.shotgun.primaryFirstShotAccuracy != defaults.shotgun.primaryFirstShotAccuracy ||
           !EqualTrimmed(document.shotgun.primaryFirstShotSpeedThreshold, defaults.shotgun.primaryFirstShotSpeedThreshold) ||
           !EqualTrimmed(document.shotgun.primarySpreadRecovery, defaults.shotgun.primarySpreadRecovery) ||
           !EqualTrimmed(document.shotgun.primaryMaxSpread, defaults.shotgun.primaryMaxSpread) ||
           !EqualTrimmed(document.shotgun.primaryDamagePerPellet, defaults.shotgun.primaryDamagePerPellet) ||
           !EqualTrimmed(document.shotgun.primaryPelletCount, defaults.shotgun.primaryPelletCount) ||
           !EqualTrimmed(document.shotgun.primaryHeadshotScale, defaults.shotgun.primaryHeadshotScale) ||
           document.shotgun.primaryHeadshotLethal != defaults.shotgun.primaryHeadshotLethal ||
           document.shotgun.labLoadout != defaults.shotgun.labLoadout ||
           !EqualTrimmed(document.shotgun.labAmmo, defaults.shotgun.labAmmo) ||
           document.shotgun.labAutoswitch != defaults.shotgun.labAutoswitch;
}

bool IsDummyRelevant(const ProjectDocument& document) {
    const ProjectDocument defaults = CreateDefaultProject();
    return document.targetDummy.enabled != defaults.targetDummy.enabled ||
           !EqualTrimmed(document.targetDummy.targetProfileName, defaults.targetDummy.targetProfileName) ||
           !EqualTrimmed(document.targetDummy.dummyHealth, defaults.targetDummy.dummyHealth) ||
           !EqualTrimmed(document.targetDummy.dummyArmor, defaults.targetDummy.dummyArmor) ||
           document.targetDummy.dummyHeadProtected != defaults.targetDummy.dummyHeadProtected ||
           !EqualTrimmed(document.targetDummy.armorHealthFraction, defaults.targetDummy.armorHealthFraction) ||
           !EqualTrimmed(document.targetDummy.armorDrainScale, defaults.targetDummy.armorDrainScale) ||
           document.targetDummy.autorespawn != defaults.targetDummy.autorespawn ||
           !EqualTrimmed(document.targetDummy.respawnDelay, defaults.targetDummy.respawnDelay) ||
           !EqualTrimmed(document.targetDummy.spawnDistance, defaults.targetDummy.spawnDistance) ||
           !EqualTrimmed(document.targetDummy.offsetRight, defaults.targetDummy.offsetRight) ||
           !EqualTrimmed(document.targetDummy.offsetUp, defaults.targetDummy.offsetUp) ||
           document.targetDummy.facePlayer != defaults.targetDummy.facePlayer ||
           !EqualTrimmed(document.targetDummy.model, defaults.targetDummy.model);
}

bool IsRoundRelevant(const ProjectDocument& document) {
    const ProjectDocument defaults = CreateDefaultProject();
    return document.roundMode.enabled != defaults.roundMode.enabled ||
           !EqualTrimmed(document.roundMode.freezeTime, defaults.roundMode.freezeTime) ||
           !EqualTrimmed(document.roundMode.restartDelay, defaults.roundMode.restartDelay) ||
           !EqualTrimmed(document.roundMode.startHealth, defaults.roundMode.startHealth) ||
           !EqualTrimmed(document.roundMode.startArmor, defaults.roundMode.startArmor) ||
           document.roundMode.noRespawn != defaults.roundMode.noRespawn ||
           document.roundMode.friendlyFire != defaults.roundMode.friendlyFire ||
           !EqualTrimmed(document.roundMode.weaponProfile, defaults.roundMode.weaponProfile) ||
           !EqualTrimmed(document.roundMode.loadoutMode, defaults.roundMode.loadoutMode);
}

bool IsTeamRoundRelevant(const ProjectDocument& document) {
    const ProjectDocument defaults = CreateDefaultProject();
    return document.teamRound.enabled != defaults.teamRound.enabled ||
           document.teamRound.teamplay != defaults.teamRound.teamplay ||
           !EqualTrimmed(document.teamRound.spawnMode, defaults.teamRound.spawnMode) ||
           !EqualTrimmed(document.teamRound.team1Name, defaults.teamRound.team1Name) ||
           !EqualTrimmed(document.teamRound.team2Name, defaults.teamRound.team2Name) ||
           !EqualTrimmed(document.teamRound.team1Loadout, defaults.teamRound.team1Loadout) ||
           !EqualTrimmed(document.teamRound.team2Loadout, defaults.teamRound.team2Loadout) ||
           !EqualTrimmed(document.teamRound.team1Health, defaults.teamRound.team1Health) ||
           !EqualTrimmed(document.teamRound.team2Health, defaults.teamRound.team2Health) ||
           !EqualTrimmed(document.teamRound.team1Armor, defaults.teamRound.team1Armor) ||
           !EqualTrimmed(document.teamRound.team2Armor, defaults.teamRound.team2Armor);
}

bool IsBuyRelevant(const ProjectDocument& document) {
    const ProjectDocument defaults = CreateDefaultProject();
    return document.buy.enabled != defaults.buy.enabled ||
           document.buy.freezeOnly != defaults.buy.freezeOnly ||
           document.buy.teamSharedCatalog != defaults.buy.teamSharedCatalog ||
           !EqualTrimmed(document.buy.startMoney, defaults.buy.startMoney) ||
           !EqualTrimmed(document.buy.roundWinReward, defaults.buy.roundWinReward) ||
           !EqualTrimmed(document.buy.roundLossReward, defaults.buy.roundLossReward) ||
           !EqualTrimmed(document.buy.maxMoney, defaults.buy.maxMoney) ||
           document.buy.allowGlock != defaults.buy.allowGlock ||
           document.buy.allowMp5 != defaults.buy.allowMp5 ||
           document.buy.allow357 != defaults.buy.allow357 ||
           document.buy.allowShotgun != defaults.buy.allowShotgun ||
           document.buy.allowArmor != defaults.buy.allowArmor ||
           document.buy.allowHelmet != defaults.buy.allowHelmet ||
           document.buy.allowHandgrenade != defaults.buy.allowHandgrenade ||
           !EqualTrimmed(document.buy.costGlock, defaults.buy.costGlock) ||
           !EqualTrimmed(document.buy.costMp5, defaults.buy.costMp5) ||
           !EqualTrimmed(document.buy.cost357, defaults.buy.cost357) ||
           !EqualTrimmed(document.buy.costShotgun, defaults.buy.costShotgun) ||
           !EqualTrimmed(document.buy.costArmor, defaults.buy.costArmor) ||
           !EqualTrimmed(document.buy.costHelmet, defaults.buy.costHelmet) ||
           !EqualTrimmed(document.buy.costHandgrenade, defaults.buy.costHandgrenade);
}

bool IsArmorRelevant(const ProjectDocument& document) {
    const ProjectDocument defaults = CreateDefaultProject();
    return document.armorEquipment.armorMode != defaults.armorEquipment.armorMode ||
           !EqualTrimmed(document.armorEquipment.armorStartValue, defaults.armorEquipment.armorStartValue) ||
           !EqualTrimmed(document.armorEquipment.armorMaxValue, defaults.armorEquipment.armorMaxValue) ||
           !EqualTrimmed(document.armorEquipment.armorHealthFraction, defaults.armorEquipment.armorHealthFraction) ||
           !EqualTrimmed(document.armorEquipment.armorDrainScale, defaults.armorEquipment.armorDrainScale) ||
           document.armorEquipment.helmetMode != defaults.armorEquipment.helmetMode ||
           document.armorEquipment.helmetStartEnabled != defaults.armorEquipment.helmetStartEnabled ||
           document.armorEquipment.helmetHeadshotProtection != defaults.armorEquipment.helmetHeadshotProtection;
}

std::vector<std::wstring> SplitPathParts(const std::filesystem::path& path) {
    std::vector<std::wstring> parts;
    for (const auto& part : path.lexically_normal()) {
        parts.push_back(ToLower(part.wstring()));
    }

    return parts;
}

bool TryMakeRelativePath(const std::filesystem::path& fullPath, const std::filesystem::path& root, std::filesystem::path& relativePath) {
    const auto fullParts = SplitPathParts(fullPath);
    const auto rootParts = SplitPathParts(root);
    if (rootParts.empty() || fullParts.size() < rootParts.size()) {
        return false;
    }

    for (std::size_t index = 0; index < rootParts.size(); ++index) {
        if (fullParts[index] != rootParts[index]) {
            return false;
        }
    }

    std::filesystem::path result;
    std::size_t currentIndex = 0;
    for (const auto& part : fullPath.lexically_normal()) {
        if (currentIndex >= rootParts.size()) {
            result /= part;
        }
        ++currentIndex;
    }

    relativePath = result;
    return true;
}

bool TryBuildCfgProfile(const std::filesystem::path& exportPath, const EnvironmentPaths& environment, std::filesystem::path& cfgProfile) {
    std::filesystem::path relativePath;
    if (!environment.liveModRoot.empty() &&
        TryMakeRelativePath(exportPath, std::filesystem::path(environment.liveModRoot), relativePath) &&
        !relativePath.empty()) {
        cfgProfile = relativePath;
        return true;
    }

    if (!environment.stagedLiveModRoot.empty() &&
        TryMakeRelativePath(exportPath, std::filesystem::path(environment.stagedLiveModRoot), relativePath) &&
        !relativePath.empty()) {
        cfgProfile = relativePath;
        return true;
    }

    if (ToLower(exportPath.parent_path().filename().wstring()) == L"cfg_profiles") {
        cfgProfile = std::filesystem::path(L"cfg_profiles") / exportPath.filename();
        return true;
    }

    return false;
}

std::wstring BuildExecCommand(const std::filesystem::path& cfgProfile, const std::filesystem::path& exportPath) {
    if (!cfgProfile.empty()) {
        return L"exec " + cfgProfile.generic_wstring();
    }

    return L"exec " + exportPath.filename().generic_wstring();
}

std::wstring BuildLauncherCommand(const std::filesystem::path& cfgProfile) {
    if (cfgProfile.empty()) {
        return {};
    }

    std::wstring windowsProfile = cfgProfile.wstring();
    std::replace(windowsProfile.begin(), windowsProfile.end(), L'/', L'\\');
    return L"scripts\\play-hlserver-testbed-direct.bat -CfgProfile \"" + windowsProfile + L"\"";
}

bool BuildCfgLines(const ProjectDocument& document, std::vector<std::wstring>& lines, std::wstring& errorMessage) {
    std::wstring normalized;
    const bool includeGlock = IsGlockRelevant(document);
    const bool includeMp5 = IsMp5Relevant(document);
    const bool include357 = Is357Relevant(document);
    const bool includeShotgun = IsShotgunRelevant(document);
    const bool includeDummy = IsDummyRelevant(document);
    const bool includeRound = IsRoundRelevant(document);
    const bool includeTeamRound = IsTeamRoundRelevant(document);
    const bool includeBuy = IsBuyRelevant(document);
    const bool includeArmor = IsArmorRelevant(document);

    AddLine(lines, L"sv_exp_weapon_under_test", QuoteCfgString(document.general.weaponUnderTest));
    AddLine(lines, L"sv_exp_session_tag", QuoteCfgString(document.general.sessionTag));
    AddLine(lines, L"sv_exp_debug_weaponlog", document.general.debugWeaponLog ? L"1" : L"0");
    AddLine(lines, L"sv_exp_debug_weaponlog_rejections", document.general.debugWeaponLogRejections ? L"1" : L"0");

    if (includeGlock) {
        AddBlankLine(lines);
        AddLine(lines, L"sv_exp_pistol_tapfire", document.glock.tapFire ? L"1" : L"0");
        AddLine(lines, L"sv_exp_first_shot_accuracy", document.glock.firstShotAccuracy ? L"1" : L"0");
        if (!NormalizeFloatValue(document.glock.spreadRecovery, normalized, errorMessage, L"Glock spread recovery")) {
            return false;
        }
        AddLine(lines, L"sv_exp_spread_recovery", normalized);
        if (!NormalizeFloatValue(document.glock.moveSpreadScale, normalized, errorMessage, L"Glock move spread scale")) {
            return false;
        }
        AddLine(lines, L"sv_exp_move_spread_scale", normalized);
        AddLine(lines, L"sv_exp_glock_profile_name", QuoteCfgString(document.glock.profileName));
        if (!NormalizeFloatValue(document.glock.primaryBaseSpread, normalized, errorMessage, L"Glock primary base spread")) {
            return false;
        }
        AddLine(lines, L"sv_exp_glock_primary_base_spread", normalized);
        if (!NormalizeFloatValue(document.glock.primaryGroundMovePenalty, normalized, errorMessage, L"Glock ground move penalty")) {
            return false;
        }
        AddLine(lines, L"sv_exp_glock_primary_ground_move_penalty", normalized);
        if (!NormalizeFloatValue(document.glock.primaryAirMovePenalty, normalized, errorMessage, L"Glock air move penalty")) {
            return false;
        }
        AddLine(lines, L"sv_exp_glock_primary_air_move_penalty", normalized);
        if (!NormalizeFloatValue(document.glock.primaryDuckPenaltyScale, normalized, errorMessage, L"Glock duck penalty scale")) {
            return false;
        }
        AddLine(lines, L"sv_exp_glock_primary_duck_penalty_scale", normalized);
        if (!NormalizeFloatValue(document.glock.primaryFirstShotSpeedThreshold, normalized, errorMessage, L"Glock first-shot speed threshold")) {
            return false;
        }
        AddLine(lines, L"sv_exp_glock_primary_first_shot_speed_threshold", normalized);
        if (!NormalizeFloatValue(document.glock.primaryMaxSpread, normalized, errorMessage, L"Glock max spread")) {
            return false;
        }
        AddLine(lines, L"sv_exp_glock_primary_max_spread", normalized);
        if (!NormalizeFloatValue(document.glock.primaryDamage, normalized, errorMessage, L"Glock damage")) {
            return false;
        }
        AddLine(lines, L"sv_exp_glock_primary_damage", normalized);
        if (!NormalizeFloatValue(document.glock.primaryHeadshotScale, normalized, errorMessage, L"Glock headshot scale")) {
            return false;
        }
        AddLine(lines, L"sv_exp_glock_primary_headshot_scale", normalized);
        AddLine(lines, L"sv_exp_glock_primary_headshot_lethal", document.glock.primaryHeadshotLethal ? L"1" : L"0");
    }

    if (includeMp5) {
        AddBlankLine(lines);
        AddLine(lines, L"sv_exp_mp5_primary_enabled", document.mp5.primaryEnabled ? L"1" : L"0");
        AddLine(lines, L"sv_exp_mp5_profile_name", QuoteCfgString(document.mp5.profileName));
        if (!NormalizeFloatValue(document.mp5.primaryBaseSpread, normalized, errorMessage, L"MP5 base spread")) {
            return false;
        }
        AddLine(lines, L"sv_exp_mp5_primary_base_spread", normalized);
        if (!NormalizeFloatValue(document.mp5.primaryGroundMovePenalty, normalized, errorMessage, L"MP5 ground move penalty")) {
            return false;
        }
        AddLine(lines, L"sv_exp_mp5_primary_ground_move_penalty", normalized);
        if (!NormalizeFloatValue(document.mp5.primaryAirMovePenalty, normalized, errorMessage, L"MP5 air move penalty")) {
            return false;
        }
        AddLine(lines, L"sv_exp_mp5_primary_air_move_penalty", normalized);
        if (!NormalizeFloatValue(document.mp5.primaryDuckPenaltyScale, normalized, errorMessage, L"MP5 duck penalty scale")) {
            return false;
        }
        AddLine(lines, L"sv_exp_mp5_primary_duck_penalty_scale", normalized);
        if (!NormalizeFloatValue(document.mp5.primaryBurstGrowth, normalized, errorMessage, L"MP5 burst growth")) {
            return false;
        }
        AddLine(lines, L"sv_exp_mp5_primary_burst_growth", normalized);
        if (!NormalizeFloatValue(document.mp5.primaryBurstMaxAdditionalSpread, normalized, errorMessage, L"MP5 burst max additional spread")) {
            return false;
        }
        AddLine(lines, L"sv_exp_mp5_primary_burst_max_additional_spread", normalized);
        if (!NormalizeFloatValue(document.mp5.primarySpreadRecovery, normalized, errorMessage, L"MP5 spread recovery")) {
            return false;
        }
        AddLine(lines, L"sv_exp_mp5_primary_spread_recovery", normalized);
        AddLine(lines, L"sv_exp_mp5_primary_first_shot_accuracy", document.mp5.primaryFirstShotAccuracy ? L"1" : L"0");
        if (!NormalizeFloatValue(document.mp5.primaryFirstShotSpeedThreshold, normalized, errorMessage, L"MP5 first-shot speed threshold")) {
            return false;
        }
        AddLine(lines, L"sv_exp_mp5_primary_first_shot_speed_threshold", normalized);
        if (!NormalizeFloatValue(document.mp5.primaryMaxSpread, normalized, errorMessage, L"MP5 max spread")) {
            return false;
        }
        AddLine(lines, L"sv_exp_mp5_primary_max_spread", normalized);
        if (!NormalizeFloatValue(document.mp5.primaryDamage, normalized, errorMessage, L"MP5 damage")) {
            return false;
        }
        AddLine(lines, L"sv_exp_mp5_primary_damage", normalized);
        if (!NormalizeFloatValue(document.mp5.primaryHeadshotScale, normalized, errorMessage, L"MP5 headshot scale")) {
            return false;
        }
        AddLine(lines, L"sv_exp_mp5_primary_headshot_scale", normalized);
        AddLine(lines, L"sv_exp_mp5_primary_headshot_lethal", document.mp5.primaryHeadshotLethal ? L"1" : L"0");
        AddLine(lines, L"sv_exp_mp5_lab_loadout", document.mp5.labLoadout ? L"1" : L"0");
        if (!NormalizeIntegerValue(document.mp5.labAmmo, normalized, errorMessage, L"MP5 lab ammo")) {
            return false;
        }
        AddLine(lines, L"sv_exp_mp5_lab_ammo", normalized);
        AddLine(lines, L"sv_exp_mp5_lab_autoswitch", document.mp5.labAutoswitch ? L"1" : L"0");
    }

    if (include357) {
        AddBlankLine(lines);
        AddLine(lines, L"sv_exp_357_primary_enabled", document.weapon357.primaryEnabled ? L"1" : L"0");
        AddLine(lines, L"sv_exp_357_profile_name", QuoteCfgString(document.weapon357.profileName));
        if (!NormalizeFloatValue(document.weapon357.primaryBaseSpread, normalized, errorMessage, L"357 base spread")) {
            return false;
        }
        AddLine(lines, L"sv_exp_357_primary_base_spread", normalized);
        if (!NormalizeFloatValue(document.weapon357.primaryGroundMovePenalty, normalized, errorMessage, L"357 ground move penalty")) {
            return false;
        }
        AddLine(lines, L"sv_exp_357_primary_ground_move_penalty", normalized);
        if (!NormalizeFloatValue(document.weapon357.primaryAirMovePenalty, normalized, errorMessage, L"357 air move penalty")) {
            return false;
        }
        AddLine(lines, L"sv_exp_357_primary_air_move_penalty", normalized);
        if (!NormalizeFloatValue(document.weapon357.primaryDuckPenaltyScale, normalized, errorMessage, L"357 duck penalty scale")) {
            return false;
        }
        AddLine(lines, L"sv_exp_357_primary_duck_penalty_scale", normalized);
        AddLine(lines, L"sv_exp_357_primary_first_shot_accuracy", document.weapon357.primaryFirstShotAccuracy ? L"1" : L"0");
        if (!NormalizeFloatValue(document.weapon357.primaryFirstShotSpeedThreshold, normalized, errorMessage, L"357 first-shot speed threshold")) {
            return false;
        }
        AddLine(lines, L"sv_exp_357_primary_first_shot_speed_threshold", normalized);
        if (!NormalizeFloatValue(document.weapon357.primarySpreadRecovery, normalized, errorMessage, L"357 spread recovery")) {
            return false;
        }
        AddLine(lines, L"sv_exp_357_primary_spread_recovery", normalized);
        if (!NormalizeFloatValue(document.weapon357.primaryMaxSpread, normalized, errorMessage, L"357 max spread")) {
            return false;
        }
        AddLine(lines, L"sv_exp_357_primary_max_spread", normalized);
        if (!NormalizeFloatValue(document.weapon357.primaryDamage, normalized, errorMessage, L"357 damage")) {
            return false;
        }
        AddLine(lines, L"sv_exp_357_primary_damage", normalized);
        if (!NormalizeFloatValue(document.weapon357.primaryHeadshotScale, normalized, errorMessage, L"357 headshot scale")) {
            return false;
        }
        AddLine(lines, L"sv_exp_357_primary_headshot_scale", normalized);
        AddLine(lines, L"sv_exp_357_primary_headshot_lethal", document.weapon357.primaryHeadshotLethal ? L"1" : L"0");
        AddLine(lines, L"sv_exp_357_lab_loadout", document.weapon357.labLoadout ? L"1" : L"0");
        if (!NormalizeIntegerValue(document.weapon357.labAmmo, normalized, errorMessage, L"357 lab ammo")) {
            return false;
        }
        AddLine(lines, L"sv_exp_357_lab_ammo", normalized);
        AddLine(lines, L"sv_exp_357_lab_autoswitch", document.weapon357.labAutoswitch ? L"1" : L"0");
    }

    if (includeShotgun) {
        AddBlankLine(lines);
        AddLine(lines, L"sv_exp_shotgun_primary_enabled", document.shotgun.primaryEnabled ? L"1" : L"0");
        AddLine(lines, L"sv_exp_shotgun_profile_name", QuoteCfgString(document.shotgun.profileName));
        if (!NormalizeFloatValue(document.shotgun.primaryBaseSpread, normalized, errorMessage, L"Shotgun base spread")) {
            return false;
        }
        AddLine(lines, L"sv_exp_shotgun_primary_base_spread", normalized);
        if (!NormalizeFloatValue(document.shotgun.primaryGroundMovePenalty, normalized, errorMessage, L"Shotgun ground move penalty")) {
            return false;
        }
        AddLine(lines, L"sv_exp_shotgun_primary_ground_move_penalty", normalized);
        if (!NormalizeFloatValue(document.shotgun.primaryAirMovePenalty, normalized, errorMessage, L"Shotgun air move penalty")) {
            return false;
        }
        AddLine(lines, L"sv_exp_shotgun_primary_air_move_penalty", normalized);
        if (!NormalizeFloatValue(document.shotgun.primaryDuckPenaltyScale, normalized, errorMessage, L"Shotgun duck penalty scale")) {
            return false;
        }
        AddLine(lines, L"sv_exp_shotgun_primary_duck_penalty_scale", normalized);
        AddLine(lines, L"sv_exp_shotgun_primary_first_shot_accuracy", document.shotgun.primaryFirstShotAccuracy ? L"1" : L"0");
        if (!NormalizeFloatValue(document.shotgun.primaryFirstShotSpeedThreshold, normalized, errorMessage, L"Shotgun first-shot speed threshold")) {
            return false;
        }
        AddLine(lines, L"sv_exp_shotgun_primary_first_shot_speed_threshold", normalized);
        if (!NormalizeFloatValue(document.shotgun.primarySpreadRecovery, normalized, errorMessage, L"Shotgun spread recovery")) {
            return false;
        }
        AddLine(lines, L"sv_exp_shotgun_primary_spread_recovery", normalized);
        if (!NormalizeFloatValue(document.shotgun.primaryMaxSpread, normalized, errorMessage, L"Shotgun max spread")) {
            return false;
        }
        AddLine(lines, L"sv_exp_shotgun_primary_max_spread", normalized);
        if (!NormalizeFloatValue(document.shotgun.primaryDamagePerPellet, normalized, errorMessage, L"Shotgun damage per pellet")) {
            return false;
        }
        AddLine(lines, L"sv_exp_shotgun_primary_damage_per_pellet", normalized);
        if (!NormalizeIntegerValue(document.shotgun.primaryPelletCount, normalized, errorMessage, L"Shotgun pellet count")) {
            return false;
        }
        AddLine(lines, L"sv_exp_shotgun_primary_pellet_count", normalized);
        if (!NormalizeFloatValue(document.shotgun.primaryHeadshotScale, normalized, errorMessage, L"Shotgun headshot scale")) {
            return false;
        }
        AddLine(lines, L"sv_exp_shotgun_primary_headshot_scale", normalized);
        AddLine(lines, L"sv_exp_shotgun_primary_headshot_lethal", document.shotgun.primaryHeadshotLethal ? L"1" : L"0");
        AddLine(lines, L"sv_exp_shotgun_lab_loadout", document.shotgun.labLoadout ? L"1" : L"0");
        if (!NormalizeIntegerValue(document.shotgun.labAmmo, normalized, errorMessage, L"Shotgun lab ammo")) {
            return false;
        }
        AddLine(lines, L"sv_exp_shotgun_lab_ammo", normalized);
        AddLine(lines, L"sv_exp_shotgun_lab_autoswitch", document.shotgun.labAutoswitch ? L"1" : L"0");
    }

    if (includeDummy) {
        AddBlankLine(lines);
        AddLine(lines, L"sv_exp_glock_lab_dummy", document.targetDummy.enabled ? L"1" : L"0");
        AddLine(lines, L"sv_exp_glock_lab_target_profile_name", QuoteCfgString(document.targetDummy.targetProfileName));
        if (!NormalizeFloatValue(document.targetDummy.dummyHealth, normalized, errorMessage, L"Dummy health")) {
            return false;
        }
        AddLine(lines, L"sv_exp_glock_lab_dummy_health", normalized);
        if (!NormalizeFloatValue(document.targetDummy.dummyArmor, normalized, errorMessage, L"Dummy armor")) {
            return false;
        }
        AddLine(lines, L"sv_exp_glock_lab_dummy_armor", normalized);
        AddLine(lines, L"sv_exp_glock_lab_dummy_head_protected", document.targetDummy.dummyHeadProtected ? L"1" : L"0");
        if (!NormalizeFloatValue(document.targetDummy.armorHealthFraction, normalized, errorMessage, L"Dummy armor health fraction")) {
            return false;
        }
        AddLine(lines, L"sv_exp_glock_lab_dummy_armor_health_fraction", normalized);
        if (!NormalizeFloatValue(document.targetDummy.armorDrainScale, normalized, errorMessage, L"Dummy armor drain scale")) {
            return false;
        }
        AddLine(lines, L"sv_exp_glock_lab_dummy_armor_drain_scale", normalized);
        AddLine(lines, L"sv_exp_glock_lab_dummy_autorespawn", document.targetDummy.autorespawn ? L"1" : L"0");
        if (!NormalizeFloatValue(document.targetDummy.respawnDelay, normalized, errorMessage, L"Dummy respawn delay")) {
            return false;
        }
        AddLine(lines, L"sv_exp_glock_lab_dummy_respawn_delay", normalized);
        if (!NormalizeFloatValue(document.targetDummy.spawnDistance, normalized, errorMessage, L"Dummy spawn distance")) {
            return false;
        }
        AddLine(lines, L"sv_exp_glock_lab_dummy_spawn_distance", normalized);
        if (!NormalizeFloatValue(document.targetDummy.offsetRight, normalized, errorMessage, L"Dummy right offset")) {
            return false;
        }
        AddLine(lines, L"sv_exp_glock_lab_dummy_offset_right", normalized);
        if (!NormalizeFloatValue(document.targetDummy.offsetUp, normalized, errorMessage, L"Dummy up offset")) {
            return false;
        }
        AddLine(lines, L"sv_exp_glock_lab_dummy_offset_up", normalized);
        AddLine(lines, L"sv_exp_glock_lab_dummy_face_player", document.targetDummy.facePlayer ? L"1" : L"0");
        AddLine(lines, L"sv_exp_glock_lab_dummy_model", QuoteCfgString(document.targetDummy.model));
    }

    if (includeRound) {
        AddBlankLine(lines);
        AddLine(lines, L"sv_exp_round_mode", document.roundMode.enabled ? L"1" : L"0");
        if (!NormalizeFloatValue(document.roundMode.freezeTime, normalized, errorMessage, L"Round freeze time")) {
            return false;
        }
        AddLine(lines, L"sv_exp_round_freeze_time", normalized);
        if (!NormalizeFloatValue(document.roundMode.restartDelay, normalized, errorMessage, L"Round restart delay")) {
            return false;
        }
        AddLine(lines, L"sv_exp_round_restart_delay", normalized);
        if (!NormalizeFloatValue(document.roundMode.startHealth, normalized, errorMessage, L"Round start health")) {
            return false;
        }
        AddLine(lines, L"sv_exp_round_start_health", normalized);
        if (!NormalizeFloatValue(document.roundMode.startArmor, normalized, errorMessage, L"Round start armor")) {
            return false;
        }
        AddLine(lines, L"sv_exp_round_start_armor", normalized);
        AddLine(lines, L"sv_exp_round_no_respawn", document.roundMode.noRespawn ? L"1" : L"0");
        AddLine(lines, L"sv_exp_round_friendlyfire", document.roundMode.friendlyFire ? L"1" : L"0");
        AddLine(lines, L"sv_exp_round_weapon_profile", QuoteCfgString(document.roundMode.weaponProfile));
        AddLine(lines, L"sv_exp_round_loadout_mode", QuoteCfgString(document.roundMode.loadoutMode));
    }

    if (includeTeamRound) {
        AddBlankLine(lines);
        AddLine(lines, L"sv_exp_team_round_mode", document.teamRound.enabled ? L"1" : L"0");
        AddLine(lines, L"sv_exp_team_round_teamplay", document.teamRound.teamplay ? L"1" : L"0");
        AddLine(lines, L"sv_exp_team_round_spawn_mode", QuoteCfgString(document.teamRound.spawnMode));
        AddLine(lines, L"sv_exp_team_round_team1_name", QuoteCfgString(document.teamRound.team1Name));
        AddLine(lines, L"sv_exp_team_round_team2_name", QuoteCfgString(document.teamRound.team2Name));
        AddLine(lines, L"sv_exp_team_round_team1_loadout", QuoteCfgString(document.teamRound.team1Loadout));
        AddLine(lines, L"sv_exp_team_round_team2_loadout", QuoteCfgString(document.teamRound.team2Loadout));
        if (!NormalizeFloatValue(document.teamRound.team1Health, normalized, errorMessage, L"Team 1 health")) {
            return false;
        }
        AddLine(lines, L"sv_exp_team_round_team1_health", normalized);
        if (!NormalizeFloatValue(document.teamRound.team2Health, normalized, errorMessage, L"Team 2 health")) {
            return false;
        }
        AddLine(lines, L"sv_exp_team_round_team2_health", normalized);
        if (!NormalizeFloatValue(document.teamRound.team1Armor, normalized, errorMessage, L"Team 1 armor")) {
            return false;
        }
        AddLine(lines, L"sv_exp_team_round_team1_armor", normalized);
        if (!NormalizeFloatValue(document.teamRound.team2Armor, normalized, errorMessage, L"Team 2 armor")) {
            return false;
        }
        AddLine(lines, L"sv_exp_team_round_team2_armor", normalized);
    }

    if (includeBuy) {
        AddBlankLine(lines);
        AddLine(lines, L"sv_exp_buy_mode", document.buy.enabled ? L"1" : L"0");
        AddLine(lines, L"sv_exp_buy_freeze_only", document.buy.freezeOnly ? L"1" : L"0");
        AddLine(lines, L"sv_exp_buy_team_shared_catalog", document.buy.teamSharedCatalog ? L"1" : L"0");
        if (!NormalizeIntegerValue(document.buy.startMoney, normalized, errorMessage, L"Buy start money")) {
            return false;
        }
        AddLine(lines, L"sv_exp_buy_start_money", normalized);
        if (!NormalizeIntegerValue(document.buy.roundWinReward, normalized, errorMessage, L"Buy round win reward")) {
            return false;
        }
        AddLine(lines, L"sv_exp_buy_round_win_reward", normalized);
        if (!NormalizeIntegerValue(document.buy.roundLossReward, normalized, errorMessage, L"Buy round loss reward")) {
            return false;
        }
        AddLine(lines, L"sv_exp_buy_round_loss_reward", normalized);
        if (!NormalizeIntegerValue(document.buy.maxMoney, normalized, errorMessage, L"Buy max money")) {
            return false;
        }
        AddLine(lines, L"sv_exp_buy_max_money", normalized);
        AddLine(lines, L"sv_exp_buy_allow_glock", document.buy.allowGlock ? L"1" : L"0");
        AddLine(lines, L"sv_exp_buy_allow_mp5", document.buy.allowMp5 ? L"1" : L"0");
        AddLine(lines, L"sv_exp_buy_allow_357", document.buy.allow357 ? L"1" : L"0");
        AddLine(lines, L"sv_exp_buy_allow_shotgun", document.buy.allowShotgun ? L"1" : L"0");
        AddLine(lines, L"sv_exp_buy_allow_armor", document.buy.allowArmor ? L"1" : L"0");
        AddLine(lines, L"sv_exp_buy_allow_helmet", document.buy.allowHelmet ? L"1" : L"0");
        AddLine(lines, L"sv_exp_buy_allow_handgrenade", document.buy.allowHandgrenade ? L"1" : L"0");
        if (!NormalizeIntegerValue(document.buy.costGlock, normalized, errorMessage, L"Buy cost glock")) {
            return false;
        }
        AddLine(lines, L"sv_exp_buy_cost_glock", normalized);
        if (!NormalizeIntegerValue(document.buy.costMp5, normalized, errorMessage, L"Buy cost mp5")) {
            return false;
        }
        AddLine(lines, L"sv_exp_buy_cost_mp5", normalized);
        if (!NormalizeIntegerValue(document.buy.cost357, normalized, errorMessage, L"Buy cost 357")) {
            return false;
        }
        AddLine(lines, L"sv_exp_buy_cost_357", normalized);
        if (!NormalizeIntegerValue(document.buy.costShotgun, normalized, errorMessage, L"Buy cost shotgun")) {
            return false;
        }
        AddLine(lines, L"sv_exp_buy_cost_shotgun", normalized);
        if (!NormalizeIntegerValue(document.buy.costArmor, normalized, errorMessage, L"Buy cost armor")) {
            return false;
        }
        AddLine(lines, L"sv_exp_buy_cost_armor", normalized);
        if (!NormalizeIntegerValue(document.buy.costHelmet, normalized, errorMessage, L"Buy cost helmet")) {
            return false;
        }
        AddLine(lines, L"sv_exp_buy_cost_helmet", normalized);
        if (!NormalizeIntegerValue(document.buy.costHandgrenade, normalized, errorMessage, L"Buy cost handgrenade")) {
            return false;
        }
        AddLine(lines, L"sv_exp_buy_cost_handgrenade", normalized);
    }

    if (includeArmor) {
        AddBlankLine(lines);
        AddLine(lines, L"sv_exp_armor_mode", document.armorEquipment.armorMode ? L"1" : L"0");
        if (!NormalizeFloatValue(document.armorEquipment.armorStartValue, normalized, errorMessage, L"Armor start value")) {
            return false;
        }
        AddLine(lines, L"sv_exp_armor_start_value", normalized);
        if (!NormalizeFloatValue(document.armorEquipment.armorMaxValue, normalized, errorMessage, L"Armor max value")) {
            return false;
        }
        AddLine(lines, L"sv_exp_armor_max_value", normalized);
        if (!NormalizeFloatValue(document.armorEquipment.armorHealthFraction, normalized, errorMessage, L"Armor health fraction")) {
            return false;
        }
        AddLine(lines, L"sv_exp_armor_health_fraction", normalized);
        if (!NormalizeFloatValue(document.armorEquipment.armorDrainScale, normalized, errorMessage, L"Armor drain scale")) {
            return false;
        }
        AddLine(lines, L"sv_exp_armor_drain_scale", normalized);
        AddLine(lines, L"sv_exp_helmet_mode", document.armorEquipment.helmetMode ? L"1" : L"0");
        AddLine(lines, L"sv_exp_helmet_start_enabled", document.armorEquipment.helmetStartEnabled ? L"1" : L"0");
        AddLine(lines, L"sv_exp_helmet_headshot_protection", document.armorEquipment.helmetHeadshotProtection ? L"1" : L"0");
    }

    return true;
}

}  // namespace

EnvironmentPaths ResolveEnvironmentPaths(const std::wstring& moduleFilePath) {
    EnvironmentPaths paths;
    paths.repoRoot = FindRepoRoot(moduleFilePath);

    if (!paths.repoRoot.empty()) {
        paths.stagedLiveModRoot = (std::filesystem::path(paths.repoRoot) / L"testbed" / L"mods" / L"hlserver_testbed").wstring();
        paths.logsRoot = (std::filesystem::path(paths.repoRoot) / L"testbed" / L"logs").wstring();
    }

    const std::wstring halfLifeRoot = DetectHalfLifeRoot(paths.repoRoot);
    if (!halfLifeRoot.empty()) {
        paths.liveModRoot = (std::filesystem::path(halfLifeRoot) / L"hlserver_testbed").wstring();
        paths.defaultExportFolder = paths.liveModRoot;
    } else if (!paths.stagedLiveModRoot.empty()) {
        paths.defaultExportFolder = paths.stagedLiveModRoot;
    }

    return paths;
}

bool BuildExportResult(const ProjectDocument& document, const EnvironmentPaths& environment, ExportResult& result, std::wstring& errorMessage) {
    std::wstring exportFolder = Trim(document.exportSettings.exportFolder);
    if (exportFolder.empty()) {
        exportFolder = environment.defaultExportFolder;
    }

    if (exportFolder.empty()) {
        errorMessage = L"Choose an export folder before exporting.";
        return false;
    }

    std::vector<std::wstring> lines;
    if (!BuildCfgLines(document, lines, errorMessage)) {
        return false;
    }

    std::wstring cfgText;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        cfgText += lines[index];
        cfgText += L"\r\n";
    }

    const std::filesystem::path exportPath = std::filesystem::path(exportFolder) / EnsureCfgFileName(document.exportSettings.cfgFileName);
    std::filesystem::path cfgProfile;
    TryBuildCfgProfile(exportPath, environment, cfgProfile);

    result.cfgText = std::move(cfgText);
    result.exportPath = exportPath.wstring();
    result.cfgProfile = cfgProfile.generic_wstring();
    result.execCommand = BuildExecCommand(cfgProfile, exportPath);
    result.launcherCommand = BuildLauncherCommand(cfgProfile);
    return true;
}

bool ExportCfgToFile(const ProjectDocument& document, const EnvironmentPaths& environment, ExportResult& result, std::wstring& errorMessage) {
    if (!BuildExportResult(document, environment, result, errorMessage)) {
        return false;
    }

    return WriteUtf8TextFile(result.exportPath, result.cfgText, errorMessage);
}

}  // namespace hlcfg
