#include "Presets.h"

namespace hlcfg {

namespace {

void ApplyGlockDefaults(ProjectDocument& document) {
    document.general.weaponUnderTest = L"glock";
    document.glock.tapFire = false;
    document.glock.firstShotAccuracy = false;
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
    document.glock.primaryHeadshotLethal = false;
}

void ApplyMp5Defaults(ProjectDocument& document) {
    document.general.weaponUnderTest = L"mp5";
    document.mp5.primaryEnabled = false;
    document.mp5.profileName = L"default";
    document.mp5.primaryBaseSpread = L"0.0523";
    document.mp5.primaryGroundMovePenalty = L"0.0200";
    document.mp5.primaryAirMovePenalty = L"0.0400";
    document.mp5.primaryDuckPenaltyScale = L"0.7000";
    document.mp5.primaryBurstGrowth = L"0.0060";
    document.mp5.primaryBurstMaxAdditionalSpread = L"0.0600";
    document.mp5.primarySpreadRecovery = L"0.3000";
    document.mp5.primaryFirstShotAccuracy = false;
    document.mp5.primaryFirstShotSpeedThreshold = L"30.0";
    document.mp5.primaryMaxSpread = L"0.1200";
    document.mp5.primaryDamage = L"12.0";
    document.mp5.primaryHeadshotScale = L"3.0";
    document.mp5.primaryHeadshotLethal = false;
    document.mp5.labLoadout = false;
    document.mp5.labAmmo = L"250";
    document.mp5.labAutoswitch = true;
}

void Apply357Defaults(ProjectDocument& document) {
    document.general.weaponUnderTest = L"357";
    document.weapon357.primaryEnabled = true;
    document.weapon357.profileName = L"default";
    document.weapon357.primaryBaseSpread = L"0.0087";
    document.weapon357.primaryGroundMovePenalty = L"0.0200";
    document.weapon357.primaryAirMovePenalty = L"0.0800";
    document.weapon357.primaryDuckPenaltyScale = L"0.7000";
    document.weapon357.primaryFirstShotAccuracy = false;
    document.weapon357.primaryFirstShotSpeedThreshold = L"25.0";
    document.weapon357.primarySpreadRecovery = L"0.7500";
    document.weapon357.primaryMaxSpread = L"0.1200";
    document.weapon357.primaryDamage = L"40.0";
    document.weapon357.primaryHeadshotScale = L"2.5";
    document.weapon357.primaryHeadshotLethal = false;
    document.weapon357.labLoadout = true;
    document.weapon357.labAmmo = L"36";
    document.weapon357.labAutoswitch = true;
}

void ApplyShotgunDefaults(ProjectDocument& document) {
    document.general.weaponUnderTest = L"shotgun";
    document.shotgun.primaryEnabled = true;
    document.shotgun.profileName = L"default";
    document.shotgun.primaryBaseSpread = L"0.0600";
    document.shotgun.primaryGroundMovePenalty = L"0.0300";
    document.shotgun.primaryAirMovePenalty = L"0.0800";
    document.shotgun.primaryDuckPenaltyScale = L"0.8000";
    document.shotgun.primaryFirstShotAccuracy = false;
    document.shotgun.primaryFirstShotSpeedThreshold = L"35.0";
    document.shotgun.primarySpreadRecovery = L"0.8500";
    document.shotgun.primaryMaxSpread = L"0.1200";
    document.shotgun.primaryDamagePerPellet = L"5.0";
    document.shotgun.primaryPelletCount = L"6";
    document.shotgun.primaryHeadshotScale = L"1.5";
    document.shotgun.primaryHeadshotLethal = false;
    document.shotgun.labLoadout = true;
    document.shotgun.labAmmo = L"48";
    document.shotgun.labAutoswitch = true;
}

void ApplyDummyDefaults(ProjectDocument& document) {
    document.targetDummy.enabled = true;
    document.targetDummy.targetProfileName = L"unarmored";
    document.targetDummy.dummyHealth = L"100.0";
    document.targetDummy.dummyArmor = L"0";
    document.targetDummy.dummyHeadProtected = false;
    document.targetDummy.armorHealthFraction = L"0.5";
    document.targetDummy.armorDrainScale = L"1.0";
    document.targetDummy.autorespawn = true;
    document.targetDummy.respawnDelay = L"1.0";
    document.targetDummy.spawnDistance = L"256.0";
    document.targetDummy.offsetRight = L"0.0";
    document.targetDummy.offsetUp = L"0.0";
    document.targetDummy.facePlayer = true;
    document.targetDummy.model = L"models/barney.mdl";
}

void ApplyRoundDefaults(ProjectDocument& document) {
    document.roundMode.enabled = false;
    document.roundMode.freezeTime = L"3.0";
    document.roundMode.restartDelay = L"3.0";
    document.roundMode.startHealth = L"100.0";
    document.roundMode.startArmor = L"0.0";
    document.roundMode.noRespawn = true;
    document.roundMode.friendlyFire = false;
    document.roundMode.weaponProfile.clear();
    document.roundMode.loadoutMode = L"none";
}

void ApplyTeamRoundDefaults(ProjectDocument& document) {
    document.teamRound.enabled = false;
    document.teamRound.teamplay = true;
    document.teamRound.spawnMode = L"dm_spawns";
    document.teamRound.team1Name = L"alpha";
    document.teamRound.team2Name = L"bravo";
    document.teamRound.team1Loadout.clear();
    document.teamRound.team2Loadout.clear();
    document.teamRound.team1Health = L"-1.0";
    document.teamRound.team2Health = L"-1.0";
    document.teamRound.team1Armor = L"-1.0";
    document.teamRound.team2Armor = L"-1.0";
}

void ApplyBuyDefaults(ProjectDocument& document) {
    document.buy.enabled = false;
    document.buy.freezeOnly = true;
    document.buy.teamSharedCatalog = true;
    document.buy.startMoney = L"2000";
    document.buy.roundWinReward = L"1000";
    document.buy.roundLossReward = L"500";
    document.buy.maxMoney = L"16000";
    document.buy.allowGlock = true;
    document.buy.allowMp5 = true;
    document.buy.allow357 = true;
    document.buy.allowShotgun = true;
    document.buy.allowArmor = true;
    document.buy.allowHelmet = true;
    document.buy.allowHandgrenade = true;
    document.buy.costGlock = L"200";
    document.buy.costMp5 = L"1500";
    document.buy.cost357 = L"1200";
    document.buy.costShotgun = L"1700";
    document.buy.costArmor = L"650";
    document.buy.costHelmet = L"350";
    document.buy.costHandgrenade = L"300";
}

void ApplyArmorEquipmentDefaults(ProjectDocument& document) {
    document.armorEquipment.armorMode = false;
    document.armorEquipment.armorStartValue = L"0.0";
    document.armorEquipment.armorMaxValue = L"100.0";
    document.armorEquipment.armorHealthFraction = L"0.5";
    document.armorEquipment.armorDrainScale = L"1.0";
    document.armorEquipment.helmetMode = false;
    document.armorEquipment.helmetStartEnabled = false;
    document.armorEquipment.helmetHeadshotProtection = true;
}

}  // namespace

void ApplyGlockPreset(ProjectDocument& document, const std::wstring& presetName) {
    ApplyGlockDefaults(document);

    if (presetName == L"default") {
        return;
    }

    if (presetName == L"cs_like_soft") {
        document.glock.tapFire = true;
        document.glock.firstShotAccuracy = true;
        document.glock.spreadRecovery = L"0.25";
        document.glock.moveSpreadScale = L"1.0";
        document.glock.profileName = L"cs_like_soft";
        document.glock.primaryBaseSpread = L"0.012";
        document.glock.primaryGroundMovePenalty = L"0.06";
        document.glock.primaryAirMovePenalty = L"0.09";
        document.glock.primaryDuckPenaltyScale = L"0.65";
        document.glock.primaryFirstShotSpeedThreshold = L"55.0";
        document.glock.primaryMaxSpread = L"0.18";
        document.glock.primaryDamage = L"9.0";
        document.glock.primaryHeadshotScale = L"3.5";
        document.glock.primaryHeadshotLethal = false;
        return;
    }

    if (presetName == L"cs_tight") {
        document.glock.tapFire = true;
        document.glock.firstShotAccuracy = true;
        document.glock.spreadRecovery = L"0.35";
        document.glock.moveSpreadScale = L"1.0";
        document.glock.profileName = L"cs_tight";
        document.glock.primaryBaseSpread = L"0.008";
        document.glock.primaryGroundMovePenalty = L"0.095";
        document.glock.primaryAirMovePenalty = L"0.14";
        document.glock.primaryDuckPenaltyScale = L"0.7";
        document.glock.primaryFirstShotSpeedThreshold = L"30.0";
        document.glock.primaryMaxSpread = L"0.16";
        document.glock.primaryDamage = L"10.0";
        document.glock.primaryHeadshotScale = L"4.0";
        document.glock.primaryHeadshotLethal = true;
        return;
    }

    if (presetName == L"headshot_test") {
        document.glock.tapFire = true;
        document.glock.firstShotAccuracy = true;
        document.glock.spreadRecovery = L"0.30";
        document.glock.moveSpreadScale = L"1.0";
        document.glock.profileName = L"headshot_test";
        document.glock.primaryBaseSpread = L"0.009";
        document.glock.primaryGroundMovePenalty = L"0.07";
        document.glock.primaryAirMovePenalty = L"0.12";
        document.glock.primaryDuckPenaltyScale = L"0.65";
        document.glock.primaryFirstShotSpeedThreshold = L"25.0";
        document.glock.primaryMaxSpread = L"0.14";
        document.glock.primaryDamage = L"11.0";
        document.glock.primaryHeadshotScale = L"4.5";
        document.glock.primaryHeadshotLethal = true;
    }
}

void ApplyMp5Preset(ProjectDocument& document, const std::wstring& presetName) {
    ApplyMp5Defaults(document);

    if (presetName == L"default") {
        return;
    }

    document.mp5.primaryEnabled = true;
    document.mp5.labLoadout = true;

    if (presetName == L"cs_burst") {
        document.mp5.profileName = L"cs_burst";
        document.mp5.primaryBaseSpread = L"0.0400";
        document.mp5.primaryGroundMovePenalty = L"0.0180";
        document.mp5.primaryAirMovePenalty = L"0.0500";
        document.mp5.primaryDuckPenaltyScale = L"0.6500";
        document.mp5.primaryBurstGrowth = L"0.0100";
        document.mp5.primaryBurstMaxAdditionalSpread = L"0.0900";
        document.mp5.primarySpreadRecovery = L"0.4500";
        document.mp5.primaryFirstShotAccuracy = true;
        document.mp5.primaryFirstShotSpeedThreshold = L"25.0";
        document.mp5.primaryMaxSpread = L"0.1300";
        document.mp5.primaryDamage = L"12.0";
        document.mp5.primaryHeadshotScale = L"3.5";
        return;
    }

    if (presetName == L"cs_mobile") {
        document.mp5.profileName = L"cs_mobile";
        document.mp5.primaryBaseSpread = L"0.0480";
        document.mp5.primaryGroundMovePenalty = L"0.0120";
        document.mp5.primaryAirMovePenalty = L"0.0300";
        document.mp5.primaryDuckPenaltyScale = L"0.7500";
        document.mp5.primaryBurstGrowth = L"0.0050";
        document.mp5.primaryBurstMaxAdditionalSpread = L"0.0550";
        document.mp5.primarySpreadRecovery = L"0.2500";
        document.mp5.primaryFirstShotAccuracy = true;
        document.mp5.primaryFirstShotSpeedThreshold = L"40.0";
        document.mp5.primaryMaxSpread = L"0.1000";
        document.mp5.primaryDamage = L"11.0";
        document.mp5.primaryHeadshotScale = L"3.0";
        return;
    }

    if (presetName == L"spray_test") {
        document.mp5.profileName = L"spray_test";
        document.mp5.primaryBaseSpread = L"0.0550";
        document.mp5.primaryGroundMovePenalty = L"0.0250";
        document.mp5.primaryAirMovePenalty = L"0.0600";
        document.mp5.primaryDuckPenaltyScale = L"0.8000";
        document.mp5.primaryBurstGrowth = L"0.0120";
        document.mp5.primaryBurstMaxAdditionalSpread = L"0.1100";
        document.mp5.primarySpreadRecovery = L"0.2000";
        document.mp5.primaryFirstShotAccuracy = false;
        document.mp5.primaryFirstShotSpeedThreshold = L"20.0";
        document.mp5.primaryMaxSpread = L"0.1500";
        document.mp5.primaryDamage = L"11.0";
        document.mp5.primaryHeadshotScale = L"3.0";
    }
}

void Apply357Preset(ProjectDocument& document, const std::wstring& presetName) {
    Apply357Defaults(document);

    if (presetName == L"default") {
        return;
    }

    if (presetName == L"precision_test") {
        document.weapon357.profileName = L"precision_test";
        document.weapon357.primaryBaseSpread = L"0.0045";
        document.weapon357.primaryGroundMovePenalty = L"0.0150";
        document.weapon357.primaryAirMovePenalty = L"0.0600";
        document.weapon357.primaryDuckPenaltyScale = L"0.6000";
        document.weapon357.primaryFirstShotAccuracy = true;
        document.weapon357.primaryFirstShotSpeedThreshold = L"18.0";
        document.weapon357.primarySpreadRecovery = L"0.6500";
        document.weapon357.primaryMaxSpread = L"0.0800";
        document.weapon357.primaryDamage = L"42.0";
        document.weapon357.primaryHeadshotScale = L"2.8";
        document.weapon357.primaryHeadshotLethal = false;
        document.weapon357.labAmmo = L"48";
        return;
    }

    if (presetName == L"headshot_test") {
        document.weapon357.profileName = L"headshot_test";
        document.weapon357.primaryBaseSpread = L"0.0060";
        document.weapon357.primaryGroundMovePenalty = L"0.0180";
        document.weapon357.primaryAirMovePenalty = L"0.0700";
        document.weapon357.primaryDuckPenaltyScale = L"0.6500";
        document.weapon357.primaryFirstShotAccuracy = true;
        document.weapon357.primaryFirstShotSpeedThreshold = L"22.0";
        document.weapon357.primarySpreadRecovery = L"0.7000";
        document.weapon357.primaryMaxSpread = L"0.0900";
        document.weapon357.primaryDamage = L"55.0";
        document.weapon357.primaryHeadshotScale = L"4.0";
        document.weapon357.primaryHeadshotLethal = true;
        document.weapon357.labAmmo = L"48";
    }
}

void ApplyShotgunPreset(ProjectDocument& document, const std::wstring& presetName) {
    ApplyShotgunDefaults(document);

    if (presetName == L"default") {
        return;
    }

    if (presetName == L"close_quickkill") {
        document.shotgun.profileName = L"close_quickkill";
        document.shotgun.primaryBaseSpread = L"0.0450";
        document.shotgun.primaryGroundMovePenalty = L"0.0220";
        document.shotgun.primaryAirMovePenalty = L"0.0700";
        document.shotgun.primaryDuckPenaltyScale = L"0.7500";
        document.shotgun.primaryFirstShotAccuracy = true;
        document.shotgun.primaryFirstShotSpeedThreshold = L"20.0";
        document.shotgun.primarySpreadRecovery = L"0.7000";
        document.shotgun.primaryMaxSpread = L"0.0900";
        document.shotgun.primaryDamagePerPellet = L"8.0";
        document.shotgun.primaryPelletCount = L"8";
        document.shotgun.primaryHeadshotScale = L"1.7";
        document.shotgun.primaryHeadshotLethal = false;
        document.shotgun.labAmmo = L"60";
        return;
    }

    if (presetName == L"precision_test") {
        document.shotgun.profileName = L"precision_test";
        document.shotgun.primaryBaseSpread = L"0.0280";
        document.shotgun.primaryGroundMovePenalty = L"0.0160";
        document.shotgun.primaryAirMovePenalty = L"0.0500";
        document.shotgun.primaryDuckPenaltyScale = L"0.7000";
        document.shotgun.primaryFirstShotAccuracy = true;
        document.shotgun.primaryFirstShotSpeedThreshold = L"18.0";
        document.shotgun.primarySpreadRecovery = L"0.6500";
        document.shotgun.primaryMaxSpread = L"0.0600";
        document.shotgun.primaryDamagePerPellet = L"6.0";
        document.shotgun.primaryPelletCount = L"4";
        document.shotgun.primaryHeadshotScale = L"1.6";
        document.shotgun.primaryHeadshotLethal = false;
        document.shotgun.labAmmo = L"48";
    }
}

void ApplyDummyPreset(ProjectDocument& document, const std::wstring& presetName) {
    ApplyDummyDefaults(document);

    if (presetName == L"unarmored") {
        return;
    }

    if (presetName == L"vest") {
        document.targetDummy.targetProfileName = L"vest";
        document.targetDummy.dummyArmor = L"100";
        document.targetDummy.dummyHeadProtected = false;
        return;
    }

    if (presetName == L"vest_headprotected") {
        document.targetDummy.targetProfileName = L"vest_headprotected";
        document.targetDummy.dummyArmor = L"100";
        document.targetDummy.dummyHeadProtected = true;
    }
}

void ApplyMatchPreset(ProjectDocument& document, const std::wstring& presetName) {
    ApplyRoundDefaults(document);
    ApplyTeamRoundDefaults(document);
    ApplyBuyDefaults(document);
    ApplyArmorEquipmentDefaults(document);

    if (presetName == L"duel_glock") {
        ApplyGlockPreset(document, L"cs_tight");
        document.roundMode.enabled = true;
        document.roundMode.weaponProfile = L"duel_glock";
        document.roundMode.loadoutMode = L"glock";
        return;
    }

    if (presetName == L"duel_357") {
        Apply357Preset(document, L"precision_test");
        document.roundMode.enabled = true;
        document.roundMode.weaponProfile = L"duel_357";
        document.roundMode.loadoutMode = L"357";
        return;
    }

    if (presetName == L"team_mp5") {
        ApplyMp5Preset(document, L"cs_burst");
        document.roundMode.enabled = true;
        document.roundMode.weaponProfile = L"team_mp5";
        document.teamRound.enabled = true;
        document.teamRound.team1Loadout = L"mp5";
        document.teamRound.team2Loadout = L"mp5";
        document.teamRound.team1Health = L"100.0";
        document.teamRound.team2Health = L"100.0";
        document.teamRound.team1Armor = L"0.0";
        document.teamRound.team2Armor = L"0.0";
        return;
    }

    if (presetName == L"team_shotgun") {
        ApplyShotgunPreset(document, L"close_quickkill");
        document.roundMode.enabled = true;
        document.roundMode.weaponProfile = L"team_shotgun";
        document.teamRound.enabled = true;
        document.teamRound.team1Loadout = L"shotgun";
        document.teamRound.team2Loadout = L"shotgun";
        document.teamRound.team1Health = L"120.0";
        document.teamRound.team2Health = L"120.0";
        document.teamRound.team1Armor = L"25.0";
        document.teamRound.team2Armor = L"25.0";
        return;
    }

    if (presetName == L"armor_test") {
        Apply357Preset(document, L"headshot_test");
        document.roundMode.enabled = true;
        document.roundMode.weaponProfile = L"armor_test";
        document.roundMode.loadoutMode = L"357";
        document.armorEquipment.armorMode = true;
        document.armorEquipment.armorStartValue = L"100.0";
        document.armorEquipment.helmetMode = true;
        document.armorEquipment.helmetStartEnabled = true;
        document.armorEquipment.helmetHeadshotProtection = true;
        return;
    }

    if (presetName == L"buy_test") {
        ApplyMp5Preset(document, L"default");
        document.general.weaponUnderTest = L"mp5";
        document.roundMode.enabled = true;
        document.roundMode.weaponProfile = L"buy_test";
        document.buy.enabled = true;
        document.buy.startMoney = L"2500";
        document.buy.freezeOnly = true;
        document.armorEquipment.armorMode = true;
        document.armorEquipment.helmetMode = true;
        return;
    }
}

}  // namespace hlcfg
