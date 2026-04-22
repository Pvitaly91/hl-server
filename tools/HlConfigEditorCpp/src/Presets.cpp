#include "Presets.h"

namespace hlcfg {

namespace {

void ApplyGlockDefaults(ProjectDocument& document) {
    document.general.weaponUnderTest = L"glock";
    document.glock.tapFire = false;
    document.glock.firstShotAccuracy = true;
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
    document.glock.patternMode = false;
    document.glock.patternScaleX = L"0.2000";
    document.glock.patternScaleY = L"0.3500";
    document.glock.patternResetTime = L"0.3500";
    document.glock.patternMaxIndex = L"5";
    document.glock.primaryDamage = L"10.0";
    document.glock.primaryHeadshotScale = L"4.0";
    document.glock.primaryHeadshotLethal = false;
}

void ApplyMp5Defaults(ProjectDocument& document) {
    document.general.weaponUnderTest = L"mp5";
    document.mp5.primaryEnabled = false;
    document.mp5.profileName = L"default";
    document.mp5.primaryBaseSpread = L"0.0380";
    document.mp5.primaryGroundMovePenalty = L"0.0240";
    document.mp5.primaryAirMovePenalty = L"0.0550";
    document.mp5.primaryDuckPenaltyScale = L"0.6200";
    document.mp5.primaryBurstGrowth = L"0.0140";
    document.mp5.primaryBurstMaxAdditionalSpread = L"0.0950";
    document.mp5.primarySpreadRecovery = L"0.8000";
    document.mp5.primaryFirstShotAccuracy = true;
    document.mp5.primaryFirstShotSpeedThreshold = L"30.0";
    document.mp5.primaryMaxSpread = L"0.1250";
    document.mp5.patternMode = false;
    document.mp5.patternScaleX = L"0.2600";
    document.mp5.patternScaleY = L"0.5500";
    document.mp5.patternResetTime = L"0.2800";
    document.mp5.patternMaxIndex = L"6";
    document.mp5.primaryDamage = L"12.0";
    document.mp5.primaryHeadshotScale = L"3.25";
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

    if (presetName == L"cs_like_soft" || presetName == L"glock_cs_like_soft" || presetName == L"cs_mobile" || presetName == L"glock_pattern_soft") {
        document.glock.tapFire = false;
        document.glock.firstShotAccuracy = true;
        document.glock.spreadRecovery = L"0.6500";
        document.glock.moveSpreadScale = L"1.0";
        document.glock.profileName = presetName == L"glock_pattern_soft" ? L"glock_pattern_soft" : L"glock_cs_like_soft";
        document.glock.primaryBaseSpread = L"0.0105";
        document.glock.primaryGroundMovePenalty = L"0.0650";
        document.glock.primaryAirMovePenalty = L"0.1000";
        document.glock.primaryDuckPenaltyScale = L"0.55";
        document.glock.primaryShotGrowth = L"0.0550";
        document.glock.primaryFirstShotSpeedThreshold = L"60.0";
        document.glock.primaryMaxSpread = L"0.1750";
        document.glock.patternMode = true;
        document.glock.patternScaleX = L"0.1700";
        document.glock.patternScaleY = L"0.2800";
        document.glock.patternResetTime = L"0.4200";
        document.glock.patternMaxIndex = L"4";
        document.glock.primaryDamage = L"10.0";
        document.glock.primaryHeadshotScale = L"3.75";
        document.glock.primaryHeadshotLethal = false;
        return;
    }

    if (presetName == L"cs_tight" || presetName == L"glock_cs_like_tight" || presetName == L"glock_pattern_tight") {
        document.glock.tapFire = false;
        document.glock.firstShotAccuracy = true;
        document.glock.spreadRecovery = L"0.8500";
        document.glock.moveSpreadScale = L"1.0";
        document.glock.profileName = presetName == L"glock_pattern_tight" ? L"glock_pattern_tight" : L"glock_cs_like_tight";
        document.glock.primaryBaseSpread = L"0.0085";
        document.glock.primaryGroundMovePenalty = L"0.0950";
        document.glock.primaryAirMovePenalty = L"0.1450";
        document.glock.primaryDuckPenaltyScale = L"0.52";
        document.glock.primaryShotGrowth = L"0.0900";
        document.glock.primaryFirstShotSpeedThreshold = L"35.0";
        document.glock.primaryMaxSpread = L"0.1650";
        document.glock.patternMode = true;
        document.glock.patternScaleX = L"0.2400";
        document.glock.patternScaleY = L"0.4200";
        document.glock.patternResetTime = L"0.3200";
        document.glock.patternMaxIndex = L"5";
        document.glock.primaryDamage = L"10.0";
        document.glock.primaryHeadshotScale = L"4.25";
        document.glock.primaryHeadshotLethal = true;
        return;
    }

    if (presetName == L"headshot_test") {
        document.glock.tapFire = false;
        document.glock.firstShotAccuracy = true;
        document.glock.spreadRecovery = L"0.8000";
        document.glock.moveSpreadScale = L"1.0";
        document.glock.profileName = L"headshot_test";
        document.glock.primaryBaseSpread = L"0.009";
        document.glock.primaryGroundMovePenalty = L"0.075";
        document.glock.primaryAirMovePenalty = L"0.12";
        document.glock.primaryDuckPenaltyScale = L"0.55";
        document.glock.primaryShotGrowth = L"0.0850";
        document.glock.primaryFirstShotSpeedThreshold = L"25.0";
        document.glock.primaryMaxSpread = L"0.14";
        document.glock.patternMode = true;
        document.glock.patternScaleX = L"0.2100";
        document.glock.patternScaleY = L"0.3600";
        document.glock.patternResetTime = L"0.3000";
        document.glock.patternMaxIndex = L"5";
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

    if (presetName == L"cs_burst" || presetName == L"mp5_controlled_burst" || presetName == L"mp5_pattern_burst") {
        document.mp5.profileName = presetName == L"mp5_pattern_burst" ? L"mp5_pattern_burst" : L"mp5_controlled_burst";
        document.mp5.primaryBaseSpread = L"0.0360";
        document.mp5.primaryGroundMovePenalty = L"0.0240";
        document.mp5.primaryAirMovePenalty = L"0.0550";
        document.mp5.primaryDuckPenaltyScale = L"0.6000";
        document.mp5.primaryBurstGrowth = L"0.0180";
        document.mp5.primaryBurstMaxAdditionalSpread = L"0.0950";
        document.mp5.primarySpreadRecovery = L"0.9500";
        document.mp5.primaryFirstShotAccuracy = true;
        document.mp5.primaryFirstShotSpeedThreshold = L"28.0";
        document.mp5.primaryMaxSpread = L"0.1250";
        document.mp5.patternMode = true;
        document.mp5.patternScaleX = L"0.2400";
        document.mp5.patternScaleY = L"0.5000";
        document.mp5.patternResetTime = L"0.2600";
        document.mp5.patternMaxIndex = L"6";
        document.mp5.primaryDamage = L"12.0";
        document.mp5.primaryHeadshotScale = L"3.30";
        return;
    }

    if (presetName == L"cs_mobile" || presetName == L"mp5_mobile_soft" || presetName == L"mp5_pattern_mobile") {
        document.mp5.profileName = presetName == L"mp5_pattern_mobile" ? L"mp5_pattern_mobile" : L"mp5_mobile_soft";
        document.mp5.primaryBaseSpread = L"0.0440";
        document.mp5.primaryGroundMovePenalty = L"0.0180";
        document.mp5.primaryAirMovePenalty = L"0.0400";
        document.mp5.primaryDuckPenaltyScale = L"0.7000";
        document.mp5.primaryBurstGrowth = L"0.0130";
        document.mp5.primaryBurstMaxAdditionalSpread = L"0.0800";
        document.mp5.primarySpreadRecovery = L"0.9000";
        document.mp5.primaryFirstShotAccuracy = true;
        document.mp5.primaryFirstShotSpeedThreshold = L"40.0";
        document.mp5.primaryMaxSpread = L"0.1150";
        document.mp5.patternMode = true;
        document.mp5.patternScaleX = L"0.1800";
        document.mp5.patternScaleY = L"0.3600";
        document.mp5.patternResetTime = L"0.3200";
        document.mp5.patternMaxIndex = L"5";
        document.mp5.primaryDamage = L"11.5";
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
    document.matchPack.name.clear();
    document.matchPack.description.clear();
    document.matchPack.tags.clear();

    if (presetName == L"duel_glock") {
        ApplyGlockPreset(document, L"glock_cs_like_tight");
        document.roundMode.enabled = true;
        document.roundMode.weaponProfile = L"duel_glock";
        document.roundMode.loadoutMode = L"glock";
        document.matchPack.name = L"duel_glock";
        document.matchPack.description = L"1v1 Glock duel pack";
        document.matchPack.tags = L"duel,glock";
        return;
    }

    if (presetName == L"duel_357") {
        Apply357Preset(document, L"precision_test");
        document.roundMode.enabled = true;
        document.roundMode.weaponProfile = L"duel_357";
        document.roundMode.loadoutMode = L"357";
        document.matchPack.name = L"duel_357";
        document.matchPack.description = L"1v1 357 duel pack";
        document.matchPack.tags = L"duel,357";
        return;
    }

    if (presetName == L"team_mp5") {
        ApplyMp5Preset(document, L"mp5_controlled_burst");
        document.roundMode.enabled = true;
        document.roundMode.weaponProfile = L"team_mp5";
        document.teamRound.enabled = true;
        document.teamRound.team1Loadout = L"mp5";
        document.teamRound.team2Loadout = L"mp5";
        document.teamRound.team1Health = L"100.0";
        document.teamRound.team2Health = L"100.0";
        document.teamRound.team1Armor = L"0.0";
        document.teamRound.team2Armor = L"0.0";
        document.matchPack.name = L"team_mp5";
        document.matchPack.description = L"Simple team MP5 round pack";
        document.matchPack.tags = L"team,mp5";
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
        document.matchPack.name = L"team_shotgun";
        document.matchPack.description = L"Simple team shotgun round pack";
        document.matchPack.tags = L"team,shotgun";
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
        document.matchPack.name = L"armor_test";
        document.matchPack.description = L"Armor and helmet validation pack";
        document.matchPack.tags = L"armor,helmet,357";
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
        document.matchPack.name = L"buy_test";
        document.matchPack.description = L"Freeze-time buy prototype pack";
        document.matchPack.tags = L"buy,armor,mp5";
        return;
    }
}

}  // namespace hlcfg
