#include "ConfigProject.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <vector>
#include <windows.h>

#include "JsonLite.h"

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

bool ReadUtf8TextFile(const std::wstring& path, std::wstring& content, std::wstring& errorMessage) {
    std::ifstream stream(std::filesystem::path(path), std::ios::binary);
    if (!stream) {
        errorMessage = L"Unable to open the project file.";
        return false;
    }

    const std::string bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    content = Utf8ToWide(bytes);
    return true;
}

bool WriteUtf8TextFile(const std::wstring& path, const std::wstring& content, std::wstring& errorMessage) {
    const std::filesystem::path filePath(path);
    if (filePath.has_parent_path()) {
        std::error_code createDirectoriesError;
        std::filesystem::create_directories(filePath.parent_path(), createDirectoriesError);
        if (createDirectoriesError) {
            errorMessage = L"Unable to create the destination directory.";
            return false;
        }
    }

    std::ofstream stream(filePath, std::ios::binary | std::ios::trunc);
    if (!stream) {
        errorMessage = L"Unable to write the project file.";
        return false;
    }

    const std::string bytes = WideToUtf8(content);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!stream.good()) {
        errorMessage = L"Unable to finish writing the project file.";
        return false;
    }

    return true;
}

bool EndsWithInsensitive(const std::wstring& value, const std::wstring& suffix) {
    if (suffix.size() > value.size()) {
        return false;
    }

    return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin(), [](wchar_t left, wchar_t right) {
        return ::towlower(left) == ::towlower(right);
    });
}

JsonValue::Object MakeObject() {
    return JsonValue::MakeObject().AsObject();
}

JsonValue::Object& AddObjectMember(JsonValue::Object& parent, const std::wstring& key) {
    parent[key] = JsonValue::MakeObject();
    return parent[key].AsObject();
}

void SetString(JsonValue::Object& object, const std::wstring& key, const std::wstring& value) {
    object[key] = JsonValue(value);
}

void SetBool(JsonValue::Object& object, const std::wstring& key, bool value) {
    object[key] = JsonValue(value);
}

const JsonValue* FindMember(const JsonValue::Object& object, const std::wstring& key) {
    const auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
}

std::wstring ReadStringValue(const JsonValue::Object& object, const std::wstring& key, const std::wstring& defaultValue) {
    const JsonValue* value = FindMember(object, key);
    return value != nullptr && value->IsString() ? value->AsString() : defaultValue;
}

bool ReadBoolValue(const JsonValue::Object& object, const std::wstring& key, bool defaultValue) {
    const JsonValue* value = FindMember(object, key);
    return value != nullptr && value->IsBool() ? value->AsBool() : defaultValue;
}

int ReadIntValue(const JsonValue::Object& object, const std::wstring& key, int defaultValue) {
    const JsonValue* value = FindMember(object, key);
    if (value == nullptr || !value->IsNumber()) {
        return defaultValue;
    }

    return static_cast<int>(value->AsNumber());
}

const JsonValue::Object* FindObject(const JsonValue::Object& parent, const std::wstring& key) {
    const JsonValue* value = FindMember(parent, key);
    return value != nullptr && value->IsObject() ? &value->AsObject() : nullptr;
}

}  // namespace

ProjectDocument CreateDefaultProject() {
    ProjectDocument document;
    document.metadata.projectName = L"Untitled weapon project";
    document.exportSettings.cfgFileName = L"weapon-test.cfg";

    document.glock.spreadRecovery = L"0.8000";
    document.glock.moveSpreadScale = L"1.0";
    document.glock.profileName = L"default";
    document.glock.primaryBaseSpread = L"0.01";
    document.glock.primaryGroundMovePenalty = L"0.08";
    document.glock.primaryAirMovePenalty = L"0.12";
    document.glock.primaryDuckPenaltyScale = L"0.60";
    document.glock.primaryShotGrowth = L"0.0800";
    document.glock.primaryFirstShotSpeedThreshold = L"45.0";
    document.glock.primaryMaxSpread = L"0.1800";
    document.glock.cadenceMode = false;
    document.glock.primaryCadenceCycleTime = L"0.4200";
    document.glock.primaryClickPenalty = L"0.0350";
    document.glock.primaryClickPenaltyScale = L"1.4000";
    document.glock.primaryClickResetTime = L"0.5000";
    document.glock.primaryHoldPenaltyScale = L"0.5000";
    document.glock.patternScaleX = L"0.2000";
    document.glock.patternScaleY = L"0.3500";
    document.glock.patternResetTime = L"0.3500";
    document.glock.patternMaxIndex = L"5";
    document.glock.primaryDamage = L"10.0";
    document.glock.primaryHeadshotScale = L"4.0";

    document.mp5.profileName = L"default";
    document.mp5.primaryBaseSpread = L"0.0380";
    document.mp5.primaryGroundMovePenalty = L"0.0240";
    document.mp5.primaryAirMovePenalty = L"0.0550";
    document.mp5.primaryDuckPenaltyScale = L"0.6200";
    document.mp5.primaryBurstGrowth = L"0.0140";
    document.mp5.primaryBurstMaxAdditionalSpread = L"0.0950";
    document.mp5.primarySpreadRecovery = L"0.8000";
    document.mp5.primaryFirstShotSpeedThreshold = L"30.0";
    document.mp5.primaryMaxSpread = L"0.1250";
    document.mp5.patternScaleX = L"0.2600";
    document.mp5.patternScaleY = L"0.5500";
    document.mp5.patternResetTime = L"0.2800";
    document.mp5.patternMaxIndex = L"6";
    document.mp5.primaryDamage = L"12.0";
    document.mp5.primaryHeadshotScale = L"3.25";
    document.mp5.labAmmo = L"250";

    document.weapon357.profileName = L"default";
    document.weapon357.primaryBaseSpread = L"0.0087";
    document.weapon357.primaryGroundMovePenalty = L"0.0200";
    document.weapon357.primaryAirMovePenalty = L"0.0800";
    document.weapon357.primaryDuckPenaltyScale = L"0.7000";
    document.weapon357.primaryFirstShotSpeedThreshold = L"25.0";
    document.weapon357.primarySpreadRecovery = L"0.7500";
    document.weapon357.primaryMaxSpread = L"0.1200";
    document.weapon357.cadenceMode = false;
    document.weapon357.primaryCadenceCycleTime = L"1.0000";
    document.weapon357.primaryClickPenalty = L"0.0500";
    document.weapon357.primaryClickPenaltyScale = L"1.2500";
    document.weapon357.primaryClickResetTime = L"1.2000";
    document.weapon357.primaryHoldPenaltyScale = L"0.4500";
    document.weapon357.primaryDamage = L"40.0";
    document.weapon357.primaryHeadshotScale = L"2.5";
    document.weapon357.labAmmo = L"36";

    document.shotgun.profileName = L"default";
    document.shotgun.primaryBaseSpread = L"0.0600";
    document.shotgun.primaryGroundMovePenalty = L"0.0300";
    document.shotgun.primaryAirMovePenalty = L"0.0800";
    document.shotgun.primaryDuckPenaltyScale = L"0.8000";
    document.shotgun.primaryFirstShotSpeedThreshold = L"35.0";
    document.shotgun.primarySpreadRecovery = L"0.8500";
    document.shotgun.primaryMaxSpread = L"0.1200";
    document.shotgun.primaryDamagePerPellet = L"5.0";
    document.shotgun.primaryPelletCount = L"6";
    document.shotgun.primaryHeadshotScale = L"1.5";
    document.shotgun.labAmmo = L"48";

    document.targetDummy.targetProfileName = L"default";
    document.targetDummy.dummyHealth = L"100.0";
    document.targetDummy.dummyArmor = L"0.0";
    document.targetDummy.armorHealthFraction = L"0.5";
    document.targetDummy.armorDrainScale = L"1.0";
    document.targetDummy.respawnDelay = L"1.0";
    document.targetDummy.spawnDistance = L"256.0";
    document.targetDummy.offsetRight = L"0.0";
    document.targetDummy.offsetUp = L"0.0";
    document.targetDummy.model = L"models/barney.mdl";

    document.roundMode.freezeTime = L"3.0";
    document.roundMode.restartDelay = L"3.0";
    document.roundMode.startHealth = L"100.0";
    document.roundMode.startArmor = L"0.0";
    document.roundMode.weaponProfile = L"";
    document.roundMode.loadoutMode = L"none";

    document.teamRound.spawnMode = L"dm_spawns";
    document.teamRound.team1Name = L"team1";
    document.teamRound.team2Name = L"team2";
    document.teamRound.team1Loadout = L"";
    document.teamRound.team2Loadout = L"";
    document.teamRound.team1Health = L"-1.0";
    document.teamRound.team2Health = L"-1.0";
    document.teamRound.team1Armor = L"-1.0";
    document.teamRound.team2Armor = L"-1.0";

    document.buy.startMoney = L"2000";
    document.buy.roundWinReward = L"1000";
    document.buy.roundLossReward = L"500";
    document.buy.maxMoney = L"16000";
    document.buy.costGlock = L"200";
    document.buy.costMp5 = L"1500";
    document.buy.cost357 = L"1200";
    document.buy.costShotgun = L"1700";
    document.buy.costArmor = L"650";
    document.buy.costHelmet = L"350";
    document.buy.costHandgrenade = L"300";

    document.armorEquipment.armorStartValue = L"0.0";
    document.armorEquipment.armorMaxValue = L"100.0";
    document.armorEquipment.armorHealthFraction = L"0.5";
    document.armorEquipment.armorDrainScale = L"1.0";

    return document;
}

std::wstring Trimmed(const std::wstring& value) {
    std::size_t start = 0;
    while (start < value.size() && ::iswspace(value[start])) {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && ::iswspace(value[end - 1])) {
        --end;
    }

    return value.substr(start, end - start);
}

std::wstring EnsureProjectFileName(const std::wstring& value) {
    std::wstring fileName = Trimmed(value);
    if (fileName.empty()) {
        fileName = L"untitled.hlcfg.json";
    }

    if (!EndsWithInsensitive(fileName, L".hlcfg.json") && !EndsWithInsensitive(fileName, L".json")) {
        fileName += L".hlcfg.json";
    }

    return fileName;
}

std::wstring EnsureCfgFileName(const std::wstring& value) {
    std::wstring fileName = std::filesystem::path(Trimmed(value)).filename().wstring();
    if (fileName.empty()) {
        fileName = L"weapon-test.cfg";
    }

    if (!EndsWithInsensitive(fileName, L".cfg")) {
        fileName += L".cfg";
    }

    return fileName;
}

bool SaveProjectDocumentToFile(const ProjectDocument& document, const std::wstring& path, std::wstring& errorMessage) {
    JsonValue root = JsonValue::MakeObject();
    JsonValue::Object& rootObject = root.AsObject();

    rootObject[L"schemaVersion"] = JsonValue(static_cast<double>(ProjectDocument::kSchemaVersion));

    JsonValue::Object& metadata = AddObjectMember(rootObject, L"metadata");
    SetString(metadata, L"projectName", document.metadata.projectName);
    SetString(metadata, L"author", document.metadata.author);
    SetString(metadata, L"notes", document.metadata.notes);

    JsonValue::Object& exportSettings = AddObjectMember(rootObject, L"export");
    SetString(exportSettings, L"folder", document.exportSettings.exportFolder);
    SetString(exportSettings, L"fileName", EnsureCfgFileName(document.exportSettings.cfgFileName));

    JsonValue::Object& matchPack = AddObjectMember(rootObject, L"matchPack");
    SetString(matchPack, L"name", document.matchPack.name);
    SetString(matchPack, L"description", document.matchPack.description);
    SetString(matchPack, L"tags", document.matchPack.tags);

    JsonValue::Object& general = AddObjectMember(rootObject, L"general");
    SetString(general, L"sv_exp_weapon_under_test", document.general.weaponUnderTest);
    SetString(general, L"sv_exp_session_tag", document.general.sessionTag);
    SetBool(general, L"sv_exp_debug_weaponlog", document.general.debugWeaponLog);
    SetBool(general, L"sv_exp_debug_weaponlog_rejections", document.general.debugWeaponLogRejections);

    JsonValue::Object& glock = AddObjectMember(rootObject, L"glock");
    SetBool(glock, L"sv_exp_pistol_tapfire", document.glock.tapFire);
    SetBool(glock, L"sv_exp_first_shot_accuracy", document.glock.firstShotAccuracy);
    SetString(glock, L"sv_exp_spread_recovery", document.glock.spreadRecovery);
    SetString(glock, L"sv_exp_move_spread_scale", document.glock.moveSpreadScale);
    SetString(glock, L"sv_exp_glock_profile_name", document.glock.profileName);
    SetString(glock, L"sv_exp_glock_primary_base_spread", document.glock.primaryBaseSpread);
    SetString(glock, L"sv_exp_glock_primary_ground_move_penalty", document.glock.primaryGroundMovePenalty);
    SetString(glock, L"sv_exp_glock_primary_air_move_penalty", document.glock.primaryAirMovePenalty);
    SetString(glock, L"sv_exp_glock_primary_duck_penalty_scale", document.glock.primaryDuckPenaltyScale);
    SetString(glock, L"sv_exp_glock_primary_shot_growth", document.glock.primaryShotGrowth);
    SetString(glock, L"sv_exp_glock_primary_first_shot_speed_threshold", document.glock.primaryFirstShotSpeedThreshold);
    SetString(glock, L"sv_exp_glock_primary_max_spread", document.glock.primaryMaxSpread);
    SetBool(glock, L"sv_exp_glock_primary_cadence_mode", document.glock.cadenceMode);
    SetString(glock, L"sv_exp_glock_primary_cycle_time", document.glock.primaryCadenceCycleTime);
    SetString(glock, L"sv_exp_glock_primary_click_penalty", document.glock.primaryClickPenalty);
    SetString(glock, L"sv_exp_glock_primary_click_penalty_scale", document.glock.primaryClickPenaltyScale);
    SetString(glock, L"sv_exp_glock_primary_click_reset_time", document.glock.primaryClickResetTime);
    SetString(glock, L"sv_exp_glock_primary_hold_penalty_scale", document.glock.primaryHoldPenaltyScale);
    SetBool(glock, L"sv_exp_glock_pattern_mode", document.glock.patternMode);
    SetString(glock, L"sv_exp_glock_pattern_scale_x", document.glock.patternScaleX);
    SetString(glock, L"sv_exp_glock_pattern_scale_y", document.glock.patternScaleY);
    SetString(glock, L"sv_exp_glock_pattern_reset_time", document.glock.patternResetTime);
    SetString(glock, L"sv_exp_glock_pattern_max_index", document.glock.patternMaxIndex);
    SetString(glock, L"sv_exp_glock_primary_damage", document.glock.primaryDamage);
    SetString(glock, L"sv_exp_glock_primary_headshot_scale", document.glock.primaryHeadshotScale);
    SetBool(glock, L"sv_exp_glock_primary_headshot_lethal", document.glock.primaryHeadshotLethal);

    JsonValue::Object& mp5 = AddObjectMember(rootObject, L"mp5");
    SetBool(mp5, L"sv_exp_mp5_primary_enabled", document.mp5.primaryEnabled);
    SetString(mp5, L"sv_exp_mp5_profile_name", document.mp5.profileName);
    SetString(mp5, L"sv_exp_mp5_primary_base_spread", document.mp5.primaryBaseSpread);
    SetString(mp5, L"sv_exp_mp5_primary_ground_move_penalty", document.mp5.primaryGroundMovePenalty);
    SetString(mp5, L"sv_exp_mp5_primary_air_move_penalty", document.mp5.primaryAirMovePenalty);
    SetString(mp5, L"sv_exp_mp5_primary_duck_penalty_scale", document.mp5.primaryDuckPenaltyScale);
    SetString(mp5, L"sv_exp_mp5_primary_burst_growth", document.mp5.primaryBurstGrowth);
    SetString(mp5, L"sv_exp_mp5_primary_burst_max_additional_spread", document.mp5.primaryBurstMaxAdditionalSpread);
    SetString(mp5, L"sv_exp_mp5_primary_spread_recovery", document.mp5.primarySpreadRecovery);
    SetBool(mp5, L"sv_exp_mp5_primary_first_shot_accuracy", document.mp5.primaryFirstShotAccuracy);
    SetString(mp5, L"sv_exp_mp5_primary_first_shot_speed_threshold", document.mp5.primaryFirstShotSpeedThreshold);
    SetString(mp5, L"sv_exp_mp5_primary_max_spread", document.mp5.primaryMaxSpread);
    SetBool(mp5, L"sv_exp_mp5_pattern_mode", document.mp5.patternMode);
    SetString(mp5, L"sv_exp_mp5_pattern_scale_x", document.mp5.patternScaleX);
    SetString(mp5, L"sv_exp_mp5_pattern_scale_y", document.mp5.patternScaleY);
    SetString(mp5, L"sv_exp_mp5_pattern_reset_time", document.mp5.patternResetTime);
    SetString(mp5, L"sv_exp_mp5_pattern_max_index", document.mp5.patternMaxIndex);
    SetString(mp5, L"sv_exp_mp5_primary_damage", document.mp5.primaryDamage);
    SetString(mp5, L"sv_exp_mp5_primary_headshot_scale", document.mp5.primaryHeadshotScale);
    SetBool(mp5, L"sv_exp_mp5_primary_headshot_lethal", document.mp5.primaryHeadshotLethal);
    SetBool(mp5, L"sv_exp_mp5_lab_loadout", document.mp5.labLoadout);
    SetString(mp5, L"sv_exp_mp5_lab_ammo", document.mp5.labAmmo);
    SetBool(mp5, L"sv_exp_mp5_lab_autoswitch", document.mp5.labAutoswitch);

    JsonValue::Object& weapon357 = AddObjectMember(rootObject, L"weapon357");
    SetBool(weapon357, L"sv_exp_357_primary_enabled", document.weapon357.primaryEnabled);
    SetString(weapon357, L"sv_exp_357_profile_name", document.weapon357.profileName);
    SetString(weapon357, L"sv_exp_357_primary_base_spread", document.weapon357.primaryBaseSpread);
    SetString(weapon357, L"sv_exp_357_primary_ground_move_penalty", document.weapon357.primaryGroundMovePenalty);
    SetString(weapon357, L"sv_exp_357_primary_air_move_penalty", document.weapon357.primaryAirMovePenalty);
    SetString(weapon357, L"sv_exp_357_primary_duck_penalty_scale", document.weapon357.primaryDuckPenaltyScale);
    SetBool(weapon357, L"sv_exp_357_primary_first_shot_accuracy", document.weapon357.primaryFirstShotAccuracy);
    SetString(weapon357, L"sv_exp_357_primary_first_shot_speed_threshold", document.weapon357.primaryFirstShotSpeedThreshold);
    SetString(weapon357, L"sv_exp_357_primary_spread_recovery", document.weapon357.primarySpreadRecovery);
    SetString(weapon357, L"sv_exp_357_primary_max_spread", document.weapon357.primaryMaxSpread);
    SetBool(weapon357, L"sv_exp_357_primary_cadence_mode", document.weapon357.cadenceMode);
    SetString(weapon357, L"sv_exp_357_primary_cycle_time", document.weapon357.primaryCadenceCycleTime);
    SetString(weapon357, L"sv_exp_357_primary_click_penalty", document.weapon357.primaryClickPenalty);
    SetString(weapon357, L"sv_exp_357_primary_click_penalty_scale", document.weapon357.primaryClickPenaltyScale);
    SetString(weapon357, L"sv_exp_357_primary_click_reset_time", document.weapon357.primaryClickResetTime);
    SetString(weapon357, L"sv_exp_357_primary_hold_penalty_scale", document.weapon357.primaryHoldPenaltyScale);
    SetString(weapon357, L"sv_exp_357_primary_damage", document.weapon357.primaryDamage);
    SetString(weapon357, L"sv_exp_357_primary_headshot_scale", document.weapon357.primaryHeadshotScale);
    SetBool(weapon357, L"sv_exp_357_primary_headshot_lethal", document.weapon357.primaryHeadshotLethal);
    SetBool(weapon357, L"sv_exp_357_lab_loadout", document.weapon357.labLoadout);
    SetString(weapon357, L"sv_exp_357_lab_ammo", document.weapon357.labAmmo);
    SetBool(weapon357, L"sv_exp_357_lab_autoswitch", document.weapon357.labAutoswitch);

    JsonValue::Object& shotgun = AddObjectMember(rootObject, L"shotgun");
    SetBool(shotgun, L"sv_exp_shotgun_primary_enabled", document.shotgun.primaryEnabled);
    SetString(shotgun, L"sv_exp_shotgun_profile_name", document.shotgun.profileName);
    SetString(shotgun, L"sv_exp_shotgun_primary_base_spread", document.shotgun.primaryBaseSpread);
    SetString(shotgun, L"sv_exp_shotgun_primary_ground_move_penalty", document.shotgun.primaryGroundMovePenalty);
    SetString(shotgun, L"sv_exp_shotgun_primary_air_move_penalty", document.shotgun.primaryAirMovePenalty);
    SetString(shotgun, L"sv_exp_shotgun_primary_duck_penalty_scale", document.shotgun.primaryDuckPenaltyScale);
    SetBool(shotgun, L"sv_exp_shotgun_primary_first_shot_accuracy", document.shotgun.primaryFirstShotAccuracy);
    SetString(shotgun, L"sv_exp_shotgun_primary_first_shot_speed_threshold", document.shotgun.primaryFirstShotSpeedThreshold);
    SetString(shotgun, L"sv_exp_shotgun_primary_spread_recovery", document.shotgun.primarySpreadRecovery);
    SetString(shotgun, L"sv_exp_shotgun_primary_max_spread", document.shotgun.primaryMaxSpread);
    SetString(shotgun, L"sv_exp_shotgun_primary_damage_per_pellet", document.shotgun.primaryDamagePerPellet);
    SetString(shotgun, L"sv_exp_shotgun_primary_pellet_count", document.shotgun.primaryPelletCount);
    SetString(shotgun, L"sv_exp_shotgun_primary_headshot_scale", document.shotgun.primaryHeadshotScale);
    SetBool(shotgun, L"sv_exp_shotgun_primary_headshot_lethal", document.shotgun.primaryHeadshotLethal);
    SetBool(shotgun, L"sv_exp_shotgun_lab_loadout", document.shotgun.labLoadout);
    SetString(shotgun, L"sv_exp_shotgun_lab_ammo", document.shotgun.labAmmo);
    SetBool(shotgun, L"sv_exp_shotgun_lab_autoswitch", document.shotgun.labAutoswitch);

    JsonValue::Object& targetDummy = AddObjectMember(rootObject, L"targetDummy");
    SetBool(targetDummy, L"sv_exp_glock_lab_dummy", document.targetDummy.enabled);
    SetString(targetDummy, L"sv_exp_glock_lab_target_profile_name", document.targetDummy.targetProfileName);
    SetString(targetDummy, L"sv_exp_glock_lab_dummy_health", document.targetDummy.dummyHealth);
    SetString(targetDummy, L"sv_exp_glock_lab_dummy_armor", document.targetDummy.dummyArmor);
    SetBool(targetDummy, L"sv_exp_glock_lab_dummy_head_protected", document.targetDummy.dummyHeadProtected);
    SetString(targetDummy, L"sv_exp_glock_lab_dummy_armor_health_fraction", document.targetDummy.armorHealthFraction);
    SetString(targetDummy, L"sv_exp_glock_lab_dummy_armor_drain_scale", document.targetDummy.armorDrainScale);
    SetBool(targetDummy, L"sv_exp_glock_lab_dummy_autorespawn", document.targetDummy.autorespawn);
    SetString(targetDummy, L"sv_exp_glock_lab_dummy_respawn_delay", document.targetDummy.respawnDelay);
    SetString(targetDummy, L"sv_exp_glock_lab_dummy_spawn_distance", document.targetDummy.spawnDistance);
    SetString(targetDummy, L"sv_exp_glock_lab_dummy_offset_right", document.targetDummy.offsetRight);
    SetString(targetDummy, L"sv_exp_glock_lab_dummy_offset_up", document.targetDummy.offsetUp);
    SetBool(targetDummy, L"sv_exp_glock_lab_dummy_face_player", document.targetDummy.facePlayer);
    SetString(targetDummy, L"sv_exp_glock_lab_dummy_model", document.targetDummy.model);

    JsonValue::Object& roundMode = AddObjectMember(rootObject, L"roundMode");
    SetBool(roundMode, L"sv_exp_round_mode", document.roundMode.enabled);
    SetString(roundMode, L"sv_exp_round_freeze_time", document.roundMode.freezeTime);
    SetString(roundMode, L"sv_exp_round_restart_delay", document.roundMode.restartDelay);
    SetString(roundMode, L"sv_exp_round_start_health", document.roundMode.startHealth);
    SetString(roundMode, L"sv_exp_round_start_armor", document.roundMode.startArmor);
    SetBool(roundMode, L"sv_exp_round_no_respawn", document.roundMode.noRespawn);
    SetBool(roundMode, L"sv_exp_round_friendlyfire", document.roundMode.friendlyFire);
    SetString(roundMode, L"sv_exp_round_weapon_profile", document.roundMode.weaponProfile);
    SetString(roundMode, L"sv_exp_round_loadout_mode", document.roundMode.loadoutMode);

    JsonValue::Object& teamRound = AddObjectMember(rootObject, L"teamRound");
    SetBool(teamRound, L"sv_exp_team_round_mode", document.teamRound.enabled);
    SetBool(teamRound, L"sv_exp_team_round_teamplay", document.teamRound.teamplay);
    SetString(teamRound, L"sv_exp_team_round_spawn_mode", document.teamRound.spawnMode);
    SetString(teamRound, L"sv_exp_team_round_team1_name", document.teamRound.team1Name);
    SetString(teamRound, L"sv_exp_team_round_team2_name", document.teamRound.team2Name);
    SetString(teamRound, L"sv_exp_team_round_team1_loadout", document.teamRound.team1Loadout);
    SetString(teamRound, L"sv_exp_team_round_team2_loadout", document.teamRound.team2Loadout);
    SetString(teamRound, L"sv_exp_team_round_team1_health", document.teamRound.team1Health);
    SetString(teamRound, L"sv_exp_team_round_team2_health", document.teamRound.team2Health);
    SetString(teamRound, L"sv_exp_team_round_team1_armor", document.teamRound.team1Armor);
    SetString(teamRound, L"sv_exp_team_round_team2_armor", document.teamRound.team2Armor);

    JsonValue::Object& buy = AddObjectMember(rootObject, L"buy");
    SetBool(buy, L"sv_exp_buy_mode", document.buy.enabled);
    SetBool(buy, L"sv_exp_buy_freeze_only", document.buy.freezeOnly);
    SetBool(buy, L"sv_exp_buy_team_shared_catalog", document.buy.teamSharedCatalog);
    SetString(buy, L"sv_exp_buy_start_money", document.buy.startMoney);
    SetString(buy, L"sv_exp_buy_round_win_reward", document.buy.roundWinReward);
    SetString(buy, L"sv_exp_buy_round_loss_reward", document.buy.roundLossReward);
    SetString(buy, L"sv_exp_buy_max_money", document.buy.maxMoney);
    SetBool(buy, L"sv_exp_buy_allow_glock", document.buy.allowGlock);
    SetBool(buy, L"sv_exp_buy_allow_mp5", document.buy.allowMp5);
    SetBool(buy, L"sv_exp_buy_allow_357", document.buy.allow357);
    SetBool(buy, L"sv_exp_buy_allow_shotgun", document.buy.allowShotgun);
    SetBool(buy, L"sv_exp_buy_allow_armor", document.buy.allowArmor);
    SetBool(buy, L"sv_exp_buy_allow_helmet", document.buy.allowHelmet);
    SetBool(buy, L"sv_exp_buy_allow_handgrenade", document.buy.allowHandgrenade);
    SetString(buy, L"sv_exp_buy_cost_glock", document.buy.costGlock);
    SetString(buy, L"sv_exp_buy_cost_mp5", document.buy.costMp5);
    SetString(buy, L"sv_exp_buy_cost_357", document.buy.cost357);
    SetString(buy, L"sv_exp_buy_cost_shotgun", document.buy.costShotgun);
    SetString(buy, L"sv_exp_buy_cost_armor", document.buy.costArmor);
    SetString(buy, L"sv_exp_buy_cost_helmet", document.buy.costHelmet);
    SetString(buy, L"sv_exp_buy_cost_handgrenade", document.buy.costHandgrenade);

    JsonValue::Object& armorEquipment = AddObjectMember(rootObject, L"armorEquipment");
    SetBool(armorEquipment, L"sv_exp_armor_mode", document.armorEquipment.armorMode);
    SetString(armorEquipment, L"sv_exp_armor_start_value", document.armorEquipment.armorStartValue);
    SetString(armorEquipment, L"sv_exp_armor_max_value", document.armorEquipment.armorMaxValue);
    SetString(armorEquipment, L"sv_exp_armor_health_fraction", document.armorEquipment.armorHealthFraction);
    SetString(armorEquipment, L"sv_exp_armor_drain_scale", document.armorEquipment.armorDrainScale);
    SetBool(armorEquipment, L"sv_exp_helmet_mode", document.armorEquipment.helmetMode);
    SetBool(armorEquipment, L"sv_exp_helmet_start_enabled", document.armorEquipment.helmetStartEnabled);
    SetBool(armorEquipment, L"sv_exp_helmet_headshot_protection", document.armorEquipment.helmetHeadshotProtection);

    return WriteUtf8TextFile(path, SerializeJsonText(root), errorMessage);
}

bool LoadProjectDocumentFromFile(const std::wstring& path, ProjectDocument& document, std::wstring& errorMessage) {
    std::wstring text;
    if (!ReadUtf8TextFile(path, text, errorMessage)) {
        return false;
    }

    JsonValue root;
    if (!ParseJsonText(text, root, errorMessage)) {
        return false;
    }

    if (!root.IsObject()) {
        errorMessage = L"The project file root must be a JSON object.";
        return false;
    }

    ProjectDocument loaded = CreateDefaultProject();
    const JsonValue::Object& rootObject = root.AsObject();
    loaded.schemaVersion = ReadIntValue(rootObject, L"schemaVersion", ProjectDocument::kSchemaVersion);

    if (const JsonValue::Object* metadata = FindObject(rootObject, L"metadata")) {
        loaded.metadata.projectName = ReadStringValue(*metadata, L"projectName", loaded.metadata.projectName);
        loaded.metadata.author = ReadStringValue(*metadata, L"author", loaded.metadata.author);
        loaded.metadata.notes = ReadStringValue(*metadata, L"notes", loaded.metadata.notes);
    }

    if (const JsonValue::Object* exportSettings = FindObject(rootObject, L"export")) {
        loaded.exportSettings.exportFolder = ReadStringValue(*exportSettings, L"folder", loaded.exportSettings.exportFolder);
        loaded.exportSettings.cfgFileName = EnsureCfgFileName(ReadStringValue(*exportSettings, L"fileName", loaded.exportSettings.cfgFileName));
    }

    if (const JsonValue::Object* matchPack = FindObject(rootObject, L"matchPack")) {
        loaded.matchPack.name = ReadStringValue(*matchPack, L"name", loaded.matchPack.name);
        loaded.matchPack.description = ReadStringValue(*matchPack, L"description", loaded.matchPack.description);
        loaded.matchPack.tags = ReadStringValue(*matchPack, L"tags", loaded.matchPack.tags);
    }

    if (const JsonValue::Object* general = FindObject(rootObject, L"general")) {
        loaded.general.weaponUnderTest = ReadStringValue(*general, L"sv_exp_weapon_under_test", loaded.general.weaponUnderTest);
        loaded.general.sessionTag = ReadStringValue(*general, L"sv_exp_session_tag", loaded.general.sessionTag);
        loaded.general.debugWeaponLog = ReadBoolValue(*general, L"sv_exp_debug_weaponlog", loaded.general.debugWeaponLog);
        loaded.general.debugWeaponLogRejections = ReadBoolValue(*general, L"sv_exp_debug_weaponlog_rejections", loaded.general.debugWeaponLogRejections);
    }

    if (const JsonValue::Object* glock = FindObject(rootObject, L"glock")) {
        loaded.glock.tapFire = ReadBoolValue(*glock, L"sv_exp_pistol_tapfire", loaded.glock.tapFire);
        loaded.glock.firstShotAccuracy = ReadBoolValue(*glock, L"sv_exp_first_shot_accuracy", loaded.glock.firstShotAccuracy);
        loaded.glock.spreadRecovery = ReadStringValue(*glock, L"sv_exp_spread_recovery", loaded.glock.spreadRecovery);
        loaded.glock.moveSpreadScale = ReadStringValue(*glock, L"sv_exp_move_spread_scale", loaded.glock.moveSpreadScale);
        loaded.glock.profileName = ReadStringValue(*glock, L"sv_exp_glock_profile_name", loaded.glock.profileName);
        loaded.glock.primaryBaseSpread = ReadStringValue(*glock, L"sv_exp_glock_primary_base_spread", loaded.glock.primaryBaseSpread);
        loaded.glock.primaryGroundMovePenalty = ReadStringValue(*glock, L"sv_exp_glock_primary_ground_move_penalty", loaded.glock.primaryGroundMovePenalty);
        loaded.glock.primaryAirMovePenalty = ReadStringValue(*glock, L"sv_exp_glock_primary_air_move_penalty", loaded.glock.primaryAirMovePenalty);
        loaded.glock.primaryDuckPenaltyScale = ReadStringValue(*glock, L"sv_exp_glock_primary_duck_penalty_scale", loaded.glock.primaryDuckPenaltyScale);
    loaded.glock.primaryShotGrowth = ReadStringValue(*glock, L"sv_exp_glock_primary_shot_growth", loaded.glock.primaryShotGrowth);
    loaded.glock.primaryFirstShotSpeedThreshold = ReadStringValue(*glock, L"sv_exp_glock_primary_first_shot_speed_threshold", loaded.glock.primaryFirstShotSpeedThreshold);
    loaded.glock.primaryMaxSpread = ReadStringValue(*glock, L"sv_exp_glock_primary_max_spread", loaded.glock.primaryMaxSpread);
    loaded.glock.cadenceMode = ReadBoolValue(*glock, L"sv_exp_glock_primary_cadence_mode", loaded.glock.cadenceMode);
    loaded.glock.primaryCadenceCycleTime = ReadStringValue(*glock, L"sv_exp_glock_primary_cycle_time", loaded.glock.primaryCadenceCycleTime);
    loaded.glock.primaryClickPenalty = ReadStringValue(*glock, L"sv_exp_glock_primary_click_penalty", loaded.glock.primaryClickPenalty);
    loaded.glock.primaryClickPenaltyScale = ReadStringValue(*glock, L"sv_exp_glock_primary_click_penalty_scale", loaded.glock.primaryClickPenaltyScale);
    loaded.glock.primaryClickResetTime = ReadStringValue(*glock, L"sv_exp_glock_primary_click_reset_time", loaded.glock.primaryClickResetTime);
    loaded.glock.primaryHoldPenaltyScale = ReadStringValue(*glock, L"sv_exp_glock_primary_hold_penalty_scale", loaded.glock.primaryHoldPenaltyScale);
    loaded.glock.patternMode = ReadBoolValue(*glock, L"sv_exp_glock_pattern_mode", loaded.glock.patternMode);
        loaded.glock.patternScaleX = ReadStringValue(*glock, L"sv_exp_glock_pattern_scale_x", loaded.glock.patternScaleX);
        loaded.glock.patternScaleY = ReadStringValue(*glock, L"sv_exp_glock_pattern_scale_y", loaded.glock.patternScaleY);
        loaded.glock.patternResetTime = ReadStringValue(*glock, L"sv_exp_glock_pattern_reset_time", loaded.glock.patternResetTime);
        loaded.glock.patternMaxIndex = ReadStringValue(*glock, L"sv_exp_glock_pattern_max_index", loaded.glock.patternMaxIndex);
        loaded.glock.primaryDamage = ReadStringValue(*glock, L"sv_exp_glock_primary_damage", loaded.glock.primaryDamage);
        loaded.glock.primaryHeadshotScale = ReadStringValue(*glock, L"sv_exp_glock_primary_headshot_scale", loaded.glock.primaryHeadshotScale);
        loaded.glock.primaryHeadshotLethal = ReadBoolValue(*glock, L"sv_exp_glock_primary_headshot_lethal", loaded.glock.primaryHeadshotLethal);
    }

    if (const JsonValue::Object* mp5 = FindObject(rootObject, L"mp5")) {
        loaded.mp5.primaryEnabled = ReadBoolValue(*mp5, L"sv_exp_mp5_primary_enabled", loaded.mp5.primaryEnabled);
        loaded.mp5.profileName = ReadStringValue(*mp5, L"sv_exp_mp5_profile_name", loaded.mp5.profileName);
        loaded.mp5.primaryBaseSpread = ReadStringValue(*mp5, L"sv_exp_mp5_primary_base_spread", loaded.mp5.primaryBaseSpread);
        loaded.mp5.primaryGroundMovePenalty = ReadStringValue(*mp5, L"sv_exp_mp5_primary_ground_move_penalty", loaded.mp5.primaryGroundMovePenalty);
        loaded.mp5.primaryAirMovePenalty = ReadStringValue(*mp5, L"sv_exp_mp5_primary_air_move_penalty", loaded.mp5.primaryAirMovePenalty);
        loaded.mp5.primaryDuckPenaltyScale = ReadStringValue(*mp5, L"sv_exp_mp5_primary_duck_penalty_scale", loaded.mp5.primaryDuckPenaltyScale);
        loaded.mp5.primaryBurstGrowth = ReadStringValue(*mp5, L"sv_exp_mp5_primary_burst_growth", loaded.mp5.primaryBurstGrowth);
        loaded.mp5.primaryBurstMaxAdditionalSpread = ReadStringValue(*mp5, L"sv_exp_mp5_primary_burst_max_additional_spread", loaded.mp5.primaryBurstMaxAdditionalSpread);
        loaded.mp5.primarySpreadRecovery = ReadStringValue(*mp5, L"sv_exp_mp5_primary_spread_recovery", loaded.mp5.primarySpreadRecovery);
        loaded.mp5.primaryFirstShotAccuracy = ReadBoolValue(*mp5, L"sv_exp_mp5_primary_first_shot_accuracy", loaded.mp5.primaryFirstShotAccuracy);
        loaded.mp5.primaryFirstShotSpeedThreshold = ReadStringValue(*mp5, L"sv_exp_mp5_primary_first_shot_speed_threshold", loaded.mp5.primaryFirstShotSpeedThreshold);
        loaded.mp5.primaryMaxSpread = ReadStringValue(*mp5, L"sv_exp_mp5_primary_max_spread", loaded.mp5.primaryMaxSpread);
        loaded.mp5.patternMode = ReadBoolValue(*mp5, L"sv_exp_mp5_pattern_mode", loaded.mp5.patternMode);
        loaded.mp5.patternScaleX = ReadStringValue(*mp5, L"sv_exp_mp5_pattern_scale_x", loaded.mp5.patternScaleX);
        loaded.mp5.patternScaleY = ReadStringValue(*mp5, L"sv_exp_mp5_pattern_scale_y", loaded.mp5.patternScaleY);
        loaded.mp5.patternResetTime = ReadStringValue(*mp5, L"sv_exp_mp5_pattern_reset_time", loaded.mp5.patternResetTime);
        loaded.mp5.patternMaxIndex = ReadStringValue(*mp5, L"sv_exp_mp5_pattern_max_index", loaded.mp5.patternMaxIndex);
        loaded.mp5.primaryDamage = ReadStringValue(*mp5, L"sv_exp_mp5_primary_damage", loaded.mp5.primaryDamage);
        loaded.mp5.primaryHeadshotScale = ReadStringValue(*mp5, L"sv_exp_mp5_primary_headshot_scale", loaded.mp5.primaryHeadshotScale);
        loaded.mp5.primaryHeadshotLethal = ReadBoolValue(*mp5, L"sv_exp_mp5_primary_headshot_lethal", loaded.mp5.primaryHeadshotLethal);
        loaded.mp5.labLoadout = ReadBoolValue(*mp5, L"sv_exp_mp5_lab_loadout", loaded.mp5.labLoadout);
        loaded.mp5.labAmmo = ReadStringValue(*mp5, L"sv_exp_mp5_lab_ammo", loaded.mp5.labAmmo);
        loaded.mp5.labAutoswitch = ReadBoolValue(*mp5, L"sv_exp_mp5_lab_autoswitch", loaded.mp5.labAutoswitch);
    }

    if (const JsonValue::Object* weapon357 = FindObject(rootObject, L"weapon357")) {
        loaded.weapon357.primaryEnabled = ReadBoolValue(*weapon357, L"sv_exp_357_primary_enabled", loaded.weapon357.primaryEnabled);
        loaded.weapon357.profileName = ReadStringValue(*weapon357, L"sv_exp_357_profile_name", loaded.weapon357.profileName);
        loaded.weapon357.primaryBaseSpread = ReadStringValue(*weapon357, L"sv_exp_357_primary_base_spread", loaded.weapon357.primaryBaseSpread);
        loaded.weapon357.primaryGroundMovePenalty = ReadStringValue(*weapon357, L"sv_exp_357_primary_ground_move_penalty", loaded.weapon357.primaryGroundMovePenalty);
        loaded.weapon357.primaryAirMovePenalty = ReadStringValue(*weapon357, L"sv_exp_357_primary_air_move_penalty", loaded.weapon357.primaryAirMovePenalty);
        loaded.weapon357.primaryDuckPenaltyScale = ReadStringValue(*weapon357, L"sv_exp_357_primary_duck_penalty_scale", loaded.weapon357.primaryDuckPenaltyScale);
    loaded.weapon357.primaryFirstShotAccuracy = ReadBoolValue(*weapon357, L"sv_exp_357_primary_first_shot_accuracy", loaded.weapon357.primaryFirstShotAccuracy);
    loaded.weapon357.primaryFirstShotSpeedThreshold = ReadStringValue(*weapon357, L"sv_exp_357_primary_first_shot_speed_threshold", loaded.weapon357.primaryFirstShotSpeedThreshold);
    loaded.weapon357.primarySpreadRecovery = ReadStringValue(*weapon357, L"sv_exp_357_primary_spread_recovery", loaded.weapon357.primarySpreadRecovery);
    loaded.weapon357.primaryMaxSpread = ReadStringValue(*weapon357, L"sv_exp_357_primary_max_spread", loaded.weapon357.primaryMaxSpread);
    loaded.weapon357.cadenceMode = ReadBoolValue(*weapon357, L"sv_exp_357_primary_cadence_mode", loaded.weapon357.cadenceMode);
    loaded.weapon357.primaryCadenceCycleTime = ReadStringValue(*weapon357, L"sv_exp_357_primary_cycle_time", loaded.weapon357.primaryCadenceCycleTime);
    loaded.weapon357.primaryClickPenalty = ReadStringValue(*weapon357, L"sv_exp_357_primary_click_penalty", loaded.weapon357.primaryClickPenalty);
    loaded.weapon357.primaryClickPenaltyScale = ReadStringValue(*weapon357, L"sv_exp_357_primary_click_penalty_scale", loaded.weapon357.primaryClickPenaltyScale);
    loaded.weapon357.primaryClickResetTime = ReadStringValue(*weapon357, L"sv_exp_357_primary_click_reset_time", loaded.weapon357.primaryClickResetTime);
    loaded.weapon357.primaryHoldPenaltyScale = ReadStringValue(*weapon357, L"sv_exp_357_primary_hold_penalty_scale", loaded.weapon357.primaryHoldPenaltyScale);
    loaded.weapon357.primaryDamage = ReadStringValue(*weapon357, L"sv_exp_357_primary_damage", loaded.weapon357.primaryDamage);
        loaded.weapon357.primaryHeadshotScale = ReadStringValue(*weapon357, L"sv_exp_357_primary_headshot_scale", loaded.weapon357.primaryHeadshotScale);
        loaded.weapon357.primaryHeadshotLethal = ReadBoolValue(*weapon357, L"sv_exp_357_primary_headshot_lethal", loaded.weapon357.primaryHeadshotLethal);
        loaded.weapon357.labLoadout = ReadBoolValue(*weapon357, L"sv_exp_357_lab_loadout", loaded.weapon357.labLoadout);
        loaded.weapon357.labAmmo = ReadStringValue(*weapon357, L"sv_exp_357_lab_ammo", loaded.weapon357.labAmmo);
        loaded.weapon357.labAutoswitch = ReadBoolValue(*weapon357, L"sv_exp_357_lab_autoswitch", loaded.weapon357.labAutoswitch);
    }

    if (const JsonValue::Object* shotgun = FindObject(rootObject, L"shotgun")) {
        loaded.shotgun.primaryEnabled = ReadBoolValue(*shotgun, L"sv_exp_shotgun_primary_enabled", loaded.shotgun.primaryEnabled);
        loaded.shotgun.profileName = ReadStringValue(*shotgun, L"sv_exp_shotgun_profile_name", loaded.shotgun.profileName);
        loaded.shotgun.primaryBaseSpread = ReadStringValue(*shotgun, L"sv_exp_shotgun_primary_base_spread", loaded.shotgun.primaryBaseSpread);
        loaded.shotgun.primaryGroundMovePenalty = ReadStringValue(*shotgun, L"sv_exp_shotgun_primary_ground_move_penalty", loaded.shotgun.primaryGroundMovePenalty);
        loaded.shotgun.primaryAirMovePenalty = ReadStringValue(*shotgun, L"sv_exp_shotgun_primary_air_move_penalty", loaded.shotgun.primaryAirMovePenalty);
        loaded.shotgun.primaryDuckPenaltyScale = ReadStringValue(*shotgun, L"sv_exp_shotgun_primary_duck_penalty_scale", loaded.shotgun.primaryDuckPenaltyScale);
        loaded.shotgun.primaryFirstShotAccuracy = ReadBoolValue(*shotgun, L"sv_exp_shotgun_primary_first_shot_accuracy", loaded.shotgun.primaryFirstShotAccuracy);
        loaded.shotgun.primaryFirstShotSpeedThreshold = ReadStringValue(*shotgun, L"sv_exp_shotgun_primary_first_shot_speed_threshold", loaded.shotgun.primaryFirstShotSpeedThreshold);
        loaded.shotgun.primarySpreadRecovery = ReadStringValue(*shotgun, L"sv_exp_shotgun_primary_spread_recovery", loaded.shotgun.primarySpreadRecovery);
        loaded.shotgun.primaryMaxSpread = ReadStringValue(*shotgun, L"sv_exp_shotgun_primary_max_spread", loaded.shotgun.primaryMaxSpread);
        loaded.shotgun.primaryDamagePerPellet = ReadStringValue(*shotgun, L"sv_exp_shotgun_primary_damage_per_pellet", loaded.shotgun.primaryDamagePerPellet);
        loaded.shotgun.primaryPelletCount = ReadStringValue(*shotgun, L"sv_exp_shotgun_primary_pellet_count", loaded.shotgun.primaryPelletCount);
        loaded.shotgun.primaryHeadshotScale = ReadStringValue(*shotgun, L"sv_exp_shotgun_primary_headshot_scale", loaded.shotgun.primaryHeadshotScale);
        loaded.shotgun.primaryHeadshotLethal = ReadBoolValue(*shotgun, L"sv_exp_shotgun_primary_headshot_lethal", loaded.shotgun.primaryHeadshotLethal);
        loaded.shotgun.labLoadout = ReadBoolValue(*shotgun, L"sv_exp_shotgun_lab_loadout", loaded.shotgun.labLoadout);
        loaded.shotgun.labAmmo = ReadStringValue(*shotgun, L"sv_exp_shotgun_lab_ammo", loaded.shotgun.labAmmo);
        loaded.shotgun.labAutoswitch = ReadBoolValue(*shotgun, L"sv_exp_shotgun_lab_autoswitch", loaded.shotgun.labAutoswitch);
    }

    if (const JsonValue::Object* targetDummy = FindObject(rootObject, L"targetDummy")) {
        loaded.targetDummy.enabled = ReadBoolValue(*targetDummy, L"sv_exp_glock_lab_dummy", loaded.targetDummy.enabled);
        loaded.targetDummy.targetProfileName = ReadStringValue(*targetDummy, L"sv_exp_glock_lab_target_profile_name", loaded.targetDummy.targetProfileName);
        loaded.targetDummy.dummyHealth = ReadStringValue(*targetDummy, L"sv_exp_glock_lab_dummy_health", loaded.targetDummy.dummyHealth);
        loaded.targetDummy.dummyArmor = ReadStringValue(*targetDummy, L"sv_exp_glock_lab_dummy_armor", loaded.targetDummy.dummyArmor);
        loaded.targetDummy.dummyHeadProtected = ReadBoolValue(*targetDummy, L"sv_exp_glock_lab_dummy_head_protected", loaded.targetDummy.dummyHeadProtected);
        loaded.targetDummy.armorHealthFraction = ReadStringValue(*targetDummy, L"sv_exp_glock_lab_dummy_armor_health_fraction", loaded.targetDummy.armorHealthFraction);
        loaded.targetDummy.armorDrainScale = ReadStringValue(*targetDummy, L"sv_exp_glock_lab_dummy_armor_drain_scale", loaded.targetDummy.armorDrainScale);
        loaded.targetDummy.autorespawn = ReadBoolValue(*targetDummy, L"sv_exp_glock_lab_dummy_autorespawn", loaded.targetDummy.autorespawn);
        loaded.targetDummy.respawnDelay = ReadStringValue(*targetDummy, L"sv_exp_glock_lab_dummy_respawn_delay", loaded.targetDummy.respawnDelay);
        loaded.targetDummy.spawnDistance = ReadStringValue(*targetDummy, L"sv_exp_glock_lab_dummy_spawn_distance", loaded.targetDummy.spawnDistance);
        loaded.targetDummy.offsetRight = ReadStringValue(*targetDummy, L"sv_exp_glock_lab_dummy_offset_right", loaded.targetDummy.offsetRight);
        loaded.targetDummy.offsetUp = ReadStringValue(*targetDummy, L"sv_exp_glock_lab_dummy_offset_up", loaded.targetDummy.offsetUp);
        loaded.targetDummy.facePlayer = ReadBoolValue(*targetDummy, L"sv_exp_glock_lab_dummy_face_player", loaded.targetDummy.facePlayer);
        loaded.targetDummy.model = ReadStringValue(*targetDummy, L"sv_exp_glock_lab_dummy_model", loaded.targetDummy.model);
    }

    if (const JsonValue::Object* roundMode = FindObject(rootObject, L"roundMode")) {
        loaded.roundMode.enabled = ReadBoolValue(*roundMode, L"sv_exp_round_mode", loaded.roundMode.enabled);
        loaded.roundMode.freezeTime = ReadStringValue(*roundMode, L"sv_exp_round_freeze_time", loaded.roundMode.freezeTime);
        loaded.roundMode.restartDelay = ReadStringValue(*roundMode, L"sv_exp_round_restart_delay", loaded.roundMode.restartDelay);
        loaded.roundMode.startHealth = ReadStringValue(*roundMode, L"sv_exp_round_start_health", loaded.roundMode.startHealth);
        loaded.roundMode.startArmor = ReadStringValue(*roundMode, L"sv_exp_round_start_armor", loaded.roundMode.startArmor);
        loaded.roundMode.noRespawn = ReadBoolValue(*roundMode, L"sv_exp_round_no_respawn", loaded.roundMode.noRespawn);
        loaded.roundMode.friendlyFire = ReadBoolValue(*roundMode, L"sv_exp_round_friendlyfire", loaded.roundMode.friendlyFire);
        loaded.roundMode.weaponProfile = ReadStringValue(*roundMode, L"sv_exp_round_weapon_profile", loaded.roundMode.weaponProfile);
        loaded.roundMode.loadoutMode = ReadStringValue(*roundMode, L"sv_exp_round_loadout_mode", loaded.roundMode.loadoutMode);
    }

    if (const JsonValue::Object* teamRound = FindObject(rootObject, L"teamRound")) {
        loaded.teamRound.enabled = ReadBoolValue(*teamRound, L"sv_exp_team_round_mode", loaded.teamRound.enabled);
        loaded.teamRound.teamplay = ReadBoolValue(*teamRound, L"sv_exp_team_round_teamplay", loaded.teamRound.teamplay);
        loaded.teamRound.spawnMode = ReadStringValue(*teamRound, L"sv_exp_team_round_spawn_mode", loaded.teamRound.spawnMode);
        loaded.teamRound.team1Name = ReadStringValue(*teamRound, L"sv_exp_team_round_team1_name", loaded.teamRound.team1Name);
        loaded.teamRound.team2Name = ReadStringValue(*teamRound, L"sv_exp_team_round_team2_name", loaded.teamRound.team2Name);
        loaded.teamRound.team1Loadout = ReadStringValue(*teamRound, L"sv_exp_team_round_team1_loadout", loaded.teamRound.team1Loadout);
        loaded.teamRound.team2Loadout = ReadStringValue(*teamRound, L"sv_exp_team_round_team2_loadout", loaded.teamRound.team2Loadout);
        loaded.teamRound.team1Health = ReadStringValue(*teamRound, L"sv_exp_team_round_team1_health", loaded.teamRound.team1Health);
        loaded.teamRound.team2Health = ReadStringValue(*teamRound, L"sv_exp_team_round_team2_health", loaded.teamRound.team2Health);
        loaded.teamRound.team1Armor = ReadStringValue(*teamRound, L"sv_exp_team_round_team1_armor", loaded.teamRound.team1Armor);
        loaded.teamRound.team2Armor = ReadStringValue(*teamRound, L"sv_exp_team_round_team2_armor", loaded.teamRound.team2Armor);
    }

    if (const JsonValue::Object* buy = FindObject(rootObject, L"buy")) {
        loaded.buy.enabled = ReadBoolValue(*buy, L"sv_exp_buy_mode", loaded.buy.enabled);
        loaded.buy.freezeOnly = ReadBoolValue(*buy, L"sv_exp_buy_freeze_only", loaded.buy.freezeOnly);
        loaded.buy.teamSharedCatalog = ReadBoolValue(*buy, L"sv_exp_buy_team_shared_catalog", loaded.buy.teamSharedCatalog);
        loaded.buy.startMoney = ReadStringValue(*buy, L"sv_exp_buy_start_money", loaded.buy.startMoney);
        loaded.buy.roundWinReward = ReadStringValue(*buy, L"sv_exp_buy_round_win_reward", loaded.buy.roundWinReward);
        loaded.buy.roundLossReward = ReadStringValue(*buy, L"sv_exp_buy_round_loss_reward", loaded.buy.roundLossReward);
        loaded.buy.maxMoney = ReadStringValue(*buy, L"sv_exp_buy_max_money", loaded.buy.maxMoney);
        loaded.buy.allowGlock = ReadBoolValue(*buy, L"sv_exp_buy_allow_glock", loaded.buy.allowGlock);
        loaded.buy.allowMp5 = ReadBoolValue(*buy, L"sv_exp_buy_allow_mp5", loaded.buy.allowMp5);
        loaded.buy.allow357 = ReadBoolValue(*buy, L"sv_exp_buy_allow_357", loaded.buy.allow357);
        loaded.buy.allowShotgun = ReadBoolValue(*buy, L"sv_exp_buy_allow_shotgun", loaded.buy.allowShotgun);
        loaded.buy.allowArmor = ReadBoolValue(*buy, L"sv_exp_buy_allow_armor", loaded.buy.allowArmor);
        loaded.buy.allowHelmet = ReadBoolValue(*buy, L"sv_exp_buy_allow_helmet", loaded.buy.allowHelmet);
        loaded.buy.allowHandgrenade = ReadBoolValue(*buy, L"sv_exp_buy_allow_handgrenade", loaded.buy.allowHandgrenade);
        loaded.buy.costGlock = ReadStringValue(*buy, L"sv_exp_buy_cost_glock", loaded.buy.costGlock);
        loaded.buy.costMp5 = ReadStringValue(*buy, L"sv_exp_buy_cost_mp5", loaded.buy.costMp5);
        loaded.buy.cost357 = ReadStringValue(*buy, L"sv_exp_buy_cost_357", loaded.buy.cost357);
        loaded.buy.costShotgun = ReadStringValue(*buy, L"sv_exp_buy_cost_shotgun", loaded.buy.costShotgun);
        loaded.buy.costArmor = ReadStringValue(*buy, L"sv_exp_buy_cost_armor", loaded.buy.costArmor);
        loaded.buy.costHelmet = ReadStringValue(*buy, L"sv_exp_buy_cost_helmet", loaded.buy.costHelmet);
        loaded.buy.costHandgrenade = ReadStringValue(*buy, L"sv_exp_buy_cost_handgrenade", loaded.buy.costHandgrenade);
    }

    if (const JsonValue::Object* armorEquipment = FindObject(rootObject, L"armorEquipment")) {
        loaded.armorEquipment.armorMode = ReadBoolValue(*armorEquipment, L"sv_exp_armor_mode", loaded.armorEquipment.armorMode);
        loaded.armorEquipment.armorStartValue = ReadStringValue(*armorEquipment, L"sv_exp_armor_start_value", loaded.armorEquipment.armorStartValue);
        loaded.armorEquipment.armorMaxValue = ReadStringValue(*armorEquipment, L"sv_exp_armor_max_value", loaded.armorEquipment.armorMaxValue);
        loaded.armorEquipment.armorHealthFraction = ReadStringValue(*armorEquipment, L"sv_exp_armor_health_fraction", loaded.armorEquipment.armorHealthFraction);
        loaded.armorEquipment.armorDrainScale = ReadStringValue(*armorEquipment, L"sv_exp_armor_drain_scale", loaded.armorEquipment.armorDrainScale);
        loaded.armorEquipment.helmetMode = ReadBoolValue(*armorEquipment, L"sv_exp_helmet_mode", loaded.armorEquipment.helmetMode);
        loaded.armorEquipment.helmetStartEnabled = ReadBoolValue(*armorEquipment, L"sv_exp_helmet_start_enabled", loaded.armorEquipment.helmetStartEnabled);
        loaded.armorEquipment.helmetHeadshotProtection =
            ReadBoolValue(*armorEquipment, L"sv_exp_helmet_headshot_protection", loaded.armorEquipment.helmetHeadshotProtection);
    }

    document = std::move(loaded);
    return true;
}

}  // namespace hlcfg
