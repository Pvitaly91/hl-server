#pragma once

#include <map>
#include <string>
#include <vector>

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

struct MatchPackConfig {
    std::wstring name;
    std::wstring description;
    std::wstring tags;
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
    std::wstring primaryShotGrowth;
    std::wstring primaryFirstShotSpeedThreshold;
    std::wstring primaryMaxSpread;
    bool cadenceMode = false;
    std::wstring primaryCadenceCycleTime;
    std::wstring primaryClickPenalty;
    std::wstring primaryClickPenaltyScale;
    std::wstring primaryClickResetTime;
    std::wstring primaryHoldPenaltyScale;
    bool patternMode = false;
    std::wstring patternScaleX;
    std::wstring patternScaleY;
    std::wstring patternResetTime;
    std::wstring patternMaxIndex;
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
    bool patternMode = false;
    std::wstring patternScaleX;
    std::wstring patternScaleY;
    std::wstring patternResetTime;
    std::wstring patternMaxIndex;
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
    bool cadenceMode = false;
    std::wstring primaryCadenceCycleTime;
    std::wstring primaryClickPenalty;
    std::wstring primaryClickPenaltyScale;
    std::wstring primaryClickResetTime;
    std::wstring primaryHoldPenaltyScale;
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
    std::wstring primaryShotGrowth;
    bool patternMode = false;
    std::wstring patternScaleX;
    std::wstring patternScaleY;
    std::wstring patternResetTime;
    std::wstring patternMaxIndex;
    std::wstring primaryPelletSpreadMode;
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

struct RoundModeConfig {
    bool enabled = false;
    std::wstring freezeTime;
    std::wstring restartDelay;
    std::wstring startHealth;
    std::wstring startArmor;
    bool noRespawn = true;
    bool friendlyFire = false;
    std::wstring weaponProfile;
    std::wstring loadoutMode;
};

struct TeamRoundConfig {
    bool enabled = false;
    bool teamplay = true;
    std::wstring spawnMode;
    std::wstring team1Name;
    std::wstring team2Name;
    std::wstring team1Loadout;
    std::wstring team2Loadout;
    std::wstring team1Health;
    std::wstring team2Health;
    std::wstring team1Armor;
    std::wstring team2Armor;
};

struct BuyConfig {
    bool enabled = false;
    bool freezeOnly = true;
    bool teamSharedCatalog = true;
    std::wstring startMoney;
    std::wstring roundWinReward;
    std::wstring roundLossReward;
    std::wstring maxMoney;
    bool allowGlock = true;
    bool allowMp5 = true;
    bool allow357 = true;
    bool allowShotgun = true;
    bool allowArmor = true;
    bool allowHelmet = true;
    bool allowHandgrenade = true;
    std::wstring costGlock;
    std::wstring costMp5;
    std::wstring cost357;
    std::wstring costShotgun;
    std::wstring costArmor;
    std::wstring costHelmet;
    std::wstring costHandgrenade;
};

struct ArmorEquipmentConfig {
    bool armorMode = false;
    std::wstring armorStartValue;
    std::wstring armorMaxValue;
    std::wstring armorHealthFraction;
    std::wstring armorDrainScale;
    bool helmetMode = false;
    bool helmetStartEnabled = false;
    bool helmetHeadshotProtection = true;
};

struct ProjectDocument {
    static constexpr int kSchemaVersion = 7;

    int schemaVersion = kSchemaVersion;
    ProjectMetadata metadata;
    ExportSettings exportSettings;
    MatchPackConfig matchPack;
    GeneralConfig general;
    GlockConfig glock;
    Mp5Config mp5;
    Weapon357Config weapon357;
    ShotgunConfig shotgun;
    TargetDummyConfig targetDummy;
    RoundModeConfig roundMode;
    TeamRoundConfig teamRound;
    BuyConfig buy;
    ArmorEquipmentConfig armorEquipment;
};

using CvarMap = std::map<std::wstring, std::wstring>;

ProjectDocument CreateDefaultProject();

std::wstring Trimmed(const std::wstring& value);
std::wstring EnsureProjectFileName(const std::wstring& value);
std::wstring EnsureCfgFileName(const std::wstring& value);

CvarMap BuildKnownCvarMap(const ProjectDocument& document);
void MergeKnownCvarMap(ProjectDocument& document, const CvarMap& cvars, std::vector<std::wstring>* unknownCvars = nullptr);

bool SaveProjectDocumentToFile(const ProjectDocument& document, const std::wstring& path, std::wstring& errorMessage);
bool LoadProjectDocumentFromFile(const std::wstring& path, ProjectDocument& document, std::wstring& errorMessage);

}  // namespace hlcfg
