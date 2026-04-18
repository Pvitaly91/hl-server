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

    document.glock.spreadRecovery = L"0.0";
    document.glock.moveSpreadScale = L"0.0";
    document.glock.profileName = L"default";
    document.glock.primaryBaseSpread = L"0.01";
    document.glock.primaryGroundMovePenalty = L"0.08";
    document.glock.primaryAirMovePenalty = L"0.12";
    document.glock.primaryDuckPenaltyScale = L"0.75";
    document.glock.primaryFirstShotSpeedThreshold = L"40.0";
    document.glock.primaryMaxSpread = L"0.2";
    document.glock.primaryDamage = L"8.0";
    document.glock.primaryHeadshotScale = L"3.0";

    document.mp5.profileName = L"default";
    document.mp5.primaryBaseSpread = L"0.0523";
    document.mp5.primaryGroundMovePenalty = L"0.0200";
    document.mp5.primaryAirMovePenalty = L"0.0400";
    document.mp5.primaryDuckPenaltyScale = L"0.7000";
    document.mp5.primaryBurstGrowth = L"0.0060";
    document.mp5.primaryBurstMaxAdditionalSpread = L"0.0600";
    document.mp5.primarySpreadRecovery = L"0.3000";
    document.mp5.primaryFirstShotSpeedThreshold = L"30.0";
    document.mp5.primaryMaxSpread = L"0.1200";
    document.mp5.primaryDamage = L"12.0";
    document.mp5.primaryHeadshotScale = L"3.0";
    document.mp5.labAmmo = L"250";

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
    SetString(glock, L"sv_exp_glock_primary_first_shot_speed_threshold", document.glock.primaryFirstShotSpeedThreshold);
    SetString(glock, L"sv_exp_glock_primary_max_spread", document.glock.primaryMaxSpread);
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
    SetString(mp5, L"sv_exp_mp5_primary_damage", document.mp5.primaryDamage);
    SetString(mp5, L"sv_exp_mp5_primary_headshot_scale", document.mp5.primaryHeadshotScale);
    SetBool(mp5, L"sv_exp_mp5_primary_headshot_lethal", document.mp5.primaryHeadshotLethal);
    SetBool(mp5, L"sv_exp_mp5_lab_loadout", document.mp5.labLoadout);
    SetString(mp5, L"sv_exp_mp5_lab_ammo", document.mp5.labAmmo);
    SetBool(mp5, L"sv_exp_mp5_lab_autoswitch", document.mp5.labAutoswitch);

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
        loaded.glock.primaryFirstShotSpeedThreshold = ReadStringValue(*glock, L"sv_exp_glock_primary_first_shot_speed_threshold", loaded.glock.primaryFirstShotSpeedThreshold);
        loaded.glock.primaryMaxSpread = ReadStringValue(*glock, L"sv_exp_glock_primary_max_spread", loaded.glock.primaryMaxSpread);
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
        loaded.mp5.primaryDamage = ReadStringValue(*mp5, L"sv_exp_mp5_primary_damage", loaded.mp5.primaryDamage);
        loaded.mp5.primaryHeadshotScale = ReadStringValue(*mp5, L"sv_exp_mp5_primary_headshot_scale", loaded.mp5.primaryHeadshotScale);
        loaded.mp5.primaryHeadshotLethal = ReadBoolValue(*mp5, L"sv_exp_mp5_primary_headshot_lethal", loaded.mp5.primaryHeadshotLethal);
        loaded.mp5.labLoadout = ReadBoolValue(*mp5, L"sv_exp_mp5_lab_loadout", loaded.mp5.labLoadout);
        loaded.mp5.labAmmo = ReadStringValue(*mp5, L"sv_exp_mp5_lab_ammo", loaded.mp5.labAmmo);
        loaded.mp5.labAutoswitch = ReadBoolValue(*mp5, L"sv_exp_mp5_lab_autoswitch", loaded.mp5.labAutoswitch);
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

    document = std::move(loaded);
    return true;
}

}  // namespace hlcfg
