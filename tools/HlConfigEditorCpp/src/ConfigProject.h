#pragma once

#include <string>

namespace hlcfg {

struct ProjectMetadata {
    std::wstring projectName;
    std::wstring author;
    std::wstring notes;
};

struct ExportSettings {
    std::wstring exportFolder;
    std::wstring cfgFileName;
};

struct GeneralConfig {
    std::wstring weaponUnderTest;
    std::wstring sessionTag;
    bool debugWeaponLog = false;
    bool debugWeaponLogRejections = false;
};

struct GlockConfig {
    bool tapFire = false;
    bool firstShotAccuracy = false;
    std::wstring spreadRecovery;
    std::wstring moveSpreadScale;
    std::wstring profileName;
    std::wstring primaryBaseSpread;
    std::wstring primaryGroundMovePenalty;
    std::wstring primaryAirMovePenalty;
    std::wstring primaryDuckPenaltyScale;
    std::wstring primaryFirstShotSpeedThreshold;
    std::wstring primaryMaxSpread;
    std::wstring primaryDamage;
    std::wstring primaryHeadshotScale;
    bool primaryHeadshotLethal = false;
};

struct Mp5Config {
    bool primaryEnabled = false;
    std::wstring profileName;
    std::wstring primaryBaseSpread;
    std::wstring primaryGroundMovePenalty;
    std::wstring primaryAirMovePenalty;
    std::wstring primaryDuckPenaltyScale;
    std::wstring primaryBurstGrowth;
    std::wstring primaryBurstMaxAdditionalSpread;
    std::wstring primarySpreadRecovery;
    bool primaryFirstShotAccuracy = false;
    std::wstring primaryFirstShotSpeedThreshold;
    std::wstring primaryMaxSpread;
    std::wstring primaryDamage;
    std::wstring primaryHeadshotScale;
    bool primaryHeadshotLethal = false;
    bool labLoadout = false;
    std::wstring labAmmo;
    bool labAutoswitch = true;
};

struct Weapon357Config {
    bool primaryEnabled = false;
    std::wstring profileName;
    std::wstring primaryBaseSpread;
    std::wstring primaryGroundMovePenalty;
    std::wstring primaryAirMovePenalty;
    std::wstring primaryDuckPenaltyScale;
    bool primaryFirstShotAccuracy = false;
    std::wstring primaryFirstShotSpeedThreshold;
    std::wstring primarySpreadRecovery;
    std::wstring primaryMaxSpread;
    std::wstring primaryDamage;
    std::wstring primaryHeadshotScale;
    bool primaryHeadshotLethal = false;
    bool labLoadout = false;
    std::wstring labAmmo;
    bool labAutoswitch = true;
};

struct ShotgunConfig {
    bool primaryEnabled = false;
    std::wstring profileName;
    std::wstring primaryBaseSpread;
    std::wstring primaryGroundMovePenalty;
    std::wstring primaryAirMovePenalty;
    std::wstring primaryDuckPenaltyScale;
    bool primaryFirstShotAccuracy = false;
    std::wstring primaryFirstShotSpeedThreshold;
    std::wstring primarySpreadRecovery;
    std::wstring primaryMaxSpread;
    std::wstring primaryDamagePerPellet;
    std::wstring primaryPelletCount;
    std::wstring primaryHeadshotScale;
    bool primaryHeadshotLethal = false;
    bool labLoadout = false;
    std::wstring labAmmo;
    bool labAutoswitch = true;
};

struct TargetDummyConfig {
    bool enabled = false;
    std::wstring targetProfileName;
    std::wstring dummyHealth;
    std::wstring dummyArmor;
    bool dummyHeadProtected = false;
    std::wstring armorHealthFraction;
    std::wstring armorDrainScale;
    bool autorespawn = true;
    std::wstring respawnDelay;
    std::wstring spawnDistance;
    std::wstring offsetRight;
    std::wstring offsetUp;
    bool facePlayer = true;
    std::wstring model;
};

struct ProjectDocument {
    static constexpr int kSchemaVersion = 1;

    int schemaVersion = kSchemaVersion;
    ProjectMetadata metadata;
    ExportSettings exportSettings;
    GeneralConfig general;
    GlockConfig glock;
    Mp5Config mp5;
    Weapon357Config weapon357;
    ShotgunConfig shotgun;
    TargetDummyConfig targetDummy;
};

ProjectDocument CreateDefaultProject();

std::wstring Trimmed(const std::wstring& value);
std::wstring EnsureProjectFileName(const std::wstring& value);
std::wstring EnsureCfgFileName(const std::wstring& value);

bool SaveProjectDocumentToFile(const ProjectDocument& document, const std::wstring& path, std::wstring& errorMessage);
bool LoadProjectDocumentFromFile(const std::wstring& path, ProjectDocument& document, std::wstring& errorMessage);

}  // namespace hlcfg
