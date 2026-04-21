#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"
#include "weapons.h"
#include "client.h"
#include "gamerules.h"

#include "future_gameplay_hooks.h"
#include "weapon_debug_logger.h"

#include <io.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <windows.h>

namespace
{
const char *kDefaultGlockLabTargetProfileName = "default";
const float kDefaultGlockLabDummyHealth = 100.0f;
const float kDefaultGlockLabDummyRespawnDelay = 1.0f;
const float kDefaultGlockLabDummySpawnDistance = 256.0f;
const float kGlockLabDummyRetryDelay = 1.0f;
const float kGlockLabDummyPlacementSearchStep = 32.0f;
const float kGlockLabDummyPlacementMinDistance = 64.0f;
const float kGlockLabDummyPlacementSideStep = 16.0f;
const float kGlockLabDummyPlacementForwardStep = 16.0f;
const float kGlockLabDummyPlacementVerticalStep = 18.0f;
const float kGlockLabDummyPlacementWideStep = 64.0f;
const float kGlockLabDummyPlacementLongStep = 96.0f;
const char *kDefaultGlockLabDummyModel = "models/barney.mdl";
const char *kGlockLabDummyDisplayName = "Damage Dummy";
const size_t kMaxLiveCfgRequestLength = 512;
const size_t kMaxLiveCfgExecPathLength = 512;
const size_t kMaxLiveCfgPathLength = 1024;
const size_t kMaxLiveCfgFailureLength = 512;
const size_t kMaxLabDummySpotNameLength = 64;
const size_t kMaxLabDummySpotStorageLength = 16;
const size_t kMaxLabDummySpotTimestampLength = 64;
const size_t kMaxLabDummySpotNoteLength = 128;
const size_t kMaxLabDummySpotFileFailureLength = 512;
const size_t kMaxLabDummySpotListLength = 512;
const size_t kMaxLabDummySpotsPerMap = 32;
const size_t kMaxLabDummySpotFileSize = 64 * 1024;
const size_t kMaxTeamSpawnSpotsPerTeam = 16;
const char *kAllowedGlockLabDummyModels[] = {
    "models/barney.mdl",
    "models/scientist.mdl"};
const char *kGlockLabDummyClassname = "glock_lab_dummy";
const char *kGlockLabDummyTargetname = "exp_glock_lab_dummy";
const char *kLabDummySourceSavedSpot = "saved_spot";
const char *kLabDummySourceCurrentAnchor = "current_anchor";
const char *kLabDummySourceLastGood = "last_known_good";
const char *kDefaultLabDummySpotName = "default";
const char *kLabDummySpotStorageDisk = "disk";
const char *kLabDummySpotStorageSession = "session";
const char *kLabDummyTargetSpotsDirectoryName = "target_spots";
const char *kTeamSpawnSpotStorageDisk = "disk";
const char *kTeamSpawnSpotStorageSession = "session";
const char *kTeamSpawnSpotsDirectoryName = "team_spawns";
const float kRoundEndHoldSeconds = 0.25f;
const float kDefaultRoundFreezeTime = 3.0f;
const float kDefaultRoundRestartDelay = 3.0f;
const float kDefaultRoundStartHealth = 100.0f;
const float kDefaultTeamRoundOverrideValue = -1.0f;
const int kMaxRoundTeamPlayerSlots = 33;
const int kMaxRoundFakeClientNameLength = 64;
const char *kDefaultTeamRoundSpawnMode = "dm_spawns";
const char *kManualTeamRoundSpawnMode = "manual_spots";
const char *kDefaultTeamRoundTeam1Name = "team1";
const char *kDefaultTeamRoundTeam2Name = "team2";
const char *kRoundEndReasonTeamsIncomplete = "teams_incomplete";
const char *kTeamSpawnSourceSavedSpot = "saved_spot";
const char *kTeamSpawnSourceDmSpawn = "dm_spawn";
const char *kTeamSpawnCandidateDefault = "default";

struct LiveCfgState
{
    bool cfgDrivenModeActive;
    bool hasActiveCfg;
    bool hasLastSuccessfulCfg;
    bool lastCommandSucceeded;
    char activeRequestedPath[kMaxLiveCfgRequestLength];
    char activeExecPath[kMaxLiveCfgExecPathLength];
    char activeResolvedPath[kMaxLiveCfgPathLength];
    char lastSuccessfulExecPath[kMaxLiveCfgExecPathLength];
    char lastSuccessfulResolvedPath[kMaxLiveCfgPathLength];
    char lastAction[32];
    char lastAppliedAt[64];
    float lastApplyServerTime;
    char lastFailureAction[32];
    char lastFailure[kMaxLiveCfgFailureLength];
};

struct ResolvedLiveCfgSelection
{
    char requestedPath[kMaxLiveCfgRequestLength];
    char execPath[kMaxLiveCfgExecPathLength];
    char resolvedPath[kMaxLiveCfgPathLength];
};

struct LabDummyProfileDefinition
{
    const char *name;
    const char *armor;
    const char *headProtected;
    const char *armorHealthFraction;
    const char *armorDrainScale;
    const char *description;
};

struct LabDummyTransformMemory
{
    bool valid;
    Vector origin;
    Vector angles;
    char source[32];
    char candidate[64];
};

struct LabDummySavedSpotRecord
{
    bool valid;
    bool loadedFromDisk;
    Vector origin;
    Vector angles;
    char name[kMaxLabDummySpotNameLength];
    char candidate[64];
    char createdAt[kMaxLabDummySpotTimestampLength];
    char updatedAt[kMaxLabDummySpotTimestampLength];
    char note[kMaxLabDummySpotNoteLength];
};

struct LabDummySpawnSelection
{
    Vector origin;
    Vector angles;
    CBasePlayer *anchorPlayer;
    char source[32];
    char candidate[64];
    char spotName[kMaxLabDummySpotNameLength];
    char spotStorage[kMaxLabDummySpotStorageLength];
};

struct LabDummyFailureInfo
{
    char code[64];
    char source[32];
    char candidate[64];
    char reason[512];
    char spotName[kMaxLabDummySpotNameLength];
    char spotStorage[kMaxLabDummySpotStorageLength];
};

struct LabDummyPlacementCandidate
{
    const char *label;
    float forwardOffset;
    float rightOffset;
    float upOffset;
};

struct TeamSpawnSavedSpotRecord
{
    bool valid;
    bool loadedFromDisk;
    int teamId;
    Vector origin;
    Vector angles;
    char name[kMaxLabDummySpotNameLength];
    char createdAt[kMaxLabDummySpotTimestampLength];
    char updatedAt[kMaxLabDummySpotTimestampLength];
    char note[kMaxLabDummySpotNoteLength];
};

struct TeamSpawnSelection
{
    bool valid;
    int teamId;
    Vector origin;
    Vector angles;
    char source[32];
    char candidate[64];
    char spotName[kMaxLabDummySpotNameLength];
    char spotStorage[kMaxLabDummySpotStorageLength];
};

struct TeamSpawnFailureInfo
{
    int teamId;
    char code[64];
    char source[32];
    char candidate[64];
    char reason[512];
    char spotName[kMaxLabDummySpotNameLength];
    char spotStorage[kMaxLabDummySpotStorageLength];
};

struct TeamSpawnPlacementCandidate
{
    const char *label;
    float forwardOffset;
    float rightOffset;
    float upOffset;
};

struct TeamSpawnRuntimeStatus
{
    bool valid;
    int teamId;
    int roundNumber;
    Vector origin;
    Vector angles;
    char playerName[64];
    int playerEntIndex;
    int playerUserId;
    char source[32];
    char candidate[64];
    char spotName[kMaxLabDummySpotNameLength];
    char spotStorage[kMaxLabDummySpotStorageLength];
    char reason[512];
};

enum ExpRoundStateType
{
    kExpRoundStateDisabled = 0,
    kExpRoundStateWaitingForPlayers,
    kExpRoundStateFreezeTime,
    kExpRoundStateLive,
    kExpRoundStateRoundEnd,
    kExpRoundStateRestartPending
};

enum ExpRoundTeamId
{
    kExpRoundTeamNone = 0,
    kExpRoundTeam1 = 1,
    kExpRoundTeam2 = 2
};

struct ExpRoundTeamAssignment
{
    bool assigned;
    int teamId;
    int userId;
    char playerName[64];
};

struct ExpRoundFakeClientRecord
{
    bool active;
    int userId;
    char playerName[64];
};

struct ExpRoundRuntimeState
{
    ExpRoundStateType state;
    int roundNumber;
    int connectedPlayers;
    int alivePlayers;
    int team1ConnectedPlayers;
    int team2ConnectedPlayers;
    int unassignedConnectedPlayers;
    int team1AlivePlayers;
    int team2AlivePlayers;
    int unassignedAlivePlayers;
    float stateEnteredAt;
    float nextTransitionAt;
    bool applyingRoundReset;
    char lastWinnerName[64];
    int lastWinnerEntIndex;
    int lastWinnerUserId;
    int lastWinnerTeamId;
    char lastWinnerTeamName[64];
    char lastEndReason[64];
};

struct ExpRoundPlayerSnapshot
{
    int connectedPlayers;
    int alivePlayers;
    int team1ConnectedPlayers;
    int team2ConnectedPlayers;
    int unassignedConnectedPlayers;
    int team1AlivePlayers;
    int team2AlivePlayers;
    int unassignedAlivePlayers;
    CBasePlayer *lastAlivePlayer;
    CBasePlayer *lastAliveTeam1Player;
    CBasePlayer *lastAliveTeam2Player;
};

const LabDummyProfileDefinition kBuiltInLabDummyProfiles[] = {
    {"unarmored", "0", "0", "0.5", "1.0", "baseline unarmored target"},
    {"vest", "100", "0", "0.5", "1.0", "torso-armored target"},
    {"vest_headprotected", "100", "1", "0.5", "1.0", "armored target with protected head"}};

const LabDummyPlacementCandidate kLabDummyPlacementCandidates[] = {
    {"center", 0.0f, 0.0f, 0.0f},
    {"right_16", 0.0f, kGlockLabDummyPlacementSideStep, 0.0f},
    {"left_16", 0.0f, -kGlockLabDummyPlacementSideStep, 0.0f},
    {"forward_16", kGlockLabDummyPlacementForwardStep, 0.0f, 0.0f},
    {"back_16", -kGlockLabDummyPlacementForwardStep, 0.0f, 0.0f},
    {"up_18", 0.0f, 0.0f, kGlockLabDummyPlacementVerticalStep},
    {"down_18", 0.0f, 0.0f, -kGlockLabDummyPlacementVerticalStep},
    {"right_32", 0.0f, kGlockLabDummyPlacementSideStep * 2.0f, 0.0f},
    {"left_32", 0.0f, -kGlockLabDummyPlacementSideStep * 2.0f, 0.0f},
    {"forward_32", kGlockLabDummyPlacementForwardStep * 2.0f, 0.0f, 0.0f},
    {"back_32", -kGlockLabDummyPlacementForwardStep * 2.0f, 0.0f, 0.0f},
    {"right_16_up_18", 0.0f, kGlockLabDummyPlacementSideStep, kGlockLabDummyPlacementVerticalStep},
    {"left_16_up_18", 0.0f, -kGlockLabDummyPlacementSideStep, kGlockLabDummyPlacementVerticalStep},
    {"forward_16_up_18", kGlockLabDummyPlacementForwardStep, 0.0f, kGlockLabDummyPlacementVerticalStep},
    {"back_16_up_18", -kGlockLabDummyPlacementForwardStep, 0.0f, kGlockLabDummyPlacementVerticalStep},
    {"right_64", 0.0f, kGlockLabDummyPlacementWideStep, 0.0f},
    {"left_64", 0.0f, -kGlockLabDummyPlacementWideStep, 0.0f},
    {"forward_64", kGlockLabDummyPlacementWideStep, 0.0f, 0.0f},
    {"back_64", -kGlockLabDummyPlacementWideStep, 0.0f, 0.0f},
    {"right_96", 0.0f, kGlockLabDummyPlacementLongStep, 0.0f},
    {"left_96", 0.0f, -kGlockLabDummyPlacementLongStep, 0.0f},
    {"forward_96", kGlockLabDummyPlacementLongStep, 0.0f, 0.0f},
    {"back_96", -kGlockLabDummyPlacementLongStep, 0.0f, 0.0f},
    {"right_64_back_64", -kGlockLabDummyPlacementWideStep, kGlockLabDummyPlacementWideStep, 0.0f},
    {"left_64_back_64", -kGlockLabDummyPlacementWideStep, -kGlockLabDummyPlacementWideStep, 0.0f},
    {"right_64_forward_64", kGlockLabDummyPlacementWideStep, kGlockLabDummyPlacementWideStep, 0.0f},
    {"left_64_forward_64", kGlockLabDummyPlacementWideStep, -kGlockLabDummyPlacementWideStep, 0.0f},
    {"right_96_up_18", 0.0f, kGlockLabDummyPlacementLongStep, kGlockLabDummyPlacementVerticalStep},
    {"left_96_up_18", 0.0f, -kGlockLabDummyPlacementLongStep, kGlockLabDummyPlacementVerticalStep},
    {"back_64_up_18", -kGlockLabDummyPlacementWideStep, 0.0f, kGlockLabDummyPlacementVerticalStep},
    {"back_96_up_18", -kGlockLabDummyPlacementLongStep, 0.0f, kGlockLabDummyPlacementVerticalStep}};

const TeamSpawnPlacementCandidate kTeamSpawnPlacementCandidates[] = {
    {"center", 0.0f, 0.0f, 0.0f},
    {"right_32", 0.0f, kGlockLabDummyPlacementSideStep * 2.0f, 0.0f},
    {"left_32", 0.0f, -kGlockLabDummyPlacementSideStep * 2.0f, 0.0f},
    {"forward_32", kGlockLabDummyPlacementForwardStep * 2.0f, 0.0f, 0.0f},
    {"back_32", -kGlockLabDummyPlacementForwardStep * 2.0f, 0.0f, 0.0f},
    {"up_18", 0.0f, 0.0f, kGlockLabDummyPlacementVerticalStep},
    {"right_64", 0.0f, kGlockLabDummyPlacementWideStep, 0.0f},
    {"left_64", 0.0f, -kGlockLabDummyPlacementWideStep, 0.0f},
    {"forward_64", kGlockLabDummyPlacementWideStep, 0.0f, 0.0f},
    {"back_64", -kGlockLabDummyPlacementWideStep, 0.0f, 0.0f},
    {"right_32_up_18", 0.0f, kGlockLabDummyPlacementSideStep * 2.0f, kGlockLabDummyPlacementVerticalStep},
    {"left_32_up_18", 0.0f, -kGlockLabDummyPlacementSideStep * 2.0f, kGlockLabDummyPlacementVerticalStep}};

cvar_t sv_exp_pistol_tapfire = {"sv_exp_pistol_tapfire", "0", FCVAR_SERVER};
cvar_t sv_exp_move_spread_scale = {"sv_exp_move_spread_scale", "0.0", FCVAR_SERVER};
cvar_t sv_exp_first_shot_accuracy = {"sv_exp_first_shot_accuracy", "0", FCVAR_SERVER};
cvar_t sv_exp_spread_recovery = {"sv_exp_spread_recovery", "0.0", FCVAR_SERVER};
cvar_t sv_exp_weapon_under_test = {"sv_exp_weapon_under_test", "", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_glock_profile_name = {"sv_exp_glock_profile_name", "default", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_mp5_profile_name = {"sv_exp_mp5_profile_name", "default", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_357_profile_name = {"sv_exp_357_profile_name", "default", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_shotgun_profile_name = {"sv_exp_shotgun_profile_name", "default", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_session_tag = {"sv_exp_session_tag", "", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_matrix_name = {"sv_exp_matrix_name", "", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_matrix_step = {"sv_exp_matrix_step", "", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_glock_primary_base_spread = {"sv_exp_glock_primary_base_spread", "0.01", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_ground_move_penalty = {"sv_exp_glock_primary_ground_move_penalty", "0.08", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_air_move_penalty = {"sv_exp_glock_primary_air_move_penalty", "0.12", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_duck_penalty_scale = {"sv_exp_glock_primary_duck_penalty_scale", "0.75", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_first_shot_speed_threshold = {"sv_exp_glock_primary_first_shot_speed_threshold", "40.0", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_max_spread = {"sv_exp_glock_primary_max_spread", "0.2", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_damage = {"sv_exp_glock_primary_damage", "8.0", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_headshot_scale = {"sv_exp_glock_primary_headshot_scale", "3.0", FCVAR_SERVER};
cvar_t sv_exp_glock_primary_headshot_lethal = {"sv_exp_glock_primary_headshot_lethal", "0", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_enabled = {"sv_exp_mp5_primary_enabled", "0", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_base_spread = {"sv_exp_mp5_primary_base_spread", "0.0523", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_ground_move_penalty = {"sv_exp_mp5_primary_ground_move_penalty", "0.0200", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_air_move_penalty = {"sv_exp_mp5_primary_air_move_penalty", "0.0400", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_duck_penalty_scale = {"sv_exp_mp5_primary_duck_penalty_scale", "0.7000", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_burst_growth = {"sv_exp_mp5_primary_burst_growth", "0.0060", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_burst_max_additional_spread = {"sv_exp_mp5_primary_burst_max_additional_spread", "0.0600", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_spread_recovery = {"sv_exp_mp5_primary_spread_recovery", "0.3000", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_damage = {"sv_exp_mp5_primary_damage", "12.0", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_headshot_scale = {"sv_exp_mp5_primary_headshot_scale", "3.0", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_headshot_lethal = {"sv_exp_mp5_primary_headshot_lethal", "0", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_first_shot_accuracy = {"sv_exp_mp5_primary_first_shot_accuracy", "0", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_first_shot_speed_threshold = {"sv_exp_mp5_primary_first_shot_speed_threshold", "30.0", FCVAR_SERVER};
cvar_t sv_exp_mp5_primary_max_spread = {"sv_exp_mp5_primary_max_spread", "0.1200", FCVAR_SERVER};
cvar_t sv_exp_mp5_lab_loadout = {"sv_exp_mp5_lab_loadout", "0", FCVAR_SERVER};
cvar_t sv_exp_mp5_lab_ammo = {"sv_exp_mp5_lab_ammo", "250", FCVAR_SERVER};
cvar_t sv_exp_mp5_lab_autoswitch = {"sv_exp_mp5_lab_autoswitch", "1", FCVAR_SERVER};
cvar_t sv_exp_357_primary_enabled = {"sv_exp_357_primary_enabled", "0", FCVAR_SERVER};
cvar_t sv_exp_357_primary_base_spread = {"sv_exp_357_primary_base_spread", "0.0125", FCVAR_SERVER};
cvar_t sv_exp_357_primary_ground_move_penalty = {"sv_exp_357_primary_ground_move_penalty", "0.0300", FCVAR_SERVER};
cvar_t sv_exp_357_primary_air_move_penalty = {"sv_exp_357_primary_air_move_penalty", "0.0800", FCVAR_SERVER};
cvar_t sv_exp_357_primary_duck_penalty_scale = {"sv_exp_357_primary_duck_penalty_scale", "0.6500", FCVAR_SERVER};
cvar_t sv_exp_357_primary_first_shot_accuracy = {"sv_exp_357_primary_first_shot_accuracy", "1", FCVAR_SERVER};
cvar_t sv_exp_357_primary_first_shot_speed_threshold = {"sv_exp_357_primary_first_shot_speed_threshold", "35.0", FCVAR_SERVER};
cvar_t sv_exp_357_primary_spread_recovery = {"sv_exp_357_primary_spread_recovery", "0.6000", FCVAR_SERVER};
cvar_t sv_exp_357_primary_max_spread = {"sv_exp_357_primary_max_spread", "0.1200", FCVAR_SERVER};
cvar_t sv_exp_357_primary_damage = {"sv_exp_357_primary_damage", "40.0", FCVAR_SERVER};
cvar_t sv_exp_357_primary_headshot_scale = {"sv_exp_357_primary_headshot_scale", "3.0", FCVAR_SERVER};
cvar_t sv_exp_357_primary_headshot_lethal = {"sv_exp_357_primary_headshot_lethal", "0", FCVAR_SERVER};
cvar_t sv_exp_357_lab_loadout = {"sv_exp_357_lab_loadout", "0", FCVAR_SERVER};
cvar_t sv_exp_357_lab_ammo = {"sv_exp_357_lab_ammo", "24", FCVAR_SERVER};
cvar_t sv_exp_357_lab_autoswitch = {"sv_exp_357_lab_autoswitch", "1", FCVAR_SERVER};
cvar_t sv_exp_shotgun_primary_enabled = {"sv_exp_shotgun_primary_enabled", "0", FCVAR_SERVER};
cvar_t sv_exp_shotgun_primary_base_spread = {"sv_exp_shotgun_primary_base_spread", "0.0600", FCVAR_SERVER};
cvar_t sv_exp_shotgun_primary_ground_move_penalty = {"sv_exp_shotgun_primary_ground_move_penalty", "0.0300", FCVAR_SERVER};
cvar_t sv_exp_shotgun_primary_air_move_penalty = {"sv_exp_shotgun_primary_air_move_penalty", "0.0800", FCVAR_SERVER};
cvar_t sv_exp_shotgun_primary_duck_penalty_scale = {"sv_exp_shotgun_primary_duck_penalty_scale", "0.8000", FCVAR_SERVER};
cvar_t sv_exp_shotgun_primary_first_shot_accuracy = {"sv_exp_shotgun_primary_first_shot_accuracy", "0", FCVAR_SERVER};
cvar_t sv_exp_shotgun_primary_first_shot_speed_threshold = {"sv_exp_shotgun_primary_first_shot_speed_threshold", "35.0", FCVAR_SERVER};
cvar_t sv_exp_shotgun_primary_spread_recovery = {"sv_exp_shotgun_primary_spread_recovery", "0.8500", FCVAR_SERVER};
cvar_t sv_exp_shotgun_primary_max_spread = {"sv_exp_shotgun_primary_max_spread", "0.1200", FCVAR_SERVER};
cvar_t sv_exp_shotgun_primary_damage_per_pellet = {"sv_exp_shotgun_primary_damage_per_pellet", "5.0", FCVAR_SERVER};
cvar_t sv_exp_shotgun_primary_pellet_count = {"sv_exp_shotgun_primary_pellet_count", "6", FCVAR_SERVER};
cvar_t sv_exp_shotgun_primary_headshot_scale = {"sv_exp_shotgun_primary_headshot_scale", "1.5", FCVAR_SERVER};
cvar_t sv_exp_shotgun_primary_headshot_lethal = {"sv_exp_shotgun_primary_headshot_lethal", "0", FCVAR_SERVER};
cvar_t sv_exp_shotgun_lab_loadout = {"sv_exp_shotgun_lab_loadout", "0", FCVAR_SERVER};
cvar_t sv_exp_shotgun_lab_ammo = {"sv_exp_shotgun_lab_ammo", "48", FCVAR_SERVER};
cvar_t sv_exp_shotgun_lab_autoswitch = {"sv_exp_shotgun_lab_autoswitch", "1", FCVAR_SERVER};
cvar_t sv_exp_round_mode = {"sv_exp_round_mode", "0", FCVAR_SERVER};
cvar_t sv_exp_round_freeze_time = {"sv_exp_round_freeze_time", "3.0", FCVAR_SERVER};
cvar_t sv_exp_round_restart_delay = {"sv_exp_round_restart_delay", "3.0", FCVAR_SERVER};
cvar_t sv_exp_round_start_health = {"sv_exp_round_start_health", "100.0", FCVAR_SERVER};
cvar_t sv_exp_round_start_armor = {"sv_exp_round_start_armor", "0.0", FCVAR_SERVER};
cvar_t sv_exp_round_no_respawn = {"sv_exp_round_no_respawn", "1", FCVAR_SERVER};
cvar_t sv_exp_round_friendlyfire = {"sv_exp_round_friendlyfire", "0", FCVAR_SERVER};
cvar_t sv_exp_round_weapon_profile = {"sv_exp_round_weapon_profile", "", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_round_loadout_mode = {"sv_exp_round_loadout_mode", "none", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_team_round_mode = {"sv_exp_team_round_mode", "0", FCVAR_SERVER};
cvar_t sv_exp_team_round_teamplay = {"sv_exp_team_round_teamplay", "1", FCVAR_SERVER};
cvar_t sv_exp_team_round_spawn_mode = {"sv_exp_team_round_spawn_mode", "dm_spawns", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_team_round_team1_name = {"sv_exp_team_round_team1_name", "team1", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_team_round_team2_name = {"sv_exp_team_round_team2_name", "team2", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_team_round_team1_loadout = {"sv_exp_team_round_team1_loadout", "", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_team_round_team2_loadout = {"sv_exp_team_round_team2_loadout", "", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_team_round_team1_health = {"sv_exp_team_round_team1_health", "-1.0", FCVAR_SERVER};
cvar_t sv_exp_team_round_team2_health = {"sv_exp_team_round_team2_health", "-1.0", FCVAR_SERVER};
cvar_t sv_exp_team_round_team1_armor = {"sv_exp_team_round_team1_armor", "-1.0", FCVAR_SERVER};
cvar_t sv_exp_team_round_team2_armor = {"sv_exp_team_round_team2_armor", "-1.0", FCVAR_SERVER};
cvar_t sv_exp_debug_weaponlog = {"sv_exp_debug_weaponlog", "0", FCVAR_SERVER};
cvar_t sv_exp_debug_weaponlog_rejections = {"sv_exp_debug_weaponlog_rejections", "0", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy = {"sv_exp_glock_lab_dummy", "0", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_target_profile_name = {"sv_exp_glock_lab_target_profile_name", "default", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_glock_lab_dummy_health = {"sv_exp_glock_lab_dummy_health", "100.0", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_armor = {"sv_exp_glock_lab_dummy_armor", "0.0", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_head_protected = {"sv_exp_glock_lab_dummy_head_protected", "0", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_armor_health_fraction = {"sv_exp_glock_lab_dummy_armor_health_fraction", "0.5", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_armor_drain_scale = {"sv_exp_glock_lab_dummy_armor_drain_scale", "1.0", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_autorespawn = {"sv_exp_glock_lab_dummy_autorespawn", "1", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_respawn_delay = {"sv_exp_glock_lab_dummy_respawn_delay", "1.0", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_spawn_distance = {"sv_exp_glock_lab_dummy_spawn_distance", "256.0", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_model = {"sv_exp_glock_lab_dummy_model", "models/barney.mdl", FCVAR_SERVER | FCVAR_PRINTABLEONLY | FCVAR_NOEXTRAWHITEPACE};
cvar_t sv_exp_glock_lab_dummy_face_player = {"sv_exp_glock_lab_dummy_face_player", "1", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_offset_right = {"sv_exp_glock_lab_dummy_offset_right", "0.0", FCVAR_SERVER};
cvar_t sv_exp_glock_lab_dummy_offset_up = {"sv_exp_glock_lab_dummy_offset_up", "0.0", FCVAR_SERVER};

bool g_futureGameplayCvarsRegistered = false;
bool g_futureGameplayCommandsRegistered = false;
EHANDLE g_glockLabDummy;
EHANDLE g_glockLabDummyAnchorPlayer;
LabDummySavedSpotRecord g_glockLabDummySavedSpots[kMaxLabDummySpotsPerMap] = {};
int g_glockLabDummySavedSpotCount = 0;
char g_glockLabDummyActiveSpotName[kMaxLabDummySpotNameLength] = "";
char g_glockLabDummyTargetSpotsPath[kMaxLiveCfgPathLength] = "";
char g_glockLabDummyTargetSpotsLoadFailure[kMaxLabDummySpotFileFailureLength] = "";
TeamSpawnSavedSpotRecord g_teamSpawnSavedSpots[3][kMaxTeamSpawnSpotsPerTeam] = {};
int g_teamSpawnSavedSpotCounts[3] = {};
char g_teamSpawnActiveSpotNames[3][kMaxLabDummySpotNameLength] = {};
char g_teamSpawnSpotsPath[kMaxLiveCfgPathLength] = "";
char g_teamSpawnSpotsLoadFailure[kMaxLabDummySpotFileFailureLength] = "";
TeamSpawnRuntimeStatus g_teamSpawnLastApplied[3] = {};
TeamSpawnFailureInfo g_teamSpawnLastFailure[3] = {};
char g_teamSpawnLastFailureAt[3][64] = {};
LabDummyTransformMemory g_glockLabDummyLastGoodTransform = {};
LabDummyFailureInfo g_glockLabDummyLastSpawnFailure = {};
char g_glockLabDummyLastFailureAt[64] = "";
bool g_glockLabDummyRespawnPending = false;
float g_glockLabDummyRespawnTime = 0.0f;
float g_glockLabDummyRetryTime = 0.0f;
char g_futureHooksMapName[64] = "";
LiveCfgState g_liveCfgState = {};
ExpRoundRuntimeState g_expRoundState = {};
ExpRoundTeamAssignment g_expRoundTeamAssignments[kMaxRoundTeamPlayerSlots] = {};
ExpRoundFakeClientRecord g_expRoundFakeClientRecords[kMaxRoundTeamPlayerSlots] = {};

void PrintLabDummyStatus();
void PrintRoundStatus();
void PrintTeamStatus();
void PrintTeamSpawnStatus();
void RefreshFutureHooksMapState();
bool IsRoundManagedPlayer(CBasePlayer *pPlayer);
bool IsRoundFakeClient(CBasePlayer *pPlayer);
void UpdateRoundPopulationSnapshot();
void TrimCfgRequestString(const char *input, char *buffer, size_t bufferSize);
bool EnsureLabDummyMonsterSpawningEnabled(LabDummyFailureInfo *failure, const LabDummySpawnSelection *selection);
bool SaveLabDummySpotsForCurrentMap(char *failureReason, size_t failureReasonSize);
void LoadLabDummySpotsForCurrentMap();
bool SaveTeamSpawnSpotsForCurrentMap(char *failureReason, size_t failureReasonSize);
void LoadTeamSpawnSpotsForCurrentMap();
bool TryResolveTeamSpawnSpotName(const char *requestedName, char *buffer, size_t bufferSize, bool useDefaultIfEmpty, char *failureReason, size_t failureReasonSize);
const TeamSpawnSavedSpotRecord *FindTeamSpawnSavedSpotConst(int teamId, const char *spotName);
const TeamSpawnSavedSpotRecord *GetActiveTeamSpawnSavedSpot(int teamId);
const TeamSpawnSavedSpotRecord *GetDefaultTeamSpawnSavedSpot(int teamId);
void BuildTeamSpawnNamesSummary(int teamId, char *buffer, size_t bufferSize);
TeamSpawnSavedSpotRecord *UpsertTeamSpawnSavedSpot(int teamId, const char *spotName, const Vector &origin, const Vector &angles, char *failureReason, size_t failureReasonSize);
bool RemoveTeamSpawnSavedSpot(int teamId, const char *spotName, TeamSpawnSavedSpotRecord *removedSpot, char *failureReason, size_t failureReasonSize);

float GetNonNegativeCvarValue(const cvar_t &cvar)
{
    return cvar.value >= 0.0f ? cvar.value : 0.0f;
}

float GetPositiveOrDefaultCvarValue(const cvar_t &cvar, float fallback)
{
    return cvar.value > 0.0f ? cvar.value : fallback;
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

const char *GetNonEmptyCvarString(const cvar_t &cvar, const char *fallback)
{
    if (cvar.string == NULL || cvar.string[0] == '\0')
    {
        return fallback;
    }

    return cvar.string;
}

const char *GetOptionalCvarString(const cvar_t &cvar)
{
    return cvar.string != NULL ? cvar.string : "";
}

const char *GetCurrentMapName()
{
    if (gpGlobals == NULL || gpGlobals->mapname == 0)
    {
        return "";
    }

    return STRING(gpGlobals->mapname);
}

bool StringEqualsIgnoreCase(const char *left, const char *right)
{
    if (left == NULL || right == NULL)
    {
        return false;
    }

    return _stricmp(left, right) == 0;
}

const char *GetSafePlayerName(CBasePlayer *pPlayer)
{
    if (pPlayer == NULL || pPlayer->pev == NULL || pPlayer->pev->netname == 0)
    {
        return "none";
    }

    const char *name = STRING(pPlayer->pev->netname);
    return name != NULL && name[0] != '\0' ? name : "none";
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

void FormatVector3(char *buffer, size_t bufferSize, const Vector &value)
{
    _snprintf_s(buffer, bufferSize, _TRUNCATE, "%.1f %.1f %.1f", value.x, value.y, value.z);
}

void ClearLabDummyFailureInfo(LabDummyFailureInfo *failure)
{
    if (failure == NULL)
    {
        return;
    }

    failure->code[0] = '\0';
    failure->source[0] = '\0';
    failure->candidate[0] = '\0';
    failure->reason[0] = '\0';
    failure->spotName[0] = '\0';
    failure->spotStorage[0] = '\0';
}

void SetLabDummyFailureInfo(
    LabDummyFailureInfo *failure,
    const char *code,
    const char *source,
    const char *candidate,
    const char *reason,
    const char *spotName = NULL,
    const char *spotStorage = NULL)
{
    if (failure == NULL)
    {
        return;
    }

    strncpy_s(failure->code, sizeof(failure->code), code != NULL ? code : "", _TRUNCATE);
    strncpy_s(failure->source, sizeof(failure->source), source != NULL ? source : "", _TRUNCATE);
    strncpy_s(failure->candidate, sizeof(failure->candidate), candidate != NULL ? candidate : "", _TRUNCATE);
    strncpy_s(failure->reason, sizeof(failure->reason), reason != NULL ? reason : "", _TRUNCATE);
    strncpy_s(failure->spotName, sizeof(failure->spotName), spotName != NULL ? spotName : "", _TRUNCATE);
    strncpy_s(failure->spotStorage, sizeof(failure->spotStorage), spotStorage != NULL ? spotStorage : "", _TRUNCATE);
}

void ClearLabDummyTransformMemory(LabDummyTransformMemory *memory)
{
    if (memory == NULL)
    {
        return;
    }

    memory->valid = false;
    memory->origin = g_vecZero;
    memory->angles = g_vecZero;
    memory->source[0] = '\0';
    memory->candidate[0] = '\0';
}

void StoreLabDummyTransformMemory(LabDummyTransformMemory *memory, const Vector &origin, const Vector &angles, const char *source, const char *candidate)
{
    if (memory == NULL)
    {
        return;
    }

    memory->valid = true;
    memory->origin = origin;
    memory->angles = angles;
    strncpy_s(memory->source, sizeof(memory->source), source != NULL ? source : "", _TRUNCATE);
    strncpy_s(memory->candidate, sizeof(memory->candidate), candidate != NULL ? candidate : "", _TRUNCATE);
}

void ClearLabDummySavedSpotRecord(LabDummySavedSpotRecord *spot)
{
    if (spot == NULL)
    {
        return;
    }

    spot->valid = false;
    spot->loadedFromDisk = false;
    spot->origin = g_vecZero;
    spot->angles = g_vecZero;
    spot->name[0] = '\0';
    spot->candidate[0] = '\0';
    spot->createdAt[0] = '\0';
    spot->updatedAt[0] = '\0';
    spot->note[0] = '\0';
}

void StoreLabDummySavedSpotRecord(
    LabDummySavedSpotRecord *spot,
    const char *name,
    const Vector &origin,
    const Vector &angles,
    const char *candidate,
    const char *createdAt,
    const char *updatedAt,
    const char *note,
    bool loadedFromDisk)
{
    if (spot == NULL)
    {
        return;
    }

    spot->valid = true;
    spot->loadedFromDisk = loadedFromDisk;
    spot->origin = origin;
    spot->angles = angles;
    strncpy_s(spot->name, sizeof(spot->name), name != NULL ? name : "", _TRUNCATE);
    strncpy_s(spot->candidate, sizeof(spot->candidate), candidate != NULL ? candidate : "", _TRUNCATE);
    strncpy_s(spot->createdAt, sizeof(spot->createdAt), createdAt != NULL ? createdAt : "", _TRUNCATE);
    strncpy_s(spot->updatedAt, sizeof(spot->updatedAt), updatedAt != NULL ? updatedAt : "", _TRUNCATE);
    strncpy_s(spot->note, sizeof(spot->note), note != NULL ? note : "", _TRUNCATE);
}

void ClearTeamSpawnSavedSpotRecord(TeamSpawnSavedSpotRecord *spot)
{
    if (spot == NULL)
    {
        return;
    }

    spot->valid = false;
    spot->loadedFromDisk = false;
    spot->teamId = kExpRoundTeamNone;
    spot->origin = g_vecZero;
    spot->angles = g_vecZero;
    spot->name[0] = '\0';
    spot->createdAt[0] = '\0';
    spot->updatedAt[0] = '\0';
    spot->note[0] = '\0';
}

void StoreTeamSpawnSavedSpotRecord(
    TeamSpawnSavedSpotRecord *spot,
    int teamId,
    const char *name,
    const Vector &origin,
    const Vector &angles,
    const char *createdAt,
    const char *updatedAt,
    const char *note,
    bool loadedFromDisk)
{
    if (spot == NULL)
    {
        return;
    }

    spot->valid = true;
    spot->loadedFromDisk = loadedFromDisk;
    spot->teamId = (teamId == kExpRoundTeam1 || teamId == kExpRoundTeam2) ? teamId : kExpRoundTeamNone;
    spot->origin = origin;
    spot->angles = angles;
    strncpy_s(spot->name, sizeof(spot->name), name != NULL ? name : "", _TRUNCATE);
    strncpy_s(spot->createdAt, sizeof(spot->createdAt), createdAt != NULL ? createdAt : "", _TRUNCATE);
    strncpy_s(spot->updatedAt, sizeof(spot->updatedAt), updatedAt != NULL ? updatedAt : "", _TRUNCATE);
    strncpy_s(spot->note, sizeof(spot->note), note != NULL ? note : "", _TRUNCATE);
}

void ClearTeamSpawnFailureInfo(TeamSpawnFailureInfo *failure)
{
    if (failure == NULL)
    {
        return;
    }

    memset(failure, 0, sizeof(*failure));
    failure->teamId = kExpRoundTeamNone;
}

void ClearTeamSpawnRuntimeStatus(TeamSpawnRuntimeStatus *status)
{
    if (status == NULL)
    {
        return;
    }

    memset(status, 0, sizeof(*status));
    status->teamId = kExpRoundTeamNone;
}

void ClearAllLabDummySavedSpots()
{
    for (int spotIndex = 0; spotIndex < ARRAYSIZE(g_glockLabDummySavedSpots); ++spotIndex)
    {
        ClearLabDummySavedSpotRecord(&g_glockLabDummySavedSpots[spotIndex]);
    }

    g_glockLabDummySavedSpotCount = 0;
    g_glockLabDummyActiveSpotName[0] = '\0';
    g_glockLabDummyTargetSpotsPath[0] = '\0';
    g_glockLabDummyTargetSpotsLoadFailure[0] = '\0';
}

void ClearAllTeamSpawnSavedSpots()
{
    for (int teamId = kExpRoundTeam1; teamId <= kExpRoundTeam2; ++teamId)
    {
        for (int spotIndex = 0; spotIndex < ARRAYSIZE(g_teamSpawnSavedSpots[teamId]); ++spotIndex)
        {
            ClearTeamSpawnSavedSpotRecord(&g_teamSpawnSavedSpots[teamId][spotIndex]);
        }

        g_teamSpawnSavedSpotCounts[teamId] = 0;
        g_teamSpawnActiveSpotNames[teamId][0] = '\0';
        ClearTeamSpawnRuntimeStatus(&g_teamSpawnLastApplied[teamId]);
        ClearTeamSpawnFailureInfo(&g_teamSpawnLastFailure[teamId]);
        g_teamSpawnLastFailureAt[teamId][0] = '\0';
    }

    g_teamSpawnSpotsPath[0] = '\0';
    g_teamSpawnSpotsLoadFailure[0] = '\0';
}

void AppendLabDummyFailureAttempt(char *buffer, size_t bufferSize, const LabDummyFailureInfo &failure)
{
    if (buffer == NULL || bufferSize == 0 || failure.reason[0] == '\0')
    {
        return;
    }

    if (buffer[0] != '\0')
    {
        strncat_s(buffer, bufferSize, "; ", _TRUNCATE);
    }

    char attempt[768];
    _snprintf_s(
        attempt,
        sizeof(attempt),
        _TRUNCATE,
        "%s[%s%s%s%s%s]: %s",
        failure.source[0] != '\0' ? failure.source : "target",
        failure.code[0] != '\0' ? failure.code : "failed",
        failure.candidate[0] != '\0' ? "/" : "",
        failure.candidate[0] != '\0' ? failure.candidate : "",
        failure.spotName[0] != '\0' ? " spot=" : "",
        failure.spotName[0] != '\0' ? failure.spotName : "",
        failure.reason);
    strncat_s(buffer, bufferSize, attempt, _TRUNCATE);
}

void PrintLabDummyConsoleLine(const char *format, ...)
{
    char line[512];
    va_list args;
    va_start(args, format);
    _vsnprintf_s(line, sizeof(line), _TRUNCATE, format, args);
    va_end(args);

    ALERT(at_console, "[hl-server] %s\n", line);
    LogLiveLabConsoleMessage(line);
}

void FormatFutureGameplayTimestamp(char *buffer, size_t bufferSize)
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

const char *GetValueOrFallback(const char *value, const char *fallback)
{
    if (value == NULL || value[0] == '\0')
    {
        return fallback;
    }

    return value;
}

void BuildCommandArgumentString(int firstArgIndex, char *buffer, size_t bufferSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return;
    }

    buffer[0] = '\0';
    const int argc = CMD_ARGC();
    for (int argIndex = firstArgIndex; argIndex < argc; ++argIndex)
    {
        if (argIndex > firstArgIndex)
        {
            strncat_s(buffer, bufferSize, " ", _TRUNCATE);
        }

        strncat_s(buffer, bufferSize, CMD_ARGV(argIndex), _TRUNCATE);
    }
}

const char *GetRoundStateName(ExpRoundStateType state)
{
    switch (state)
    {
    case kExpRoundStateWaitingForPlayers:
        return "waiting_for_players";
    case kExpRoundStateFreezeTime:
        return "freeze_time";
    case kExpRoundStateLive:
        return "live";
    case kExpRoundStateRoundEnd:
        return "round_end";
    case kExpRoundStateRestartPending:
        return "restart_pending";
    case kExpRoundStateDisabled:
    default:
        return "disabled";
    }
}

bool TeamRoundModeConfigured()
{
    return sv_exp_team_round_mode.value != 0.0f;
}

bool TeamRoundTeamplayConfigured()
{
    return sv_exp_team_round_teamplay.value != 0.0f;
}

const char *GetConfiguredTeamRoundSpawnMode()
{
    return GetNonEmptyCvarString(sv_exp_team_round_spawn_mode, kDefaultTeamRoundSpawnMode);
}

bool TeamRoundManualSpawnsConfigured()
{
    return StringEqualsIgnoreCase(GetConfiguredTeamRoundSpawnMode(), kManualTeamRoundSpawnMode);
}

bool TeamRoundSpawnModeSupported()
{
    return StringEqualsIgnoreCase(GetConfiguredTeamRoundSpawnMode(), kDefaultTeamRoundSpawnMode) ||
        StringEqualsIgnoreCase(GetConfiguredTeamRoundSpawnMode(), kManualTeamRoundSpawnMode);
}

const char *GetResolvedTeamRoundSpawnMode()
{
    return TeamRoundManualSpawnsConfigured() ? kManualTeamRoundSpawnMode : kDefaultTeamRoundSpawnMode;
}

const char *GetConfiguredTeamRoundName(int teamId)
{
    if (teamId == kExpRoundTeam1)
    {
        return GetNonEmptyCvarString(sv_exp_team_round_team1_name, kDefaultTeamRoundTeam1Name);
    }

    if (teamId == kExpRoundTeam2)
    {
        return GetNonEmptyCvarString(sv_exp_team_round_team2_name, kDefaultTeamRoundTeam2Name);
    }

    return "none";
}

int NormalizeRoundTeamId(int teamId)
{
    if (teamId == kExpRoundTeam1 || teamId == kExpRoundTeam2)
    {
        return teamId;
    }

    return kExpRoundTeamNone;
}

int GetRoundTeamSlot(CBasePlayer *pPlayer)
{
    const int entityIndex = GetPlayerEntityIndex(pPlayer);
    if (entityIndex <= 0 || entityIndex >= kMaxRoundTeamPlayerSlots)
    {
        return 0;
    }

    return entityIndex;
}

void ClearRoundFakeClientRecordSlot(int slot)
{
    if (slot <= 0 || slot >= kMaxRoundTeamPlayerSlots)
    {
        return;
    }

    memset(&g_expRoundFakeClientRecords[slot], 0, sizeof(g_expRoundFakeClientRecords[slot]));
}

void MarkRoundFakeClient(CBasePlayer *pPlayer)
{
    const int slot = GetRoundTeamSlot(pPlayer);
    if (slot <= 0 || pPlayer == NULL)
    {
        return;
    }

    ExpRoundFakeClientRecord &record = g_expRoundFakeClientRecords[slot];
    memset(&record, 0, sizeof(record));
    record.active = true;
    record.userId = GetPlayerUserId(pPlayer);
    strncpy_s(record.playerName, sizeof(record.playerName), GetSafePlayerName(pPlayer), _TRUNCATE);
    if (pPlayer->pev != NULL)
    {
        pPlayer->pev->flags |= (FL_CLIENT | FL_FAKECLIENT);
    }
}

void ClearRoundFakeClientRecord(CBasePlayer *pPlayer)
{
    ClearRoundFakeClientRecordSlot(GetRoundTeamSlot(pPlayer));
}

void ApplyRoundTeamLabel(CBasePlayer *pPlayer, int teamId)
{
    if (pPlayer == NULL)
    {
        return;
    }

    const char *teamName = teamId == kExpRoundTeamNone ? "" : GetConfiguredTeamRoundName(teamId);
    strncpy_s(pPlayer->m_szTeamName, sizeof(pPlayer->m_szTeamName), teamName, _TRUNCATE);

    if (pPlayer->edict() != NULL)
    {
        g_engfuncs.pfnSetClientKeyValue(
            pPlayer->entindex(),
            g_engfuncs.pfnGetInfoKeyBuffer(pPlayer->edict()),
            "team",
            pPlayer->m_szTeamName);
    }
}

void ClearRoundTeamAssignmentSlot(int slot)
{
    if (slot <= 0 || slot >= kMaxRoundTeamPlayerSlots)
    {
        return;
    }

    memset(&g_expRoundTeamAssignments[slot], 0, sizeof(g_expRoundTeamAssignments[slot]));
}

void ResetRoundTeamAssignments()
{
    memset(g_expRoundTeamAssignments, 0, sizeof(g_expRoundTeamAssignments));
}

void AssignRoundTeamToPlayer(CBasePlayer *pPlayer, int teamId)
{
    const int slot = GetRoundTeamSlot(pPlayer);
    teamId = NormalizeRoundTeamId(teamId);
    if (slot <= 0 || pPlayer == NULL)
    {
        return;
    }

    ExpRoundTeamAssignment &assignment = g_expRoundTeamAssignments[slot];
    memset(&assignment, 0, sizeof(assignment));
    assignment.assigned = teamId != kExpRoundTeamNone;
    assignment.teamId = teamId;
    assignment.userId = GetPlayerUserId(pPlayer);
    strncpy_s(assignment.playerName, sizeof(assignment.playerName), GetSafePlayerName(pPlayer), _TRUNCATE);
    ApplyRoundTeamLabel(pPlayer, teamId);
}

void ClearRoundTeamAssignment(CBasePlayer *pPlayer)
{
    const int slot = GetRoundTeamSlot(pPlayer);
    if (slot <= 0)
    {
        return;
    }

    ClearRoundTeamAssignmentSlot(slot);
    ApplyRoundTeamLabel(pPlayer, kExpRoundTeamNone);
}

int GetAssignedRoundTeamId(CBasePlayer *pPlayer)
{
    if (!IsRoundManagedPlayer(pPlayer))
    {
        return kExpRoundTeamNone;
    }

    const int slot = GetRoundTeamSlot(pPlayer);
    if (slot > 0)
    {
        ExpRoundTeamAssignment &assignment = g_expRoundTeamAssignments[slot];
        if (assignment.assigned)
        {
            const int userId = GetPlayerUserId(pPlayer);
            if (assignment.userId <= 0 || userId <= 0 || assignment.userId == userId)
            {
                ApplyRoundTeamLabel(pPlayer, assignment.teamId);
                return NormalizeRoundTeamId(assignment.teamId);
            }

            ClearRoundTeamAssignmentSlot(slot);
        }
    }

    if (StringEqualsIgnoreCase(pPlayer->m_szTeamName, GetConfiguredTeamRoundName(kExpRoundTeam1)))
    {
        return kExpRoundTeam1;
    }

    if (StringEqualsIgnoreCase(pPlayer->m_szTeamName, GetConfiguredTeamRoundName(kExpRoundTeam2)))
    {
        return kExpRoundTeam2;
    }

    return kExpRoundTeamNone;
}

int CountConnectedPlayersOnRoundTeam(int teamId)
{
    if (gpGlobals == NULL)
    {
        return 0;
    }

    int connectedPlayers = 0;
    for (int playerIndex = 1; playerIndex <= gpGlobals->maxClients; ++playerIndex)
    {
        CBaseEntity *pEntity = UTIL_PlayerByIndex(playerIndex);
        if (pEntity == NULL || !pEntity->IsPlayer() || pEntity->pev == NULL)
        {
            continue;
        }

        CBasePlayer *pPlayer = (CBasePlayer *)pEntity;
        if (!IsRoundManagedPlayer(pPlayer))
        {
            continue;
        }

        if (GetAssignedRoundTeamId(pPlayer) == teamId)
        {
            ++connectedPlayers;
        }
    }

    return connectedPlayers;
}

int ChoosePreferredAutoAssignTeam()
{
    const int team1Players = CountConnectedPlayersOnRoundTeam(kExpRoundTeam1);
    const int team2Players = CountConnectedPlayersOnRoundTeam(kExpRoundTeam2);
    return team1Players <= team2Players ? kExpRoundTeam1 : kExpRoundTeam2;
}

void AutoAssignPlayerToRoundTeam(CBasePlayer *pPlayer)
{
    if (!IsRoundManagedPlayer(pPlayer))
    {
        return;
    }

    AssignRoundTeamToPlayer(pPlayer, ChoosePreferredAutoAssignTeam());
}

int AutoAssignRoundTeams(bool reassignAllPlayers)
{
    if (gpGlobals == NULL)
    {
        return 0;
    }

    if (reassignAllPlayers)
    {
        ResetRoundTeamAssignments();
    }

    int assignedPlayers = 0;
    for (int playerIndex = 1; playerIndex <= gpGlobals->maxClients; ++playerIndex)
    {
        CBaseEntity *pEntity = UTIL_PlayerByIndex(playerIndex);
        if (pEntity == NULL || !pEntity->IsPlayer() || pEntity->pev == NULL)
        {
            continue;
        }

        CBasePlayer *pPlayer = (CBasePlayer *)pEntity;
        if (!IsRoundManagedPlayer(pPlayer))
        {
            continue;
        }

        if (!reassignAllPlayers && GetAssignedRoundTeamId(pPlayer) != kExpRoundTeamNone)
        {
            continue;
        }

        AutoAssignPlayerToRoundTeam(pPlayer);
        ++assignedPlayers;
    }

    return assignedPlayers;
}

bool TryResolveRoundTeamId(const char *requestedTeam, int *pTeamId)
{
    if (pTeamId == NULL)
    {
        return false;
    }

    *pTeamId = kExpRoundTeamNone;

    char trimmedToken[64];
    TrimCfgRequestString(requestedTeam, trimmedToken, sizeof(trimmedToken));
    if (trimmedToken[0] == '\0')
    {
        return false;
    }

    if (StringEqualsIgnoreCase(trimmedToken, "1") ||
        StringEqualsIgnoreCase(trimmedToken, "team1") ||
        StringEqualsIgnoreCase(trimmedToken, GetConfiguredTeamRoundName(kExpRoundTeam1)))
    {
        *pTeamId = kExpRoundTeam1;
        return true;
    }

    if (StringEqualsIgnoreCase(trimmedToken, "2") ||
        StringEqualsIgnoreCase(trimmedToken, "team2") ||
        StringEqualsIgnoreCase(trimmedToken, GetConfiguredTeamRoundName(kExpRoundTeam2)))
    {
        *pTeamId = kExpRoundTeam2;
        return true;
    }

    return false;
}

const char *GetRoundTeamBucketKey(int teamId)
{
    if (teamId == kExpRoundTeam1)
    {
        return "team1";
    }

    if (teamId == kExpRoundTeam2)
    {
        return "team2";
    }

    return "none";
}

const char *GetTeamSpawnSpotStorageLabel(const TeamSpawnSavedSpotRecord *spot)
{
    if (spot == NULL || !spot->valid)
    {
        return "";
    }

    return spot->loadedFromDisk ? kTeamSpawnSpotStorageDisk : kTeamSpawnSpotStorageSession;
}

CBasePlayer *FindFirstManagedPlayerOnRoundTeam(int teamId, bool requireAlive)
{
    if (gpGlobals == NULL)
    {
        return NULL;
    }

    for (int playerIndex = 1; playerIndex <= gpGlobals->maxClients; ++playerIndex)
    {
        CBaseEntity *pEntity = UTIL_PlayerByIndex(playerIndex);
        if (pEntity == NULL || !pEntity->IsPlayer() || pEntity->pev == NULL)
        {
            continue;
        }

        CBasePlayer *pPlayer = (CBasePlayer *)pEntity;
        if (!IsRoundManagedPlayer(pPlayer) || GetAssignedRoundTeamId(pPlayer) != teamId)
        {
            continue;
        }

        if (requireAlive && (!pPlayer->IsAlive() || pPlayer->pev->deadflag != DEAD_NO))
        {
            continue;
        }

        return pPlayer;
    }

    return NULL;
}

bool TryResolveRoundPlayerToken(const char *requestedPlayer, CBasePlayer **ppPlayer, char *failureReason, size_t failureReasonSize)
{
    if (ppPlayer == NULL)
    {
        return false;
    }

    *ppPlayer = NULL;
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    if (gpGlobals == NULL)
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            strcpy_s(failureReason, failureReasonSize, "player list is unavailable");
        }
        return false;
    }

    char trimmedToken[64];
    TrimCfgRequestString(requestedPlayer, trimmedToken, sizeof(trimmedToken));
    if (trimmedToken[0] == '\0')
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            strcpy_s(failureReason, failureReasonSize, "player token cannot be empty");
        }
        return false;
    }

    const int numericToken = atoi(trimmedToken);
    CBasePlayer *pNameMatch = NULL;
    int partialMatches = 0;
    for (int playerIndex = 1; playerIndex <= gpGlobals->maxClients; ++playerIndex)
    {
        CBaseEntity *pEntity = UTIL_PlayerByIndex(playerIndex);
        if (pEntity == NULL || !pEntity->IsPlayer() || pEntity->pev == NULL)
        {
            continue;
        }

        CBasePlayer *pPlayer = (CBasePlayer *)pEntity;
        if (!IsRoundManagedPlayer(pPlayer))
        {
            continue;
        }

        if (numericToken > 0 &&
            (GetPlayerEntityIndex(pPlayer) == numericToken || GetPlayerUserId(pPlayer) == numericToken))
        {
            *ppPlayer = pPlayer;
            return true;
        }

        const char *playerName = GetSafePlayerName(pPlayer);
        if (StringEqualsIgnoreCase(playerName, trimmedToken))
        {
            *ppPlayer = pPlayer;
            return true;
        }

        if (_strnicmp(playerName, trimmedToken, strlen(trimmedToken)) == 0)
        {
            pNameMatch = pPlayer;
            ++partialMatches;
        }
    }

    if (partialMatches == 1 && pNameMatch != NULL)
    {
        *ppPlayer = pNameMatch;
        return true;
    }

    if (failureReason != NULL && failureReasonSize > 0)
    {
        _snprintf_s(
            failureReason,
            failureReasonSize,
            _TRUNCATE,
            partialMatches > 1 ? "player token \"%s\" matched multiple players" : "no player matched \"%s\"",
            trimmedToken);
    }

    return false;
}

void ClearRoundRuntimeState()
{
    memset(&g_expRoundState, 0, sizeof(g_expRoundState));
    g_expRoundState.state = kExpRoundStateDisabled;
}

bool IsRoundManagedPlayer(CBasePlayer *pPlayer)
{
    return pPlayer != NULL &&
        pPlayer->pev != NULL &&
        pPlayer->edict() != NULL &&
        pPlayer->IsObserver() == 0 &&
        pPlayer->pev->netname != 0 &&
        STRING(pPlayer->pev->netname)[0] != '\0';
}

bool IsRoundFakeClient(CBasePlayer *pPlayer)
{
    if (pPlayer == NULL)
    {
        return false;
    }

    const int slot = GetRoundTeamSlot(pPlayer);
    if (slot > 0)
    {
        ExpRoundFakeClientRecord &record = g_expRoundFakeClientRecords[slot];
        if (record.active)
        {
            const int userId = GetPlayerUserId(pPlayer);
            if ((record.userId <= 0 || userId <= 0 || record.userId == userId) &&
                (record.playerName[0] == '\0' || StringEqualsIgnoreCase(record.playerName, GetSafePlayerName(pPlayer))))
            {
                if (pPlayer->pev != NULL)
                {
                    pPlayer->pev->flags |= (FL_CLIENT | FL_FAKECLIENT);
                }
                return true;
            }

            ClearRoundFakeClientRecordSlot(slot);
        }
    }

    return pPlayer->pev != NULL && FBitSet(pPlayer->pev->flags, FL_FAKECLIENT);
}

bool IsRoundPlayerNameTaken(const char *playerName)
{
    if (gpGlobals == NULL || playerName == NULL || playerName[0] == '\0')
    {
        return false;
    }

    for (int playerIndex = 1; playerIndex <= gpGlobals->maxClients; ++playerIndex)
    {
        CBaseEntity *pEntity = UTIL_PlayerByIndex(playerIndex);
        if (pEntity == NULL || !pEntity->IsPlayer() || pEntity->pev == NULL)
        {
            continue;
        }

        CBasePlayer *pPlayer = (CBasePlayer *)pEntity;
        if (!IsRoundManagedPlayer(pPlayer))
        {
            continue;
        }

        if (StringEqualsIgnoreCase(GetSafePlayerName(pPlayer), playerName))
        {
            return true;
        }
    }

    return false;
}

void SanitizeRoundFakeClientName(const char *input, const char *fallbackName, char *buffer, size_t bufferSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return;
    }

    TrimCfgRequestString(input, buffer, bufferSize);
    if (buffer[0] == '\0' && fallbackName != NULL)
    {
        strncpy_s(buffer, bufferSize, fallbackName, _TRUNCATE);
    }

    for (char *cursor = buffer; *cursor != '\0'; ++cursor)
    {
        const unsigned char ch = (unsigned char)(*cursor);
        if (!isalnum(ch) && *cursor != '_' && *cursor != '-')
        {
            *cursor = '_';
        }
    }

    if (buffer[0] == '\0')
    {
        strncpy_s(buffer, bufferSize, "round_fake", _TRUNCATE);
    }
}

bool BuildUniqueRoundFakeClientName(
    int teamId,
    const char *requestedName,
    char *buffer,
    size_t bufferSize,
    char *failureReason,
    size_t failureReasonSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return false;
    }

    buffer[0] = '\0';
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    char fallbackName[kMaxRoundFakeClientNameLength];
    _snprintf_s(
        fallbackName,
        sizeof(fallbackName),
        _TRUNCATE,
        "%s_fake",
        teamId == kExpRoundTeam2 ? GetConfiguredTeamRoundName(kExpRoundTeam2) : GetConfiguredTeamRoundName(kExpRoundTeam1));

    char baseName[kMaxRoundFakeClientNameLength];
    SanitizeRoundFakeClientName(requestedName, fallbackName, baseName, sizeof(baseName));
    if (!IsRoundPlayerNameTaken(baseName))
    {
        strncpy_s(buffer, bufferSize, baseName, _TRUNCATE);
        return true;
    }

    for (int suffix = 2; suffix <= 32; ++suffix)
    {
        char candidateName[kMaxRoundFakeClientNameLength];
        _snprintf_s(candidateName, sizeof(candidateName), _TRUNCATE, "%s_%d", baseName, suffix);
        if (!IsRoundPlayerNameTaken(candidateName))
        {
            strncpy_s(buffer, bufferSize, candidateName, _TRUNCATE);
            return true;
        }
    }

    if (failureReason != NULL && failureReasonSize > 0)
    {
        strcpy_s(failureReason, failureReasonSize, "could not find an available fake player name");
    }

    return false;
}

bool CreateRoundFakeClient(
    int teamId,
    const char *requestedName,
    char *createdName,
    size_t createdNameSize,
    char *failureReason,
    size_t failureReasonSize)
{
    if (createdName != NULL && createdNameSize > 0)
    {
        createdName[0] = '\0';
    }

    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    if (gpGlobals == NULL)
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            strcpy_s(failureReason, failureReasonSize, "global state is unavailable");
        }
        return false;
    }

    if (g_engfuncs.pfnCreateFakeClient == NULL)
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            strcpy_s(failureReason, failureReasonSize, "pfnCreateFakeClient is unavailable in this engine build");
        }
        return false;
    }

    char fakeName[kMaxRoundFakeClientNameLength];
    if (!BuildUniqueRoundFakeClientName(teamId, requestedName, fakeName, sizeof(fakeName), failureReason, failureReasonSize))
    {
        return false;
    }

    edict_t *pEdict = g_engfuncs.pfnCreateFakeClient(fakeName);
    if (FNullEnt(pEdict))
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            strcpy_s(failureReason, failureReasonSize, "engine refused to create a fake client");
        }
        return false;
    }

    FREE_PRIVATE(pEdict);
    CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)VARS(pEdict));
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        SERVER_COMMAND(UTIL_VarArgs("kick \"%s\"\n", fakeName));
        SERVER_EXECUTE();
        if (failureReason != NULL && failureReasonSize > 0)
        {
            strcpy_s(failureReason, failureReasonSize, "failed to allocate player state for the fake client");
        }
        return false;
    }

    char *infoBuffer = g_engfuncs.pfnGetInfoKeyBuffer(pEdict);
    if (infoBuffer != NULL)
    {
        g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pEdict), infoBuffer, "model", (char *)"gordon");
        g_engfuncs.pfnSetClientKeyValue(ENTINDEX(pEdict), infoBuffer, "team", (char *)"");
    }

    pPlayer->pev->flags |= (FL_CLIENT | FL_FAKECLIENT);

    char rejectReason[128] = "";
    if (!ClientConnect(pEdict, fakeName, "127.0.0.1", rejectReason))
    {
        SERVER_COMMAND(UTIL_VarArgs("kick \"%s\"\n", fakeName));
        SERVER_EXECUTE();
        if (failureReason != NULL && failureReasonSize > 0)
        {
            if (rejectReason[0] != '\0')
            {
                strncpy_s(failureReason, failureReasonSize, rejectReason, _TRUNCATE);
            }
            else
            {
                strcpy_s(failureReason, failureReasonSize, "ClientConnect rejected the fake client");
            }
        }
        return false;
    }

    ClientPutInServer(pEdict);
    pPlayer = (CBasePlayer *)CBaseEntity::Instance(pEdict);
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        SERVER_COMMAND(UTIL_VarArgs("kick \"%s\"\n", fakeName));
        SERVER_EXECUTE();
        if (failureReason != NULL && failureReasonSize > 0)
        {
            strcpy_s(failureReason, failureReasonSize, "fake client player entity did not initialize");
        }
        return false;
    }

    pPlayer->pev->flags |= (FL_CLIENT | FL_FAKECLIENT);
    if (g_pGameRules != NULL)
    {
        g_pGameRules->InitHUD(pPlayer);
    }
    pPlayer->m_fInitHUD = FALSE;
    pPlayer->m_fGameHUDInitialized = TRUE;
    MarkRoundFakeClient(pPlayer);
    AssignRoundTeamToPlayer(pPlayer, teamId);
    UpdateRoundPopulationSnapshot();

    if (createdName != NULL && createdNameSize > 0)
    {
        strncpy_s(createdName, createdNameSize, fakeName, _TRUNCATE);
    }

    return true;
}

int KickAllRoundFakeClients()
{
    int kickedClients = 0;
    if (gpGlobals == NULL)
    {
        return kickedClients;
    }

    for (int playerIndex = 1; playerIndex <= gpGlobals->maxClients; ++playerIndex)
    {
        CBaseEntity *pEntity = UTIL_PlayerByIndex(playerIndex);
        if (pEntity == NULL || !pEntity->IsPlayer() || pEntity->pev == NULL)
        {
            continue;
        }

        CBasePlayer *pPlayer = (CBasePlayer *)pEntity;
        if (!IsRoundManagedPlayer(pPlayer) || !IsRoundFakeClient(pPlayer))
        {
            continue;
        }

        SERVER_COMMAND(UTIL_VarArgs("kick \"%s\"\n", GetSafePlayerName(pPlayer)));
        ++kickedClients;
    }

    if (kickedClients > 0)
    {
        SERVER_EXECUTE();
    }

    return kickedClients;
}

ExpRoundPlayerSnapshot CollectRoundPlayerSnapshot()
{
    ExpRoundPlayerSnapshot snapshot = {};
    if (gpGlobals == NULL)
    {
        return snapshot;
    }

    for (int playerIndex = 1; playerIndex <= gpGlobals->maxClients; ++playerIndex)
    {
        CBaseEntity *pEntity = UTIL_PlayerByIndex(playerIndex);
        if (pEntity == NULL || !pEntity->IsPlayer() || pEntity->pev == NULL)
        {
            continue;
        }

        CBasePlayer *pPlayer = (CBasePlayer *)pEntity;
        if (!IsRoundManagedPlayer(pPlayer))
        {
            continue;
        }

        ++snapshot.connectedPlayers;
        const int teamId = GetAssignedRoundTeamId(pPlayer);
        if (teamId == kExpRoundTeam1)
        {
            ++snapshot.team1ConnectedPlayers;
        }
        else if (teamId == kExpRoundTeam2)
        {
            ++snapshot.team2ConnectedPlayers;
        }
        else
        {
            ++snapshot.unassignedConnectedPlayers;
        }

        if (pPlayer->IsAlive() && pPlayer->pev->deadflag == DEAD_NO)
        {
            ++snapshot.alivePlayers;
            snapshot.lastAlivePlayer = pPlayer;
            if (teamId == kExpRoundTeam1)
            {
                ++snapshot.team1AlivePlayers;
                snapshot.lastAliveTeam1Player = pPlayer;
            }
            else if (teamId == kExpRoundTeam2)
            {
                ++snapshot.team2AlivePlayers;
                snapshot.lastAliveTeam2Player = pPlayer;
            }
            else
            {
                ++snapshot.unassignedAlivePlayers;
            }
        }
    }

    return snapshot;
}

void UpdateRoundPopulationSnapshot()
{
    const ExpRoundPlayerSnapshot snapshot = CollectRoundPlayerSnapshot();
    g_expRoundState.connectedPlayers = snapshot.connectedPlayers;
    g_expRoundState.alivePlayers = snapshot.alivePlayers;
    g_expRoundState.team1ConnectedPlayers = snapshot.team1ConnectedPlayers;
    g_expRoundState.team2ConnectedPlayers = snapshot.team2ConnectedPlayers;
    g_expRoundState.unassignedConnectedPlayers = snapshot.unassignedConnectedPlayers;
    g_expRoundState.team1AlivePlayers = snapshot.team1AlivePlayers;
    g_expRoundState.team2AlivePlayers = snapshot.team2AlivePlayers;
    g_expRoundState.unassignedAlivePlayers = snapshot.unassignedAlivePlayers;
}

void SetRoundState(ExpRoundStateType state, float transitionDelaySeconds)
{
    g_expRoundState.state = state;
    g_expRoundState.stateEnteredAt = gpGlobals != NULL ? gpGlobals->time : 0.0f;
    g_expRoundState.nextTransitionAt = transitionDelaySeconds > 0.0f ? g_expRoundState.stateEnteredAt + transitionDelaySeconds : 0.0f;
    UpdateRoundPopulationSnapshot();
}

void StoreRoundWinner(CBasePlayer *pWinner, int winnerTeamId, const char *reason)
{
    g_expRoundState.lastWinnerName[0] = '\0';
    g_expRoundState.lastWinnerEntIndex = 0;
    g_expRoundState.lastWinnerUserId = 0;
    g_expRoundState.lastWinnerTeamId = kExpRoundTeamNone;
    g_expRoundState.lastWinnerTeamName[0] = '\0';
    g_expRoundState.lastEndReason[0] = '\0';

    if (pWinner != NULL)
    {
        strncpy_s(g_expRoundState.lastWinnerName, sizeof(g_expRoundState.lastWinnerName), GetSafePlayerName(pWinner), _TRUNCATE);
        g_expRoundState.lastWinnerEntIndex = pWinner->edict() != NULL ? ENTINDEX(pWinner->edict()) : 0;
        g_expRoundState.lastWinnerUserId = GetPlayerUserId(pWinner);
        if (winnerTeamId == kExpRoundTeamNone)
        {
            winnerTeamId = GetAssignedRoundTeamId(pWinner);
        }
    }

    winnerTeamId = NormalizeRoundTeamId(winnerTeamId);
    if (winnerTeamId != kExpRoundTeamNone)
    {
        g_expRoundState.lastWinnerTeamId = winnerTeamId;
        strncpy_s(
            g_expRoundState.lastWinnerTeamName,
            sizeof(g_expRoundState.lastWinnerTeamName),
            GetConfiguredTeamRoundName(winnerTeamId),
            _TRUNCATE);
    }

    if (reason != NULL)
    {
        strncpy_s(g_expRoundState.lastEndReason, sizeof(g_expRoundState.lastEndReason), reason, _TRUNCATE);
    }
}

bool RoundTeamsReady(const ExpRoundPlayerSnapshot &snapshot)
{
    if (!TeamRoundModeConfigured())
    {
        return snapshot.connectedPlayers > 0;
    }

    return snapshot.team1ConnectedPlayers > 0 && snapshot.team2ConnectedPlayers > 0;
}

const char *GetRoundWaitingReasonForSnapshot(const ExpRoundPlayerSnapshot &snapshot)
{
    if (snapshot.connectedPlayers <= 0)
    {
        return "no_players";
    }

    if (TeamRoundModeConfigured() && !RoundTeamsReady(snapshot))
    {
        return kRoundEndReasonTeamsIncomplete;
    }

    return "players_ready";
}

const char *GetConfiguredRoundLoadoutMode()
{
    return GetNonEmptyCvarString(sv_exp_round_loadout_mode, "none");
}

bool IsSupportedRoundLoadoutMode(const char *mode)
{
    return StringEqualsIgnoreCase(mode, "none") ||
        StringEqualsIgnoreCase(mode, "glock") ||
        StringEqualsIgnoreCase(mode, "mp5") ||
        StringEqualsIgnoreCase(mode, "357") ||
        StringEqualsIgnoreCase(mode, "shotgun");
}

const char *GetResolvedRoundLoadoutMode()
{
    const char *configuredMode = GetConfiguredRoundLoadoutMode();
    if (StringEqualsIgnoreCase(configuredMode, "glock"))
    {
        return "glock";
    }

    if (StringEqualsIgnoreCase(configuredMode, "mp5"))
    {
        return "mp5";
    }

    if (StringEqualsIgnoreCase(configuredMode, "357"))
    {
        return "357";
    }

    if (StringEqualsIgnoreCase(configuredMode, "shotgun"))
    {
        return "shotgun";
    }

    return "none";
}

const char *GetConfiguredTeamRoundLoadoutMode(int teamId)
{
    if (teamId == kExpRoundTeam1)
    {
        return GetOptionalCvarString(sv_exp_team_round_team1_loadout);
    }

    if (teamId == kExpRoundTeam2)
    {
        return GetOptionalCvarString(sv_exp_team_round_team2_loadout);
    }

    return "";
}

const char *GetResolvedRoundLoadoutModeForTeam(int teamId)
{
    const char *configuredTeamLoadout = GetConfiguredTeamRoundLoadoutMode(teamId);
    if (configuredTeamLoadout[0] == '\0')
    {
        return GetResolvedRoundLoadoutMode();
    }

    if (StringEqualsIgnoreCase(configuredTeamLoadout, "glock"))
    {
        return "glock";
    }

    if (StringEqualsIgnoreCase(configuredTeamLoadout, "mp5"))
    {
        return "mp5";
    }

    if (StringEqualsIgnoreCase(configuredTeamLoadout, "357"))
    {
        return "357";
    }

    if (StringEqualsIgnoreCase(configuredTeamLoadout, "shotgun"))
    {
        return "shotgun";
    }

    if (StringEqualsIgnoreCase(configuredTeamLoadout, "none"))
    {
        return "none";
    }

    return GetResolvedRoundLoadoutMode();
}

float GetResolvedRoundStartHealthForTeam(int teamId)
{
    if (teamId == kExpRoundTeam1 && sv_exp_team_round_team1_health.value > 0.0f)
    {
        return sv_exp_team_round_team1_health.value;
    }

    if (teamId == kExpRoundTeam2 && sv_exp_team_round_team2_health.value > 0.0f)
    {
        return sv_exp_team_round_team2_health.value;
    }

    return ExpRoundStartHealth();
}

float GetResolvedRoundStartArmorForTeam(int teamId)
{
    if (teamId == kExpRoundTeam1 && sv_exp_team_round_team1_armor.value >= 0.0f)
    {
        return sv_exp_team_round_team1_armor.value;
    }

    if (teamId == kExpRoundTeam2 && sv_exp_team_round_team2_armor.value >= 0.0f)
    {
        return sv_exp_team_round_team2_armor.value;
    }

    return ExpRoundStartArmor();
}

const char *GetResolvedRoundLoadoutModeForPlayer(CBasePlayer *pPlayer)
{
    if (TeamRoundModeConfigured())
    {
        return GetResolvedRoundLoadoutModeForTeam(GetAssignedRoundTeamId(pPlayer));
    }

    return GetResolvedRoundLoadoutMode();
}

float GetResolvedRoundStartHealthForPlayer(CBasePlayer *pPlayer)
{
    if (TeamRoundModeConfigured())
    {
        return GetResolvedRoundStartHealthForTeam(GetAssignedRoundTeamId(pPlayer));
    }

    return ExpRoundStartHealth();
}

float GetResolvedRoundStartArmorForPlayer(CBasePlayer *pPlayer)
{
    if (TeamRoundModeConfigured())
    {
        return GetResolvedRoundStartArmorForTeam(GetAssignedRoundTeamId(pPlayer));
    }

    return ExpRoundStartArmor();
}

void GiveRoundAmmo(CBasePlayer *pPlayer, const char *ammoName, int amount, int maxCarry)
{
    if (pPlayer == NULL || amount <= 0)
    {
        return;
    }

    pPlayer->GiveAmmo(amount, (char *)ammoName, maxCarry);
}

void ApplyRoundResetToPlayer(CBasePlayer *pPlayer)
{
    if (!IsRoundManagedPlayer(pPlayer))
    {
        return;
    }

    const char *loadoutMode = GetResolvedRoundLoadoutModeForPlayer(pPlayer);
    const bool customLoadout = !StringEqualsIgnoreCase(loadoutMode, "none");
    const int savedAutoSwitch = pPlayer->m_iAutoWepSwitch;
    pPlayer->m_iAutoWepSwitch = 1;

    if (customLoadout)
    {
        pPlayer->RemoveAllItems(FALSE);
        pPlayer->pev->weapons |= (1 << WEAPON_SUIT);
        pPlayer->GiveNamedItem("weapon_crowbar");

        if (StringEqualsIgnoreCase(loadoutMode, "glock"))
        {
            pPlayer->GiveNamedItem("weapon_9mmhandgun");
            GiveRoundAmmo(pPlayer, "9mm", 68, _9MM_MAX_CARRY);
            pPlayer->SelectItem("weapon_9mmhandgun");
        }
        else if (StringEqualsIgnoreCase(loadoutMode, "mp5"))
        {
            pPlayer->GiveNamedItem("weapon_9mmAR");
            GiveRoundAmmo(pPlayer, "9mm", (int)ClampFloat(ExpMP5LabAmmo(), 0.0f, (float)_9MM_MAX_CARRY), _9MM_MAX_CARRY);
            pPlayer->SelectItem("weapon_9mmAR");
        }
        else if (StringEqualsIgnoreCase(loadoutMode, "357"))
        {
            pPlayer->GiveNamedItem("weapon_357");
            GiveRoundAmmo(pPlayer, "357", (int)ClampFloat(Exp357LabAmmo(), 0.0f, (float)_357_MAX_CARRY), _357_MAX_CARRY);
            pPlayer->SelectItem("weapon_357");
        }
        else if (StringEqualsIgnoreCase(loadoutMode, "shotgun"))
        {
            pPlayer->GiveNamedItem("weapon_shotgun");
            GiveRoundAmmo(pPlayer, "buckshot", (int)ClampFloat(ExpShotgunLabAmmo(), 0.0f, (float)BUCKSHOT_MAX_CARRY), BUCKSHOT_MAX_CARRY);
            pPlayer->SelectItem("weapon_shotgun");
        }
    }

    const float startHealth = GetResolvedRoundStartHealthForPlayer(pPlayer);
    pPlayer->pev->health = startHealth;
    pPlayer->pev->max_health = startHealth;
    pPlayer->pev->armorvalue = GetResolvedRoundStartArmorForPlayer(pPlayer);
    pPlayer->m_iAutoWepSwitch = savedAutoSwitch;
}

void RespawnPlayerForRound(CBasePlayer *pPlayer)
{
    if (!IsRoundManagedPlayer(pPlayer))
    {
        return;
    }

    if (pPlayer->IsAlive() && pPlayer->pev->deadflag == DEAD_NO)
    {
        pPlayer->Spawn();
        return;
    }

    respawn(pPlayer->pev, FALSE);
}

void RespawnAllPlayersForRound()
{
    if (gpGlobals == NULL)
    {
        return;
    }

    g_expRoundState.applyingRoundReset = true;
    for (int playerIndex = 1; playerIndex <= gpGlobals->maxClients; ++playerIndex)
    {
        CBaseEntity *pEntity = UTIL_PlayerByIndex(playerIndex);
        if (pEntity == NULL || !pEntity->IsPlayer())
        {
            continue;
        }

        RespawnPlayerForRound((CBasePlayer *)pEntity);
    }
    g_expRoundState.applyingRoundReset = false;
    UpdateRoundPopulationSnapshot();
}

void RespawnDeadPlayersForDeathmatch()
{
    if (gpGlobals == NULL)
    {
        return;
    }

    for (int playerIndex = 1; playerIndex <= gpGlobals->maxClients; ++playerIndex)
    {
        CBaseEntity *pEntity = UTIL_PlayerByIndex(playerIndex);
        if (pEntity == NULL || !pEntity->IsPlayer() || pEntity->pev == NULL)
        {
            continue;
        }

        CBasePlayer *pPlayer = (CBasePlayer *)pEntity;
        if (!IsRoundManagedPlayer(pPlayer))
        {
            continue;
        }

        if (!pPlayer->IsAlive() || pPlayer->pev->deadflag != DEAD_NO)
        {
            respawn(pPlayer->pev, FALSE);
        }
    }
}

int SlayRoundPlayers(bool slayAllPlayers, int teamFilter)
{
    if (gpGlobals == NULL)
    {
        return 0;
    }

    int killedPlayers = 0;
    for (int playerIndex = 1; playerIndex <= gpGlobals->maxClients; ++playerIndex)
    {
        CBaseEntity *pEntity = UTIL_PlayerByIndex(playerIndex);
        if (pEntity == NULL || !pEntity->IsPlayer() || pEntity->pev == NULL)
        {
            continue;
        }

        CBasePlayer *pPlayer = (CBasePlayer *)pEntity;
        if (!IsRoundManagedPlayer(pPlayer) || !pPlayer->IsAlive() || pPlayer->pev->deadflag != DEAD_NO)
        {
            continue;
        }

        if (teamFilter != kExpRoundTeamNone && GetAssignedRoundTeamId(pPlayer) != teamFilter)
        {
            continue;
        }

        const float forcedDamage = pPlayer->pev->health + pPlayer->pev->armorvalue + 50.0f;
        pPlayer->TakeDamage(pPlayer->pev, pPlayer->pev, forcedDamage, DMG_GENERIC);
        ++killedPlayers;

        if (!slayAllPlayers)
        {
            break;
        }
    }

    return killedPlayers;
}

void ApplyRoundFriendlyFireSetting()
{
    CVAR_SET_FLOAT("mp_friendlyfire", ExpRoundFriendlyFireEnabled() ? 1.0f : 0.0f);
}

void BeginRoundWaiting(const char *reason, bool printMessage)
{
    SetRoundState(kExpRoundStateWaitingForPlayers, 0.0f);
    if (!printMessage)
    {
        return;
    }

    PrintLabDummyConsoleLine(
        "round mode waiting_for_players: connected=%d alive=%d reason=%s loadout=%s profile=%s",
        g_expRoundState.connectedPlayers,
        g_expRoundState.alivePlayers,
        GetValueOrFallback(reason, "waiting_for_players"),
        GetConfiguredRoundLoadoutMode(),
        ExpRoundWeaponProfile()[0] != '\0' ? ExpRoundWeaponProfile() : "none");

    if (TeamRoundModeConfigured())
    {
        PrintLabDummyConsoleLine(
            "team waiting detail: %s=%d/%d %s=%d/%d unassigned=%d/%d spawn_mode=%s resolved_spawn=%s",
            GetConfiguredTeamRoundName(kExpRoundTeam1),
            g_expRoundState.team1ConnectedPlayers,
            g_expRoundState.team1AlivePlayers,
            GetConfiguredTeamRoundName(kExpRoundTeam2),
            g_expRoundState.team2ConnectedPlayers,
            g_expRoundState.team2AlivePlayers,
            g_expRoundState.unassignedConnectedPlayers,
            g_expRoundState.unassignedAlivePlayers,
            GetConfiguredTeamRoundSpawnMode(),
            GetResolvedTeamRoundSpawnMode());

        if (!TeamRoundSpawnModeSupported())
        {
            PrintLabDummyConsoleLine(
                "team spawn note: configured spawn_mode \"%s\" is unsupported; falling back to dm_spawns.",
                GetConfiguredTeamRoundSpawnMode());
        }
    }
}

void BeginRoundFreeze(bool emitRestartEvent, const char *reason)
{
    if (TeamRoundModeConfigured())
    {
        AutoAssignRoundTeams(false);
    }

    const ExpRoundPlayerSnapshot snapshot = CollectRoundPlayerSnapshot();
    if (!RoundTeamsReady(snapshot))
    {
        BeginRoundWaiting(GetRoundWaitingReasonForSnapshot(snapshot), true);
        return;
    }

    if (emitRestartEvent)
    {
        LogRoundEvent("round_restart", GetRoundStateName(g_expRoundState.state), g_expRoundState.roundNumber, snapshot.connectedPlayers, snapshot.alivePlayers, NULL, reason);
    }

    ApplyRoundFriendlyFireSetting();
    ++g_expRoundState.roundNumber;
    RespawnAllPlayersForRound();
    SetRoundState(kExpRoundStateFreezeTime, ExpRoundFreezeTimeSeconds());
    PrintLabDummyConsoleLine(
        "round %d freeze time: connected=%d alive=%d loadout=%s profile=%s health=%.1f armor=%.1f live_in=%.1fs",
        g_expRoundState.roundNumber,
        g_expRoundState.connectedPlayers,
        g_expRoundState.alivePlayers,
        TeamRoundModeConfigured() ? "per_team" : GetConfiguredRoundLoadoutMode(),
        ExpRoundWeaponProfile()[0] != '\0' ? ExpRoundWeaponProfile() : "none",
        ExpRoundStartHealth(),
        ExpRoundStartArmor(),
        ExpRoundFreezeTimeSeconds());

    if (TeamRoundModeConfigured())
    {
        PrintLabDummyConsoleLine(
            "team freeze detail: %s=%d/%d loadout=%s health=%.1f armor=%.1f | %s=%d/%d loadout=%s health=%.1f armor=%.1f",
            GetConfiguredTeamRoundName(kExpRoundTeam1),
            g_expRoundState.team1ConnectedPlayers,
            g_expRoundState.team1AlivePlayers,
            GetResolvedRoundLoadoutModeForTeam(kExpRoundTeam1),
            GetResolvedRoundStartHealthForTeam(kExpRoundTeam1),
            GetResolvedRoundStartArmorForTeam(kExpRoundTeam1),
            GetConfiguredTeamRoundName(kExpRoundTeam2),
            g_expRoundState.team2ConnectedPlayers,
            g_expRoundState.team2AlivePlayers,
            GetResolvedRoundLoadoutModeForTeam(kExpRoundTeam2),
            GetResolvedRoundStartHealthForTeam(kExpRoundTeam2),
            GetResolvedRoundStartArmorForTeam(kExpRoundTeam2));
    }

    LogRoundEvent("round_start", GetRoundStateName(g_expRoundState.state), g_expRoundState.roundNumber, g_expRoundState.connectedPlayers, g_expRoundState.alivePlayers, NULL, reason);
}

void BeginRoundLive(const char *reason)
{
    SetRoundState(kExpRoundStateLive, 0.0f);
    PrintLabDummyConsoleLine(
        "round %d is now live: connected=%d alive=%d loadout=%s no_respawn=%s",
        g_expRoundState.roundNumber,
        g_expRoundState.connectedPlayers,
        g_expRoundState.alivePlayers,
        TeamRoundModeConfigured() ? "per_team" : GetConfiguredRoundLoadoutMode(),
        ExpRoundNoRespawn() ? "yes" : "no");

    if (TeamRoundModeConfigured())
    {
        PrintLabDummyConsoleLine(
            "team live detail: %s alive=%d %s alive=%d unassigned=%d teamplay=%s",
            GetConfiguredTeamRoundName(kExpRoundTeam1),
            g_expRoundState.team1AlivePlayers,
            GetConfiguredTeamRoundName(kExpRoundTeam2),
            g_expRoundState.team2AlivePlayers,
            g_expRoundState.unassignedAlivePlayers,
            TeamRoundTeamplayConfigured() ? "yes" : "no");
    }
    else if (g_expRoundState.connectedPlayers <= 1)
    {
        PrintLabDummyConsoleLine("round note: single-player live round; it ends after the only player is eliminated.");
    }
    LogRoundEvent("round_live", GetRoundStateName(g_expRoundState.state), g_expRoundState.roundNumber, g_expRoundState.connectedPlayers, g_expRoundState.alivePlayers, NULL, reason);
}

void EndRound(CBasePlayer *pWinner, int winnerTeamId, const char *reason)
{
    StoreRoundWinner(pWinner, winnerTeamId, reason);
    SetRoundState(kExpRoundStateRoundEnd, kRoundEndHoldSeconds);
    PrintLabDummyConsoleLine(
        "round %d ended: winner=%s winner_team=%s reason=%s connected=%d alive=%d restart_in=%.1fs",
        g_expRoundState.roundNumber,
        g_expRoundState.lastWinnerName[0] != '\0' ? g_expRoundState.lastWinnerName : "none",
        g_expRoundState.lastWinnerTeamName[0] != '\0' ? g_expRoundState.lastWinnerTeamName : "none",
        g_expRoundState.lastEndReason[0] != '\0' ? g_expRoundState.lastEndReason : "unknown",
        g_expRoundState.connectedPlayers,
        g_expRoundState.alivePlayers,
        ExpRoundRestartDelaySeconds());
    LogRoundEvent("round_end", GetRoundStateName(g_expRoundState.state), g_expRoundState.roundNumber, g_expRoundState.connectedPlayers, g_expRoundState.alivePlayers, pWinner, reason);
}

void BeginRoundRestartPending(const char *reason)
{
    SetRoundState(kExpRoundStateRestartPending, ExpRoundRestartDelaySeconds());
    PrintLabDummyConsoleLine(
        "round %d restart pending: next_round_in=%.1fs reason=%s",
        g_expRoundState.roundNumber,
        ExpRoundRestartDelaySeconds(),
        GetValueOrFallback(reason, g_expRoundState.lastEndReason));
    LogRoundEvent("round_restart", GetRoundStateName(g_expRoundState.state), g_expRoundState.roundNumber, g_expRoundState.connectedPlayers, g_expRoundState.alivePlayers, NULL, reason);
}

void EvaluateRoundOutcome()
{
    const ExpRoundPlayerSnapshot snapshot = CollectRoundPlayerSnapshot();
    g_expRoundState.connectedPlayers = snapshot.connectedPlayers;
    g_expRoundState.alivePlayers = snapshot.alivePlayers;
    g_expRoundState.team1ConnectedPlayers = snapshot.team1ConnectedPlayers;
    g_expRoundState.team2ConnectedPlayers = snapshot.team2ConnectedPlayers;
    g_expRoundState.unassignedConnectedPlayers = snapshot.unassignedConnectedPlayers;
    g_expRoundState.team1AlivePlayers = snapshot.team1AlivePlayers;
    g_expRoundState.team2AlivePlayers = snapshot.team2AlivePlayers;
    g_expRoundState.unassignedAlivePlayers = snapshot.unassignedAlivePlayers;

    if (snapshot.connectedPlayers <= 0)
    {
        BeginRoundWaiting("no_players", false);
        return;
    }

    if (TeamRoundModeConfigured())
    {
        if (snapshot.team1AlivePlayers > 0 && snapshot.team2AlivePlayers <= 0)
        {
            EndRound(snapshot.lastAliveTeam1Player, kExpRoundTeam1, "last_team_alive");
            return;
        }

        if (snapshot.team2AlivePlayers > 0 && snapshot.team1AlivePlayers <= 0)
        {
            EndRound(snapshot.lastAliveTeam2Player, kExpRoundTeam2, "last_team_alive");
            return;
        }

        if (snapshot.team1AlivePlayers <= 0 && snapshot.team2AlivePlayers <= 0)
        {
            EndRound(NULL, kExpRoundTeamNone, "all_teams_eliminated");
            return;
        }

        if (!RoundTeamsReady(snapshot))
        {
            BeginRoundWaiting(kRoundEndReasonTeamsIncomplete, false);
        }
        return;
    }

    if (snapshot.connectedPlayers == 1)
    {
        if (snapshot.alivePlayers <= 0)
        {
            EndRound(NULL, kExpRoundTeamNone, "solo_eliminated");
        }
        return;
    }

    if (snapshot.alivePlayers == 1)
    {
        EndRound(snapshot.lastAlivePlayer, kExpRoundTeamNone, "last_alive");
        return;
    }

    if (snapshot.alivePlayers <= 0)
    {
        EndRound(NULL, kExpRoundTeamNone, "all_eliminated");
    }
}

void StopRoundMode(bool respawnDeadPlayers, const char *reason, bool printMessage)
{
    if (respawnDeadPlayers)
    {
        RespawnDeadPlayersForDeathmatch();
    }

    ClearRoundRuntimeState();
    if (printMessage)
    {
        PrintLabDummyConsoleLine(
            "round mode disabled: deathmatch respawn flow restored. reason=%s",
            GetValueOrFallback(reason, "round_mode_off"));
    }
}

void EnsureRoundModeState()
{
    if (!ExpRoundModeEnabled())
    {
        if (g_expRoundState.state != kExpRoundStateDisabled)
        {
            StopRoundMode(true, "cvar_disabled", false);
        }
        return;
    }

    if (g_expRoundState.state == kExpRoundStateDisabled)
    {
        if (TeamRoundModeConfigured())
        {
            AutoAssignRoundTeams(false);
        }

        const ExpRoundPlayerSnapshot snapshot = CollectRoundPlayerSnapshot();
        if (RoundTeamsReady(snapshot))
        {
            BeginRoundFreeze(false, "mode_enabled");
        }
        else
        {
            BeginRoundWaiting(GetRoundWaitingReasonForSnapshot(snapshot), false);
        }
    }
}

void UpdateRoundModeFrame()
{
    EnsureRoundModeState();
    if (g_expRoundState.state == kExpRoundStateDisabled)
    {
        return;
    }

    if (g_expRoundState.state == kExpRoundStateWaitingForPlayers)
    {
        if (TeamRoundModeConfigured())
        {
            AutoAssignRoundTeams(false);
        }

        const ExpRoundPlayerSnapshot snapshot = CollectRoundPlayerSnapshot();
        g_expRoundState.connectedPlayers = snapshot.connectedPlayers;
        g_expRoundState.alivePlayers = snapshot.alivePlayers;
        g_expRoundState.team1ConnectedPlayers = snapshot.team1ConnectedPlayers;
        g_expRoundState.team2ConnectedPlayers = snapshot.team2ConnectedPlayers;
        g_expRoundState.unassignedConnectedPlayers = snapshot.unassignedConnectedPlayers;
        g_expRoundState.team1AlivePlayers = snapshot.team1AlivePlayers;
        g_expRoundState.team2AlivePlayers = snapshot.team2AlivePlayers;
        g_expRoundState.unassignedAlivePlayers = snapshot.unassignedAlivePlayers;
        if (RoundTeamsReady(snapshot))
        {
            BeginRoundFreeze(false, "players_ready");
        }
        return;
    }

    if (g_expRoundState.state == kExpRoundStateFreezeTime)
    {
        const ExpRoundPlayerSnapshot snapshot = CollectRoundPlayerSnapshot();
        g_expRoundState.connectedPlayers = snapshot.connectedPlayers;
        g_expRoundState.alivePlayers = snapshot.alivePlayers;
        g_expRoundState.team1ConnectedPlayers = snapshot.team1ConnectedPlayers;
        g_expRoundState.team2ConnectedPlayers = snapshot.team2ConnectedPlayers;
        g_expRoundState.unassignedConnectedPlayers = snapshot.unassignedConnectedPlayers;
        g_expRoundState.team1AlivePlayers = snapshot.team1AlivePlayers;
        g_expRoundState.team2AlivePlayers = snapshot.team2AlivePlayers;
        g_expRoundState.unassignedAlivePlayers = snapshot.unassignedAlivePlayers;
        if (!RoundTeamsReady(snapshot))
        {
            BeginRoundWaiting(GetRoundWaitingReasonForSnapshot(snapshot), false);
            return;
        }

        if (g_expRoundState.nextTransitionAt > 0.0f && gpGlobals->time >= g_expRoundState.nextTransitionAt)
        {
            BeginRoundLive("freeze_complete");
        }
        return;
    }

    if (g_expRoundState.state == kExpRoundStateLive)
    {
        EvaluateRoundOutcome();
        return;
    }

    if (g_expRoundState.state == kExpRoundStateRoundEnd)
    {
        const ExpRoundPlayerSnapshot snapshot = CollectRoundPlayerSnapshot();
        g_expRoundState.connectedPlayers = snapshot.connectedPlayers;
        g_expRoundState.alivePlayers = snapshot.alivePlayers;
        g_expRoundState.team1ConnectedPlayers = snapshot.team1ConnectedPlayers;
        g_expRoundState.team2ConnectedPlayers = snapshot.team2ConnectedPlayers;
        g_expRoundState.unassignedConnectedPlayers = snapshot.unassignedConnectedPlayers;
        g_expRoundState.team1AlivePlayers = snapshot.team1AlivePlayers;
        g_expRoundState.team2AlivePlayers = snapshot.team2AlivePlayers;
        g_expRoundState.unassignedAlivePlayers = snapshot.unassignedAlivePlayers;
        if (!RoundTeamsReady(snapshot))
        {
            BeginRoundWaiting(GetRoundWaitingReasonForSnapshot(snapshot), false);
            return;
        }

        if (g_expRoundState.nextTransitionAt > 0.0f && gpGlobals->time >= g_expRoundState.nextTransitionAt)
        {
            BeginRoundRestartPending(g_expRoundState.lastEndReason);
        }
        return;
    }

    if (g_expRoundState.state == kExpRoundStateRestartPending)
    {
        const ExpRoundPlayerSnapshot snapshot = CollectRoundPlayerSnapshot();
        g_expRoundState.connectedPlayers = snapshot.connectedPlayers;
        g_expRoundState.alivePlayers = snapshot.alivePlayers;
        g_expRoundState.team1ConnectedPlayers = snapshot.team1ConnectedPlayers;
        g_expRoundState.team2ConnectedPlayers = snapshot.team2ConnectedPlayers;
        g_expRoundState.unassignedConnectedPlayers = snapshot.unassignedConnectedPlayers;
        g_expRoundState.team1AlivePlayers = snapshot.team1AlivePlayers;
        g_expRoundState.team2AlivePlayers = snapshot.team2AlivePlayers;
        g_expRoundState.unassignedAlivePlayers = snapshot.unassignedAlivePlayers;
        if (!RoundTeamsReady(snapshot))
        {
            BeginRoundWaiting(GetRoundWaitingReasonForSnapshot(snapshot), false);
            return;
        }

        if (g_expRoundState.nextTransitionAt > 0.0f && gpGlobals->time >= g_expRoundState.nextTransitionAt)
        {
            BeginRoundFreeze(true, "auto_restart");
        }
    }
}

void PrintRoundStatus()
{
    UpdateRoundPopulationSnapshot();

    const bool modeEnabled = ExpRoundModeEnabled();
    const char *configuredLoadout = GetConfiguredRoundLoadoutMode();
    const char *resolvedLoadout = GetResolvedRoundLoadoutMode();
    const bool validLoadout = IsSupportedRoundLoadoutMode(configuredLoadout);
    const float secondsRemaining = (g_expRoundState.nextTransitionAt > 0.0f && gpGlobals != NULL)
        ? max(0.0f, g_expRoundState.nextTransitionAt - gpGlobals->time)
        : 0.0f;

    PrintLabDummyConsoleLine(
        "round mode: %s state=%s round=%d connected=%d alive=%d",
        modeEnabled ? "on" : "off",
        GetRoundStateName(g_expRoundState.state),
        g_expRoundState.roundNumber,
        g_expRoundState.connectedPlayers,
        g_expRoundState.alivePlayers);
    PrintLabDummyConsoleLine(
        "round config: freeze=%.1fs restart=%.1fs start_health=%.1f start_armor=%.1f no_respawn=%s friendlyfire=%s",
        ExpRoundFreezeTimeSeconds(),
        ExpRoundRestartDelaySeconds(),
        ExpRoundStartHealth(),
        ExpRoundStartArmor(),
        ExpRoundNoRespawn() ? "yes" : "no",
        ExpRoundFriendlyFireEnabled() ? "yes" : "no");
    PrintLabDummyConsoleLine(
        "round loadout: configured=%s resolved=%s valid=%s weapon_profile=%s",
        configuredLoadout,
        resolvedLoadout,
        validLoadout ? "yes" : "no",
        ExpRoundWeaponProfile()[0] != '\0' ? ExpRoundWeaponProfile() : "none");

    PrintLabDummyConsoleLine(
        "team round config: enabled=%s teamplay=%s spawn_mode=%s resolved_spawn=%s",
        TeamRoundModeConfigured() ? "yes" : "no",
        TeamRoundTeamplayConfigured() ? "yes" : "no",
        GetConfiguredTeamRoundSpawnMode(),
        GetResolvedTeamRoundSpawnMode());

    if (TeamRoundModeConfigured())
    {
        PrintLabDummyConsoleLine(
            "team counts: %s connected=%d alive=%d | %s connected=%d alive=%d | unassigned connected=%d alive=%d",
            GetConfiguredTeamRoundName(kExpRoundTeam1),
            g_expRoundState.team1ConnectedPlayers,
            g_expRoundState.team1AlivePlayers,
            GetConfiguredTeamRoundName(kExpRoundTeam2),
            g_expRoundState.team2ConnectedPlayers,
            g_expRoundState.team2AlivePlayers,
            g_expRoundState.unassignedConnectedPlayers,
            g_expRoundState.unassignedAlivePlayers);
        PrintLabDummyConsoleLine(
            "team loadouts: %s=%s %s=%s",
            GetConfiguredTeamRoundName(kExpRoundTeam1),
            GetResolvedRoundLoadoutModeForTeam(kExpRoundTeam1),
            GetConfiguredTeamRoundName(kExpRoundTeam2),
            GetResolvedRoundLoadoutModeForTeam(kExpRoundTeam2));
        PrintLabDummyConsoleLine(
            "team health/armor: %s=%.1f/%.1f %s=%.1f/%.1f",
            GetConfiguredTeamRoundName(kExpRoundTeam1),
            GetResolvedRoundStartHealthForTeam(kExpRoundTeam1),
            GetResolvedRoundStartArmorForTeam(kExpRoundTeam1),
            GetConfiguredTeamRoundName(kExpRoundTeam2),
            GetResolvedRoundStartHealthForTeam(kExpRoundTeam2),
            GetResolvedRoundStartArmorForTeam(kExpRoundTeam2));
        PrintTeamSpawnStatus();
    }

    if (g_expRoundState.nextTransitionAt > 0.0f)
    {
        PrintLabDummyConsoleLine("round timer: next_state_in=%.1fs", secondsRemaining);
    }
    else
    {
        PrintLabDummyConsoleLine("round timer: none");
    }

    if (g_expRoundState.lastEndReason[0] != '\0' || g_expRoundState.lastWinnerName[0] != '\0')
    {
        PrintLabDummyConsoleLine(
            "last round result: winner=%s winner_team=%s entindex=%d userid=%d reason=%s",
            g_expRoundState.lastWinnerName[0] != '\0' ? g_expRoundState.lastWinnerName : "none",
            g_expRoundState.lastWinnerTeamName[0] != '\0' ? g_expRoundState.lastWinnerTeamName : "none",
            g_expRoundState.lastWinnerEntIndex,
            g_expRoundState.lastWinnerUserId,
            g_expRoundState.lastEndReason[0] != '\0' ? g_expRoundState.lastEndReason : "unknown");
    }
    else
    {
        PrintLabDummyConsoleLine("last round result: none");
    }

    if (g_expRoundState.connectedPlayers <= 1 && g_expRoundState.state == kExpRoundStateLive)
    {
        PrintLabDummyConsoleLine("round note: single-player duel mode is active; the round restarts after the only player is eliminated.");
    }

    PrintLabDummyConsoleLine("commands: exp_round_start | exp_round_restart | exp_round_status | exp_round_stop | exp_round_slay [all|team1|team2]");
    PrintLabDummyConsoleLine("team commands: exp_team_join <player> <team> | exp_team_autoassign | exp_team_status | exp_team_fake_add <team> [name] | exp_team_fake_clear");
    PrintLabDummyConsoleLine("team spawn commands: exp_team_spawn_mark <team> [name] | exp_team_spawn_unmark <team> <name> | exp_team_spawn_list | exp_team_spawn_use <team> <name> | exp_team_spawn_status");
}

void ExpRoundStartCommand()
{
    CVAR_SET_FLOAT("sv_exp_round_mode", 1.0f);
    if (TeamRoundModeConfigured())
    {
        AutoAssignRoundTeams(false);
    }

    const ExpRoundPlayerSnapshot snapshot = CollectRoundPlayerSnapshot();
    if (!RoundTeamsReady(snapshot))
    {
        BeginRoundWaiting(GetRoundWaitingReasonForSnapshot(snapshot), true);
        PrintRoundStatus();
        return;
    }

    if (g_expRoundState.state == kExpRoundStateFreezeTime ||
        g_expRoundState.state == kExpRoundStateLive ||
        g_expRoundState.state == kExpRoundStateRoundEnd ||
        g_expRoundState.state == kExpRoundStateRestartPending)
    {
        PrintLabDummyConsoleLine("round mode is already active. Use exp_round_restart to reset the current round.");
        PrintRoundStatus();
        return;
    }

    BeginRoundFreeze(false, "manual_start");
    PrintRoundStatus();
}

void ExpRoundRestartCommand()
{
    CVAR_SET_FLOAT("sv_exp_round_mode", 1.0f);
    if (TeamRoundModeConfigured())
    {
        AutoAssignRoundTeams(false);
    }

    const ExpRoundPlayerSnapshot snapshot = CollectRoundPlayerSnapshot();
    if (!RoundTeamsReady(snapshot))
    {
        BeginRoundWaiting(GetRoundWaitingReasonForSnapshot(snapshot), true);
        PrintRoundStatus();
        return;
    }

    BeginRoundFreeze(true, "manual_restart");
    PrintRoundStatus();
}

void ExpRoundStatusCommand()
{
    PrintRoundStatus();
}

void ExpRoundStopCommand()
{
    if (g_expRoundState.state == kExpRoundStateDisabled && !ExpRoundModeEnabled())
    {
        PrintLabDummyConsoleLine("round mode is already off.");
        PrintRoundStatus();
        return;
    }

    CVAR_SET_FLOAT("sv_exp_round_mode", 0.0f);
    StopRoundMode(true, "command_stop", true);
    PrintRoundStatus();
}

void ExpRoundSlayCommand()
{
    bool slayAllPlayers = false;
    int teamFilter = kExpRoundTeamNone;
    if (CMD_ARGC() >= 2)
    {
        if (StringEqualsIgnoreCase(CMD_ARGV(1), "all"))
        {
            slayAllPlayers = true;
        }
        else if (!TryResolveRoundTeamId(CMD_ARGV(1), &teamFilter))
        {
            PrintLabDummyConsoleLine("usage: exp_round_slay [all|team1|team2]");
            return;
        }
    }

    const int killedPlayers = SlayRoundPlayers(slayAllPlayers || teamFilter != kExpRoundTeamNone, teamFilter);
    if (killedPlayers <= 0)
    {
        PrintLabDummyConsoleLine("round slay: no live players were available to eliminate.");
        return;
    }

    PrintLabDummyConsoleLine(
        "round slay: eliminated %d %splayer%s%s.",
        killedPlayers,
        slayAllPlayers ? "live " : "",
        killedPlayers == 1 ? "" : "s",
        teamFilter == kExpRoundTeamNone ? "" : UTIL_VarArgs(" from %s", GetConfiguredTeamRoundName(teamFilter)));
}

bool ShouldApplyRoundResetOnSpawn()
{
    return g_expRoundState.applyingRoundReset || g_expRoundState.state == kExpRoundStateFreezeTime;
}

void TrimCfgRequestString(const char *input, char *buffer, size_t bufferSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return;
    }

    buffer[0] = '\0';
    if (input == NULL)
    {
        return;
    }

    const char *start = input;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n' || *start == '"')
    {
        ++start;
    }

    const char *end = input + strlen(input);
    while (end > start &&
           (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n' || end[-1] == '"'))
    {
        --end;
    }

    const size_t length = (size_t)(end - start);
    strncpy_s(buffer, bufferSize, start, length);
}

void PrintTeamSpawnStatus()
{
    RefreshFutureHooksMapState();

    PrintLabDummyConsoleLine(
        "team spawn storage: file=%s load_state=%s",
        g_teamSpawnSpotsPath[0] != '\0' ? g_teamSpawnSpotsPath : "n/a",
        g_teamSpawnSpotsLoadFailure[0] != '\0' ? g_teamSpawnSpotsLoadFailure : "ok");

    for (int teamId = kExpRoundTeam1; teamId <= kExpRoundTeam2; ++teamId)
    {
        char namesSummary[kMaxLabDummySpotListLength];
        char origin[64];
        BuildTeamSpawnNamesSummary(teamId, namesSummary, sizeof(namesSummary));
        strcpy_s(origin, sizeof(origin), "n/a");

        const TeamSpawnSavedSpotRecord *activeSpot = GetActiveTeamSpawnSavedSpot(teamId);
        const TeamSpawnSavedSpotRecord *defaultSpot = GetDefaultTeamSpawnSavedSpot(teamId);
        PrintLabDummyConsoleLine(
            "team spawn spots: team=%s count=%d active=%s names=%s",
            GetConfiguredTeamRoundName(teamId),
            g_teamSpawnSavedSpotCounts[teamId],
            g_teamSpawnActiveSpotNames[teamId][0] != '\0' ? g_teamSpawnActiveSpotNames[teamId] : "none",
            namesSummary[0] != '\0' ? namesSummary : "none");

        if (activeSpot != NULL)
        {
            FormatVector3(origin, sizeof(origin), activeSpot->origin);
            PrintLabDummyConsoleLine(
                "team spawn active: team=%s name=%s storage=%s origin=%s yaw=%.1f updated_at=%s",
                GetConfiguredTeamRoundName(teamId),
                activeSpot->name,
                GetTeamSpawnSpotStorageLabel(activeSpot),
                origin,
                activeSpot->angles.y,
                activeSpot->updatedAt[0] != '\0' ? activeSpot->updatedAt : "n/a");
        }
        else if (g_teamSpawnActiveSpotNames[teamId][0] != '\0')
        {
            PrintLabDummyConsoleLine(
                "team spawn active: team=%s name=%s state=missing",
                GetConfiguredTeamRoundName(teamId),
                g_teamSpawnActiveSpotNames[teamId]);
        }
        else if (defaultSpot != NULL)
        {
            FormatVector3(origin, sizeof(origin), defaultSpot->origin);
            PrintLabDummyConsoleLine(
                "team spawn active: team=%s none selected fallback=%s storage=%s origin=%s yaw=%.1f",
                GetConfiguredTeamRoundName(teamId),
                defaultSpot->name,
                GetTeamSpawnSpotStorageLabel(defaultSpot),
                origin,
                defaultSpot->angles.y);
        }
        else
        {
            PrintLabDummyConsoleLine("team spawn active: team=%s none selected", GetConfiguredTeamRoundName(teamId));
        }

        if (g_teamSpawnLastApplied[teamId].valid)
        {
            FormatVector3(origin, sizeof(origin), g_teamSpawnLastApplied[teamId].origin);
            PrintLabDummyConsoleLine(
                "team spawn last applied: team=%s round=%d player=%s entindex=%d userid=%d source=%s candidate=%s origin=%s yaw=%.1f%s%s%s%s%s%s",
                GetConfiguredTeamRoundName(teamId),
                g_teamSpawnLastApplied[teamId].roundNumber,
                g_teamSpawnLastApplied[teamId].playerName[0] != '\0' ? g_teamSpawnLastApplied[teamId].playerName : "unknown",
                g_teamSpawnLastApplied[teamId].playerEntIndex,
                g_teamSpawnLastApplied[teamId].playerUserId,
                g_teamSpawnLastApplied[teamId].source[0] != '\0' ? g_teamSpawnLastApplied[teamId].source : "unknown",
                g_teamSpawnLastApplied[teamId].candidate[0] != '\0' ? g_teamSpawnLastApplied[teamId].candidate : "n/a",
                origin,
                g_teamSpawnLastApplied[teamId].angles.y,
                g_teamSpawnLastApplied[teamId].spotName[0] != '\0' ? " spot=" : "",
                g_teamSpawnLastApplied[teamId].spotName[0] != '\0' ? g_teamSpawnLastApplied[teamId].spotName : "",
                g_teamSpawnLastApplied[teamId].spotStorage[0] != '\0' ? " storage=" : "",
                g_teamSpawnLastApplied[teamId].spotStorage[0] != '\0' ? g_teamSpawnLastApplied[teamId].spotStorage : "",
                g_teamSpawnLastApplied[teamId].reason[0] != '\0' ? " reason=" : "",
                g_teamSpawnLastApplied[teamId].reason[0] != '\0' ? g_teamSpawnLastApplied[teamId].reason : "");
        }
        else
        {
            PrintLabDummyConsoleLine("team spawn last applied: team=%s none", GetConfiguredTeamRoundName(teamId));
        }

        if (g_teamSpawnLastFailure[teamId].reason[0] != '\0')
        {
            PrintLabDummyConsoleLine(
                "team spawn last failure: team=%s at=%s code=%s source=%s candidate=%s%s%s%s%s reason=%s",
                GetConfiguredTeamRoundName(teamId),
                g_teamSpawnLastFailureAt[teamId][0] != '\0' ? g_teamSpawnLastFailureAt[teamId] : "unknown",
                g_teamSpawnLastFailure[teamId].code[0] != '\0' ? g_teamSpawnLastFailure[teamId].code : "unknown",
                g_teamSpawnLastFailure[teamId].source[0] != '\0' ? g_teamSpawnLastFailure[teamId].source : "unknown",
                g_teamSpawnLastFailure[teamId].candidate[0] != '\0' ? g_teamSpawnLastFailure[teamId].candidate : "n/a",
                g_teamSpawnLastFailure[teamId].spotName[0] != '\0' ? " spot=" : "",
                g_teamSpawnLastFailure[teamId].spotName[0] != '\0' ? g_teamSpawnLastFailure[teamId].spotName : "",
                g_teamSpawnLastFailure[teamId].spotStorage[0] != '\0' ? " storage=" : "",
                g_teamSpawnLastFailure[teamId].spotStorage[0] != '\0' ? g_teamSpawnLastFailure[teamId].spotStorage : "",
                g_teamSpawnLastFailure[teamId].reason);
        }
        else
        {
            PrintLabDummyConsoleLine("team spawn last failure: team=%s none", GetConfiguredTeamRoundName(teamId));
        }
    }
}

void PrintTeamStatus()
{
    UpdateRoundPopulationSnapshot();

    PrintLabDummyConsoleLine(
        "team round: %s teamplay=%s spawn_mode=%s resolved_spawn=%s",
        TeamRoundModeConfigured() ? "on" : "off",
        TeamRoundTeamplayConfigured() ? "on" : "off",
        GetConfiguredTeamRoundSpawnMode(),
        GetResolvedTeamRoundSpawnMode());
    PrintLabDummyConsoleLine(
        "team config: %s loadout=%s health=%.1f armor=%.1f | %s loadout=%s health=%.1f armor=%.1f",
        GetConfiguredTeamRoundName(kExpRoundTeam1),
        GetResolvedRoundLoadoutModeForTeam(kExpRoundTeam1),
        GetResolvedRoundStartHealthForTeam(kExpRoundTeam1),
        GetResolvedRoundStartArmorForTeam(kExpRoundTeam1),
        GetConfiguredTeamRoundName(kExpRoundTeam2),
        GetResolvedRoundLoadoutModeForTeam(kExpRoundTeam2),
        GetResolvedRoundStartHealthForTeam(kExpRoundTeam2),
        GetResolvedRoundStartArmorForTeam(kExpRoundTeam2));
    PrintLabDummyConsoleLine(
        "team counts: %s connected=%d alive=%d | %s connected=%d alive=%d | unassigned connected=%d alive=%d",
        GetConfiguredTeamRoundName(kExpRoundTeam1),
        g_expRoundState.team1ConnectedPlayers,
        g_expRoundState.team1AlivePlayers,
        GetConfiguredTeamRoundName(kExpRoundTeam2),
        g_expRoundState.team2ConnectedPlayers,
        g_expRoundState.team2AlivePlayers,
        g_expRoundState.unassignedConnectedPlayers,
        g_expRoundState.unassignedAlivePlayers);

    if (!TeamRoundSpawnModeSupported())
    {
        PrintLabDummyConsoleLine("team spawn note: configured spawn_mode \"%s\" is unsupported; dm_spawns remains the active behavior.", GetConfiguredTeamRoundSpawnMode());
    }

    PrintTeamSpawnStatus();

    bool anyPlayers = false;
    if (gpGlobals != NULL)
    {
        for (int playerIndex = 1; playerIndex <= gpGlobals->maxClients; ++playerIndex)
        {
            CBaseEntity *pEntity = UTIL_PlayerByIndex(playerIndex);
            if (pEntity == NULL || !pEntity->IsPlayer() || pEntity->pev == NULL)
            {
                continue;
            }

            CBasePlayer *pPlayer = (CBasePlayer *)pEntity;
            if (!IsRoundManagedPlayer(pPlayer))
            {
                continue;
            }

            anyPlayers = true;
            const int teamId = GetAssignedRoundTeamId(pPlayer);
            PrintLabDummyConsoleLine(
                "team player: name=%s entindex=%d userid=%d fake=%s team=%s alive=%s health=%.1f armor=%.1f",
                GetSafePlayerName(pPlayer),
                GetPlayerEntityIndex(pPlayer),
                GetPlayerUserId(pPlayer),
                IsRoundFakeClient(pPlayer) ? "yes" : "no",
                teamId == kExpRoundTeamNone ? "unassigned" : GetConfiguredTeamRoundName(teamId),
                (pPlayer->IsAlive() && pPlayer->pev->deadflag == DEAD_NO) ? "yes" : "no",
                pPlayer->pev->health,
                pPlayer->pev->armorvalue);
        }
    }

    if (!anyPlayers)
    {
        PrintLabDummyConsoleLine("team player list: none");
    }
}

void ExpTeamJoinCommand()
{
    if (CMD_ARGC() < 3)
    {
        PrintLabDummyConsoleLine("usage: exp_team_join <player> <team>");
        PrintLabDummyConsoleLine(
            "teams: %s | %s",
            GetConfiguredTeamRoundName(kExpRoundTeam1),
            GetConfiguredTeamRoundName(kExpRoundTeam2));
        return;
    }

    char failureReason[128];
    CBasePlayer *pPlayer = NULL;
    if (!TryResolveRoundPlayerToken(CMD_ARGV(1), &pPlayer, failureReason, sizeof(failureReason)))
    {
        PrintLabDummyConsoleLine("team join failed: %s", failureReason);
        return;
    }

    int teamId = kExpRoundTeamNone;
    if (!TryResolveRoundTeamId(CMD_ARGV(2), &teamId))
    {
        PrintLabDummyConsoleLine(
            "team join failed: unknown team \"%s\". Use team1/team2 or %s/%s.",
            CMD_ARGV(2),
            GetConfiguredTeamRoundName(kExpRoundTeam1),
            GetConfiguredTeamRoundName(kExpRoundTeam2));
        return;
    }

    AssignRoundTeamToPlayer(pPlayer, teamId);
    UpdateRoundPopulationSnapshot();
    PrintLabDummyConsoleLine(
        "team join: %s assigned to %s.",
        GetSafePlayerName(pPlayer),
        GetConfiguredTeamRoundName(teamId));
    PrintTeamStatus();
}

void ExpTeamAutoassignCommand()
{
    const int assignedPlayers = AutoAssignRoundTeams(true);
    UpdateRoundPopulationSnapshot();
    PrintLabDummyConsoleLine(
        "team autoassign: assigned %d player%s across %s and %s.",
        assignedPlayers,
        assignedPlayers == 1 ? "" : "s",
        GetConfiguredTeamRoundName(kExpRoundTeam1),
        GetConfiguredTeamRoundName(kExpRoundTeam2));
    PrintTeamStatus();
}

void ExpTeamStatusCommand()
{
    PrintTeamStatus();
}

void ExpTeamFakeAddCommand()
{
    if (CMD_ARGC() < 2)
    {
        PrintLabDummyConsoleLine("usage: exp_team_fake_add <team> [name]");
        PrintLabDummyConsoleLine(
            "teams: %s | %s",
            GetConfiguredTeamRoundName(kExpRoundTeam1),
            GetConfiguredTeamRoundName(kExpRoundTeam2));
        return;
    }

    int teamId = kExpRoundTeamNone;
    if (!TryResolveRoundTeamId(CMD_ARGV(1), &teamId))
    {
        PrintLabDummyConsoleLine(
            "team fake add failed: unknown team \"%s\". Use team1/team2 or %s/%s.",
            CMD_ARGV(1),
            GetConfiguredTeamRoundName(kExpRoundTeam1),
            GetConfiguredTeamRoundName(kExpRoundTeam2));
        return;
    }

    char createdName[kMaxRoundFakeClientNameLength];
    char failureReason[128];
    const char *requestedName = CMD_ARGC() >= 3 ? CMD_ARGV(2) : "";
    if (!CreateRoundFakeClient(teamId, requestedName, createdName, sizeof(createdName), failureReason, sizeof(failureReason)))
    {
        PrintLabDummyConsoleLine("team fake add failed: %s", failureReason[0] != '\0' ? failureReason : "unknown error");
        return;
    }

    PrintLabDummyConsoleLine(
        "team fake add: created %s on %s for local team-round verification.",
        createdName,
        GetConfiguredTeamRoundName(teamId));
    PrintTeamStatus();
}

void ExpTeamFakeClearCommand()
{
    const int kickedClients = KickAllRoundFakeClients();
    UpdateRoundPopulationSnapshot();
    PrintLabDummyConsoleLine(
        "team fake clear: removed %d fake client%s.",
        kickedClients,
        kickedClients == 1 ? "" : "s");
    PrintTeamStatus();
}

bool TryGetTeamSpawnCommandArgs(
    int teamArgIndex,
    int nameArgIndex,
    int *pTeamId,
    char *spotName,
    size_t spotNameSize,
    bool useDefaultIfEmpty,
    char *failureReason,
    size_t failureReasonSize)
{
    if (pTeamId == NULL || spotName == NULL || spotNameSize == 0)
    {
        return false;
    }

    *pTeamId = kExpRoundTeamNone;
    spotName[0] = '\0';
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    if (!TryResolveRoundTeamId(CMD_ARGV(teamArgIndex), pTeamId))
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "unknown team \"%s\". Use team1/team2 or %s/%s.",
                CMD_ARGV(teamArgIndex),
                GetConfiguredTeamRoundName(kExpRoundTeam1),
                GetConfiguredTeamRoundName(kExpRoundTeam2));
        }
        return false;
    }

    char requestedName[kMaxLabDummySpotNameLength];
    BuildCommandArgumentString(nameArgIndex, requestedName, sizeof(requestedName));
    return TryResolveTeamSpawnSpotName(requestedName, spotName, spotNameSize, useDefaultIfEmpty, failureReason, failureReasonSize);
}

void ExpTeamSpawnMarkCommand()
{
    RefreshFutureHooksMapState();

    if (CMD_ARGC() < 2)
    {
        PrintLabDummyConsoleLine("usage: exp_team_spawn_mark <team> [name]");
        return;
    }

    int teamId = kExpRoundTeamNone;
    char spotName[kMaxLabDummySpotNameLength];
    char failureReason[kMaxLabDummySpotFileFailureLength];
    if (!TryGetTeamSpawnCommandArgs(1, 2, &teamId, spotName, sizeof(spotName), true, failureReason, sizeof(failureReason)))
    {
        PrintLabDummyConsoleLine("team spawn mark failed: %s", failureReason);
        return;
    }

    CBasePlayer *pPlayer = FindFirstManagedPlayerOnRoundTeam(teamId, true);
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        PrintLabDummyConsoleLine(
            "team spawn mark failed: no live player on %s is available to mark a spawn spot",
            GetConfiguredTeamRoundName(teamId));
        return;
    }

    Vector markedOrigin = pPlayer->pev->origin;
    Vector markedAngles = pPlayer->pev->v_angle;
    markedAngles.x = 0.0f;
    markedAngles.z = 0.0f;

    TeamSpawnSavedSpotRecord *spot = UpsertTeamSpawnSavedSpot(teamId, spotName, markedOrigin, markedAngles, failureReason, sizeof(failureReason));
    if (spot == NULL)
    {
        PrintLabDummyConsoleLine("team spawn mark failed: %s", failureReason);
        return;
    }

    strncpy_s(g_teamSpawnActiveSpotNames[teamId], sizeof(g_teamSpawnActiveSpotNames[teamId]), spot->name, _TRUNCATE);
    const bool savedToDisk = SaveTeamSpawnSpotsForCurrentMap(failureReason, sizeof(failureReason));
    LogTeamSpawnEvent(
        "team_spawn_mark",
        pPlayer,
        teamId,
        GetConfiguredTeamRoundName(teamId),
        markedOrigin,
        markedAngles,
        kTeamSpawnSourceSavedSpot,
        "mark",
        spot->name,
        kTeamSpawnSpotStorageSession,
        savedToDisk ? "" : failureReason);

    char origin[64];
    FormatVector3(origin, sizeof(origin), markedOrigin);
    PrintLabDummyConsoleLine(
        "saved %s team spawn \"%s\" for map %s: origin=%s yaw=%.1f active=%s disk=%s",
        GetConfiguredTeamRoundName(teamId),
        spot->name,
        GetCurrentMapName(),
        origin,
        markedAngles.y,
        g_teamSpawnActiveSpotNames[teamId],
        savedToDisk ? g_teamSpawnSpotsPath : failureReason);
}

void ExpTeamSpawnUnmarkCommand()
{
    RefreshFutureHooksMapState();

    if (CMD_ARGC() < 3)
    {
        PrintLabDummyConsoleLine("usage: exp_team_spawn_unmark <team> <name>");
        return;
    }

    int teamId = kExpRoundTeamNone;
    char spotName[kMaxLabDummySpotNameLength];
    char failureReason[kMaxLabDummySpotFileFailureLength];
    if (!TryGetTeamSpawnCommandArgs(1, 2, &teamId, spotName, sizeof(spotName), false, failureReason, sizeof(failureReason)))
    {
        PrintLabDummyConsoleLine("team spawn unmark failed: %s", failureReason);
        return;
    }

    TeamSpawnSavedSpotRecord removedSpot = {};
    if (!RemoveTeamSpawnSavedSpot(teamId, spotName, &removedSpot, failureReason, sizeof(failureReason)))
    {
        PrintLabDummyConsoleLine("team spawn unmark failed: %s", failureReason);
        return;
    }

    const bool savedToDisk = SaveTeamSpawnSpotsForCurrentMap(failureReason, sizeof(failureReason));
    LogTeamSpawnEvent(
        "team_spawn_unmark",
        FindFirstManagedPlayerOnRoundTeam(teamId, false),
        teamId,
        GetConfiguredTeamRoundName(teamId),
        removedSpot.origin,
        removedSpot.angles,
        kTeamSpawnSourceSavedSpot,
        "unmark",
        removedSpot.name,
        GetTeamSpawnSpotStorageLabel(&removedSpot),
        savedToDisk ? "" : failureReason);
    PrintLabDummyConsoleLine(
        "cleared %s team spawn \"%s\" for map %s. active=%s disk=%s",
        GetConfiguredTeamRoundName(teamId),
        removedSpot.name,
        GetCurrentMapName(),
        g_teamSpawnActiveSpotNames[teamId][0] != '\0' ? g_teamSpawnActiveSpotNames[teamId] : "none",
        savedToDisk ? g_teamSpawnSpotsPath : failureReason);
}

void ExpTeamSpawnListCommand()
{
    RefreshFutureHooksMapState();
    PrintTeamSpawnStatus();
}

void ExpTeamSpawnUseCommand()
{
    RefreshFutureHooksMapState();

    if (CMD_ARGC() < 3)
    {
        PrintLabDummyConsoleLine("usage: exp_team_spawn_use <team> <name>");
        return;
    }

    int teamId = kExpRoundTeamNone;
    char spotName[kMaxLabDummySpotNameLength];
    char failureReason[kMaxLabDummySpotFileFailureLength];
    if (!TryGetTeamSpawnCommandArgs(1, 2, &teamId, spotName, sizeof(spotName), false, failureReason, sizeof(failureReason)))
    {
        PrintLabDummyConsoleLine("team spawn use failed: %s", failureReason);
        return;
    }

    const TeamSpawnSavedSpotRecord *spot = FindTeamSpawnSavedSpotConst(teamId, spotName);
    if (spot == NULL)
    {
        PrintLabDummyConsoleLine(
            "team spawn use failed: %s team spawn \"%s\" was not found for map %s",
            GetConfiguredTeamRoundName(teamId),
            spotName,
            GetCurrentMapName());
        PrintTeamSpawnStatus();
        return;
    }

    strncpy_s(g_teamSpawnActiveSpotNames[teamId], sizeof(g_teamSpawnActiveSpotNames[teamId]), spot->name, _TRUNCATE);
    const bool savedToDisk = SaveTeamSpawnSpotsForCurrentMap(failureReason, sizeof(failureReason));
    LogTeamSpawnEvent(
        "team_spawn_use",
        FindFirstManagedPlayerOnRoundTeam(teamId, false),
        teamId,
        GetConfiguredTeamRoundName(teamId),
        spot->origin,
        spot->angles,
        kTeamSpawnSourceSavedSpot,
        "selected_active_spot",
        spot->name,
        GetTeamSpawnSpotStorageLabel(spot),
        savedToDisk ? "" : failureReason);
    PrintLabDummyConsoleLine(
        "active %s team spawn is now \"%s\" for map %s. Use exp_round_restart to respawn players there. disk=%s",
        GetConfiguredTeamRoundName(teamId),
        spot->name,
        GetCurrentMapName(),
        savedToDisk ? g_teamSpawnSpotsPath : failureReason);
}

void ExpTeamSpawnStatusCommand()
{
    RefreshFutureHooksMapState();
    PrintTeamSpawnStatus();
}

bool TryResolveLabDummySpotName(
    const char *requestedName,
    char *buffer,
    size_t bufferSize,
    bool useDefaultIfEmpty,
    char *failureReason,
    size_t failureReasonSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return false;
    }

    buffer[0] = '\0';
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    char trimmedName[kMaxLabDummySpotNameLength];
    TrimCfgRequestString(requestedName, trimmedName, sizeof(trimmedName));
    if (trimmedName[0] == '\0' && useDefaultIfEmpty)
    {
        strncpy_s(trimmedName, sizeof(trimmedName), kDefaultLabDummySpotName, _TRUNCATE);
    }

    if (trimmedName[0] == '\0')
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            strcpy_s(failureReason, failureReasonSize, "target spot name cannot be empty");
        }
        return false;
    }

    if (strlen(trimmedName) >= bufferSize)
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "target spot name \"%s\" is too long (max %u characters)",
                trimmedName,
                (unsigned int)(bufferSize - 1));
        }
        return false;
    }

    for (const char *cursor = trimmedName; *cursor != '\0'; ++cursor)
    {
        const unsigned char ch = (unsigned char)(*cursor);
        if (ch < 32 || *cursor == '"' || *cursor == '\\' || *cursor == '/')
        {
            if (failureReason != NULL && failureReasonSize > 0)
            {
                _snprintf_s(
                    failureReason,
                    failureReasonSize,
                    _TRUNCATE,
                    "target spot name \"%s\" contains unsupported characters",
                    trimmedName);
            }
            return false;
        }
    }

    strncpy_s(buffer, bufferSize, trimmedName, _TRUNCATE);
    return true;
}

void NormalizeSlashes(char *path, char separator)
{
    if (path == NULL)
    {
        return;
    }

    for (char *cursor = path; *cursor != '\0'; ++cursor)
    {
        if (*cursor == '\\' || *cursor == '/')
        {
            *cursor = separator;
        }
    }
}

bool HasAnyPathSeparator(const char *path)
{
    return path != NULL && (strchr(path, '\\') != NULL || strchr(path, '/') != NULL);
}

bool IsAbsoluteCfgPath(const char *path)
{
    if (path == NULL || path[0] == '\0')
    {
        return false;
    }

    return (strlen(path) >= 2 && path[1] == ':') ||
        (path[0] == '\\' && path[1] == '\\');
}

bool EndsWithCfgExtension(const char *path)
{
    if (path == NULL)
    {
        return false;
    }

    const size_t pathLength = strlen(path);
    return pathLength >= 4 && _stricmp(path + pathLength - 4, ".cfg") == 0;
}

bool FileExists(const char *path)
{
    return path != NULL && path[0] != '\0' && _access(path, 0) == 0;
}

bool TryGetFullPathString(const char *path, char *buffer, size_t bufferSize)
{
    if (path == NULL || path[0] == '\0' || buffer == NULL || bufferSize == 0)
    {
        return false;
    }

    DWORD length = GetFullPathNameA(path, (DWORD)bufferSize, buffer, NULL);
    return length > 0 && length < bufferSize;
}

bool IsPathWithinRoot(const char *path, const char *root)
{
    if (path == NULL || root == NULL || path[0] == '\0' || root[0] == '\0')
    {
        return false;
    }

    char normalizedPath[kMaxLiveCfgPathLength];
    char normalizedRoot[kMaxLiveCfgPathLength];
    strncpy_s(normalizedPath, sizeof(normalizedPath), path, _TRUNCATE);
    strncpy_s(normalizedRoot, sizeof(normalizedRoot), root, _TRUNCATE);
    NormalizeSlashes(normalizedPath, '\\');
    NormalizeSlashes(normalizedRoot, '\\');

    size_t rootLength = strlen(normalizedRoot);
    if (rootLength == 0)
    {
        return false;
    }

    if (normalizedRoot[rootLength - 1] != '\\')
    {
        strncat_s(normalizedRoot, sizeof(normalizedRoot), "\\", _TRUNCATE);
        rootLength = strlen(normalizedRoot);
    }

    return _strnicmp(normalizedPath, normalizedRoot, rootLength) == 0;
}

bool TryBuildRelativePathFromRoot(const char *path, const char *root, char *buffer, size_t bufferSize)
{
    if (!IsPathWithinRoot(path, root) || buffer == NULL || bufferSize == 0)
    {
        return false;
    }

    char normalizedPath[kMaxLiveCfgPathLength];
    char normalizedRoot[kMaxLiveCfgPathLength];
    strncpy_s(normalizedPath, sizeof(normalizedPath), path, _TRUNCATE);
    strncpy_s(normalizedRoot, sizeof(normalizedRoot), root, _TRUNCATE);
    NormalizeSlashes(normalizedPath, '\\');
    NormalizeSlashes(normalizedRoot, '\\');

    size_t rootLength = strlen(normalizedRoot);
    if (normalizedRoot[rootLength - 1] != '\\')
    {
        strncat_s(normalizedRoot, sizeof(normalizedRoot), "\\", _TRUNCATE);
        rootLength = strlen(normalizedRoot);
    }

    const char *relativePath = normalizedPath + rootLength;
    while (*relativePath == '\\')
    {
        ++relativePath;
    }

    strncpy_s(buffer, bufferSize, relativePath, _TRUNCATE);
    return buffer[0] != '\0';
}

bool TryGetLiveModRootPath(char *buffer, size_t bufferSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return false;
    }

    HMODULE moduleHandle = NULL;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&RegisterFutureGameplayCvars,
            &moduleHandle))
    {
        return false;
    }

    char modulePath[kMaxLiveCfgPathLength];
    DWORD length = GetModuleFileNameA(moduleHandle, modulePath, ARRAYSIZE(modulePath));
    if (length == 0 || length >= ARRAYSIZE(modulePath))
    {
        return false;
    }

    char *fileName = strrchr(modulePath, '\\');
    if (fileName == NULL)
    {
        return false;
    }

    *fileName = '\0';
    char *dllsDirectory = strrchr(modulePath, '\\');
    if (dllsDirectory == NULL || _stricmp(dllsDirectory + 1, "dlls") != 0)
    {
        return false;
    }

    *dllsDirectory = '\0';
    strncpy_s(buffer, bufferSize, modulePath, _TRUNCATE);
    return true;
}

bool IsSafeLabDummyMapFileName(const char *mapName)
{
    if (mapName == NULL || mapName[0] == '\0')
    {
        return false;
    }

    for (const char *cursor = mapName; *cursor != '\0'; ++cursor)
    {
        const unsigned char ch = (unsigned char)(*cursor);
        if (ch < 32 || *cursor == ':' || *cursor == '\\' || *cursor == '/' || *cursor == '"' || *cursor == '*' || *cursor == '?' || *cursor == '<' || *cursor == '>' || *cursor == '|')
        {
            return false;
        }
    }

    return true;
}

bool TryBuildLabDummyTargetSpotsDirectoryPath(char *buffer, size_t bufferSize, char *failureReason, size_t failureReasonSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return false;
    }

    buffer[0] = '\0';
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    char modRoot[kMaxLiveCfgPathLength];
    if (!TryGetLiveModRootPath(modRoot, sizeof(modRoot)))
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            strcpy_s(failureReason, failureReasonSize, "could not resolve the active hlserver_testbed mod root from hl.dll");
        }
        return false;
    }

    _snprintf_s(buffer, bufferSize, _TRUNCATE, "%s\\%s", modRoot, kLabDummyTargetSpotsDirectoryName);
    return buffer[0] != '\0';
}

bool TryBuildCurrentLabDummyTargetSpotsPath(char *buffer, size_t bufferSize, char *failureReason, size_t failureReasonSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return false;
    }

    buffer[0] = '\0';
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    const char *mapName = GetCurrentMapName();
    if (!IsSafeLabDummyMapFileName(mapName))
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "could not resolve a safe target-spot file name for map \"%s\"",
                GetValueOrFallback(mapName, ""));
        }
        return false;
    }

    char directoryPath[kMaxLiveCfgPathLength];
    if (!TryBuildLabDummyTargetSpotsDirectoryPath(directoryPath, sizeof(directoryPath), failureReason, failureReasonSize))
    {
        return false;
    }

    _snprintf_s(buffer, bufferSize, _TRUNCATE, "%s\\%s.json", directoryPath, mapName);
    return buffer[0] != '\0';
}

bool TryEnsureLabDummyDirectoryExists(const char *path, char *failureReason, size_t failureReasonSize)
{
    if (path == NULL || path[0] == '\0')
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            strcpy_s(failureReason, failureReasonSize, "target spots directory path is empty");
        }
        return false;
    }

    if (CreateDirectoryA(path, NULL))
    {
        return true;
    }

    const DWORD error = GetLastError();
    if (error == ERROR_ALREADY_EXISTS)
    {
        return true;
    }

    if (failureReason != NULL && failureReasonSize > 0)
    {
        _snprintf_s(
            failureReason,
            failureReasonSize,
            _TRUNCATE,
            "could not create target spots directory %s (win32=%lu)",
            path,
            (unsigned long)error);
    }
    return false;
}

int FindLabDummySavedSpotIndex(const char *spotName)
{
    if (spotName == NULL || spotName[0] == '\0')
    {
        return -1;
    }

    for (int spotIndex = 0; spotIndex < g_glockLabDummySavedSpotCount; ++spotIndex)
    {
        if (g_glockLabDummySavedSpots[spotIndex].valid && StringEqualsIgnoreCase(g_glockLabDummySavedSpots[spotIndex].name, spotName))
        {
            return spotIndex;
        }
    }

    return -1;
}

LabDummySavedSpotRecord *FindLabDummySavedSpot(const char *spotName)
{
    const int spotIndex = FindLabDummySavedSpotIndex(spotName);
    return spotIndex >= 0 ? &g_glockLabDummySavedSpots[spotIndex] : NULL;
}

const LabDummySavedSpotRecord *FindLabDummySavedSpotConst(const char *spotName)
{
    const int spotIndex = FindLabDummySavedSpotIndex(spotName);
    return spotIndex >= 0 ? &g_glockLabDummySavedSpots[spotIndex] : NULL;
}

const LabDummySavedSpotRecord *GetLabDummyActiveSavedSpot()
{
    return g_glockLabDummyActiveSpotName[0] != '\0' ? FindLabDummySavedSpotConst(g_glockLabDummyActiveSpotName) : NULL;
}

const LabDummySavedSpotRecord *GetLabDummyDefaultSavedSpot()
{
    return FindLabDummySavedSpotConst(kDefaultLabDummySpotName);
}

const LabDummySavedSpotRecord *GetPreferredLabDummySavedSpot(bool *usedDefaultFallback)
{
    if (usedDefaultFallback != NULL)
    {
        *usedDefaultFallback = false;
    }

    const LabDummySavedSpotRecord *activeSpot = GetLabDummyActiveSavedSpot();
    if (activeSpot != NULL)
    {
        return activeSpot;
    }

    if (g_glockLabDummyActiveSpotName[0] != '\0')
    {
        return NULL;
    }

    const LabDummySavedSpotRecord *defaultSpot = GetLabDummyDefaultSavedSpot();
    if (defaultSpot != NULL && usedDefaultFallback != NULL)
    {
        *usedDefaultFallback = true;
    }

    return defaultSpot;
}

const char *GetLabDummySpotStorageLabel(const LabDummySavedSpotRecord *spot)
{
    if (spot == NULL || !spot->valid)
    {
        return "";
    }

    return spot->loadedFromDisk ? kLabDummySpotStorageDisk : kLabDummySpotStorageSession;
}

void CopyLabDummySavedSpotToSelection(const LabDummySavedSpotRecord &spot, CBasePlayer *pAnchorPlayer, LabDummySpawnSelection *selection)
{
    if (selection == NULL)
    {
        return;
    }

    selection->origin = spot.origin;
    selection->angles = spot.angles;
    selection->anchorPlayer = pAnchorPlayer;
    strncpy_s(selection->source, sizeof(selection->source), kLabDummySourceSavedSpot, _TRUNCATE);
    strncpy_s(selection->candidate, sizeof(selection->candidate), spot.candidate[0] != '\0' ? spot.candidate : "marked_spot", _TRUNCATE);
    strncpy_s(selection->spotName, sizeof(selection->spotName), spot.name, _TRUNCATE);
    strncpy_s(selection->spotStorage, sizeof(selection->spotStorage), GetLabDummySpotStorageLabel(&spot), _TRUNCATE);
}

void BuildLabDummySpotNamesSummary(char *buffer, size_t bufferSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return;
    }

    buffer[0] = '\0';
    for (int spotIndex = 0; spotIndex < g_glockLabDummySavedSpotCount; ++spotIndex)
    {
        const LabDummySavedSpotRecord &spot = g_glockLabDummySavedSpots[spotIndex];
        if (!spot.valid)
        {
            continue;
        }

        if (buffer[0] != '\0')
        {
            strncat_s(buffer, bufferSize, ", ", _TRUNCATE);
        }

        strncat_s(buffer, bufferSize, spot.name, _TRUNCATE);
        if (StringEqualsIgnoreCase(g_glockLabDummyActiveSpotName, spot.name))
        {
            strncat_s(buffer, bufferSize, " [active]", _TRUNCATE);
        }
    }

    if (buffer[0] == '\0')
    {
        strcpy_s(buffer, bufferSize, "none");
    }
}

std::string EscapeLabDummyJsonString(const char *value)
{
    std::string escaped;
    const char *source = value != NULL ? value : "";
    for (const char *cursor = source; *cursor != '\0'; ++cursor)
    {
        switch (*cursor)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += *cursor;
            break;
        }
    }

    return escaped;
}

struct LabDummyJsonCursor
{
    const char *text;
    size_t length;
    size_t position;
};

bool SetLabDummyJsonParseFailure(std::string *failureReason, size_t position, const char *message)
{
    if (failureReason != NULL)
    {
        char buffer[256];
        _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, "json parse error at byte %u: %s", (unsigned int)position, GetValueOrFallback(message, "invalid value"));
        *failureReason = buffer;
    }

    return false;
}

void SkipLabDummyJsonWhitespace(LabDummyJsonCursor *cursor)
{
    if (cursor == NULL || cursor->text == NULL)
    {
        return;
    }

    while (cursor->position < cursor->length && isspace((unsigned char)cursor->text[cursor->position]))
    {
        ++cursor->position;
    }
}

bool TryConsumeLabDummyJsonChar(LabDummyJsonCursor *cursor, char expected)
{
    SkipLabDummyJsonWhitespace(cursor);
    if (cursor == NULL || cursor->text == NULL || cursor->position >= cursor->length || cursor->text[cursor->position] != expected)
    {
        return false;
    }

    ++cursor->position;
    return true;
}

int GetLabDummyJsonHexValue(char ch)
{
    if (ch >= '0' && ch <= '9')
    {
        return ch - '0';
    }

    if (ch >= 'a' && ch <= 'f')
    {
        return 10 + (ch - 'a');
    }

    if (ch >= 'A' && ch <= 'F')
    {
        return 10 + (ch - 'A');
    }

    return -1;
}

bool TryParseLabDummyJsonString(LabDummyJsonCursor *cursor, std::string *value, std::string *failureReason)
{
    SkipLabDummyJsonWhitespace(cursor);
    if (cursor == NULL || cursor->text == NULL || cursor->position >= cursor->length || cursor->text[cursor->position] != '"')
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor != NULL ? cursor->position : 0, "expected a JSON string");
    }

    ++cursor->position;
    std::string parsedValue;
    while (cursor->position < cursor->length)
    {
        char ch = cursor->text[cursor->position++];
        if (ch == '"')
        {
            if (value != NULL)
            {
                *value = parsedValue;
            }
            return true;
        }

        if (ch != '\\')
        {
            parsedValue += ch;
            continue;
        }

        if (cursor->position >= cursor->length)
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor->position, "unterminated JSON escape sequence");
        }

        const char escape = cursor->text[cursor->position++];
        switch (escape)
        {
        case '"':
        case '\\':
        case '/':
            parsedValue += escape;
            break;
        case 'b':
            parsedValue += '\b';
            break;
        case 'f':
            parsedValue += '\f';
            break;
        case 'n':
            parsedValue += '\n';
            break;
        case 'r':
            parsedValue += '\r';
            break;
        case 't':
            parsedValue += '\t';
            break;
        case 'u':
        {
            if (cursor->position + 4 > cursor->length)
            {
                return SetLabDummyJsonParseFailure(failureReason, cursor->position, "incomplete \\u escape sequence");
            }

            int codePoint = 0;
            for (int digitIndex = 0; digitIndex < 4; ++digitIndex)
            {
                const int digitValue = GetLabDummyJsonHexValue(cursor->text[cursor->position++]);
                if (digitValue < 0)
                {
                    return SetLabDummyJsonParseFailure(failureReason, cursor->position, "invalid hex digit in \\u escape sequence");
                }

                codePoint = (codePoint << 4) | digitValue;
            }

            parsedValue += codePoint >= 32 && codePoint <= 126 ? (char)codePoint : '?';
            break;
        }
        default:
            return SetLabDummyJsonParseFailure(failureReason, cursor->position, "unsupported JSON escape sequence");
        }
    }

    return SetLabDummyJsonParseFailure(failureReason, cursor->position, "unterminated JSON string");
}

bool TryParseLabDummyJsonNumber(LabDummyJsonCursor *cursor, double *value, std::string *failureReason)
{
    SkipLabDummyJsonWhitespace(cursor);
    if (cursor == NULL || cursor->text == NULL || cursor->position >= cursor->length)
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor != NULL ? cursor->position : 0, "expected a JSON number");
    }

    errno = 0;
    char *endPointer = NULL;
    const char *startPointer = cursor->text + cursor->position;
    const double parsedValue = strtod(startPointer, &endPointer);
    if (endPointer == startPointer)
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor->position, "expected a JSON number");
    }

    if (errno == ERANGE)
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor->position, "numeric value is out of range");
    }

    cursor->position += (size_t)(endPointer - startPointer);
    if (value != NULL)
    {
        *value = parsedValue;
    }
    return true;
}

bool TryParseLabDummyJsonLiteral(LabDummyJsonCursor *cursor, const char *literal, std::string *failureReason)
{
    SkipLabDummyJsonWhitespace(cursor);
    if (cursor == NULL || cursor->text == NULL || literal == NULL)
    {
        return false;
    }

    const size_t literalLength = strlen(literal);
    if (cursor->position + literalLength > cursor->length || strncmp(cursor->text + cursor->position, literal, literalLength) != 0)
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor->position, "unexpected JSON literal");
    }

    cursor->position += literalLength;
    return true;
}

bool TrySkipLabDummyJsonValue(LabDummyJsonCursor *cursor, std::string *failureReason);

bool TrySkipLabDummyJsonObject(LabDummyJsonCursor *cursor, std::string *failureReason)
{
    if (!TryConsumeLabDummyJsonChar(cursor, '{'))
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor != NULL ? cursor->position : 0, "expected '{'");
    }

    SkipLabDummyJsonWhitespace(cursor);
    if (TryConsumeLabDummyJsonChar(cursor, '}'))
    {
        return true;
    }

    while (true)
    {
        std::string key;
        if (!TryParseLabDummyJsonString(cursor, &key, failureReason))
        {
            return false;
        }

        if (!TryConsumeLabDummyJsonChar(cursor, ':'))
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor->position, "expected ':' after object key");
        }

        if (!TrySkipLabDummyJsonValue(cursor, failureReason))
        {
            return false;
        }

        if (TryConsumeLabDummyJsonChar(cursor, '}'))
        {
            return true;
        }

        if (!TryConsumeLabDummyJsonChar(cursor, ','))
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor->position, "expected ',' or '}' in object");
        }
    }
}

bool TrySkipLabDummyJsonArray(LabDummyJsonCursor *cursor, std::string *failureReason)
{
    if (!TryConsumeLabDummyJsonChar(cursor, '['))
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor != NULL ? cursor->position : 0, "expected '['");
    }

    SkipLabDummyJsonWhitespace(cursor);
    if (TryConsumeLabDummyJsonChar(cursor, ']'))
    {
        return true;
    }

    while (true)
    {
        if (!TrySkipLabDummyJsonValue(cursor, failureReason))
        {
            return false;
        }

        if (TryConsumeLabDummyJsonChar(cursor, ']'))
        {
            return true;
        }

        if (!TryConsumeLabDummyJsonChar(cursor, ','))
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor->position, "expected ',' or ']' in array");
        }
    }
}

bool TrySkipLabDummyJsonValue(LabDummyJsonCursor *cursor, std::string *failureReason)
{
    SkipLabDummyJsonWhitespace(cursor);
    if (cursor == NULL || cursor->text == NULL || cursor->position >= cursor->length)
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor != NULL ? cursor->position : 0, "expected a JSON value");
    }

    const char ch = cursor->text[cursor->position];
    if (ch == '"')
    {
        std::string ignored;
        return TryParseLabDummyJsonString(cursor, &ignored, failureReason);
    }

    if (ch == '{')
    {
        return TrySkipLabDummyJsonObject(cursor, failureReason);
    }

    if (ch == '[')
    {
        return TrySkipLabDummyJsonArray(cursor, failureReason);
    }

    if (ch == '-' || (ch >= '0' && ch <= '9'))
    {
        double ignored = 0.0;
        return TryParseLabDummyJsonNumber(cursor, &ignored, failureReason);
    }

    if (ch == 't')
    {
        return TryParseLabDummyJsonLiteral(cursor, "true", failureReason);
    }

    if (ch == 'f')
    {
        return TryParseLabDummyJsonLiteral(cursor, "false", failureReason);
    }

    if (ch == 'n')
    {
        return TryParseLabDummyJsonLiteral(cursor, "null", failureReason);
    }

    return SetLabDummyJsonParseFailure(failureReason, cursor->position, "unexpected JSON token");
}

bool TryParseLabDummyJsonVector3(LabDummyJsonCursor *cursor, Vector *value, std::string *failureReason)
{
    if (!TryConsumeLabDummyJsonChar(cursor, '['))
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor != NULL ? cursor->position : 0, "expected '[' for origin");
    }

    double coordinates[3] = {};
    for (int coordinateIndex = 0; coordinateIndex < 3; ++coordinateIndex)
    {
        if (!TryParseLabDummyJsonNumber(cursor, &coordinates[coordinateIndex], failureReason))
        {
            return false;
        }

        if (coordinateIndex < 2)
        {
            if (!TryConsumeLabDummyJsonChar(cursor, ','))
            {
                return SetLabDummyJsonParseFailure(failureReason, cursor->position, "expected ',' between origin coordinates");
            }
        }
    }

    if (!TryConsumeLabDummyJsonChar(cursor, ']'))
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor->position, "expected ']'");
    }

    if (value != NULL)
    {
        *value = Vector((float)coordinates[0], (float)coordinates[1], (float)coordinates[2]);
    }

    return true;
}

bool TryParseLabDummySpotObject(LabDummyJsonCursor *cursor, LabDummySavedSpotRecord *spot, std::string *failureReason)
{
    if (spot == NULL)
    {
        return false;
    }

    ClearLabDummySavedSpotRecord(spot);
    if (!TryConsumeLabDummyJsonChar(cursor, '{'))
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor != NULL ? cursor->position : 0, "expected '{' for target spot");
    }

    bool hasName = false;
    bool hasOrigin = false;
    bool hasYaw = false;

    SkipLabDummyJsonWhitespace(cursor);
    if (TryConsumeLabDummyJsonChar(cursor, '}'))
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor->position, "target spot object is missing required fields");
    }

    while (true)
    {
        std::string key;
        if (!TryParseLabDummyJsonString(cursor, &key, failureReason))
        {
            return false;
        }

        if (!TryConsumeLabDummyJsonChar(cursor, ':'))
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor->position, "expected ':' after target spot key");
        }

        if (key == "name")
        {
            std::string name;
            if (!TryParseLabDummyJsonString(cursor, &name, failureReason))
            {
                return false;
            }

            strncpy_s(spot->name, sizeof(spot->name), name.c_str(), _TRUNCATE);
            hasName = true;
        }
        else if (key == "origin")
        {
            if (!TryParseLabDummyJsonVector3(cursor, &spot->origin, failureReason))
            {
                return false;
            }

            hasOrigin = true;
        }
        else if (key == "yaw")
        {
            double yawValue = 0.0;
            if (!TryParseLabDummyJsonNumber(cursor, &yawValue, failureReason))
            {
                return false;
            }

            spot->angles = Vector(0.0f, (float)yawValue, 0.0f);
            hasYaw = true;
        }
        else if (key == "candidate")
        {
            std::string candidate;
            if (!TryParseLabDummyJsonString(cursor, &candidate, failureReason))
            {
                return false;
            }

            strncpy_s(spot->candidate, sizeof(spot->candidate), candidate.c_str(), _TRUNCATE);
        }
        else if (key == "created_at")
        {
            std::string createdAt;
            if (!TryParseLabDummyJsonString(cursor, &createdAt, failureReason))
            {
                return false;
            }

            strncpy_s(spot->createdAt, sizeof(spot->createdAt), createdAt.c_str(), _TRUNCATE);
        }
        else if (key == "updated_at")
        {
            std::string updatedAt;
            if (!TryParseLabDummyJsonString(cursor, &updatedAt, failureReason))
            {
                return false;
            }

            strncpy_s(spot->updatedAt, sizeof(spot->updatedAt), updatedAt.c_str(), _TRUNCATE);
        }
        else if (key == "note")
        {
            std::string note;
            if (!TryParseLabDummyJsonString(cursor, &note, failureReason))
            {
                return false;
            }

            strncpy_s(spot->note, sizeof(spot->note), note.c_str(), _TRUNCATE);
        }
        else
        {
            if (!TrySkipLabDummyJsonValue(cursor, failureReason))
            {
                return false;
            }
        }

        if (TryConsumeLabDummyJsonChar(cursor, '}'))
        {
            break;
        }

        if (!TryConsumeLabDummyJsonChar(cursor, ','))
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor->position, "expected ',' or '}' in target spot object");
        }
    }

    if (!hasName || !hasOrigin || !hasYaw)
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor->position, "target spot object is missing name, origin, or yaw");
    }

    spot->valid = true;
    spot->loadedFromDisk = true;
    if (spot->updatedAt[0] == '\0' && spot->createdAt[0] != '\0')
    {
        strncpy_s(spot->updatedAt, sizeof(spot->updatedAt), spot->createdAt, _TRUNCATE);
    }
    if (spot->createdAt[0] == '\0' && spot->updatedAt[0] != '\0')
    {
        strncpy_s(spot->createdAt, sizeof(spot->createdAt), spot->updatedAt, _TRUNCATE);
    }
    return true;
}

bool TryParseLabDummySpotsFileText(const std::string &jsonText, std::vector<LabDummySavedSpotRecord> *spots, std::string *activeSpotName, std::string *failureReason)
{
    if (spots == NULL)
    {
        return false;
    }

    spots->clear();
    if (activeSpotName != NULL)
    {
        activeSpotName->clear();
    }

    LabDummyJsonCursor cursor = {jsonText.c_str(), jsonText.length(), 0};
    if (!TryConsumeLabDummyJsonChar(&cursor, '{'))
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected a JSON object at the root");
    }

    SkipLabDummyJsonWhitespace(&cursor);
    if (TryConsumeLabDummyJsonChar(&cursor, '}'))
    {
        return true;
    }

    while (true)
    {
        std::string key;
        if (!TryParseLabDummyJsonString(&cursor, &key, failureReason))
        {
            return false;
        }

        if (!TryConsumeLabDummyJsonChar(&cursor, ':'))
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected ':' after root key");
        }

        if (key == "active_spot")
        {
            std::string name;
            if (!TryParseLabDummyJsonString(&cursor, &name, failureReason))
            {
                return false;
            }

            if (activeSpotName != NULL)
            {
                *activeSpotName = name;
            }
        }
        else if (key == "spots")
        {
            if (!TryConsumeLabDummyJsonChar(&cursor, '['))
            {
                return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected '[' for spots array");
            }

            SkipLabDummyJsonWhitespace(&cursor);
            if (!TryConsumeLabDummyJsonChar(&cursor, ']'))
            {
                while (true)
                {
                    LabDummySavedSpotRecord spot = {};
                    if (!TryParseLabDummySpotObject(&cursor, &spot, failureReason))
                    {
                        return false;
                    }

                    for (size_t existingIndex = 0; existingIndex < spots->size(); ++existingIndex)
                    {
                        if (StringEqualsIgnoreCase((*spots)[existingIndex].name, spot.name))
                        {
                            char duplicateMessage[256];
                            _snprintf_s(duplicateMessage, sizeof(duplicateMessage), _TRUNCATE, "duplicate target spot name \"%s\"", spot.name);
                            return SetLabDummyJsonParseFailure(failureReason, cursor.position, duplicateMessage);
                        }
                    }

                    spots->push_back(spot);
                    if (TryConsumeLabDummyJsonChar(&cursor, ']'))
                    {
                        break;
                    }

                    if (!TryConsumeLabDummyJsonChar(&cursor, ','))
                    {
                        return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected ',' or ']' in spots array");
                    }
                }
            }
        }
        else
        {
            if (!TrySkipLabDummyJsonValue(&cursor, failureReason))
            {
                return false;
            }
        }

        if (TryConsumeLabDummyJsonChar(&cursor, '}'))
        {
            break;
        }

        if (!TryConsumeLabDummyJsonChar(&cursor, ','))
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected ',' or '}' in root object");
        }
    }

    SkipLabDummyJsonWhitespace(&cursor);
    if (cursor.position != cursor.length)
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor.position, "unexpected trailing content after JSON object");
    }

    return true;
}

bool TryReadLabDummySpotsFile(const char *path, std::string *fileContents, char *failureReason, size_t failureReasonSize)
{
    if (fileContents == NULL)
    {
        return false;
    }

    fileContents->clear();
    if (path == NULL || path[0] == '\0')
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            strcpy_s(failureReason, failureReasonSize, "target spots file path is empty");
        }
        return false;
    }

    FILE *file = NULL;
    if (fopen_s(&file, path, "rb") != 0 || file == NULL)
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(failureReason, failureReasonSize, _TRUNCATE, "could not open target spots file %s", path);
        }
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(failureReason, failureReasonSize, _TRUNCATE, "could not read the size of target spots file %s", path);
        }
        return false;
    }

    const long fileSize = ftell(file);
    if (fileSize < 0)
    {
        fclose(file);
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(failureReason, failureReasonSize, _TRUNCATE, "could not determine the size of target spots file %s", path);
        }
        return false;
    }

    if ((size_t)fileSize > kMaxLabDummySpotFileSize)
    {
        fclose(file);
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "target spots file %s is too large (%ld bytes, max %u)",
                path,
                fileSize,
                (unsigned int)kMaxLabDummySpotFileSize);
        }
        return false;
    }

    if (fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(failureReason, failureReasonSize, _TRUNCATE, "could not rewind target spots file %s", path);
        }
        return false;
    }

    fileContents->assign((size_t)fileSize, '\0');
    const size_t bytesRead = fileSize > 0 ? fread(&(*fileContents)[0], 1, (size_t)fileSize, file) : 0;
    fclose(file);

    if ((size_t)fileSize != bytesRead)
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "could not read target spots file %s (expected %ld bytes, got %u)",
                path,
                fileSize,
                (unsigned int)bytesRead);
        }
        fileContents->clear();
        return false;
    }

    return true;
}

bool SaveLabDummySpotsForCurrentMap(char *failureReason, size_t failureReasonSize)
{
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    char filePath[kMaxLiveCfgPathLength];
    if (!TryBuildCurrentLabDummyTargetSpotsPath(filePath, sizeof(filePath), failureReason, failureReasonSize))
    {
        return false;
    }

    strncpy_s(g_glockLabDummyTargetSpotsPath, sizeof(g_glockLabDummyTargetSpotsPath), filePath, _TRUNCATE);

    if (g_glockLabDummyActiveSpotName[0] != '\0' && FindLabDummySavedSpotIndex(g_glockLabDummyActiveSpotName) < 0)
    {
        g_glockLabDummyActiveSpotName[0] = '\0';
    }

    if (g_glockLabDummySavedSpotCount <= 0)
    {
        if (DeleteFileA(filePath) || GetLastError() == ERROR_FILE_NOT_FOUND)
        {
            return true;
        }

        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "could not remove empty target-spots file %s (win32=%lu)",
                filePath,
                (unsigned long)GetLastError());
        }
        return false;
    }

    char directoryPath[kMaxLiveCfgPathLength];
    if (!TryBuildLabDummyTargetSpotsDirectoryPath(directoryPath, sizeof(directoryPath), failureReason, failureReasonSize))
    {
        return false;
    }

    if (!TryEnsureLabDummyDirectoryExists(directoryPath, failureReason, failureReasonSize))
    {
        return false;
    }

    std::string jsonText;
    jsonText += "{\n";
    jsonText += "  \"map\": \"";
    jsonText += EscapeLabDummyJsonString(GetCurrentMapName());
    jsonText += "\",\n";
    jsonText += "  \"active_spot\": \"";
    jsonText += EscapeLabDummyJsonString(g_glockLabDummyActiveSpotName);
    jsonText += "\",\n";
    jsonText += "  \"spots\": [\n";

    for (int spotIndex = 0; spotIndex < g_glockLabDummySavedSpotCount; ++spotIndex)
    {
        const LabDummySavedSpotRecord &spot = g_glockLabDummySavedSpots[spotIndex];
        if (!spot.valid)
        {
            continue;
        }

        char numberBuffer[96];
        _snprintf_s(
            numberBuffer,
            sizeof(numberBuffer),
            _TRUNCATE,
            "      \"origin\": [%.3f, %.3f, %.3f],\n      \"yaw\": %.3f,\n",
            spot.origin.x,
            spot.origin.y,
            spot.origin.z,
            spot.angles.y);

        jsonText += "    {\n";
        jsonText += "      \"name\": \"";
        jsonText += EscapeLabDummyJsonString(spot.name);
        jsonText += "\",\n";
        jsonText += numberBuffer;
        jsonText += "      \"candidate\": \"";
        jsonText += EscapeLabDummyJsonString(spot.candidate);
        jsonText += "\",\n";
        jsonText += "      \"created_at\": \"";
        jsonText += EscapeLabDummyJsonString(spot.createdAt);
        jsonText += "\",\n";
        jsonText += "      \"updated_at\": \"";
        jsonText += EscapeLabDummyJsonString(spot.updatedAt);
        jsonText += "\"";
        if (spot.note[0] != '\0')
        {
            jsonText += ",\n      \"note\": \"";
            jsonText += EscapeLabDummyJsonString(spot.note);
            jsonText += "\"";
        }
        jsonText += "\n    }";
        if (spotIndex + 1 < g_glockLabDummySavedSpotCount)
        {
            jsonText += ",";
        }
        jsonText += "\n";
    }

    jsonText += "  ]\n";
    jsonText += "}\n";

    char tempPath[kMaxLiveCfgPathLength];
    _snprintf_s(tempPath, sizeof(tempPath), _TRUNCATE, "%s.tmp", filePath);

    FILE *file = NULL;
    if (fopen_s(&file, tempPath, "wb") != 0 || file == NULL)
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(failureReason, failureReasonSize, _TRUNCATE, "could not write temporary target-spots file %s", tempPath);
        }
        return false;
    }

    const size_t bytesWritten = fwrite(jsonText.data(), 1, jsonText.length(), file);
    fclose(file);
    if (bytesWritten != jsonText.length())
    {
        DeleteFileA(tempPath);
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "could not fully write target-spots file %s (expected %u bytes, wrote %u)",
                tempPath,
                (unsigned int)jsonText.length(),
                (unsigned int)bytesWritten);
        }
        return false;
    }

    if (!MoveFileExA(tempPath, filePath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
    {
        const DWORD error = GetLastError();
        DeleteFileA(tempPath);
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "could not finalize target-spots file %s (win32=%lu)",
                filePath,
                (unsigned long)error);
        }
        return false;
    }

    return true;
}

void LoadLabDummySpotsForCurrentMap()
{
    ClearAllLabDummySavedSpots();

    char filePath[kMaxLiveCfgPathLength];
    char failureReason[kMaxLabDummySpotFileFailureLength];
    if (!TryBuildCurrentLabDummyTargetSpotsPath(filePath, sizeof(filePath), failureReason, sizeof(failureReason)))
    {
        strncpy_s(g_glockLabDummyTargetSpotsLoadFailure, sizeof(g_glockLabDummyTargetSpotsLoadFailure), failureReason, _TRUNCATE);
        PrintLabDummyConsoleLine("target spots load failed: %s", failureReason);
        return;
    }

    strncpy_s(g_glockLabDummyTargetSpotsPath, sizeof(g_glockLabDummyTargetSpotsPath), filePath, _TRUNCATE);
    if (!FileExists(filePath))
    {
        return;
    }

    std::string fileContents;
    if (!TryReadLabDummySpotsFile(filePath, &fileContents, failureReason, sizeof(failureReason)))
    {
        strncpy_s(g_glockLabDummyTargetSpotsLoadFailure, sizeof(g_glockLabDummyTargetSpotsLoadFailure), failureReason, _TRUNCATE);
        PrintLabDummyConsoleLine("target spots load failed for map %s: %s", GetCurrentMapName(), failureReason);
        return;
    }

    std::vector<LabDummySavedSpotRecord> loadedSpots;
    std::string activeSpotName;
    std::string parseFailure;
    if (!TryParseLabDummySpotsFileText(fileContents, &loadedSpots, &activeSpotName, &parseFailure))
    {
        strncpy_s(g_glockLabDummyTargetSpotsLoadFailure, sizeof(g_glockLabDummyTargetSpotsLoadFailure), parseFailure.c_str(), _TRUNCATE);
        PrintLabDummyConsoleLine("target spots load failed for map %s: %s", GetCurrentMapName(), parseFailure.c_str());
        return;
    }

    if (loadedSpots.size() > kMaxLabDummySpotsPerMap)
    {
        _snprintf_s(
            g_glockLabDummyTargetSpotsLoadFailure,
            sizeof(g_glockLabDummyTargetSpotsLoadFailure),
            _TRUNCATE,
            "target spots file has %u entries, but only %u are supported",
            (unsigned int)loadedSpots.size(),
            (unsigned int)kMaxLabDummySpotsPerMap);
        PrintLabDummyConsoleLine("target spots load failed for map %s: %s", GetCurrentMapName(), g_glockLabDummyTargetSpotsLoadFailure);
        return;
    }

    for (size_t spotIndex = 0; spotIndex < loadedSpots.size(); ++spotIndex)
    {
        g_glockLabDummySavedSpots[spotIndex] = loadedSpots[spotIndex];
    }

    g_glockLabDummySavedSpotCount = (int)loadedSpots.size();
    g_glockLabDummyTargetSpotsLoadFailure[0] = '\0';

    if (!activeSpotName.empty())
    {
        if (FindLabDummySavedSpotIndex(activeSpotName.c_str()) >= 0)
        {
            strncpy_s(g_glockLabDummyActiveSpotName, sizeof(g_glockLabDummyActiveSpotName), activeSpotName.c_str(), _TRUNCATE);
        }
        else
        {
            _snprintf_s(
                g_glockLabDummyTargetSpotsLoadFailure,
                sizeof(g_glockLabDummyTargetSpotsLoadFailure),
                _TRUNCATE,
                "active target spot \"%s\" was not found in %s; cleared active selection",
                activeSpotName.c_str(),
                filePath);
            PrintLabDummyConsoleLine("target spots load warning: %s", g_glockLabDummyTargetSpotsLoadFailure);
        }
    }

    if (g_glockLabDummySavedSpotCount > 0)
    {
        PrintLabDummyConsoleLine(
            "loaded %d persisted target spot(s) for map %s from %s%s%s",
            g_glockLabDummySavedSpotCount,
            GetCurrentMapName(),
            filePath,
            g_glockLabDummyActiveSpotName[0] != '\0' ? " active=" : "",
            g_glockLabDummyActiveSpotName[0] != '\0' ? g_glockLabDummyActiveSpotName : "");
    }
}

LabDummySavedSpotRecord *UpsertLabDummySavedSpot(
    const char *spotName,
    const Vector &origin,
    const Vector &angles,
    const char *candidate,
    char *failureReason,
    size_t failureReasonSize)
{
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    int spotIndex = FindLabDummySavedSpotIndex(spotName);
    if (spotIndex < 0)
    {
        if (g_glockLabDummySavedSpotCount >= ARRAYSIZE(g_glockLabDummySavedSpots))
        {
            if (failureReason != NULL && failureReasonSize > 0)
            {
                _snprintf_s(
                    failureReason,
                    failureReasonSize,
                    _TRUNCATE,
                    "cannot store target spot \"%s\": this map already has %u saved spots",
                    GetValueOrFallback(spotName, ""),
                    (unsigned int)ARRAYSIZE(g_glockLabDummySavedSpots));
            }
            return NULL;
        }

        spotIndex = g_glockLabDummySavedSpotCount++;
        ClearLabDummySavedSpotRecord(&g_glockLabDummySavedSpots[spotIndex]);
    }

    LabDummySavedSpotRecord *spot = &g_glockLabDummySavedSpots[spotIndex];
    char timestamp[kMaxLabDummySpotTimestampLength];
    FormatFutureGameplayTimestamp(timestamp, sizeof(timestamp));

    char createdAt[kMaxLabDummySpotTimestampLength];
    strncpy_s(createdAt, sizeof(createdAt), spot->createdAt[0] != '\0' ? spot->createdAt : timestamp, _TRUNCATE);
    char note[kMaxLabDummySpotNoteLength];
    strncpy_s(note, sizeof(note), spot->note, _TRUNCATE);

    StoreLabDummySavedSpotRecord(
        spot,
        spotName,
        origin,
        angles,
        candidate,
        createdAt,
        timestamp,
        note,
        false);
    return spot;
}

bool RemoveLabDummySavedSpot(const char *spotName, LabDummySavedSpotRecord *removedSpot, char *failureReason, size_t failureReasonSize)
{
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    const int spotIndex = FindLabDummySavedSpotIndex(spotName);
    if (spotIndex < 0)
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "target spot \"%s\" does not exist for map %s",
                GetValueOrFallback(spotName, ""),
                GetCurrentMapName());
        }
        return false;
    }

    if (removedSpot != NULL)
    {
        *removedSpot = g_glockLabDummySavedSpots[spotIndex];
    }

    for (int index = spotIndex; index + 1 < g_glockLabDummySavedSpotCount; ++index)
    {
        g_glockLabDummySavedSpots[index] = g_glockLabDummySavedSpots[index + 1];
    }

    if (g_glockLabDummySavedSpotCount > 0)
    {
        --g_glockLabDummySavedSpotCount;
        ClearLabDummySavedSpotRecord(&g_glockLabDummySavedSpots[g_glockLabDummySavedSpotCount]);
    }

    if (StringEqualsIgnoreCase(g_glockLabDummyActiveSpotName, spotName))
    {
        g_glockLabDummyActiveSpotName[0] = '\0';
    }

    return true;
}

bool TryGetLabDummySpotCommandName(int firstArgIndex, char *spotName, size_t spotNameSize, bool useDefaultIfEmpty, char *failureReason, size_t failureReasonSize)
{
    char requestedName[kMaxLabDummySpotNameLength];
    BuildCommandArgumentString(firstArgIndex, requestedName, sizeof(requestedName));
    return TryResolveLabDummySpotName(requestedName, spotName, spotNameSize, useDefaultIfEmpty, failureReason, failureReasonSize);
}

bool TryResolveTeamSpawnSpotName(
    const char *requestedName,
    char *buffer,
    size_t bufferSize,
    bool useDefaultIfEmpty,
    char *failureReason,
    size_t failureReasonSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return false;
    }

    buffer[0] = '\0';
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    char trimmedName[kMaxLabDummySpotNameLength];
    TrimCfgRequestString(requestedName, trimmedName, sizeof(trimmedName));
    if (trimmedName[0] == '\0' && useDefaultIfEmpty)
    {
        strncpy_s(trimmedName, sizeof(trimmedName), kDefaultLabDummySpotName, _TRUNCATE);
    }

    if (trimmedName[0] == '\0')
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            strcpy_s(failureReason, failureReasonSize, "team spawn spot name cannot be empty");
        }
        return false;
    }

    if (strlen(trimmedName) >= bufferSize)
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "team spawn spot name \"%s\" is too long (max %u characters)",
                trimmedName,
                (unsigned int)(bufferSize - 1));
        }
        return false;
    }

    for (const char *cursor = trimmedName; *cursor != '\0'; ++cursor)
    {
        const unsigned char ch = (unsigned char)(*cursor);
        if (ch < 32 || *cursor == '"' || *cursor == '\\' || *cursor == '/')
        {
            if (failureReason != NULL && failureReasonSize > 0)
            {
                _snprintf_s(
                    failureReason,
                    failureReasonSize,
                    _TRUNCATE,
                    "team spawn spot name \"%s\" contains unsupported characters",
                    trimmedName);
            }
            return false;
        }
    }

    strncpy_s(buffer, bufferSize, trimmedName, _TRUNCATE);
    return true;
}

bool TryBuildTeamSpawnSpotsDirectoryPath(char *buffer, size_t bufferSize, char *failureReason, size_t failureReasonSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return false;
    }

    buffer[0] = '\0';
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    char modRoot[kMaxLiveCfgPathLength];
    if (!TryGetLiveModRootPath(modRoot, sizeof(modRoot)))
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            strcpy_s(failureReason, failureReasonSize, "could not resolve the active hlserver_testbed mod root from hl.dll");
        }
        return false;
    }

    _snprintf_s(buffer, bufferSize, _TRUNCATE, "%s\\%s", modRoot, kTeamSpawnSpotsDirectoryName);
    return buffer[0] != '\0';
}

bool TryBuildCurrentTeamSpawnSpotsPath(char *buffer, size_t bufferSize, char *failureReason, size_t failureReasonSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return false;
    }

    buffer[0] = '\0';
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    const char *mapName = GetCurrentMapName();
    if (!IsSafeLabDummyMapFileName(mapName))
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "could not resolve a safe team-spawn file name for map \"%s\"",
                GetValueOrFallback(mapName, ""));
        }
        return false;
    }

    char directoryPath[kMaxLiveCfgPathLength];
    if (!TryBuildTeamSpawnSpotsDirectoryPath(directoryPath, sizeof(directoryPath), failureReason, failureReasonSize))
    {
        return false;
    }

    _snprintf_s(buffer, bufferSize, _TRUNCATE, "%s\\%s.json", directoryPath, mapName);
    return buffer[0] != '\0';
}

int FindTeamSpawnSavedSpotIndex(int teamId, const char *spotName)
{
    teamId = NormalizeRoundTeamId(teamId);
    if (teamId == kExpRoundTeamNone || spotName == NULL || spotName[0] == '\0')
    {
        return -1;
    }

    for (int spotIndex = 0; spotIndex < g_teamSpawnSavedSpotCounts[teamId]; ++spotIndex)
    {
        const TeamSpawnSavedSpotRecord &spot = g_teamSpawnSavedSpots[teamId][spotIndex];
        if (spot.valid && StringEqualsIgnoreCase(spot.name, spotName))
        {
            return spotIndex;
        }
    }

    return -1;
}

TeamSpawnSavedSpotRecord *FindTeamSpawnSavedSpot(int teamId, const char *spotName)
{
    const int spotIndex = FindTeamSpawnSavedSpotIndex(teamId, spotName);
    return spotIndex >= 0 ? &g_teamSpawnSavedSpots[teamId][spotIndex] : NULL;
}

const TeamSpawnSavedSpotRecord *FindTeamSpawnSavedSpotConst(int teamId, const char *spotName)
{
    const int spotIndex = FindTeamSpawnSavedSpotIndex(teamId, spotName);
    return spotIndex >= 0 ? &g_teamSpawnSavedSpots[teamId][spotIndex] : NULL;
}

const TeamSpawnSavedSpotRecord *GetActiveTeamSpawnSavedSpot(int teamId)
{
    teamId = NormalizeRoundTeamId(teamId);
    return teamId != kExpRoundTeamNone && g_teamSpawnActiveSpotNames[teamId][0] != '\0'
        ? FindTeamSpawnSavedSpotConst(teamId, g_teamSpawnActiveSpotNames[teamId])
        : NULL;
}

const TeamSpawnSavedSpotRecord *GetDefaultTeamSpawnSavedSpot(int teamId)
{
    return FindTeamSpawnSavedSpotConst(teamId, kDefaultLabDummySpotName);
}

void BuildTeamSpawnNamesSummary(int teamId, char *buffer, size_t bufferSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return;
    }

    buffer[0] = '\0';
    teamId = NormalizeRoundTeamId(teamId);
    if (teamId == kExpRoundTeamNone)
    {
        return;
    }

    for (int spotIndex = 0; spotIndex < g_teamSpawnSavedSpotCounts[teamId]; ++spotIndex)
    {
        const TeamSpawnSavedSpotRecord &spot = g_teamSpawnSavedSpots[teamId][spotIndex];
        if (!spot.valid)
        {
            continue;
        }

        if (buffer[0] != '\0')
        {
            strncat_s(buffer, bufferSize, ", ", _TRUNCATE);
        }

        strncat_s(buffer, bufferSize, spot.name, _TRUNCATE);
    }
}

void SetTeamSpawnFailureInfo(
    TeamSpawnFailureInfo *failure,
    int teamId,
    const char *code,
    const char *source,
    const char *candidate,
    const char *reason,
    const char *spotName,
    const char *spotStorage)
{
    if (failure == NULL)
    {
        return;
    }

    ClearTeamSpawnFailureInfo(failure);
    failure->teamId = NormalizeRoundTeamId(teamId);
    strncpy_s(failure->code, sizeof(failure->code), code != NULL ? code : "", _TRUNCATE);
    strncpy_s(failure->source, sizeof(failure->source), source != NULL ? source : "", _TRUNCATE);
    strncpy_s(failure->candidate, sizeof(failure->candidate), candidate != NULL ? candidate : "", _TRUNCATE);
    strncpy_s(failure->reason, sizeof(failure->reason), reason != NULL ? reason : "", _TRUNCATE);
    strncpy_s(failure->spotName, sizeof(failure->spotName), spotName != NULL ? spotName : "", _TRUNCATE);
    strncpy_s(failure->spotStorage, sizeof(failure->spotStorage), spotStorage != NULL ? spotStorage : "", _TRUNCATE);
}

void RememberTeamSpawnFailure(int teamId, const TeamSpawnFailureInfo &failure)
{
    teamId = NormalizeRoundTeamId(teamId);
    if (teamId == kExpRoundTeamNone)
    {
        return;
    }

    g_teamSpawnLastFailure[teamId] = failure;
    FormatFutureGameplayTimestamp(g_teamSpawnLastFailureAt[teamId], sizeof(g_teamSpawnLastFailureAt[teamId]));
}

void RememberTeamSpawnApplied(CBasePlayer *pPlayer, const TeamSpawnSelection &selection, const char *reason)
{
    const int teamId = NormalizeRoundTeamId(selection.teamId);
    if (teamId == kExpRoundTeamNone)
    {
        return;
    }

    TeamSpawnRuntimeStatus &status = g_teamSpawnLastApplied[teamId];
    ClearTeamSpawnRuntimeStatus(&status);
    status.valid = true;
    status.teamId = teamId;
    status.roundNumber = g_expRoundState.roundNumber;
    status.origin = selection.origin;
    status.angles = selection.angles;
    strncpy_s(status.playerName, sizeof(status.playerName), GetSafePlayerName(pPlayer), _TRUNCATE);
    status.playerEntIndex = GetPlayerEntityIndex(pPlayer);
    status.playerUserId = GetPlayerUserId(pPlayer);
    strncpy_s(status.source, sizeof(status.source), selection.source, _TRUNCATE);
    strncpy_s(status.candidate, sizeof(status.candidate), selection.candidate, _TRUNCATE);
    strncpy_s(status.spotName, sizeof(status.spotName), selection.spotName, _TRUNCATE);
    strncpy_s(status.spotStorage, sizeof(status.spotStorage), selection.spotStorage, _TRUNCATE);
    strncpy_s(status.reason, sizeof(status.reason), reason != NULL ? reason : "", _TRUNCATE);
}

TeamSpawnSavedSpotRecord *UpsertTeamSpawnSavedSpot(
    int teamId,
    const char *spotName,
    const Vector &origin,
    const Vector &angles,
    char *failureReason,
    size_t failureReasonSize)
{
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    teamId = NormalizeRoundTeamId(teamId);
    if (teamId == kExpRoundTeamNone)
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            strcpy_s(failureReason, failureReasonSize, "team spawn team id is invalid");
        }
        return NULL;
    }

    int spotIndex = FindTeamSpawnSavedSpotIndex(teamId, spotName);
    if (spotIndex < 0)
    {
        if (g_teamSpawnSavedSpotCounts[teamId] >= ARRAYSIZE(g_teamSpawnSavedSpots[teamId]))
        {
            if (failureReason != NULL && failureReasonSize > 0)
            {
                _snprintf_s(
                    failureReason,
                    failureReasonSize,
                    _TRUNCATE,
                    "cannot store %s spawn spot \"%s\": this team already has %u saved spots",
                    GetConfiguredTeamRoundName(teamId),
                    GetValueOrFallback(spotName, ""),
                    (unsigned int)ARRAYSIZE(g_teamSpawnSavedSpots[teamId]));
            }
            return NULL;
        }

        spotIndex = g_teamSpawnSavedSpotCounts[teamId]++;
        ClearTeamSpawnSavedSpotRecord(&g_teamSpawnSavedSpots[teamId][spotIndex]);
    }

    TeamSpawnSavedSpotRecord *spot = &g_teamSpawnSavedSpots[teamId][spotIndex];
    char timestamp[kMaxLabDummySpotTimestampLength];
    FormatFutureGameplayTimestamp(timestamp, sizeof(timestamp));

    char createdAt[kMaxLabDummySpotTimestampLength];
    strncpy_s(createdAt, sizeof(createdAt), spot->createdAt[0] != '\0' ? spot->createdAt : timestamp, _TRUNCATE);
    char note[kMaxLabDummySpotNoteLength];
    strncpy_s(note, sizeof(note), spot->note, _TRUNCATE);

    Vector normalizedAngles = angles;
    normalizedAngles.x = 0.0f;
    normalizedAngles.z = 0.0f;
    StoreTeamSpawnSavedSpotRecord(
        spot,
        teamId,
        spotName,
        origin,
        normalizedAngles,
        createdAt,
        timestamp,
        note,
        false);
    return spot;
}

bool RemoveTeamSpawnSavedSpot(int teamId, const char *spotName, TeamSpawnSavedSpotRecord *removedSpot, char *failureReason, size_t failureReasonSize)
{
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    teamId = NormalizeRoundTeamId(teamId);
    const int spotIndex = FindTeamSpawnSavedSpotIndex(teamId, spotName);
    if (teamId == kExpRoundTeamNone || spotIndex < 0)
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "%s spawn spot \"%s\" does not exist for map %s",
                GetConfiguredTeamRoundName(teamId),
                GetValueOrFallback(spotName, ""),
                GetCurrentMapName());
        }
        return false;
    }

    if (removedSpot != NULL)
    {
        *removedSpot = g_teamSpawnSavedSpots[teamId][spotIndex];
    }

    for (int index = spotIndex; index + 1 < g_teamSpawnSavedSpotCounts[teamId]; ++index)
    {
        g_teamSpawnSavedSpots[teamId][index] = g_teamSpawnSavedSpots[teamId][index + 1];
    }

    if (g_teamSpawnSavedSpotCounts[teamId] > 0)
    {
        --g_teamSpawnSavedSpotCounts[teamId];
        ClearTeamSpawnSavedSpotRecord(&g_teamSpawnSavedSpots[teamId][g_teamSpawnSavedSpotCounts[teamId]]);
    }

    if (StringEqualsIgnoreCase(g_teamSpawnActiveSpotNames[teamId], spotName))
    {
        g_teamSpawnActiveSpotNames[teamId][0] = '\0';
    }

    return true;
}

bool TryParseTeamSpawnSpotObject(LabDummyJsonCursor *cursor, TeamSpawnSavedSpotRecord *spot, std::string *failureReason)
{
    if (spot == NULL)
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor != NULL ? cursor->position : 0, "team spawn spot destination is missing");
    }

    ClearTeamSpawnSavedSpotRecord(spot);
    if (!TryConsumeLabDummyJsonChar(cursor, '{'))
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor != NULL ? cursor->position : 0, "expected '{' for team spawn spot");
    }

    bool hasName = false;
    bool hasOrigin = false;
    bool hasYaw = false;
    bool hasTeam = false;

    SkipLabDummyJsonWhitespace(cursor);
    if (TryConsumeLabDummyJsonChar(cursor, '}'))
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor->position, "team spawn spot object is missing required fields");
    }

    while (cursor != NULL && cursor->position < cursor->length)
    {
        std::string key;
        if (!TryParseLabDummyJsonString(cursor, &key, failureReason))
        {
            return false;
        }

        if (!TryConsumeLabDummyJsonChar(cursor, ':'))
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor->position, "expected ':' after team spawn key");
        }

        if (key == "team")
        {
            std::string teamValue;
            if (!TryParseLabDummyJsonString(cursor, &teamValue, failureReason))
            {
                return false;
            }

            if (StringEqualsIgnoreCase(teamValue.c_str(), "team1"))
            {
                spot->teamId = kExpRoundTeam1;
            }
            else if (StringEqualsIgnoreCase(teamValue.c_str(), "team2"))
            {
                spot->teamId = kExpRoundTeam2;
            }
            else
            {
                return SetLabDummyJsonParseFailure(failureReason, cursor->position, "team spawn spot has unknown team id");
            }

            hasTeam = true;
        }
        else if (key == "name")
        {
            std::string name;
            if (!TryParseLabDummyJsonString(cursor, &name, failureReason))
            {
                return false;
            }

            strncpy_s(spot->name, sizeof(spot->name), name.c_str(), _TRUNCATE);
            hasName = true;
        }
        else if (key == "origin")
        {
            if (!TryParseLabDummyJsonVector3(cursor, &spot->origin, failureReason))
            {
                return false;
            }

            hasOrigin = true;
        }
        else if (key == "yaw")
        {
            double yawValue = 0.0;
            if (!TryParseLabDummyJsonNumber(cursor, &yawValue, failureReason))
            {
                return false;
            }

            spot->angles = Vector(0.0f, (float)yawValue, 0.0f);
            hasYaw = true;
        }
        else if (key == "created_at")
        {
            std::string createdAt;
            if (!TryParseLabDummyJsonString(cursor, &createdAt, failureReason))
            {
                return false;
            }

            strncpy_s(spot->createdAt, sizeof(spot->createdAt), createdAt.c_str(), _TRUNCATE);
        }
        else if (key == "updated_at")
        {
            std::string updatedAt;
            if (!TryParseLabDummyJsonString(cursor, &updatedAt, failureReason))
            {
                return false;
            }

            strncpy_s(spot->updatedAt, sizeof(spot->updatedAt), updatedAt.c_str(), _TRUNCATE);
        }
        else if (key == "note")
        {
            std::string note;
            if (!TryParseLabDummyJsonString(cursor, &note, failureReason))
            {
                return false;
            }

            strncpy_s(spot->note, sizeof(spot->note), note.c_str(), _TRUNCATE);
        }
        else
        {
            if (!TrySkipLabDummyJsonValue(cursor, failureReason))
            {
                return false;
            }
        }

        if (TryConsumeLabDummyJsonChar(cursor, '}'))
        {
            break;
        }

        if (!TryConsumeLabDummyJsonChar(cursor, ','))
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor->position, "expected ',' or '}' in team spawn spot object");
        }
    }

    if (!hasName || !hasOrigin || !hasYaw || !hasTeam)
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor->position, "team spawn spot object is missing team, name, origin, or yaw");
    }

    spot->valid = true;
    spot->loadedFromDisk = true;
    return true;
}

bool TryParseTeamSpawnFileText(
    const std::string &jsonText,
    std::vector<TeamSpawnSavedSpotRecord> *team1Spots,
    std::vector<TeamSpawnSavedSpotRecord> *team2Spots,
    std::string *team1ActiveSpotName,
    std::string *team2ActiveSpotName,
    std::string *failureReason)
{
    if (team1Spots == NULL || team2Spots == NULL || team1ActiveSpotName == NULL || team2ActiveSpotName == NULL)
    {
        return false;
    }

    team1Spots->clear();
    team2Spots->clear();
    team1ActiveSpotName->clear();
    team2ActiveSpotName->clear();
    if (failureReason != NULL)
    {
        failureReason->clear();
    }

    LabDummyJsonCursor cursor = {jsonText.c_str(), jsonText.length(), 0};
    if (!TryConsumeLabDummyJsonChar(&cursor, '{'))
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected a JSON object at the root");
    }

    SkipLabDummyJsonWhitespace(&cursor);
    if (TryConsumeLabDummyJsonChar(&cursor, '}'))
    {
        return true;
    }

    while (cursor.position < cursor.length)
    {
        std::string key;
        if (!TryParseLabDummyJsonString(&cursor, &key, failureReason))
        {
            return false;
        }

        if (!TryConsumeLabDummyJsonChar(&cursor, ':'))
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected ':' after root key");
        }

        if (key == "teams")
        {
            if (!TryConsumeLabDummyJsonChar(&cursor, '['))
            {
                return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected '[' for teams array");
            }

            SkipLabDummyJsonWhitespace(&cursor);
            if (!TryConsumeLabDummyJsonChar(&cursor, ']'))
            {
                while (true)
                {
                    if (!TryConsumeLabDummyJsonChar(&cursor, '{'))
                    {
                        return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected '{' for team spawn team object");
                    }

                    int teamId = kExpRoundTeamNone;
                    std::string activeSpotName;
                    std::vector<TeamSpawnSavedSpotRecord> teamSpots;

                    SkipLabDummyJsonWhitespace(&cursor);
                    if (TryConsumeLabDummyJsonChar(&cursor, '}'))
                    {
                        return SetLabDummyJsonParseFailure(failureReason, cursor.position, "team spawn team object is missing required fields");
                    }

                    while (cursor.position < cursor.length)
                    {
                        std::string teamKey;
                        if (!TryParseLabDummyJsonString(&cursor, &teamKey, failureReason))
                        {
                            return false;
                        }

                        if (!TryConsumeLabDummyJsonChar(&cursor, ':'))
                        {
                            return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected ':' after team object key");
                        }

                        if (teamKey == "id")
                        {
                            std::string idValue;
                            if (!TryParseLabDummyJsonString(&cursor, &idValue, failureReason))
                            {
                                return false;
                            }

                            if (StringEqualsIgnoreCase(idValue.c_str(), "team1"))
                            {
                                teamId = kExpRoundTeam1;
                            }
                            else if (StringEqualsIgnoreCase(idValue.c_str(), "team2"))
                            {
                                teamId = kExpRoundTeam2;
                            }
                            else
                            {
                                return SetLabDummyJsonParseFailure(failureReason, cursor.position, "team spawn file has unknown team bucket id");
                            }
                        }
                        else if (teamKey == "active_spot")
                        {
                            if (!TryParseLabDummyJsonString(&cursor, &activeSpotName, failureReason))
                            {
                                return false;
                            }
                        }
                        else if (teamKey == "spots")
                        {
                            if (!TryConsumeLabDummyJsonChar(&cursor, '['))
                            {
                                return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected '[' for team spots array");
                            }

                            SkipLabDummyJsonWhitespace(&cursor);
                            if (!TryConsumeLabDummyJsonChar(&cursor, ']'))
                            {
                                while (true)
                                {
                                    TeamSpawnSavedSpotRecord spot = {};
                                    if (!TryParseTeamSpawnSpotObject(&cursor, &spot, failureReason))
                                    {
                                        return false;
                                    }

                                    if (spot.teamId == kExpRoundTeamNone)
                                    {
                                        spot.teamId = teamId;
                                    }

                                    if (teamId != kExpRoundTeamNone && spot.teamId != teamId)
                                    {
                                        return SetLabDummyJsonParseFailure(failureReason, cursor.position, "team spawn spot team does not match its containing team bucket");
                                    }

                                    for (size_t existingIndex = 0; existingIndex < teamSpots.size(); ++existingIndex)
                                    {
                                        if (StringEqualsIgnoreCase(teamSpots[existingIndex].name, spot.name))
                                        {
                                            char duplicateMessage[256];
                                            _snprintf_s(
                                                duplicateMessage,
                                                sizeof(duplicateMessage),
                                                _TRUNCATE,
                                                "duplicate team spawn spot \"%s\" was found in %s",
                                                spot.name,
                                                GetRoundTeamBucketKey(teamId));
                                            return SetLabDummyJsonParseFailure(failureReason, cursor.position, duplicateMessage);
                                        }
                                    }

                                    teamSpots.push_back(spot);
                                    if (TryConsumeLabDummyJsonChar(&cursor, ']'))
                                    {
                                        break;
                                    }

                                    if (!TryConsumeLabDummyJsonChar(&cursor, ','))
                                    {
                                        return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected ',' or ']' in team spots array");
                                    }
                                }
                            }
                        }
                        else
                        {
                            if (!TrySkipLabDummyJsonValue(&cursor, failureReason))
                            {
                                return false;
                            }
                        }

                        if (TryConsumeLabDummyJsonChar(&cursor, '}'))
                        {
                            break;
                        }

                        if (!TryConsumeLabDummyJsonChar(&cursor, ','))
                        {
                            return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected ',' or '}' in team object");
                        }
                    }

                    if (teamId == kExpRoundTeamNone)
                    {
                        return SetLabDummyJsonParseFailure(failureReason, cursor.position, "team spawn file team object is missing id");
                    }

                    if (teamId == kExpRoundTeam1)
                    {
                        *team1Spots = teamSpots;
                        *team1ActiveSpotName = activeSpotName;
                    }
                    else
                    {
                        *team2Spots = teamSpots;
                        *team2ActiveSpotName = activeSpotName;
                    }

                    if (TryConsumeLabDummyJsonChar(&cursor, ']'))
                    {
                        break;
                    }

                    if (!TryConsumeLabDummyJsonChar(&cursor, ','))
                    {
                        return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected ',' or ']' in teams array");
                    }
                }
            }
        }
        else
        {
            if (!TrySkipLabDummyJsonValue(&cursor, failureReason))
            {
                return false;
            }
        }

        if (TryConsumeLabDummyJsonChar(&cursor, '}'))
        {
            break;
        }

        if (!TryConsumeLabDummyJsonChar(&cursor, ','))
        {
            return SetLabDummyJsonParseFailure(failureReason, cursor.position, "expected ',' or '}' in root object");
        }
    }

    SkipLabDummyJsonWhitespace(&cursor);
    if (cursor.position != cursor.length)
    {
        return SetLabDummyJsonParseFailure(failureReason, cursor.position, "unexpected trailing content after JSON object");
    }

    return true;
}

bool SaveTeamSpawnSpotsForCurrentMap(char *failureReason, size_t failureReasonSize)
{
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    char filePath[kMaxLiveCfgPathLength];
    if (!TryBuildCurrentTeamSpawnSpotsPath(filePath, sizeof(filePath), failureReason, failureReasonSize))
    {
        return false;
    }

    strncpy_s(g_teamSpawnSpotsPath, sizeof(g_teamSpawnSpotsPath), filePath, _TRUNCATE);

    for (int teamId = kExpRoundTeam1; teamId <= kExpRoundTeam2; ++teamId)
    {
        if (g_teamSpawnActiveSpotNames[teamId][0] != '\0' && FindTeamSpawnSavedSpotIndex(teamId, g_teamSpawnActiveSpotNames[teamId]) < 0)
        {
            g_teamSpawnActiveSpotNames[teamId][0] = '\0';
        }
    }

    if (g_teamSpawnSavedSpotCounts[kExpRoundTeam1] <= 0 && g_teamSpawnSavedSpotCounts[kExpRoundTeam2] <= 0)
    {
        if (DeleteFileA(filePath) || GetLastError() == ERROR_FILE_NOT_FOUND)
        {
            return true;
        }

        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "could not remove empty team-spawns file %s (win32=%lu)",
                filePath,
                (unsigned long)GetLastError());
        }
        return false;
    }

    char directoryPath[kMaxLiveCfgPathLength];
    if (!TryBuildTeamSpawnSpotsDirectoryPath(directoryPath, sizeof(directoryPath), failureReason, failureReasonSize))
    {
        return false;
    }

    if (!TryEnsureLabDummyDirectoryExists(directoryPath, failureReason, failureReasonSize))
    {
        return false;
    }

    std::string jsonText;
    jsonText += "{\n";
    jsonText += "  \"map\": \"";
    jsonText += EscapeLabDummyJsonString(GetCurrentMapName());
    jsonText += "\",\n";
    jsonText += "  \"teams\": [\n";

    for (int teamId = kExpRoundTeam1; teamId <= kExpRoundTeam2; ++teamId)
    {
        jsonText += "    {\n";
        jsonText += "      \"id\": \"";
        jsonText += GetRoundTeamBucketKey(teamId);
        jsonText += "\",\n";
        jsonText += "      \"name\": \"";
        jsonText += EscapeLabDummyJsonString(GetConfiguredTeamRoundName(teamId));
        jsonText += "\",\n";
        jsonText += "      \"active_spot\": \"";
        jsonText += EscapeLabDummyJsonString(g_teamSpawnActiveSpotNames[teamId]);
        jsonText += "\",\n";
        jsonText += "      \"spots\": [\n";

        for (int spotIndex = 0; spotIndex < g_teamSpawnSavedSpotCounts[teamId]; ++spotIndex)
        {
            const TeamSpawnSavedSpotRecord &spot = g_teamSpawnSavedSpots[teamId][spotIndex];
            if (!spot.valid)
            {
                continue;
            }

            char numberBuffer[96];
            _snprintf_s(
                numberBuffer,
                sizeof(numberBuffer),
                _TRUNCATE,
                "          \"origin\": [%.3f, %.3f, %.3f],\n          \"yaw\": %.3f,\n",
                spot.origin.x,
                spot.origin.y,
                spot.origin.z,
                spot.angles.y);

            jsonText += "        {\n";
            jsonText += "          \"team\": \"";
            jsonText += GetRoundTeamBucketKey(teamId);
            jsonText += "\",\n";
            jsonText += "          \"name\": \"";
            jsonText += EscapeLabDummyJsonString(spot.name);
            jsonText += "\",\n";
            jsonText += numberBuffer;
            jsonText += "          \"created_at\": \"";
            jsonText += EscapeLabDummyJsonString(spot.createdAt);
            jsonText += "\",\n";
            jsonText += "          \"updated_at\": \"";
            jsonText += EscapeLabDummyJsonString(spot.updatedAt);
            jsonText += "\"";
            if (spot.note[0] != '\0')
            {
                jsonText += ",\n          \"note\": \"";
                jsonText += EscapeLabDummyJsonString(spot.note);
                jsonText += "\"";
            }
            jsonText += "\n        }";
            if (spotIndex + 1 < g_teamSpawnSavedSpotCounts[teamId])
            {
                jsonText += ",";
            }
            jsonText += "\n";
        }

        jsonText += "      ]\n";
        jsonText += "    }";
        if (teamId != kExpRoundTeam2)
        {
            jsonText += ",";
        }
        jsonText += "\n";
    }

    jsonText += "  ]\n";
    jsonText += "}\n";

    char tempPath[kMaxLiveCfgPathLength];
    _snprintf_s(tempPath, sizeof(tempPath), _TRUNCATE, "%s.tmp", filePath);

    FILE *file = NULL;
    if (fopen_s(&file, tempPath, "wb") != 0 || file == NULL)
    {
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(failureReason, failureReasonSize, _TRUNCATE, "could not write temporary team-spawns file %s", tempPath);
        }
        return false;
    }

    const size_t bytesWritten = fwrite(jsonText.data(), 1, jsonText.length(), file);
    fclose(file);
    if (bytesWritten != jsonText.length())
    {
        DeleteFileA(tempPath);
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "could not fully write team-spawns file %s (expected %u bytes, wrote %u)",
                tempPath,
                (unsigned int)jsonText.length(),
                (unsigned int)bytesWritten);
        }
        return false;
    }

    if (!MoveFileExA(tempPath, filePath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
    {
        const DWORD error = GetLastError();
        DeleteFileA(tempPath);
        if (failureReason != NULL && failureReasonSize > 0)
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "could not finalize team-spawns file %s (win32=%lu)",
                filePath,
                (unsigned long)error);
        }
        return false;
    }

    return true;
}

void LoadTeamSpawnSpotsForCurrentMap()
{
    ClearAllTeamSpawnSavedSpots();

    char filePath[kMaxLiveCfgPathLength];
    char failureReason[kMaxLabDummySpotFileFailureLength];
    if (!TryBuildCurrentTeamSpawnSpotsPath(filePath, sizeof(filePath), failureReason, sizeof(failureReason)))
    {
        strncpy_s(g_teamSpawnSpotsLoadFailure, sizeof(g_teamSpawnSpotsLoadFailure), failureReason, _TRUNCATE);
        PrintLabDummyConsoleLine("team spawn spots load failed: %s", failureReason);
        return;
    }

    strncpy_s(g_teamSpawnSpotsPath, sizeof(g_teamSpawnSpotsPath), filePath, _TRUNCATE);
    if (!FileExists(filePath))
    {
        return;
    }

    std::string fileContents;
    if (!TryReadLabDummySpotsFile(filePath, &fileContents, failureReason, sizeof(failureReason)))
    {
        strncpy_s(g_teamSpawnSpotsLoadFailure, sizeof(g_teamSpawnSpotsLoadFailure), failureReason, _TRUNCATE);
        PrintLabDummyConsoleLine("team spawn spots load failed for map %s: %s", GetCurrentMapName(), failureReason);
        return;
    }

    std::vector<TeamSpawnSavedSpotRecord> loadedTeam1Spots;
    std::vector<TeamSpawnSavedSpotRecord> loadedTeam2Spots;
    std::string team1ActiveSpotName;
    std::string team2ActiveSpotName;
    std::string parseFailure;
    if (!TryParseTeamSpawnFileText(fileContents, &loadedTeam1Spots, &loadedTeam2Spots, &team1ActiveSpotName, &team2ActiveSpotName, &parseFailure))
    {
        strncpy_s(g_teamSpawnSpotsLoadFailure, sizeof(g_teamSpawnSpotsLoadFailure), parseFailure.c_str(), _TRUNCATE);
        PrintLabDummyConsoleLine("team spawn spots load failed for map %s: %s", GetCurrentMapName(), parseFailure.c_str());
        return;
    }

    if (loadedTeam1Spots.size() > kMaxTeamSpawnSpotsPerTeam || loadedTeam2Spots.size() > kMaxTeamSpawnSpotsPerTeam)
    {
        _snprintf_s(
            g_teamSpawnSpotsLoadFailure,
            sizeof(g_teamSpawnSpotsLoadFailure),
            _TRUNCATE,
            "team spawn file has too many entries (team1=%u team2=%u max=%u)",
            (unsigned int)loadedTeam1Spots.size(),
            (unsigned int)loadedTeam2Spots.size(),
            (unsigned int)kMaxTeamSpawnSpotsPerTeam);
        PrintLabDummyConsoleLine("team spawn spots load failed for map %s: %s", GetCurrentMapName(), g_teamSpawnSpotsLoadFailure);
        return;
    }

    for (size_t spotIndex = 0; spotIndex < loadedTeam1Spots.size(); ++spotIndex)
    {
        g_teamSpawnSavedSpots[kExpRoundTeam1][spotIndex] = loadedTeam1Spots[spotIndex];
    }
    for (size_t spotIndex = 0; spotIndex < loadedTeam2Spots.size(); ++spotIndex)
    {
        g_teamSpawnSavedSpots[kExpRoundTeam2][spotIndex] = loadedTeam2Spots[spotIndex];
    }

    g_teamSpawnSavedSpotCounts[kExpRoundTeam1] = (int)loadedTeam1Spots.size();
    g_teamSpawnSavedSpotCounts[kExpRoundTeam2] = (int)loadedTeam2Spots.size();
    g_teamSpawnSpotsLoadFailure[0] = '\0';

    if (!team1ActiveSpotName.empty())
    {
        if (FindTeamSpawnSavedSpotIndex(kExpRoundTeam1, team1ActiveSpotName.c_str()) >= 0)
        {
            strncpy_s(g_teamSpawnActiveSpotNames[kExpRoundTeam1], sizeof(g_teamSpawnActiveSpotNames[kExpRoundTeam1]), team1ActiveSpotName.c_str(), _TRUNCATE);
        }
        else
        {
            _snprintf_s(
                g_teamSpawnSpotsLoadFailure,
                sizeof(g_teamSpawnSpotsLoadFailure),
                _TRUNCATE,
                "active team1 spawn \"%s\" was not found in %s; cleared active selection",
                team1ActiveSpotName.c_str(),
                filePath);
        }
    }

    if (!team2ActiveSpotName.empty())
    {
        if (FindTeamSpawnSavedSpotIndex(kExpRoundTeam2, team2ActiveSpotName.c_str()) >= 0)
        {
            strncpy_s(g_teamSpawnActiveSpotNames[kExpRoundTeam2], sizeof(g_teamSpawnActiveSpotNames[kExpRoundTeam2]), team2ActiveSpotName.c_str(), _TRUNCATE);
        }
        else if (g_teamSpawnSpotsLoadFailure[0] == '\0')
        {
            _snprintf_s(
                g_teamSpawnSpotsLoadFailure,
                sizeof(g_teamSpawnSpotsLoadFailure),
                _TRUNCATE,
                "active team2 spawn \"%s\" was not found in %s; cleared active selection",
                team2ActiveSpotName.c_str(),
                filePath);
        }
    }

    if (g_teamSpawnSavedSpotCounts[kExpRoundTeam1] > 0 || g_teamSpawnSavedSpotCounts[kExpRoundTeam2] > 0)
    {
        PrintLabDummyConsoleLine(
            "loaded persisted team spawn spots for map %s from %s: %s=%d active=%s | %s=%d active=%s",
            GetCurrentMapName(),
            filePath,
            GetConfiguredTeamRoundName(kExpRoundTeam1),
            g_teamSpawnSavedSpotCounts[kExpRoundTeam1],
            g_teamSpawnActiveSpotNames[kExpRoundTeam1][0] != '\0' ? g_teamSpawnActiveSpotNames[kExpRoundTeam1] : "none",
            GetConfiguredTeamRoundName(kExpRoundTeam2),
            g_teamSpawnSavedSpotCounts[kExpRoundTeam2],
            g_teamSpawnActiveSpotNames[kExpRoundTeam2][0] != '\0' ? g_teamSpawnActiveSpotNames[kExpRoundTeam2] : "none");
    }

    if (g_teamSpawnSpotsLoadFailure[0] != '\0')
    {
        PrintLabDummyConsoleLine("team spawn spots load warning: %s", g_teamSpawnSpotsLoadFailure);
    }
}

bool TryResolveLiveCfgSelection(const char *requestedPath, ResolvedLiveCfgSelection *pSelection, char *failureReason, size_t failureReasonSize)
{
    if (failureReason != NULL && failureReasonSize > 0)
    {
        failureReason[0] = '\0';
    }

    if (pSelection == NULL)
    {
        return false;
    }

    memset(pSelection, 0, sizeof(*pSelection));

    char trimmedRequest[kMaxLiveCfgRequestLength];
    TrimCfgRequestString(requestedPath, trimmedRequest, sizeof(trimmedRequest));
    if (trimmedRequest[0] == '\0')
    {
        strcpy_s(failureReason, failureReasonSize, "cfg path cannot be empty");
        return false;
    }

    if (!EndsWithCfgExtension(trimmedRequest))
    {
        _snprintf_s(
            failureReason,
            failureReasonSize,
            _TRUNCATE,
            "cfg request \"%s\" must point to a .cfg file",
            trimmedRequest);
        return false;
    }

    char modRoot[kMaxLiveCfgPathLength];
    if (!TryGetLiveModRootPath(modRoot, sizeof(modRoot)))
    {
        strcpy_s(failureReason, failureReasonSize, "could not resolve the active hlserver_testbed mod root from hl.dll");
        return false;
    }

    if (IsAbsoluteCfgPath(trimmedRequest))
    {
        char fullPath[kMaxLiveCfgPathLength];
        if (!TryGetFullPathString(trimmedRequest, fullPath, sizeof(fullPath)))
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "could not resolve cfg path \"%s\"",
                trimmedRequest);
            return false;
        }

        if (!IsPathWithinRoot(fullPath, modRoot))
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "cfg path \"%s\" is outside the active mod root %s",
                fullPath,
                modRoot);
            return false;
        }

        if (!FileExists(fullPath))
        {
            _snprintf_s(
                failureReason,
                failureReasonSize,
                _TRUNCATE,
                "cfg file was not found at %s",
                fullPath);
            return false;
        }

        char relativePath[kMaxLiveCfgExecPathLength];
        if (!TryBuildRelativePathFromRoot(fullPath, modRoot, relativePath, sizeof(relativePath)))
        {
            strcpy_s(failureReason, failureReasonSize, "could not convert the cfg path into a mod-relative exec path");
            return false;
        }

        NormalizeSlashes(relativePath, '/');
        strncpy_s(pSelection->requestedPath, sizeof(pSelection->requestedPath), trimmedRequest, _TRUNCATE);
        strncpy_s(pSelection->execPath, sizeof(pSelection->execPath), relativePath, _TRUNCATE);
        strncpy_s(pSelection->resolvedPath, sizeof(pSelection->resolvedPath), fullPath, _TRUNCATE);
        return true;
    }

    char normalizedRequest[kMaxLiveCfgExecPathLength];
    strncpy_s(normalizedRequest, sizeof(normalizedRequest), trimmedRequest, _TRUNCATE);
    NormalizeSlashes(normalizedRequest, '\\');
    while (normalizedRequest[0] == '\\')
    {
        memmove(normalizedRequest, normalizedRequest + 1, strlen(normalizedRequest));
    }

    while (normalizedRequest[0] == '.' && normalizedRequest[1] == '\\')
    {
        memmove(normalizedRequest, normalizedRequest + 2, strlen(normalizedRequest) - 1);
    }

    if (normalizedRequest[0] == '\0')
    {
        strcpy_s(failureReason, failureReasonSize, "cfg path cannot resolve to the mod root itself");
        return false;
    }

    char candidateRelativePaths[2][kMaxLiveCfgExecPathLength] = {};
    char candidateAbsolutePaths[2][kMaxLiveCfgPathLength] = {};
    int candidateCount = 0;

    strncpy_s(candidateRelativePaths[candidateCount++], sizeof(candidateRelativePaths[0]), normalizedRequest, _TRUNCATE);
    if (!HasAnyPathSeparator(normalizedRequest))
    {
        _snprintf_s(candidateRelativePaths[candidateCount++], sizeof(candidateRelativePaths[1]), _TRUNCATE, "cfg_profiles\\%s", normalizedRequest);
    }

    for (int candidateIndex = 0; candidateIndex < candidateCount; ++candidateIndex)
    {
        char combinedPath[kMaxLiveCfgPathLength];
        _snprintf_s(
            combinedPath,
            sizeof(combinedPath),
            _TRUNCATE,
            "%s\\%s",
            modRoot,
            candidateRelativePaths[candidateIndex]);

        if (!TryGetFullPathString(combinedPath, candidateAbsolutePaths[candidateIndex], sizeof(candidateAbsolutePaths[candidateIndex])))
        {
            continue;
        }

        if (!IsPathWithinRoot(candidateAbsolutePaths[candidateIndex], modRoot))
        {
            continue;
        }

        if (!FileExists(candidateAbsolutePaths[candidateIndex]))
        {
            continue;
        }

        char execPath[kMaxLiveCfgExecPathLength];
        strncpy_s(execPath, sizeof(execPath), candidateRelativePaths[candidateIndex], _TRUNCATE);
        NormalizeSlashes(execPath, '/');

        strncpy_s(pSelection->requestedPath, sizeof(pSelection->requestedPath), trimmedRequest, _TRUNCATE);
        strncpy_s(pSelection->execPath, sizeof(pSelection->execPath), execPath, _TRUNCATE);
        strncpy_s(pSelection->resolvedPath, sizeof(pSelection->resolvedPath), candidateAbsolutePaths[candidateIndex], _TRUNCATE);
        return true;
    }

    if (candidateCount == 1)
    {
        _snprintf_s(
            failureReason,
            failureReasonSize,
            _TRUNCATE,
            "cfg file \"%s\" was not found at %s",
            trimmedRequest,
            candidateAbsolutePaths[0][0] != '\0' ? candidateAbsolutePaths[0] : candidateRelativePaths[0]);
    }
    else
    {
        _snprintf_s(
            failureReason,
            failureReasonSize,
            _TRUNCATE,
            "cfg file \"%s\" was not found. Searched %s and %s",
            trimmedRequest,
            candidateAbsolutePaths[0][0] != '\0' ? candidateAbsolutePaths[0] : candidateRelativePaths[0],
            candidateAbsolutePaths[1][0] != '\0' ? candidateAbsolutePaths[1] : candidateRelativePaths[1]);
    }

    return false;
}

void ClearLiveCfgFailureState()
{
    g_liveCfgState.lastFailureAction[0] = '\0';
    g_liveCfgState.lastFailure[0] = '\0';
}

void RecordLiveCfgFailure(const char *action, const char *reason)
{
    g_liveCfgState.lastCommandSucceeded = false;
    strncpy_s(g_liveCfgState.lastFailureAction, sizeof(g_liveCfgState.lastFailureAction), GetValueOrFallback(action, "apply"), _TRUNCATE);
    strncpy_s(g_liveCfgState.lastFailure, sizeof(g_liveCfgState.lastFailure), GetValueOrFallback(reason, "unknown cfg failure"), _TRUNCATE);
}

void RecordLiveCfgSuccess(const char *action, const ResolvedLiveCfgSelection &selection)
{
    g_liveCfgState.cfgDrivenModeActive = true;
    g_liveCfgState.hasActiveCfg = true;
    g_liveCfgState.hasLastSuccessfulCfg = true;
    g_liveCfgState.lastCommandSucceeded = true;
    strncpy_s(g_liveCfgState.activeRequestedPath, sizeof(g_liveCfgState.activeRequestedPath), selection.requestedPath, _TRUNCATE);
    strncpy_s(g_liveCfgState.activeExecPath, sizeof(g_liveCfgState.activeExecPath), selection.execPath, _TRUNCATE);
    strncpy_s(g_liveCfgState.activeResolvedPath, sizeof(g_liveCfgState.activeResolvedPath), selection.resolvedPath, _TRUNCATE);
    strncpy_s(g_liveCfgState.lastSuccessfulExecPath, sizeof(g_liveCfgState.lastSuccessfulExecPath), selection.execPath, _TRUNCATE);
    strncpy_s(g_liveCfgState.lastSuccessfulResolvedPath, sizeof(g_liveCfgState.lastSuccessfulResolvedPath), selection.resolvedPath, _TRUNCATE);
    strncpy_s(g_liveCfgState.lastAction, sizeof(g_liveCfgState.lastAction), GetValueOrFallback(action, "apply"), _TRUNCATE);
    FormatFutureGameplayTimestamp(g_liveCfgState.lastAppliedAt, sizeof(g_liveCfgState.lastAppliedAt));
    g_liveCfgState.lastApplyServerTime = gpGlobals != NULL ? gpGlobals->time : 0.0f;
    ClearLiveCfgFailureState();
}

void PrintCurrentCfgMetadata()
{
    PrintLabDummyConsoleLine(
        "current cfg metadata: weapon=%s session_tag=%s glock_profile=%s mp5_profile=%s target_profile=%s",
        GetValueOrFallback(ExpWeaponUnderTest(), "none"),
        GetValueOrFallback(ExpSessionTag(), "none"),
        GetValueOrFallback(ExpGlockProfileName(), "default"),
        GetValueOrFallback(ExpMP5ProfileName(), "default"),
        GetValueOrFallback(ExpGlockLabTargetProfileName(), "default"));
}

void PrintLiveCfgStatus()
{
    const char *lastResult = "none";
    if (g_liveCfgState.lastAction[0] != '\0')
    {
        lastResult = g_liveCfgState.lastCommandSucceeded ? "success" : "failure";
    }
    else if (g_liveCfgState.lastFailureAction[0] != '\0')
    {
        lastResult = "failure";
    }

    PrintLabDummyConsoleLine(
        "cfg mode: cfg_driven=%d active_cfg_known=%d last_result=%s",
        g_liveCfgState.cfgDrivenModeActive ? 1 : 0,
        g_liveCfgState.hasActiveCfg ? 1 : 0,
        lastResult);

    if (g_liveCfgState.hasActiveCfg)
    {
        PrintLabDummyConsoleLine(
            "active cfg: request=%s exec=%s source=%s file_present=%d",
            GetValueOrFallback(g_liveCfgState.activeRequestedPath, "unknown"),
            GetValueOrFallback(g_liveCfgState.activeExecPath, "unknown"),
            GetValueOrFallback(g_liveCfgState.activeResolvedPath, "unknown"),
            FileExists(g_liveCfgState.activeResolvedPath) ? 1 : 0);
    }
    else
    {
        PrintLabDummyConsoleLine("active cfg: none");
    }

    if (g_liveCfgState.hasLastSuccessfulCfg)
    {
        PrintLabDummyConsoleLine(
            "last successful cfg: exec=%s source=%s",
            GetValueOrFallback(g_liveCfgState.lastSuccessfulExecPath, "unknown"),
            GetValueOrFallback(g_liveCfgState.lastSuccessfulResolvedPath, "unknown"));
        PrintLabDummyConsoleLine(
            "last successful apply: action=%s at=%s map_time=%.2f",
            GetValueOrFallback(g_liveCfgState.lastAction, "unknown"),
            GetValueOrFallback(g_liveCfgState.lastAppliedAt, "unknown"),
            g_liveCfgState.lastApplyServerTime);
    }
    else
    {
        PrintLabDummyConsoleLine("last successful cfg: none");
    }

    if (g_liveCfgState.lastFailureAction[0] != '\0' && g_liveCfgState.lastFailure[0] != '\0')
    {
        PrintLabDummyConsoleLine(
            "last cfg failure: action=%s reason=%s",
            g_liveCfgState.lastFailureAction,
            g_liveCfgState.lastFailure);
    }

    PrintCurrentCfgMetadata();
    PrintLabDummyConsoleLine("cfg commands: exp_cfg_apply <cfg_name_or_path> | exp_cfg_reload | exp_cfg_status | exp_lab_apply <cfg_name_or_path>");
}

const char *GetLabDummyProfileNames()
{
    return "unarmored, vest, vest_headprotected";
}

const LabDummyProfileDefinition *FindBuiltInLabDummyProfile(const char *profileName)
{
    if (profileName == NULL || profileName[0] == '\0')
    {
        return NULL;
    }

    if (StringEqualsIgnoreCase(profileName, "default"))
    {
        profileName = "unarmored";
    }

    for (int index = 0; index < ARRAYSIZE(kBuiltInLabDummyProfiles); ++index)
    {
        if (StringEqualsIgnoreCase(profileName, kBuiltInLabDummyProfiles[index].name))
        {
            return &kBuiltInLabDummyProfiles[index];
        }
    }

    return NULL;
}

bool ApplyBuiltInLabDummyProfile(const char *profileName)
{
    const LabDummyProfileDefinition *pProfile = FindBuiltInLabDummyProfile(profileName);
    if (pProfile == NULL)
    {
        return false;
    }

    CVAR_SET_STRING("sv_exp_glock_lab_target_profile_name", (char *)pProfile->name);
    CVAR_SET_STRING("sv_exp_glock_lab_dummy_armor", (char *)pProfile->armor);
    CVAR_SET_STRING("sv_exp_glock_lab_dummy_head_protected", (char *)pProfile->headProtected);
    CVAR_SET_STRING("sv_exp_glock_lab_dummy_armor_health_fraction", (char *)pProfile->armorHealthFraction);
    CVAR_SET_STRING("sv_exp_glock_lab_dummy_armor_drain_scale", (char *)pProfile->armorDrainScale);
    return true;
}

const char *ResolveGlockLabDummyModel()
{
    const char *requestedModel = GetNonEmptyCvarString(sv_exp_glock_lab_dummy_model, kDefaultGlockLabDummyModel);

    for (int index = 0; index < ARRAYSIZE(kAllowedGlockLabDummyModels); ++index)
    {
        if (StringEqualsIgnoreCase(requestedModel, kAllowedGlockLabDummyModels[index]))
        {
            return kAllowedGlockLabDummyModels[index];
        }
    }

    return kDefaultGlockLabDummyModel;
}

void ResetGlockLabDummyState()
{
    g_glockLabDummy = NULL;
    g_glockLabDummyAnchorPlayer = NULL;
    g_glockLabDummyRespawnPending = false;
    g_glockLabDummyRespawnTime = 0.0f;
    g_glockLabDummyRetryTime = 0.0f;
    ClearAllLabDummySavedSpots();
    ClearLabDummyTransformMemory(&g_glockLabDummyLastGoodTransform);
    ClearLabDummyFailureInfo(&g_glockLabDummyLastSpawnFailure);
    g_glockLabDummyLastFailureAt[0] = '\0';
}

void ClearGlockLabDummyRuntimeState()
{
    g_glockLabDummy = NULL;
    g_glockLabDummyAnchorPlayer = NULL;
    g_glockLabDummyRespawnPending = false;
    g_glockLabDummyRespawnTime = 0.0f;
    g_glockLabDummyRetryTime = 0.0f;
}

void ClearGlockLabDummyFailureState()
{
    ClearLabDummyFailureInfo(&g_glockLabDummyLastSpawnFailure);
    g_glockLabDummyLastFailureAt[0] = '\0';
}

void RefreshFutureHooksMapState()
{
    const char *currentMapName = GetCurrentMapName();
    if (StringEqualsIgnoreCase(g_futureHooksMapName, currentMapName))
    {
        return;
    }

    strncpy_s(g_futureHooksMapName, sizeof(g_futureHooksMapName), currentMapName, _TRUNCATE);
    ClearRoundRuntimeState();
    ResetGlockLabDummyState();
    ClearAllTeamSpawnSavedSpots();
    LoadLabDummySpotsForCurrentMap();
    LoadTeamSpawnSpotsForCurrentMap();
}

bool IsLabDummyClassname(const char *classname)
{
    return classname != NULL && classname[0] != '\0' && FStrEq(classname, kGlockLabDummyClassname);
}

bool IsLabDummyEntityInternal(CBaseEntity *pEntity)
{
    return pEntity != NULL &&
        pEntity->pev != NULL &&
        (pEntity->pev->flags & FL_KILLME) == 0 &&
        pEntity->pev->classname != 0 &&
        IsLabDummyClassname(STRING(pEntity->pev->classname));
}

CBasePlayer *FindFirstLivePlayer()
{
    if (gpGlobals == NULL)
    {
        return NULL;
    }

    for (int playerIndex = 1; playerIndex <= gpGlobals->maxClients; ++playerIndex)
    {
        CBaseEntity *pEntity = UTIL_PlayerByIndex(playerIndex);
        if (pEntity == NULL || !pEntity->IsPlayer() || pEntity->pev == NULL)
        {
            continue;
        }

        CBasePlayer *pPlayer = (CBasePlayer *)pEntity;
        if (pPlayer->IsAlive() && pPlayer->pev->deadflag == DEAD_NO)
        {
            return pPlayer;
        }
    }

    return NULL;
}

CBasePlayer *GetTrackedLabDummyAnchorPlayer(CBasePlayer *fallbackPlayer)
{
    CBaseEntity *pTrackedEntity = (CBaseEntity *)g_glockLabDummyAnchorPlayer;
    if (pTrackedEntity != NULL && pTrackedEntity->IsPlayer() && pTrackedEntity->pev != NULL)
    {
        CBasePlayer *pTrackedPlayer = (CBasePlayer *)pTrackedEntity;
        if (pTrackedPlayer->IsAlive())
        {
            return pTrackedPlayer;
        }
    }

    return fallbackPlayer;
}

CBasePlayer *FindCurrentLiveLabDummyAnchorPlayer()
{
    return GetTrackedLabDummyAnchorPlayer(FindFirstLivePlayer());
}

void RememberLabDummyLastGoodTransform(CBaseEntity *pDummy, const char *source, const char *candidate)
{
    if (pDummy == NULL || pDummy->pev == NULL)
    {
        return;
    }

    StoreLabDummyTransformMemory(&g_glockLabDummyLastGoodTransform, pDummy->pev->origin, pDummy->pev->angles, source, candidate);
}

CBaseEntity *FindExistingLabDummyEntity()
{
    return UTIL_FindEntityByClassname(NULL, kGlockLabDummyClassname);
}

void RemoveLabDummyEntities(const char *reason)
{
    CBaseEntity *pDummy = UTIL_FindEntityByClassname(NULL, kGlockLabDummyClassname);
    while (pDummy != NULL)
    {
        CBaseEntity *pNext = UTIL_FindEntityByClassname(pDummy, kGlockLabDummyClassname);
        LogGlockLabDummyClear(pDummy, reason);
        UTIL_Remove(pDummy);
        pDummy = pNext;
    }
}

void RemoveExtraLabDummyEntities(CBaseEntity *pDummyToKeep, const char *reason)
{
    CBaseEntity *pDummy = UTIL_FindEntityByClassname(NULL, kGlockLabDummyClassname);
    while (pDummy != NULL)
    {
        CBaseEntity *pNext = UTIL_FindEntityByClassname(pDummy, kGlockLabDummyClassname);
        if (pDummy != pDummyToKeep)
        {
            LogGlockLabDummyClear(pDummy, reason);
            UTIL_Remove(pDummy);
        }

        pDummy = pNext;
    }
}

CBaseEntity *GetTrackedLabDummyEntity()
{
    CBaseEntity *pDummy = (CBaseEntity *)g_glockLabDummy;
    if (IsLabDummyEntityInternal(pDummy))
    {
        RemoveExtraLabDummyEntities(pDummy, "dedupe");
        return pDummy;
    }

    pDummy = FindExistingLabDummyEntity();
    if (IsLabDummyEntityInternal(pDummy))
    {
        RemoveExtraLabDummyEntities(pDummy, "dedupe");
        g_glockLabDummy = pDummy;
        RememberLabDummyLastGoodTransform(pDummy, g_glockLabDummyLastGoodTransform.source, g_glockLabDummyLastGoodTransform.candidate);
        return pDummy;
    }

    g_glockLabDummy = NULL;
    return NULL;
}

void ClearAllLabDummyEntities(const char *reason)
{
    RemoveLabDummyEntities(reason);
    ClearGlockLabDummyRuntimeState();
}

void ApplyPlayerSpawnTransform(CBasePlayer *pPlayer, const Vector &origin, const Vector &angles)
{
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        return;
    }

    UTIL_SetOrigin(pPlayer->pev, origin);
    pPlayer->pev->origin = origin;
    pPlayer->pev->v_angle = angles;
    pPlayer->pev->velocity = g_vecZero;
    pPlayer->pev->basevelocity = g_vecZero;
    pPlayer->pev->angles = angles;
    pPlayer->pev->punchangle = g_vecZero;
    pPlayer->pev->fixangle = TRUE;
}

bool TryBuildTeamSpawnCandidateTransform(
    const TeamSpawnSavedSpotRecord &spot,
    CBasePlayer *pPlayer,
    const TeamSpawnPlacementCandidate &candidate,
    Vector *pOrigin,
    Vector *pAngles,
    char *candidateLabel,
    size_t candidateLabelSize,
    char *failureCode,
    size_t failureCodeSize,
    char *failureReason,
    size_t failureReasonSize)
{
    Vector referenceAngles = spot.angles;
    referenceAngles.x = 0.0f;
    referenceAngles.z = 0.0f;
    UTIL_MakeVectors(referenceAngles);

    Vector desiredOrigin = spot.origin +
        (gpGlobals->v_forward * candidate.forwardOffset) +
        (gpGlobals->v_right * candidate.rightOffset);
    desiredOrigin.z += candidate.upOffset;

    strncpy_s(candidateLabel, candidateLabelSize, candidate.label != NULL ? candidate.label : "candidate", _TRUNCATE);
    edict_t *ignoreEdict = pPlayer != NULL ? pPlayer->edict() : NULL;

    TraceResult exactHullTrace;
    UTIL_TraceHull(desiredOrigin, desiredOrigin, dont_ignore_monsters, human_hull, ignoreEdict, &exactHullTrace);
    if (!exactHullTrace.fStartSolid && !exactHullTrace.fAllSolid)
    {
        *pOrigin = desiredOrigin;
        *pAngles = referenceAngles;
        failureCode[0] = '\0';
        failureReason[0] = '\0';
        return true;
    }

    TraceResult groundTrace;
    UTIL_TraceLine(
        desiredOrigin + Vector(0.0f, 0.0f, 36.0f),
        desiredOrigin - Vector(0.0f, 0.0f, 72.0f),
        ignore_monsters,
        ignoreEdict,
        &groundTrace);

    if (groundTrace.fStartSolid || groundTrace.fAllSolid)
    {
        strcpy_s(failureCode, failureCodeSize, "blocked_ground");
        strcpy_s(failureReason, failureReasonSize, "ground trace started inside solid space");
        return false;
    }

    if (groundTrace.flFraction == 1.0f)
    {
        strcpy_s(failureCode, failureCodeSize, "no_floor");
        strcpy_s(failureReason, failureReasonSize, "no floor was found below the saved team spawn");
        return false;
    }

    Vector spawnOrigin = groundTrace.vecEndPos + Vector(0.0f, 0.0f, 1.0f);
    TraceResult hullTrace;
    UTIL_TraceHull(spawnOrigin, spawnOrigin, dont_ignore_monsters, human_hull, ignoreEdict, &hullTrace);
    if (hullTrace.fStartSolid || hullTrace.fAllSolid)
    {
        strcpy_s(failureCode, failureCodeSize, "blocked_hull");
        strcpy_s(failureReason, failureReasonSize, "the player standing hull is blocked at the saved team spawn");
        return false;
    }

    *pOrigin = spawnOrigin;
    *pAngles = referenceAngles;
    failureCode[0] = '\0';
    failureReason[0] = '\0';
    return true;
}

bool TryResolveNamedTeamSpawnSelection(
    const TeamSpawnSavedSpotRecord *spot,
    CBasePlayer *pPlayer,
    TeamSpawnSelection *selection,
    TeamSpawnFailureInfo *failure)
{
    if (spot == NULL || !spot->valid)
    {
        SetTeamSpawnFailureInfo(failure, kExpRoundTeamNone, "saved_spot_missing", kTeamSpawnSourceSavedSpot, "", "the requested saved team spawn spot is missing", "", "");
        return false;
    }

    TeamSpawnFailureInfo lastCandidateFailure = {};
    for (int candidateIndex = 0; candidateIndex < ARRAYSIZE(kTeamSpawnPlacementCandidates); ++candidateIndex)
    {
        Vector resolvedOrigin = g_vecZero;
        Vector resolvedAngles = g_vecZero;
        char candidateLabel[64];
        char failureCode[64];
        char failureReason[192];
        if (TryBuildTeamSpawnCandidateTransform(
                *spot,
                pPlayer,
                kTeamSpawnPlacementCandidates[candidateIndex],
                &resolvedOrigin,
                &resolvedAngles,
                candidateLabel,
                sizeof(candidateLabel),
                failureCode,
                sizeof(failureCode),
                failureReason,
                sizeof(failureReason)))
        {
            memset(selection, 0, sizeof(*selection));
            selection->valid = true;
            selection->teamId = spot->teamId;
            selection->origin = resolvedOrigin;
            selection->angles = resolvedAngles;
            strncpy_s(selection->source, sizeof(selection->source), kTeamSpawnSourceSavedSpot, _TRUNCATE);
            strncpy_s(selection->candidate, sizeof(selection->candidate), candidateLabel, _TRUNCATE);
            strncpy_s(selection->spotName, sizeof(selection->spotName), spot->name, _TRUNCATE);
            strncpy_s(selection->spotStorage, sizeof(selection->spotStorage), GetTeamSpawnSpotStorageLabel(spot), _TRUNCATE);
            return true;
        }

        char details[512];
        _snprintf_s(
            details,
            sizeof(details),
            _TRUNCATE,
            "saved team spawn \"%s\" for %s failed candidate %s: %s",
            spot->name,
            GetConfiguredTeamRoundName(spot->teamId),
            candidateLabel,
            failureReason);
        SetTeamSpawnFailureInfo(
            &lastCandidateFailure,
            spot->teamId,
            failureCode,
            kTeamSpawnSourceSavedSpot,
            candidateLabel,
            details,
            spot->name,
            GetTeamSpawnSpotStorageLabel(spot));
    }

    if (lastCandidateFailure.reason[0] == '\0')
    {
        SetTeamSpawnFailureInfo(
            &lastCandidateFailure,
            spot->teamId,
            "no_valid_candidate",
            kTeamSpawnSourceSavedSpot,
            "",
            "no valid candidate was found around the saved team spawn spot",
            spot->name,
            GetTeamSpawnSpotStorageLabel(spot));
    }

    if (failure != NULL)
    {
        *failure = lastCandidateFailure;
    }
    return false;
}

bool TrySelectPreferredTeamSpawnSelection(
    int teamId,
    CBasePlayer *pPlayer,
    TeamSpawnSelection *selection,
    TeamSpawnFailureInfo *failure,
    bool *usedDefaultFallback)
{
    if (selection == NULL)
    {
        return false;
    }

    memset(selection, 0, sizeof(*selection));
    if (failure != NULL)
    {
        ClearTeamSpawnFailureInfo(failure);
    }
    if (usedDefaultFallback != NULL)
    {
        *usedDefaultFallback = false;
    }

    teamId = NormalizeRoundTeamId(teamId);
    if (teamId == kExpRoundTeamNone)
    {
        SetTeamSpawnFailureInfo(failure, kExpRoundTeamNone, "missing_team", kTeamSpawnSourceSavedSpot, "", "no valid round team is assigned for this player spawn", "", "");
        return false;
    }

    TeamSpawnFailureInfo activeFailure = {};
    if (g_teamSpawnActiveSpotNames[teamId][0] != '\0')
    {
        const TeamSpawnSavedSpotRecord *activeSpot = GetActiveTeamSpawnSavedSpot(teamId);
        if (activeSpot != NULL && TryResolveNamedTeamSpawnSelection(activeSpot, pPlayer, selection, &activeFailure))
        {
            return true;
        }

        if (activeSpot == NULL)
        {
            char details[512];
            _snprintf_s(
                details,
                sizeof(details),
                _TRUNCATE,
                "active %s team spawn \"%s\" is selected for map %s but is not available",
                GetConfiguredTeamRoundName(teamId),
                g_teamSpawnActiveSpotNames[teamId],
                GetCurrentMapName());
            SetTeamSpawnFailureInfo(
                &activeFailure,
                teamId,
                "saved_spot_missing",
                kTeamSpawnSourceSavedSpot,
                "",
                details,
                g_teamSpawnActiveSpotNames[teamId],
                "");
        }
    }

    const TeamSpawnSavedSpotRecord *defaultSpot = GetDefaultTeamSpawnSavedSpot(teamId);
    if (defaultSpot != NULL &&
        (g_teamSpawnActiveSpotNames[teamId][0] == '\0' || !StringEqualsIgnoreCase(g_teamSpawnActiveSpotNames[teamId], defaultSpot->name)))
    {
        TeamSpawnFailureInfo defaultFailure = {};
        if (TryResolveNamedTeamSpawnSelection(defaultSpot, pPlayer, selection, &defaultFailure))
        {
            if (usedDefaultFallback != NULL)
            {
                *usedDefaultFallback = true;
            }
            return true;
        }

        if (defaultFailure.reason[0] != '\0')
        {
            activeFailure = defaultFailure;
        }
    }

    if (activeFailure.reason[0] != '\0')
    {
        if (failure != NULL)
        {
            *failure = activeFailure;
        }
        return false;
    }

    if (g_teamSpawnSavedSpotCounts[teamId] > 0)
    {
        char details[512];
        _snprintf_s(
            details,
            sizeof(details),
            _TRUNCATE,
            "no active %s team spawn is selected and no \"%s\" fallback spot is available",
            GetConfiguredTeamRoundName(teamId),
            kDefaultLabDummySpotName);
        SetTeamSpawnFailureInfo(failure, teamId, "saved_spot_missing", kTeamSpawnSourceSavedSpot, "", details, "", "");
    }
    else
    {
        char details[512];
        _snprintf_s(
            details,
            sizeof(details),
            _TRUNCATE,
            "no saved team spawn spot is marked for %s on map %s",
            GetConfiguredTeamRoundName(teamId),
            GetCurrentMapName());
        SetTeamSpawnFailureInfo(failure, teamId, "saved_spot_missing", kTeamSpawnSourceSavedSpot, "", details, "", "");
    }

    return false;
}

void ApplyTeamRoundPlayerSpawnOverrideInternal(CBasePlayer *pPlayer)
{
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        return;
    }

    RefreshFutureHooksMapState();
    if (!ExpRoundModeEnabled() || !TeamRoundModeConfigured() || !TeamRoundManualSpawnsConfigured())
    {
        return;
    }

    const int teamId = GetAssignedRoundTeamId(pPlayer);
    if (teamId == kExpRoundTeamNone)
    {
        return;
    }

    TeamSpawnSelection selection = {};
    TeamSpawnFailureInfo failure = {};
    bool usedDefaultFallback = false;
    if (TrySelectPreferredTeamSpawnSelection(teamId, pPlayer, &selection, &failure, &usedDefaultFallback))
    {
        ApplyPlayerSpawnTransform(pPlayer, selection.origin, selection.angles);
        RememberTeamSpawnApplied(pPlayer, selection, usedDefaultFallback ? "default_spot_fallback" : "");
        ClearTeamSpawnFailureInfo(&g_teamSpawnLastFailure[teamId]);
        g_teamSpawnLastFailureAt[teamId][0] = '\0';
        LogTeamSpawnEvent(
            "team_spawn_applied",
            pPlayer,
            teamId,
            GetConfiguredTeamRoundName(teamId),
            selection.origin,
            selection.angles,
            selection.source,
            selection.candidate,
            selection.spotName,
            selection.spotStorage,
            usedDefaultFallback ? "default_spot_fallback" : "");
        return;
    }

    RememberTeamSpawnFailure(teamId, failure);
    LogTeamSpawnEvent(
        "team_spawn_failed",
        pPlayer,
        teamId,
        GetConfiguredTeamRoundName(teamId),
        pPlayer->pev->origin,
        pPlayer->pev->angles,
        failure.source,
        failure.candidate,
        failure.spotName,
        failure.spotStorage,
        failure.reason);

    TeamSpawnSelection fallbackSelection = {};
    fallbackSelection.valid = true;
    fallbackSelection.teamId = teamId;
    fallbackSelection.origin = pPlayer->pev->origin;
    fallbackSelection.angles = pPlayer->pev->angles;
    strncpy_s(fallbackSelection.source, sizeof(fallbackSelection.source), kTeamSpawnSourceDmSpawn, _TRUNCATE);
    strncpy_s(fallbackSelection.candidate, sizeof(fallbackSelection.candidate), "game_rules_fallback", _TRUNCATE);
    RememberTeamSpawnApplied(pPlayer, fallbackSelection, failure.reason);
    LogTeamSpawnEvent(
        "team_spawn_applied",
        pPlayer,
        teamId,
        GetConfiguredTeamRoundName(teamId),
        fallbackSelection.origin,
        fallbackSelection.angles,
        fallbackSelection.source,
        fallbackSelection.candidate,
        "",
        "",
        failure.reason);
}

edict_t *GetLabDummyPlacementIgnoreEdict(CBasePlayer *pAnchorPlayer)
{
    CBaseEntity *pDummy = (CBaseEntity *)g_glockLabDummy;
    if (IsLabDummyEntityInternal(pDummy) && pDummy->edict() != NULL)
    {
        return pDummy->edict();
    }

    return pAnchorPlayer != NULL ? pAnchorPlayer->edict() : NULL;
}

bool TryValidateLabDummyExactTransform(const Vector &origin, CBasePlayer *pIgnorePlayer, char *failureCode, size_t failureCodeSize, char *failureReason, size_t failureReasonSize)
{
    edict_t *ignoreEdict = GetLabDummyPlacementIgnoreEdict(pIgnorePlayer);

    TraceResult groundTrace;
    UTIL_TraceLine(
        origin + Vector(0.0f, 0.0f, 36.0f),
        origin - Vector(0.0f, 0.0f, 72.0f),
        ignore_monsters,
        ignoreEdict,
        &groundTrace);

    if (groundTrace.fStartSolid || groundTrace.fAllSolid)
    {
        strcpy_s(failureCode, failureCodeSize, "blocked_ground");
        strcpy_s(failureReason, failureReasonSize, "ground trace started inside solid space");
        return false;
    }

    if (groundTrace.flFraction == 1.0f)
    {
        strcpy_s(failureCode, failureCodeSize, "no_floor");
        strcpy_s(failureReason, failureReasonSize, "no floor was found below the target spot");
        return false;
    }

    TraceResult hullTrace;
    UTIL_TraceHull(origin, origin, dont_ignore_monsters, human_hull, ignoreEdict, &hullTrace);
    if (hullTrace.fStartSolid || hullTrace.fAllSolid)
    {
        strcpy_s(failureCode, failureCodeSize, "blocked_hull");
        strcpy_s(failureReason, failureReasonSize, "the dummy's standing hull is blocked");
        return false;
    }

    failureCode[0] = '\0';
    failureReason[0] = '\0';
    return true;
}

bool TryResolveNamedSavedLabDummySelection(
    const LabDummySavedSpotRecord *spot,
    CBasePlayer *pAnchorPlayer,
    LabDummySpawnSelection *selection,
    LabDummyFailureInfo *failure)
{
    if (spot == NULL || !spot->valid)
    {
        SetLabDummyFailureInfo(failure, "saved_spot_missing", kLabDummySourceSavedSpot, "", "the requested saved target spot is missing");
        return false;
    }

    char failureCode[64];
    char failureReason[192];
    if (!TryValidateLabDummyExactTransform(spot->origin, pAnchorPlayer, failureCode, sizeof(failureCode), failureReason, sizeof(failureReason)))
    {
        char details[512];
        _snprintf_s(
            details,
            sizeof(details),
            _TRUNCATE,
            "saved target spot \"%s\" is no longer valid: %s",
            spot->name,
            failureReason);
        SetLabDummyFailureInfo(
            failure,
            "saved_spot_invalid",
            kLabDummySourceSavedSpot,
            spot->candidate[0] != '\0' ? spot->candidate : "marked_spot",
            details,
            spot->name,
            GetLabDummySpotStorageLabel(spot));
        return false;
    }

    CopyLabDummySavedSpotToSelection(*spot, pAnchorPlayer, selection);
    return true;
}

bool TryResolveSavedLabDummySelection(CBasePlayer *pAnchorPlayer, LabDummySpawnSelection *selection, LabDummyFailureInfo *failure)
{
    const LabDummySavedSpotRecord *activeSpot = GetLabDummyActiveSavedSpot();
    if (g_glockLabDummyActiveSpotName[0] != '\0' && activeSpot == NULL)
    {
        char details[512];
        _snprintf_s(
            details,
            sizeof(details),
            _TRUNCATE,
            "active target spot \"%s\" is selected for map %s but is not available",
            g_glockLabDummyActiveSpotName,
            GetCurrentMapName());
        SetLabDummyFailureInfo(
            failure,
            "saved_spot_missing",
            kLabDummySourceSavedSpot,
            "",
            details,
            g_glockLabDummyActiveSpotName,
            "");
        return false;
    }

    const LabDummySavedSpotRecord *spot = GetPreferredLabDummySavedSpot(NULL);
    if (spot == NULL)
    {
        char details[512];
        if (g_glockLabDummySavedSpotCount > 0)
        {
            _snprintf_s(
                details,
                sizeof(details),
                _TRUNCATE,
                "no active target spot is selected for map %s and no \"%s\" fallback spot is available",
                GetCurrentMapName(),
                kDefaultLabDummySpotName);
        }
        else
        {
            strcpy_s(details, sizeof(details), "no saved target spot is marked for this map");
        }

        SetLabDummyFailureInfo(failure, "saved_spot_missing", kLabDummySourceSavedSpot, "", details);
        return false;
    }

    return TryResolveNamedSavedLabDummySelection(spot, pAnchorPlayer, selection, failure);
}

bool TryResolveLastGoodLabDummySelection(CBasePlayer *pAnchorPlayer, LabDummySpawnSelection *selection, LabDummyFailureInfo *failure)
{
    if (!g_glockLabDummyLastGoodTransform.valid)
    {
        SetLabDummyFailureInfo(failure, "last_good_missing", kLabDummySourceLastGood, "", "no previous successful target transform is available yet");
        return false;
    }

    char failureCode[64];
    char failureReason[192];
    if (!TryValidateLabDummyExactTransform(g_glockLabDummyLastGoodTransform.origin, pAnchorPlayer, failureCode, sizeof(failureCode), failureReason, sizeof(failureReason)))
    {
        char details[512];
        _snprintf_s(details, sizeof(details), _TRUNCATE, "last known good target transform is no longer valid: %s", failureReason);
        SetLabDummyFailureInfo(
            failure,
            "last_good_invalid",
            kLabDummySourceLastGood,
            g_glockLabDummyLastGoodTransform.candidate[0] != '\0' ? g_glockLabDummyLastGoodTransform.candidate : "last_good",
            details);
        return false;
    }

    selection->origin = g_glockLabDummyLastGoodTransform.origin;
    selection->angles = g_glockLabDummyLastGoodTransform.angles;
    selection->anchorPlayer = pAnchorPlayer;
    strncpy_s(selection->source, sizeof(selection->source), kLabDummySourceLastGood, _TRUNCATE);
    strncpy_s(selection->candidate, sizeof(selection->candidate), g_glockLabDummyLastGoodTransform.candidate[0] != '\0' ? g_glockLabDummyLastGoodTransform.candidate : "last_good", _TRUNCATE);
    return true;
}

bool TryBuildLabDummySpawnTransformCandidate(
    CBasePlayer *pPlayer,
    float spawnDistance,
    const LabDummyPlacementCandidate &candidate,
    Vector *pOrigin,
    Vector *pAngles,
    char *candidateLabel,
    size_t candidateLabelSize,
    char *failureCode,
    size_t failureCodeSize,
    char *failureReason,
    size_t failureReasonSize)
{
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        strcpy_s(failureCode, failureCodeSize, "missing_anchor");
        strcpy_s(failureReason, failureReasonSize, "no current live player anchor is available yet");
        return false;
    }

    const float candidateDistance = spawnDistance + candidate.forwardOffset;
    const float clampedDistance = candidateDistance >= 48.0f ? candidateDistance : 48.0f;

    Vector referenceAngles = pPlayer->pev->v_angle;
    referenceAngles.x = 0.0f;
    referenceAngles.z = 0.0f;
    UTIL_MakeVectors(referenceAngles);

    Vector desiredOrigin = pPlayer->pev->origin +
        (gpGlobals->v_forward * clampedDistance) +
        (gpGlobals->v_right * (ExpGlockLabDummyOffsetRight() + candidate.rightOffset));
    desiredOrigin.z += ExpGlockLabDummyOffsetUp() + candidate.upOffset;

    _snprintf_s(candidateLabel, candidateLabelSize, _TRUNCATE, "%s@%.1f", candidate.label, clampedDistance);
    edict_t *ignoreEdict = GetLabDummyPlacementIgnoreEdict(pPlayer);

    TraceResult groundTrace;
    UTIL_TraceLine(
        desiredOrigin + Vector(0.0f, 0.0f, 64.0f),
        desiredOrigin - Vector(0.0f, 0.0f, 1024.0f),
        ignore_monsters,
        ignoreEdict,
        &groundTrace);

    if (groundTrace.fStartSolid || groundTrace.fAllSolid)
    {
        strcpy_s(failureCode, failureCodeSize, "blocked_ground");
        strcpy_s(failureReason, failureReasonSize, "ground trace started inside solid space");
        return false;
    }

    if (groundTrace.flFraction == 1.0f)
    {
        strcpy_s(failureCode, failureCodeSize, "no_floor");
        strcpy_s(failureReason, failureReasonSize, "no floor was found near the requested test position");
        return false;
    }

    Vector spawnOrigin = groundTrace.vecEndPos + Vector(0.0f, 0.0f, 37.0f);

    TraceResult frontTrace;
    UTIL_TraceLine(
        pPlayer->pev->origin + pPlayer->pev->view_ofs,
        spawnOrigin + Vector(0.0f, 0.0f, 36.0f),
        ignore_monsters,
        ignoreEdict,
        &frontTrace);

    if (frontTrace.fStartSolid || frontTrace.fAllSolid || frontTrace.flFraction < 1.0f)
    {
        strcpy_s(failureCode, failureCodeSize, "blocked_path");
        strcpy_s(failureReason, failureReasonSize, "the path from the player to the target spot is blocked");
        return false;
    }

    TraceResult hullTrace;
    UTIL_TraceHull(spawnOrigin, spawnOrigin, dont_ignore_monsters, human_hull, ignoreEdict, &hullTrace);
    if (hullTrace.fStartSolid || hullTrace.fAllSolid)
    {
        strcpy_s(failureCode, failureCodeSize, "blocked_hull");
        strcpy_s(failureReason, failureReasonSize, "the dummy's standing hull is blocked");
        return false;
    }

    Vector spawnAngles = referenceAngles;
    if (ExpGlockLabDummyFacePlayer())
    {
        Vector toPlayer = pPlayer->pev->origin - spawnOrigin;
        toPlayer.z = 0.0f;

        if (toPlayer.Length2D() > 1.0f)
        {
            spawnAngles = UTIL_VecToAngles(toPlayer);
        }
    }

    spawnAngles.x = 0.0f;
    spawnAngles.z = 0.0f;

    *pOrigin = spawnOrigin;
    *pAngles = spawnAngles;
    failureCode[0] = '\0';
    failureReason[0] = '\0';
    return true;
}

bool TryBuildLabDummySpawnTransformFromAnchor(CBasePlayer *pPlayer, LabDummySpawnSelection *selection, LabDummyFailureInfo *failure)
{
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        SetLabDummyFailureInfo(failure, "missing_anchor", kLabDummySourceCurrentAnchor, "", "no current live player anchor is available yet");
        return false;
    }

    const float requestedDistance = ExpGlockLabDummySpawnDistance();
    const float minimumDistance = requestedDistance > kGlockLabDummyPlacementMinDistance ? kGlockLabDummyPlacementMinDistance : requestedDistance;
    LabDummyFailureInfo lastCandidateFailure = {};

    for (float candidateDistance = requestedDistance; candidateDistance > minimumDistance + 0.1f; candidateDistance -= kGlockLabDummyPlacementSearchStep)
    {
        for (int candidateIndex = 0; candidateIndex < ARRAYSIZE(kLabDummyPlacementCandidates); ++candidateIndex)
        {
            char candidateLabel[64];
            char failureCode[64];
            char failureReason[192];
            if (TryBuildLabDummySpawnTransformCandidate(
                    pPlayer,
                    candidateDistance,
                    kLabDummyPlacementCandidates[candidateIndex],
                    &selection->origin,
                    &selection->angles,
                    candidateLabel,
                    sizeof(candidateLabel),
                    failureCode,
                    sizeof(failureCode),
                    failureReason,
                    sizeof(failureReason)))
            {
                selection->anchorPlayer = pPlayer;
                strncpy_s(selection->source, sizeof(selection->source), kLabDummySourceCurrentAnchor, _TRUNCATE);
                strncpy_s(selection->candidate, sizeof(selection->candidate), candidateLabel, _TRUNCATE);
                return true;
            }

            SetLabDummyFailureInfo(&lastCandidateFailure, failureCode, kLabDummySourceCurrentAnchor, candidateLabel, failureReason);
        }
    }

    for (int candidateIndex = 0; candidateIndex < ARRAYSIZE(kLabDummyPlacementCandidates); ++candidateIndex)
    {
        char candidateLabel[64];
        char failureCode[64];
        char failureReason[192];
        if (TryBuildLabDummySpawnTransformCandidate(
                pPlayer,
                minimumDistance,
                kLabDummyPlacementCandidates[candidateIndex],
                &selection->origin,
                &selection->angles,
                candidateLabel,
                sizeof(candidateLabel),
                failureCode,
                sizeof(failureCode),
                failureReason,
                sizeof(failureReason)))
        {
            selection->anchorPlayer = pPlayer;
            strncpy_s(selection->source, sizeof(selection->source), kLabDummySourceCurrentAnchor, _TRUNCATE);
            strncpy_s(selection->candidate, sizeof(selection->candidate), candidateLabel, _TRUNCATE);
            return true;
        }

        SetLabDummyFailureInfo(&lastCandidateFailure, failureCode, kLabDummySourceCurrentAnchor, candidateLabel, failureReason);
    }

    char details[512];
    if (requestedDistance > minimumDistance + 0.1f)
    {
        _snprintf_s(
            details,
            sizeof(details),
            _TRUNCATE,
            "no valid standing target position was found between %.1f and %.1f units; last candidate %s failed with %s: %s",
            requestedDistance,
            minimumDistance,
            lastCandidateFailure.candidate[0] != '\0' ? lastCandidateFailure.candidate : "unknown",
            lastCandidateFailure.code[0] != '\0' ? lastCandidateFailure.code : "unknown",
            lastCandidateFailure.reason[0] != '\0' ? lastCandidateFailure.reason : "unknown placement failure");
    }
    else
    {
        _snprintf_s(
            details,
            sizeof(details),
            _TRUNCATE,
            "no valid standing target position was found at %.1f units; last candidate %s failed with %s: %s",
            minimumDistance,
            lastCandidateFailure.candidate[0] != '\0' ? lastCandidateFailure.candidate : "unknown",
            lastCandidateFailure.code[0] != '\0' ? lastCandidateFailure.code : "unknown",
            lastCandidateFailure.reason[0] != '\0' ? lastCandidateFailure.reason : "unknown placement failure");
    }

    SetLabDummyFailureInfo(
        failure,
        "no_valid_candidate",
        kLabDummySourceCurrentAnchor,
        lastCandidateFailure.candidate,
        details);
    return false;
}

bool TrySelectPreferredLabDummySpawnTransform(CBasePlayer *pAnchorPlayer, LabDummySpawnSelection *selection, LabDummyFailureInfo *failure)
{
    LabDummyFailureInfo savedFailure = {};
    LabDummyFailureInfo anchorFailure = {};
    LabDummyFailureInfo lastGoodFailure = {};
    char attempts[1024] = "";

    if (TryResolveSavedLabDummySelection(pAnchorPlayer, selection, &savedFailure))
    {
        return true;
    }

    AppendLabDummyFailureAttempt(attempts, sizeof(attempts), savedFailure);

    if (TryBuildLabDummySpawnTransformFromAnchor(pAnchorPlayer, selection, &anchorFailure))
    {
        return true;
    }

    AppendLabDummyFailureAttempt(attempts, sizeof(attempts), anchorFailure);

    if (g_glockLabDummyLastGoodTransform.valid)
    {
        if (TryResolveLastGoodLabDummySelection(pAnchorPlayer, selection, &lastGoodFailure))
        {
            return true;
        }

        AppendLabDummyFailureAttempt(attempts, sizeof(attempts), lastGoodFailure);
    }

    if (attempts[0] == '\0')
    {
        strcpy_s(attempts, sizeof(attempts), "no saved target spot, no current live player anchor, and no last known good target transform are available yet");
    }

    LabDummyFailureInfo chosenFailure = {};
    if (savedFailure.code[0] != '\0' && !StringEqualsIgnoreCase(savedFailure.code, "saved_spot_missing"))
    {
        chosenFailure = savedFailure;
    }
    else if (anchorFailure.code[0] != '\0' && !StringEqualsIgnoreCase(anchorFailure.code, "missing_anchor"))
    {
        chosenFailure = anchorFailure;
    }
    else if (lastGoodFailure.code[0] != '\0' && !StringEqualsIgnoreCase(lastGoodFailure.code, "last_good_missing"))
    {
        chosenFailure = lastGoodFailure;
    }
    else if (savedFailure.code[0] != '\0')
    {
        chosenFailure = savedFailure;
    }
    else if (anchorFailure.code[0] != '\0')
    {
        chosenFailure = anchorFailure;
    }
    else
    {
        chosenFailure = lastGoodFailure;
    }

    SetLabDummyFailureInfo(
        failure,
        chosenFailure.code[0] != '\0' ? chosenFailure.code : "missing_anchor",
        chosenFailure.source[0] != '\0' ? chosenFailure.source : kLabDummySourceCurrentAnchor,
        chosenFailure.candidate,
        attempts,
        chosenFailure.spotName,
        chosenFailure.spotStorage);
    return false;
}

void LogLabDummySpawnFailure(const LabDummyFailureInfo &failure, CBasePlayer *pAnchorPlayer)
{
    g_glockLabDummyLastSpawnFailure = failure;
    FormatFutureGameplayTimestamp(g_glockLabDummyLastFailureAt, sizeof(g_glockLabDummyLastFailureAt));

    ALERT(
        at_console,
        "[hl-server] target dummy spawn failed [%s%s%s]: %s\n",
        failure.code[0] != '\0' ? failure.code : "failed",
        failure.candidate[0] != '\0' ? "/" : "",
        failure.candidate[0] != '\0' ? failure.candidate : "",
        failure.reason[0] != '\0' ? failure.reason : "unknown target placement failure");
    LogGlockLabDummySpawnFailed(
        pAnchorPlayer,
        failure.source,
        failure.candidate,
        failure.code,
        failure.reason,
        NULL,
        NULL,
        failure.spotName,
        failure.spotStorage);
}

void StabilizeGlockLabDummyEntity(CBaseEntity *pDummy)
{
    if (pDummy == NULL || pDummy->pev == NULL)
    {
        return;
    }

    if (g_glockLabDummyLastGoodTransform.valid)
    {
        const Vector delta = pDummy->pev->origin - g_glockLabDummyLastGoodTransform.origin;
        if (delta.Length2D() > 1.0f || fabs(delta.z) > 1.0f)
        {
            UTIL_SetOrigin(pDummy->pev, g_glockLabDummyLastGoodTransform.origin);
        }

        pDummy->pev->angles = g_glockLabDummyLastGoodTransform.angles;
    }

    pDummy->pev->ideal_yaw = pDummy->pev->angles.y;
    pDummy->pev->yaw_speed = 0;
    pDummy->pev->velocity = g_vecZero;
    pDummy->pev->avelocity = g_vecZero;
    pDummy->pev->framerate = 0.0f;
    pDummy->SetThink(NULL);
    pDummy->pev->nextthink = 0.0f;
}

void MoveGlockLabDummyToSelection(CBaseEntity *pDummy, CBasePlayer *pAnchorPlayer, const LabDummySpawnSelection &selection, const char *reason)
{
    if (pDummy == NULL || pDummy->pev == NULL)
    {
        return;
    }

    UTIL_SetOrigin(pDummy->pev, selection.origin);
    pDummy->pev->angles = selection.angles;
    g_glockLabDummyAnchorPlayer = pAnchorPlayer;
    StoreLabDummyTransformMemory(&g_glockLabDummyLastGoodTransform, selection.origin, selection.angles, selection.source, selection.candidate);
    StabilizeGlockLabDummyEntity(pDummy);
    ClearGlockLabDummyFailureState();
    LogGlockLabDummyReposition(
        pDummy,
        pAnchorPlayer,
        selection.origin,
        selection.angles,
        reason,
        selection.source,
        selection.candidate,
        selection.spotName,
        selection.spotStorage);
}

CBaseEntity *SpawnGlockLabDummy(const LabDummySpawnSelection &selection, bool logAsRespawn)
{
    LabDummyFailureInfo preflightFailure = {};
    if (!EnsureLabDummyMonsterSpawningEnabled(&preflightFailure, &selection))
    {
        LogLabDummySpawnFailure(preflightFailure, selection.anchorPlayer);
        return NULL;
    }

    edict_t *pent = CREATE_NAMED_ENTITY(MAKE_STRING("monster_generic"));
    if (FNullEnt(pent))
    {
        LabDummyFailureInfo failure = {};
        SetLabDummyFailureInfo(&failure, "entity_alloc_failed", selection.source, selection.candidate, "the engine could not allocate a monster_generic entity");
        LogLabDummySpawnFailure(failure, selection.anchorPlayer);
        return NULL;
    }

    entvars_t *pevDummy = VARS(pent);
    const float dummyHealth = ExpGlockLabDummyHealth();
    const float dummyArmor = ExpGlockLabDummyArmor();

    pevDummy->origin = selection.origin;
    pevDummy->angles = selection.angles;
    pevDummy->model = ALLOC_STRING(ExpGlockLabDummyModel());
    pevDummy->health = dummyHealth;
    pevDummy->max_health = dummyHealth;
    pevDummy->armorvalue = dummyArmor;
    pevDummy->netname = ALLOC_STRING(kGlockLabDummyDisplayName);
    SetBits(pevDummy->spawnflags, SF_MONSTER_GAG | SF_MONSTER_PRISONER);

    DispatchSpawn(pent);

    CBaseEntity *pDummy = CBaseEntity::Instance(pent);
    if (pDummy == NULL || pDummy->pev == NULL || (pDummy->pev->flags & FL_KILLME))
    {
        LabDummyFailureInfo failure = {};
        SetLabDummyFailureInfo(&failure, "entity_spawn_failed", selection.source, selection.candidate, "the dummy entity failed to finish spawning");
        LogLabDummySpawnFailure(failure, selection.anchorPlayer);
        return NULL;
    }

    pDummy->pev->classname = MAKE_STRING("glock_lab_dummy");
    pDummy->pev->targetname = MAKE_STRING("exp_glock_lab_dummy");
    pDummy->pev->netname = ALLOC_STRING(kGlockLabDummyDisplayName);
    pDummy->pev->health = dummyHealth;
    pDummy->pev->max_health = dummyHealth;
    pDummy->pev->armorvalue = dummyArmor;

    StoreLabDummyTransformMemory(&g_glockLabDummyLastGoodTransform, selection.origin, selection.angles, selection.source, selection.candidate);
    StabilizeGlockLabDummyEntity(pDummy);

    g_glockLabDummy = pDummy;
    g_glockLabDummyAnchorPlayer = selection.anchorPlayer;
    g_glockLabDummyRespawnPending = false;
    g_glockLabDummyRespawnTime = 0.0f;
    g_glockLabDummyRetryTime = 0.0f;
    ClearGlockLabDummyFailureState();

    LogGlockLabDummySpawn(
        pDummy,
        selection.anchorPlayer,
        logAsRespawn,
        selection.origin,
        selection.angles,
        selection.source,
        selection.candidate,
        selection.spotName,
        selection.spotStorage);
    return pDummy;
}

void MaintainGlockLabDummy()
{
    CBaseEntity *pDummy = GetTrackedLabDummyEntity();

    if (!ExpGlockLabDummyEnabled())
    {
        if (pDummy != NULL)
        {
            ClearAllLabDummyEntities("disabled");
        }
        else
        {
            ClearGlockLabDummyRuntimeState();
        }

        return;
    }

    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();
    if (pAnchorPlayer != NULL)
    {
        g_glockLabDummyAnchorPlayer = pAnchorPlayer;
    }

    if (pDummy != NULL && pDummy->IsAlive())
    {
        StabilizeGlockLabDummyEntity(pDummy);
        g_glockLabDummyRespawnPending = false;
        g_glockLabDummyRespawnTime = 0.0f;
        g_glockLabDummyRetryTime = 0.0f;
        return;
    }

    if (pDummy != NULL && !pDummy->IsAlive())
    {
        if (!ExpGlockLabDummyAutoRespawnEnabled())
        {
            g_glockLabDummyRespawnPending = false;
            g_glockLabDummyRespawnTime = 0.0f;
            return;
        }

        if (!g_glockLabDummyRespawnPending)
        {
            g_glockLabDummyRespawnPending = true;
            g_glockLabDummyRespawnTime = gpGlobals->time + ExpGlockLabDummyRespawnDelaySeconds();
            return;
        }

        if (gpGlobals->time < g_glockLabDummyRespawnTime)
        {
            return;
        }

        LogGlockLabDummyClear(pDummy, "respawn_cycle");
        UTIL_Remove(pDummy);
        g_glockLabDummy = NULL;
        pDummy = NULL;
    }
    else if (gpGlobals->time < g_glockLabDummyRetryTime)
    {
        return;
    }

    LabDummySpawnSelection selection = {};
    LabDummyFailureInfo failure = {};
    if (!TrySelectPreferredLabDummySpawnTransform(pAnchorPlayer, &selection, &failure))
    {
        LogLabDummySpawnFailure(failure, pAnchorPlayer);
        g_glockLabDummyRetryTime = gpGlobals->time + kGlockLabDummyRetryDelay;
        return;
    }

    if (SpawnGlockLabDummy(selection, g_glockLabDummyRespawnPending) != NULL)
    {
        return;
    }

    if (g_glockLabDummyRespawnPending)
    {
        g_glockLabDummyRespawnTime = gpGlobals->time + kGlockLabDummyRetryDelay;
    }
    else
    {
        g_glockLabDummyRetryTime = gpGlobals->time + kGlockLabDummyRetryDelay;
    }
}

void SetLabDummyEnabled(bool enabled)
{
    CVAR_SET_FLOAT("sv_exp_glock_lab_dummy", enabled ? 1.0f : 0.0f);
}

bool EnsureLabDummyMonsterSpawningEnabled(LabDummyFailureInfo *failure, const LabDummySpawnSelection *selection)
{
    if (CVAR_GET_FLOAT("mp_allowmonsters") != 0.0f)
    {
        return true;
    }

    CVAR_SET_FLOAT("mp_allowmonsters", 1.0f);
    if (CVAR_GET_FLOAT("mp_allowmonsters") != 0.0f)
    {
        return true;
    }

    SetLabDummyFailureInfo(
        failure,
        "monsters_disabled",
        selection != NULL ? selection->source : kLabDummySourceCurrentAnchor,
        selection != NULL ? selection->candidate : "",
        "mp_allowmonsters must be 1 before the server can spawn a monster_generic dummy");
    return false;
}

bool ApplyResolvedLiveCfgSelection(const char *action, const ResolvedLiveCfgSelection &selection)
{
    if (g_engfuncs.pfnServerCommand == NULL || g_engfuncs.pfnServerExecute == NULL)
    {
        const char *reason = "engine server command execution is unavailable";
        RecordLiveCfgFailure(action, reason);
        LogLiveCfgCommand(action, selection.requestedPath, selection.execPath, selection.resolvedPath, false, reason);
        PrintLabDummyConsoleLine("cfg %s failed: %s", GetValueOrFallback(action, "apply"), reason);
        return false;
    }

    char execCommand[kMaxLiveCfgExecPathLength + 16];
    _snprintf_s(execCommand, sizeof(execCommand), _TRUNCATE, "exec %s\n", selection.execPath);
    SERVER_COMMAND(execCommand);
    SERVER_EXECUTE();

    RecordLiveCfgSuccess(action, selection);
    EnsureWeaponDebugLogReady();
    LogLiveCfgCommand(action, selection.requestedPath, selection.execPath, selection.resolvedPath, true, NULL);

    PrintLabDummyConsoleLine(
        "%s cfg: exec=%s source=%s",
        StringEqualsIgnoreCase(action, "reload") ? "reloaded" : "applied",
        selection.execPath,
        selection.resolvedPath);
    PrintCurrentCfgMetadata();
    return true;
}

bool TryRespawnLabDummyInternal(const char *removeReason, char *summary, size_t summarySize, bool printStatusOnFailure)
{
    if (summary == NULL || summarySize == 0)
    {
        return false;
    }

    summary[0] = '\0';
    RefreshFutureHooksMapState();
    SetLabDummyEnabled(true);

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();
    LabDummySpawnSelection selection = {};
    LabDummyFailureInfo failure = {};
    if (!TrySelectPreferredLabDummySpawnTransform(pAnchorPlayer, &selection, &failure))
    {
        LogLabDummySpawnFailure(failure, pAnchorPlayer);
        _snprintf_s(summary, summarySize, _TRUNCATE, "target respawn failed: %s", failure.reason);
        if (printStatusOnFailure)
        {
            PrintLabDummyStatus();
        }
        return false;
    }

    if (pDummy != NULL)
    {
        RemoveLabDummyEntities(removeReason);
        g_glockLabDummy = NULL;
    }

    g_glockLabDummyRespawnPending = false;
    g_glockLabDummyRespawnTime = 0.0f;
    g_glockLabDummyRetryTime = 0.0f;

    if (SpawnGlockLabDummy(selection, true) == NULL)
    {
        _snprintf_s(
            summary,
            summarySize,
            _TRUNCATE,
            "target respawn failed: %s",
            g_glockLabDummyLastSpawnFailure.reason[0] != '\0' ? g_glockLabDummyLastSpawnFailure.reason : "the dummy could not be recreated");
        if (printStatusOnFailure)
        {
            PrintLabDummyStatus();
        }
        return false;
    }

    _snprintf_s(
        summary,
        summarySize,
        _TRUNCATE,
        "respawned \"%s\" using profile %s via %s/%s%s%s.",
        kGlockLabDummyDisplayName,
        ExpGlockLabTargetProfileName(),
        selection.source,
        selection.candidate[0] != '\0' ? selection.candidate : "default",
        selection.spotName[0] != '\0' ? " spot=" : "",
        selection.spotName[0] != '\0' ? selection.spotName : "");
    return true;
}

void PrintLabDummyStatus()
{
    RefreshFutureHooksMapState();

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();
    const LabDummyProfileDefinition *pCurrentProfile = FindBuiltInLabDummyProfile(ExpGlockLabTargetProfileName());
    const LabDummySavedSpotRecord *activeSavedSpot = GetLabDummyActiveSavedSpot();
    bool usingDefaultSavedSpot = false;
    const LabDummySavedSpotRecord *effectiveSavedSpot = GetPreferredLabDummySavedSpot(&usingDefaultSavedSpot);
    LabDummySpawnSelection nextSelection = {};
    LabDummyFailureInfo nextFailure = {};
    const bool respawnPossible = TrySelectPreferredLabDummySpawnTransform(pAnchorPlayer, &nextSelection, &nextFailure);
    char savedSpotNames[kMaxLabDummySpotListLength];
    char savedOrigin[64];
    char lastGoodOrigin[64];
    char nextOrigin[64];
    char currentOrigin[64];
    BuildLabDummySpotNamesSummary(savedSpotNames, sizeof(savedSpotNames));
    strcpy_s(savedOrigin, sizeof(savedOrigin), "n/a");
    strcpy_s(lastGoodOrigin, sizeof(lastGoodOrigin), "n/a");
    strcpy_s(nextOrigin, sizeof(nextOrigin), "n/a");
    strcpy_s(currentOrigin, sizeof(currentOrigin), "n/a");

    PrintLabDummyConsoleLine("target type: server-side standing dummy using stock model assets");
    PrintLabDummyConsoleLine(
        "target enabled=%d profile=%s health=%.1f armor=%.1f head_protected=%d autorespawn=%d respawn_delay=%.2f",
        ExpGlockLabDummyEnabled() ? 1 : 0,
        ExpGlockLabTargetProfileName(),
        ExpGlockLabDummyHealth(),
        ExpGlockLabDummyArmor(),
        ExpGlockLabDummyHeadProtected() ? 1 : 0,
        ExpGlockLabDummyAutoRespawnEnabled() ? 1 : 0,
        ExpGlockLabDummyRespawnDelaySeconds());
    PrintLabDummyConsoleLine(
        "target profile detail: %s",
        pCurrentProfile != NULL ? pCurrentProfile->description : "custom dummy coefficients");
    PrintLabDummyConsoleLine(
        "target placement: distance=%.1f right=%.1f up=%.1f face_player=%d",
        ExpGlockLabDummySpawnDistance(),
        ExpGlockLabDummyOffsetRight(),
        ExpGlockLabDummyOffsetUp(),
        ExpGlockLabDummyFacePlayer() ? 1 : 0);
    PrintLabDummyConsoleLine("target monster gate: mp_allowmonsters=%.0f", CVAR_GET_FLOAT("mp_allowmonsters"));

    if (pAnchorPlayer != NULL)
    {
        PrintLabDummyConsoleLine(
            "current live anchor: name=%s entindex=%d userid=%d",
            GetSafePlayerName(pAnchorPlayer),
            GetPlayerEntityIndex(pAnchorPlayer),
            GetPlayerUserId(pAnchorPlayer));
    }
    else
    {
        PrintLabDummyConsoleLine("current live anchor: none");
    }

    if (g_glockLabDummySavedSpotCount > 0)
    {
        PrintLabDummyConsoleLine(
            "saved target spots: count=%d active=%s names=%s",
            g_glockLabDummySavedSpotCount,
            g_glockLabDummyActiveSpotName[0] != '\0' ? g_glockLabDummyActiveSpotName : "none",
            savedSpotNames);

        if (activeSavedSpot != NULL)
        {
            FormatVector3(savedOrigin, sizeof(savedOrigin), activeSavedSpot->origin);
            PrintLabDummyConsoleLine(
                "active saved spot: name=%s storage=%s origin=%s yaw=%.1f candidate=%s created_at=%s updated_at=%s",
                activeSavedSpot->name,
                GetLabDummySpotStorageLabel(activeSavedSpot),
                savedOrigin,
                activeSavedSpot->angles.y,
                activeSavedSpot->candidate[0] != '\0' ? activeSavedSpot->candidate : "marked_spot",
                activeSavedSpot->createdAt[0] != '\0' ? activeSavedSpot->createdAt : "n/a",
                activeSavedSpot->updatedAt[0] != '\0' ? activeSavedSpot->updatedAt : "n/a");
        }
        else if (g_glockLabDummyActiveSpotName[0] != '\0')
        {
            PrintLabDummyConsoleLine("active saved spot: name=%s state=missing", g_glockLabDummyActiveSpotName);
        }
        else if (usingDefaultSavedSpot && effectiveSavedSpot != NULL)
        {
            FormatVector3(savedOrigin, sizeof(savedOrigin), effectiveSavedSpot->origin);
            PrintLabDummyConsoleLine(
                "active saved spot: none selected, fallback=%s storage=%s origin=%s yaw=%.1f candidate=%s",
                effectiveSavedSpot->name,
                GetLabDummySpotStorageLabel(effectiveSavedSpot),
                savedOrigin,
                effectiveSavedSpot->angles.y,
                effectiveSavedSpot->candidate[0] != '\0' ? effectiveSavedSpot->candidate : "marked_spot");
        }
        else
        {
            PrintLabDummyConsoleLine("active saved spot: none selected");
        }

        if (g_glockLabDummyTargetSpotsPath[0] != '\0')
        {
            PrintLabDummyConsoleLine("target spots file: %s", g_glockLabDummyTargetSpotsPath);
        }
    }
    else
    {
        PrintLabDummyConsoleLine("saved target spots: none");
        if (g_glockLabDummyTargetSpotsPath[0] != '\0')
        {
            PrintLabDummyConsoleLine("target spots file: %s", g_glockLabDummyTargetSpotsPath);
        }
    }

    if (g_glockLabDummyTargetSpotsLoadFailure[0] != '\0')
    {
        PrintLabDummyConsoleLine(
            "target spots load state: problem=%s",
            g_glockLabDummyTargetSpotsLoadFailure);
    }

    if (g_glockLabDummyLastGoodTransform.valid)
    {
        FormatVector3(lastGoodOrigin, sizeof(lastGoodOrigin), g_glockLabDummyLastGoodTransform.origin);
        PrintLabDummyConsoleLine(
            "last known good target transform: origin=%s yaw=%.1f source=%s candidate=%s",
            lastGoodOrigin,
            g_glockLabDummyLastGoodTransform.angles.y,
            g_glockLabDummyLastGoodTransform.source[0] != '\0' ? g_glockLabDummyLastGoodTransform.source : "unknown",
            g_glockLabDummyLastGoodTransform.candidate[0] != '\0' ? g_glockLabDummyLastGoodTransform.candidate : "last_good");
    }
    else
    {
        PrintLabDummyConsoleLine("last known good target transform: none");
    }

    if (g_glockLabDummyLastSpawnFailure.code[0] != '\0')
    {
        PrintLabDummyConsoleLine(
            "last spawn failure: at=%s code=%s source=%s candidate=%s%s%s%s%s reason=%s",
            g_glockLabDummyLastFailureAt[0] != '\0' ? g_glockLabDummyLastFailureAt : "unknown",
            g_glockLabDummyLastSpawnFailure.code,
            g_glockLabDummyLastSpawnFailure.source[0] != '\0' ? g_glockLabDummyLastSpawnFailure.source : "unknown",
            g_glockLabDummyLastSpawnFailure.candidate[0] != '\0' ? g_glockLabDummyLastSpawnFailure.candidate : "n/a",
            g_glockLabDummyLastSpawnFailure.spotName[0] != '\0' ? " spot=" : "",
            g_glockLabDummyLastSpawnFailure.spotName[0] != '\0' ? g_glockLabDummyLastSpawnFailure.spotName : "",
            g_glockLabDummyLastSpawnFailure.spotStorage[0] != '\0' ? " storage=" : "",
            g_glockLabDummyLastSpawnFailure.spotStorage[0] != '\0' ? g_glockLabDummyLastSpawnFailure.spotStorage : "",
            g_glockLabDummyLastSpawnFailure.reason);
    }
    else
    {
        PrintLabDummyConsoleLine("last spawn failure: none");
    }

    if (respawnPossible)
    {
        FormatVector3(nextOrigin, sizeof(nextOrigin), nextSelection.origin);
        PrintLabDummyConsoleLine(
            "target respawn possible: yes source=%s candidate=%s origin=%s yaw=%.1f%s%s%s%s",
            nextSelection.source,
            nextSelection.candidate[0] != '\0' ? nextSelection.candidate : "default",
            nextOrigin,
            nextSelection.angles.y,
            nextSelection.spotName[0] != '\0' ? " spot=" : "",
            nextSelection.spotName[0] != '\0' ? nextSelection.spotName : "",
            nextSelection.spotStorage[0] != '\0' ? " storage=" : "",
            nextSelection.spotStorage[0] != '\0' ? nextSelection.spotStorage : "");
    }
    else
    {
        PrintLabDummyConsoleLine("target respawn possible: no reason=%s", nextFailure.reason[0] != '\0' ? nextFailure.reason : "no valid placement source is available");
    }

    if (pDummy != NULL && pDummy->pev != NULL)
    {
        const char *dummyName = pDummy->pev->netname != 0 ? STRING(pDummy->pev->netname) : kGlockLabDummyDisplayName;
        const char *dummyClass = pDummy->pev->classname != 0 ? STRING(pDummy->pev->classname) : "unknown";
        const char *dummyModel = pDummy->pev->model != 0 ? STRING(pDummy->pev->model) : "unknown";
        FormatVector3(currentOrigin, sizeof(currentOrigin), pDummy->pev->origin);
        PrintLabDummyConsoleLine(
            "target entity: state=%s name=%s entindex=%d class=%s model=%s health=%.1f/%.1f armor=%.1f origin=%s yaw=%.1f",
            pDummy->IsAlive() ? "alive" : "dead",
            dummyName,
            pDummy->edict() != NULL ? ENTINDEX(pDummy->edict()) : -1,
            dummyClass,
            dummyModel,
            pDummy->pev->health,
            pDummy->pev->max_health,
            pDummy->pev->armorvalue,
            currentOrigin,
            pDummy->pev->angles.y);
    }
    else
    {
        PrintLabDummyConsoleLine("target entity: missing");
    }

    PrintLabDummyConsoleLine("built-in target profiles: %s", GetLabDummyProfileNames());
    PrintLabDummyConsoleLine("commands: exp_target_spawn | exp_target_clear | exp_target_mark [name] | exp_target_unmark [name] | exp_target_list | exp_target_use_saved <name> | exp_target_respawn | exp_target_status | exp_target_tp_front | exp_target_profile <name>");
}

void ExpCfgApplyCommand()
{
    char requestedPath[kMaxLiveCfgRequestLength];
    BuildCommandArgumentString(1, requestedPath, sizeof(requestedPath));
    if (requestedPath[0] == '\0')
    {
        PrintLabDummyConsoleLine("usage: exp_cfg_apply <cfg_name_or_path>");
        PrintLiveCfgStatus();
        return;
    }

    ResolvedLiveCfgSelection selection = {};
    char failureReason[kMaxLiveCfgFailureLength];
    if (!TryResolveLiveCfgSelection(requestedPath, &selection, failureReason, sizeof(failureReason)))
    {
        RecordLiveCfgFailure("apply", failureReason);
        LogLiveCfgCommand("apply", requestedPath, "", "", false, failureReason);
        PrintLabDummyConsoleLine("cfg apply failed: %s", failureReason);
        return;
    }

    ApplyResolvedLiveCfgSelection("apply", selection);
}

void ExpCfgReloadCommand()
{
    if (!g_liveCfgState.hasActiveCfg || g_liveCfgState.activeExecPath[0] == '\0')
    {
        const char *reason = "no active cfg is tracked yet. Use exp_cfg_apply <cfg_name_or_path> first.";
        RecordLiveCfgFailure("reload", reason);
        LogLiveCfgCommand("reload", "", "", "", false, reason);
        PrintLabDummyConsoleLine("cfg reload failed: %s", reason);
        return;
    }

    const char *reloadRequest = g_liveCfgState.activeExecPath;
    ResolvedLiveCfgSelection selection = {};
    char failureReason[kMaxLiveCfgFailureLength];
    if (!TryResolveLiveCfgSelection(reloadRequest, &selection, failureReason, sizeof(failureReason)))
    {
        RecordLiveCfgFailure("reload", failureReason);
        LogLiveCfgCommand("reload", reloadRequest, g_liveCfgState.activeExecPath, g_liveCfgState.activeResolvedPath, false, failureReason);
        PrintLabDummyConsoleLine("cfg reload failed: %s", failureReason);
        return;
    }

    ApplyResolvedLiveCfgSelection("reload", selection);
}

void ExpCfgStatusCommand()
{
    PrintLiveCfgStatus();
}

void ExpLabApplyCommand()
{
    char requestedPath[kMaxLiveCfgRequestLength];
    BuildCommandArgumentString(1, requestedPath, sizeof(requestedPath));
    if (requestedPath[0] == '\0')
    {
        PrintLabDummyConsoleLine("usage: exp_lab_apply <cfg_name_or_path>");
        return;
    }

    ResolvedLiveCfgSelection selection = {};
    char failureReason[kMaxLiveCfgFailureLength];
    if (!TryResolveLiveCfgSelection(requestedPath, &selection, failureReason, sizeof(failureReason)))
    {
        RecordLiveCfgFailure("apply", failureReason);
        LogLiveCfgCommand("apply", requestedPath, "", "", false, failureReason);
        PrintLabDummyConsoleLine("lab apply failed: %s", failureReason);
        return;
    }

    if (!ApplyResolvedLiveCfgSelection("apply", selection))
    {
        return;
    }

    char targetSummary[192];
    const bool targetRespawned = TryRespawnLabDummyInternal("lab_apply", targetSummary, sizeof(targetSummary), true);
    PrintLabDummyConsoleLine("%s", targetSummary);
    PrintLabDummyConsoleLine(
        "lab apply summary: cfg=%s target_refresh=%s target_profile=%s",
        selection.execPath,
        targetRespawned ? "ok" : "failed",
        ExpGlockLabTargetProfileName());
}

void ExpTargetSpawnCommand()
{
    RefreshFutureHooksMapState();
    SetLabDummyEnabled(true);

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    if (pDummy != NULL && pDummy->IsAlive())
    {
        PrintLabDummyConsoleLine("target is already active. Use exp_target_tp_front to move it or exp_target_respawn to recreate it.");
        PrintLabDummyStatus();
        return;
    }

    if (pDummy != NULL)
    {
        RemoveLabDummyEntities("command_spawn_cleanup");
        g_glockLabDummy = NULL;
    }

    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();
    LabDummySpawnSelection targetSelection = {};
    LabDummyFailureInfo failure = {};
    if (!TrySelectPreferredLabDummySpawnTransform(pAnchorPlayer, &targetSelection, &failure))
    {
        LogLabDummySpawnFailure(failure, pAnchorPlayer);
        PrintLabDummyConsoleLine("target spawn failed: %s", failure.reason);
        return;
    }

    g_glockLabDummyRespawnPending = false;
    g_glockLabDummyRespawnTime = 0.0f;
    g_glockLabDummyRetryTime = 0.0f;

    if (SpawnGlockLabDummy(targetSelection, false) == NULL)
    {
        PrintLabDummyStatus();
        return;
    }

    PrintLabDummyConsoleLine(
        "spawned \"%s\" using profile %s via %s/%s%s%s.",
        kGlockLabDummyDisplayName,
        ExpGlockLabTargetProfileName(),
        targetSelection.source,
        targetSelection.candidate[0] != '\0' ? targetSelection.candidate : "default",
        targetSelection.spotName[0] != '\0' ? " spot=" : "",
        targetSelection.spotName[0] != '\0' ? targetSelection.spotName : "");
}

void ExpTargetClearCommand()
{
    RefreshFutureHooksMapState();
    SetLabDummyEnabled(false);

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    if (pDummy == NULL)
    {
        ClearGlockLabDummyRuntimeState();
        PrintLabDummyConsoleLine("target is already clear. Automatic target spawning is now disabled.");
        return;
    }

    ClearAllLabDummyEntities("command_clear");
    PrintLabDummyConsoleLine("cleared the target and disabled automatic target spawning.");
}

void ExpTargetRespawnCommand()
{
    char summary[192];
    const bool success = TryRespawnLabDummyInternal("command_respawn", summary, sizeof(summary), true);
    PrintLabDummyConsoleLine("%s", summary);
    if (!success)
    {
        return;
    }
}

void ExpTargetStatusCommand()
{
    PrintLabDummyStatus();
}

void ExpTargetListCommand()
{
    RefreshFutureHooksMapState();

    PrintLabDummyConsoleLine(
        "saved target spots for map %s: count=%d active=%s file=%s",
        GetCurrentMapName(),
        g_glockLabDummySavedSpotCount,
        g_glockLabDummyActiveSpotName[0] != '\0' ? g_glockLabDummyActiveSpotName : "none",
        g_glockLabDummyTargetSpotsPath[0] != '\0' ? g_glockLabDummyTargetSpotsPath : "n/a");

    if (g_glockLabDummySavedSpotCount <= 0)
    {
        if (g_glockLabDummyTargetSpotsLoadFailure[0] != '\0')
        {
            PrintLabDummyConsoleLine("target spots load state: problem=%s", g_glockLabDummyTargetSpotsLoadFailure);
        }
        return;
    }

    for (int spotIndex = 0; spotIndex < g_glockLabDummySavedSpotCount; ++spotIndex)
    {
        const LabDummySavedSpotRecord &spot = g_glockLabDummySavedSpots[spotIndex];
        char origin[64];
        FormatVector3(origin, sizeof(origin), spot.origin);
        PrintLabDummyConsoleLine(
            "spot[%d]: name=%s%s storage=%s origin=%s yaw=%.1f candidate=%s updated_at=%s",
            spotIndex,
            spot.name,
            StringEqualsIgnoreCase(g_glockLabDummyActiveSpotName, spot.name) ? " [active]" : "",
            GetLabDummySpotStorageLabel(&spot),
            origin,
            spot.angles.y,
            spot.candidate[0] != '\0' ? spot.candidate : "marked_spot",
            spot.updatedAt[0] != '\0' ? spot.updatedAt : "n/a");
    }
}

void ExpTargetMarkCommand()
{
    RefreshFutureHooksMapState();

    char spotName[kMaxLabDummySpotNameLength];
    char failureReason[kMaxLabDummySpotFileFailureLength];
    if (!TryGetLabDummySpotCommandName(1, spotName, sizeof(spotName), true, failureReason, sizeof(failureReason)))
    {
        PrintLabDummyConsoleLine("target mark failed: %s", failureReason);
        return;
    }

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();
    Vector markedOrigin = g_vecZero;
    Vector markedAngles = g_vecZero;
    char markCandidate[64] = "";

    if (pDummy != NULL && pDummy->pev != NULL && pDummy->IsAlive())
    {
        markedOrigin = pDummy->pev->origin;
        markedAngles = pDummy->pev->angles;
        strncpy_s(markCandidate, sizeof(markCandidate), "current_dummy", _TRUNCATE);
    }
    else
    {
        LabDummySpawnSelection selection = {};
        LabDummyFailureInfo failure = {};
        if (!TryBuildLabDummySpawnTransformFromAnchor(pAnchorPlayer, &selection, &failure))
        {
            LogLabDummySpawnFailure(failure, pAnchorPlayer);
            PrintLabDummyConsoleLine("target mark failed: %s", failure.reason);
            return;
        }

        markedOrigin = selection.origin;
        markedAngles = selection.angles;
        strncpy_s(markCandidate, sizeof(markCandidate), selection.candidate, _TRUNCATE);
    }

    LabDummySavedSpotRecord *spot = UpsertLabDummySavedSpot(spotName, markedOrigin, markedAngles, markCandidate, failureReason, sizeof(failureReason));
    if (spot == NULL)
    {
        PrintLabDummyConsoleLine("target mark failed: %s", failureReason);
        return;
    }

    strncpy_s(g_glockLabDummyActiveSpotName, sizeof(g_glockLabDummyActiveSpotName), spot->name, _TRUNCATE);
    const bool savedToDisk = SaveLabDummySpotsForCurrentMap(failureReason, sizeof(failureReason));
    LogGlockLabDummyMark("target_mark", pAnchorPlayer, markedOrigin, markedAngles, markCandidate, spot->name, kLabDummySpotStorageSession);

    char origin[64];
    FormatVector3(origin, sizeof(origin), markedOrigin);
    PrintLabDummyConsoleLine(
        "saved target spot \"%s\" for map %s: origin=%s yaw=%.1f active=%s disk=%s",
        spot->name,
        GetCurrentMapName(),
        origin,
        markedAngles.y,
        g_glockLabDummyActiveSpotName,
        savedToDisk ? g_glockLabDummyTargetSpotsPath : failureReason);
}

void ExpTargetUnmarkCommand()
{
    RefreshFutureHooksMapState();

    char spotName[kMaxLabDummySpotNameLength];
    char failureReason[kMaxLabDummySpotFileFailureLength];
    if (!TryGetLabDummySpotCommandName(1, spotName, sizeof(spotName), true, failureReason, sizeof(failureReason)))
    {
        PrintLabDummyConsoleLine("target unmark failed: %s", failureReason);
        return;
    }

    LabDummySavedSpotRecord removedSpot = {};
    if (!RemoveLabDummySavedSpot(spotName, &removedSpot, failureReason, sizeof(failureReason)))
    {
        PrintLabDummyConsoleLine("target unmark failed: %s", failureReason);
        return;
    }

    const bool savedToDisk = SaveLabDummySpotsForCurrentMap(failureReason, sizeof(failureReason));
    LogGlockLabDummyMark(
        "target_unmark",
        FindCurrentLiveLabDummyAnchorPlayer(),
        removedSpot.origin,
        removedSpot.angles,
        removedSpot.candidate,
        removedSpot.name,
        GetLabDummySpotStorageLabel(&removedSpot));
    PrintLabDummyConsoleLine(
        "cleared target spot \"%s\" for map %s. active=%s disk=%s",
        removedSpot.name,
        GetCurrentMapName(),
        g_glockLabDummyActiveSpotName[0] != '\0' ? g_glockLabDummyActiveSpotName : "none",
        savedToDisk ? g_glockLabDummyTargetSpotsPath : failureReason);
}

void ExpTargetUseSavedCommand()
{
    RefreshFutureHooksMapState();

    char spotName[kMaxLabDummySpotNameLength];
    char failureReason[kMaxLabDummySpotFileFailureLength];
    if (!TryGetLabDummySpotCommandName(1, spotName, sizeof(spotName), true, failureReason, sizeof(failureReason)))
    {
        PrintLabDummyConsoleLine("target use_saved failed: %s", failureReason);
        return;
    }

    const LabDummySavedSpotRecord *spot = FindLabDummySavedSpotConst(spotName);
    if (spot == NULL)
    {
        PrintLabDummyConsoleLine("target use_saved failed: target spot \"%s\" was not found for map %s", spotName, GetCurrentMapName());
        PrintLabDummyStatus();
        return;
    }

    strncpy_s(g_glockLabDummyActiveSpotName, sizeof(g_glockLabDummyActiveSpotName), spot->name, _TRUNCATE);
    const bool savedToDisk = SaveLabDummySpotsForCurrentMap(failureReason, sizeof(failureReason));
    LogGlockLabDummyMark("target_use_saved", FindCurrentLiveLabDummyAnchorPlayer(), spot->origin, spot->angles, "selected_active_spot", spot->name, GetLabDummySpotStorageLabel(spot));
    PrintLabDummyConsoleLine(
        "active target spot is now \"%s\" for map %s. Use exp_target_respawn to rebuild the dummy there. disk=%s",
        spot->name,
        GetCurrentMapName(),
        savedToDisk ? g_glockLabDummyTargetSpotsPath : failureReason);
}

void ExpTargetTpFrontCommand()
{
    RefreshFutureHooksMapState();
    SetLabDummyEnabled(true);

    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();
    LabDummySpawnSelection selection = {};
    LabDummyFailureInfo failure = {};
    if (!TryBuildLabDummySpawnTransformFromAnchor(pAnchorPlayer, &selection, &failure))
    {
        LogLabDummySpawnFailure(failure, pAnchorPlayer);
        PrintLabDummyConsoleLine("target move failed: %s", failure.reason);
        return;
    }

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    if (pDummy != NULL && pDummy->IsAlive())
    {
        MoveGlockLabDummyToSelection(pDummy, pAnchorPlayer, selection, "command_tp_front");
        PrintLabDummyConsoleLine(
            "moved \"%s\" in front of %s using %s/%s.",
            kGlockLabDummyDisplayName,
            GetSafePlayerName(pAnchorPlayer),
            selection.source,
            selection.candidate[0] != '\0' ? selection.candidate : "default");
        return;
    }

    if (pDummy != NULL)
    {
        RemoveLabDummyEntities("command_tp_front");
        g_glockLabDummy = NULL;
    }

    g_glockLabDummyRespawnPending = false;
    g_glockLabDummyRespawnTime = 0.0f;
    g_glockLabDummyRetryTime = 0.0f;

    if (SpawnGlockLabDummy(selection, pDummy != NULL) == NULL)
    {
        PrintLabDummyStatus();
        return;
    }

    PrintLabDummyConsoleLine(
        "spawned \"%s\" in front of %s using %s/%s.",
        kGlockLabDummyDisplayName,
        GetSafePlayerName(pAnchorPlayer),
        selection.source,
        selection.candidate[0] != '\0' ? selection.candidate : "default");
}

void ExpTargetProfileCommand()
{
    RefreshFutureHooksMapState();

    if (CMD_ARGC() < 2)
    {
        PrintLabDummyConsoleLine("usage: exp_target_profile <name>");
        PrintLabDummyConsoleLine("built-in target profiles: %s", GetLabDummyProfileNames());
        return;
    }

    const char *requestedProfile = CMD_ARGV(1);
    const LabDummyProfileDefinition *pProfile = FindBuiltInLabDummyProfile(requestedProfile);
    if (pProfile == NULL)
    {
        PrintLabDummyConsoleLine("unknown target profile \"%s\". Built-in target profiles: %s", requestedProfile, GetLabDummyProfileNames());
        return;
    }

    ApplyBuiltInLabDummyProfile(pProfile->name);
    PrintLabDummyConsoleLine("applied target profile \"%s\" (%s).", pProfile->name, pProfile->description);

    CBaseEntity *pDummy = GetTrackedLabDummyEntity();
    if (pDummy == NULL && !ExpGlockLabDummyEnabled())
    {
        PrintLabDummyConsoleLine("target profile \"%s\" is stored. Use exp_target_spawn to create the target.", pProfile->name);
        return;
    }

    CBasePlayer *pAnchorPlayer = FindCurrentLiveLabDummyAnchorPlayer();
    LabDummySpawnSelection selection = {};
    LabDummyFailureInfo failure = {};
    if (!TrySelectPreferredLabDummySpawnTransform(pAnchorPlayer, &selection, &failure))
    {
        LogLabDummySpawnFailure(failure, pAnchorPlayer);
        PrintLabDummyConsoleLine("target profile \"%s\" was stored, but the target could not be refreshed: %s", pProfile->name, failure.reason);
        return;
    }

    if (pDummy != NULL)
    {
        RemoveLabDummyEntities("command_profile_change");
        g_glockLabDummy = NULL;
    }

    g_glockLabDummyRespawnPending = false;
    g_glockLabDummyRespawnTime = 0.0f;
    g_glockLabDummyRetryTime = 0.0f;

    if (SpawnGlockLabDummy(selection, pDummy != NULL) == NULL)
    {
        PrintLabDummyConsoleLine("target profile \"%s\" was stored, but the target could not be refreshed immediately.", pProfile->name);
        return;
    }

    PrintLabDummyConsoleLine(
        "refreshed \"%s\" with profile %s via %s/%s.",
        kGlockLabDummyDisplayName,
        pProfile->name,
        selection.source,
        selection.candidate[0] != '\0' ? selection.candidate : "default");
}

void RegisterFutureGameplayCommands()
{
    if (g_futureGameplayCommandsRegistered)
    {
        return;
    }

    g_futureGameplayCommandsRegistered = true;
    if (g_engfuncs.pfnAddServerCommand == NULL)
    {
        ALERT(at_console, "[hl-server] live lab commands could not be registered because pfnAddServerCommand is unavailable\n");
        return;
    }

    g_engfuncs.pfnAddServerCommand((char *)"exp_cfg_apply", ExpCfgApplyCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_cfg_reload", ExpCfgReloadCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_cfg_status", ExpCfgStatusCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_lab_apply", ExpLabApplyCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_spawn", ExpTargetSpawnCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_clear", ExpTargetClearCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_mark", ExpTargetMarkCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_unmark", ExpTargetUnmarkCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_list", ExpTargetListCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_use_saved", ExpTargetUseSavedCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_respawn", ExpTargetRespawnCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_status", ExpTargetStatusCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_tp_front", ExpTargetTpFrontCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_target_profile", ExpTargetProfileCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_round_start", ExpRoundStartCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_round_restart", ExpRoundRestartCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_round_status", ExpRoundStatusCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_round_stop", ExpRoundStopCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_round_slay", ExpRoundSlayCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_team_join", ExpTeamJoinCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_team_autoassign", ExpTeamAutoassignCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_team_status", ExpTeamStatusCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_team_fake_add", ExpTeamFakeAddCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_team_fake_clear", ExpTeamFakeClearCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_team_spawn_mark", ExpTeamSpawnMarkCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_team_spawn_unmark", ExpTeamSpawnUnmarkCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_team_spawn_list", ExpTeamSpawnListCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_team_spawn_use", ExpTeamSpawnUseCommand);
    g_engfuncs.pfnAddServerCommand((char *)"exp_team_spawn_status", ExpTeamSpawnStatusCommand);
}

void MaintainMp5LabLoadout()
{
    if (!ExpMP5LabLoadoutEnabled() || ExpRoundModeActive())
    {
        return;
    }

    CBasePlayer *pPlayer = FindFirstLivePlayer();
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        return;
    }

    const bool hadMp5 = pPlayer->HasPlayerItemFromID(WEAPON_MP5) != FALSE;
    if (!hadMp5)
    {
        pPlayer->GiveNamedItem("weapon_9mmAR");
    }

    const int ammoIndex = CBasePlayer::GetAmmoIndex("9mm");
    const int targetAmmo = (int)ClampFloat(ExpMP5LabAmmo(), 0.0f, (float)_9MM_MAX_CARRY);
    if (ammoIndex >= 0)
    {
        const int currentAmmo = pPlayer->AmmoInventory(ammoIndex);
        const int ammoToGive = targetAmmo - currentAmmo;
        if (ammoToGive > 0)
        {
            pPlayer->GiveAmmo(ammoToGive, "9mm", _9MM_MAX_CARRY);
        }
    }

    if (!hadMp5 && ExpMP5LabAutoswitch())
    {
        pPlayer->SelectItem("weapon_9mmAR");
    }
}

void Maintain357LabLoadout()
{
    if (!Exp357LabLoadoutEnabled() || ExpRoundModeActive())
    {
        return;
    }

    CBasePlayer *pPlayer = FindFirstLivePlayer();
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        return;
    }

    const bool had357 = pPlayer->HasPlayerItemFromID(WEAPON_PYTHON) != FALSE;
    if (!had357)
    {
        pPlayer->GiveNamedItem("weapon_357");
    }

    const int ammoIndex = CBasePlayer::GetAmmoIndex("357");
    const int targetAmmo = (int)ClampFloat(Exp357LabAmmo(), 0.0f, (float)_357_MAX_CARRY);
    if (ammoIndex >= 0)
    {
        const int currentAmmo = pPlayer->AmmoInventory(ammoIndex);
        const int ammoToGive = targetAmmo - currentAmmo;
        if (ammoToGive > 0)
        {
            pPlayer->GiveAmmo(ammoToGive, "357", _357_MAX_CARRY);
        }
    }

    if (!had357 && Exp357LabAutoswitch())
    {
        pPlayer->SelectItem("weapon_357");
    }
}

void MaintainShotgunLabLoadout()
{
    if (!ExpShotgunLabLoadoutEnabled() || ExpRoundModeActive())
    {
        return;
    }

    CBasePlayer *pPlayer = FindFirstLivePlayer();
    if (pPlayer == NULL || pPlayer->pev == NULL)
    {
        return;
    }

    const bool hadShotgun = pPlayer->HasPlayerItemFromID(WEAPON_SHOTGUN) != FALSE;
    if (!hadShotgun)
    {
        pPlayer->GiveNamedItem("weapon_shotgun");
    }

    const int ammoIndex = CBasePlayer::GetAmmoIndex("buckshot");
    const int targetAmmo = (int)ClampFloat(ExpShotgunLabAmmo(), 0.0f, (float)BUCKSHOT_MAX_CARRY);
    if (ammoIndex >= 0)
    {
        const int currentAmmo = pPlayer->AmmoInventory(ammoIndex);
        const int ammoToGive = targetAmmo - currentAmmo;
        if (ammoToGive > 0)
        {
            pPlayer->GiveAmmo(ammoToGive, "buckshot", BUCKSHOT_MAX_CARRY);
        }
    }

    if (!hadShotgun && ExpShotgunLabAutoswitch())
    {
        pPlayer->SelectItem("weapon_shotgun");
    }
}
}

void FutureGameplayApplyPlayerSpawnOverride(CBasePlayer *pPlayer)
{
    ApplyTeamRoundPlayerSpawnOverrideInternal(pPlayer);
}

void RegisterFutureGameplayCvars()
{
    if (g_futureGameplayCvarsRegistered)
    {
        return;
    }

    g_futureGameplayCvarsRegistered = true;

    CVAR_REGISTER(&sv_exp_pistol_tapfire);
    CVAR_REGISTER(&sv_exp_move_spread_scale);
    CVAR_REGISTER(&sv_exp_first_shot_accuracy);
    CVAR_REGISTER(&sv_exp_spread_recovery);
    CVAR_REGISTER(&sv_exp_weapon_under_test);
    CVAR_REGISTER(&sv_exp_glock_profile_name);
    CVAR_REGISTER(&sv_exp_mp5_profile_name);
    CVAR_REGISTER(&sv_exp_357_profile_name);
    CVAR_REGISTER(&sv_exp_shotgun_profile_name);
    CVAR_REGISTER(&sv_exp_session_tag);
    CVAR_REGISTER(&sv_exp_matrix_name);
    CVAR_REGISTER(&sv_exp_matrix_step);
    CVAR_REGISTER(&sv_exp_glock_primary_base_spread);
    CVAR_REGISTER(&sv_exp_glock_primary_ground_move_penalty);
    CVAR_REGISTER(&sv_exp_glock_primary_air_move_penalty);
    CVAR_REGISTER(&sv_exp_glock_primary_duck_penalty_scale);
    CVAR_REGISTER(&sv_exp_glock_primary_first_shot_speed_threshold);
    CVAR_REGISTER(&sv_exp_glock_primary_max_spread);
    CVAR_REGISTER(&sv_exp_glock_primary_damage);
    CVAR_REGISTER(&sv_exp_glock_primary_headshot_scale);
    CVAR_REGISTER(&sv_exp_glock_primary_headshot_lethal);
    CVAR_REGISTER(&sv_exp_mp5_primary_enabled);
    CVAR_REGISTER(&sv_exp_mp5_primary_base_spread);
    CVAR_REGISTER(&sv_exp_mp5_primary_ground_move_penalty);
    CVAR_REGISTER(&sv_exp_mp5_primary_air_move_penalty);
    CVAR_REGISTER(&sv_exp_mp5_primary_duck_penalty_scale);
    CVAR_REGISTER(&sv_exp_mp5_primary_burst_growth);
    CVAR_REGISTER(&sv_exp_mp5_primary_burst_max_additional_spread);
    CVAR_REGISTER(&sv_exp_mp5_primary_spread_recovery);
    CVAR_REGISTER(&sv_exp_mp5_primary_damage);
    CVAR_REGISTER(&sv_exp_mp5_primary_headshot_scale);
    CVAR_REGISTER(&sv_exp_mp5_primary_headshot_lethal);
    CVAR_REGISTER(&sv_exp_mp5_primary_first_shot_accuracy);
    CVAR_REGISTER(&sv_exp_mp5_primary_first_shot_speed_threshold);
    CVAR_REGISTER(&sv_exp_mp5_primary_max_spread);
    CVAR_REGISTER(&sv_exp_mp5_lab_loadout);
    CVAR_REGISTER(&sv_exp_mp5_lab_ammo);
    CVAR_REGISTER(&sv_exp_mp5_lab_autoswitch);
    CVAR_REGISTER(&sv_exp_357_primary_enabled);
    CVAR_REGISTER(&sv_exp_357_primary_base_spread);
    CVAR_REGISTER(&sv_exp_357_primary_ground_move_penalty);
    CVAR_REGISTER(&sv_exp_357_primary_air_move_penalty);
    CVAR_REGISTER(&sv_exp_357_primary_duck_penalty_scale);
    CVAR_REGISTER(&sv_exp_357_primary_first_shot_accuracy);
    CVAR_REGISTER(&sv_exp_357_primary_first_shot_speed_threshold);
    CVAR_REGISTER(&sv_exp_357_primary_spread_recovery);
    CVAR_REGISTER(&sv_exp_357_primary_max_spread);
    CVAR_REGISTER(&sv_exp_357_primary_damage);
    CVAR_REGISTER(&sv_exp_357_primary_headshot_scale);
    CVAR_REGISTER(&sv_exp_357_primary_headshot_lethal);
    CVAR_REGISTER(&sv_exp_357_lab_loadout);
    CVAR_REGISTER(&sv_exp_357_lab_ammo);
    CVAR_REGISTER(&sv_exp_357_lab_autoswitch);
    CVAR_REGISTER(&sv_exp_shotgun_primary_enabled);
    CVAR_REGISTER(&sv_exp_shotgun_primary_base_spread);
    CVAR_REGISTER(&sv_exp_shotgun_primary_ground_move_penalty);
    CVAR_REGISTER(&sv_exp_shotgun_primary_air_move_penalty);
    CVAR_REGISTER(&sv_exp_shotgun_primary_duck_penalty_scale);
    CVAR_REGISTER(&sv_exp_shotgun_primary_first_shot_accuracy);
    CVAR_REGISTER(&sv_exp_shotgun_primary_first_shot_speed_threshold);
    CVAR_REGISTER(&sv_exp_shotgun_primary_spread_recovery);
    CVAR_REGISTER(&sv_exp_shotgun_primary_max_spread);
    CVAR_REGISTER(&sv_exp_shotgun_primary_damage_per_pellet);
    CVAR_REGISTER(&sv_exp_shotgun_primary_pellet_count);
    CVAR_REGISTER(&sv_exp_shotgun_primary_headshot_scale);
    CVAR_REGISTER(&sv_exp_shotgun_primary_headshot_lethal);
    CVAR_REGISTER(&sv_exp_shotgun_lab_loadout);
    CVAR_REGISTER(&sv_exp_shotgun_lab_ammo);
    CVAR_REGISTER(&sv_exp_shotgun_lab_autoswitch);
    CVAR_REGISTER(&sv_exp_round_mode);
    CVAR_REGISTER(&sv_exp_round_freeze_time);
    CVAR_REGISTER(&sv_exp_round_restart_delay);
    CVAR_REGISTER(&sv_exp_round_start_health);
    CVAR_REGISTER(&sv_exp_round_start_armor);
    CVAR_REGISTER(&sv_exp_round_no_respawn);
    CVAR_REGISTER(&sv_exp_round_friendlyfire);
    CVAR_REGISTER(&sv_exp_round_weapon_profile);
    CVAR_REGISTER(&sv_exp_round_loadout_mode);
    CVAR_REGISTER(&sv_exp_team_round_mode);
    CVAR_REGISTER(&sv_exp_team_round_teamplay);
    CVAR_REGISTER(&sv_exp_team_round_spawn_mode);
    CVAR_REGISTER(&sv_exp_team_round_team1_name);
    CVAR_REGISTER(&sv_exp_team_round_team2_name);
    CVAR_REGISTER(&sv_exp_team_round_team1_loadout);
    CVAR_REGISTER(&sv_exp_team_round_team2_loadout);
    CVAR_REGISTER(&sv_exp_team_round_team1_health);
    CVAR_REGISTER(&sv_exp_team_round_team2_health);
    CVAR_REGISTER(&sv_exp_team_round_team1_armor);
    CVAR_REGISTER(&sv_exp_team_round_team2_armor);
    CVAR_REGISTER(&sv_exp_debug_weaponlog);
    CVAR_REGISTER(&sv_exp_debug_weaponlog_rejections);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy);
    CVAR_REGISTER(&sv_exp_glock_lab_target_profile_name);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_health);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_armor);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_head_protected);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_armor_health_fraction);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_armor_drain_scale);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_autorespawn);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_respawn_delay);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_spawn_distance);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_model);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_face_player);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_offset_right);
    CVAR_REGISTER(&sv_exp_glock_lab_dummy_offset_up);

    RegisterFutureGameplayCommands();

    ALERT(at_console, "[hl-server] future gameplay hooks registered\n");
}

void UpdateFutureGameplayHooksFrame()
{
    EnsureWeaponDebugLogReady();
    RefreshFutureHooksMapState();
    UpdateRoundModeFrame();
    MaintainMp5LabLoadout();
    Maintain357LabLoadout();
    MaintainShotgunLabLoadout();
    MaintainGlockLabDummy();
}

bool FutureGameplayPlayerCanRespawn(CBasePlayer *pPlayer)
{
    if (pPlayer == NULL || !ExpRoundModeActive() || !ExpRoundNoRespawn())
    {
        return true;
    }

    return g_expRoundState.state != kExpRoundStateFreezeTime &&
        g_expRoundState.state != kExpRoundStateLive &&
        g_expRoundState.state != kExpRoundStateRoundEnd &&
        g_expRoundState.state != kExpRoundStateRestartPending;
}

bool FutureGameplayPlayerCanTakeDamage(CBasePlayer *pPlayer, CBaseEntity *pAttacker)
{
    if (pPlayer == NULL || pAttacker == NULL)
    {
        return true;
    }

    if (!TeamRoundModeConfigured() || !TeamRoundTeamplayConfigured() || ExpRoundFriendlyFireEnabled())
    {
        return true;
    }

    if (!pAttacker->IsPlayer())
    {
        return true;
    }

    CBasePlayer *pAttackerPlayer = (CBasePlayer *)pAttacker;
    if (pAttackerPlayer == pPlayer)
    {
        return true;
    }

    const int victimTeam = GetAssignedRoundTeamId(pPlayer);
    const int attackerTeam = GetAssignedRoundTeamId(pAttackerPlayer);
    if (victimTeam == kExpRoundTeamNone || attackerTeam == kExpRoundTeamNone)
    {
        return true;
    }

    return victimTeam != attackerTeam;
}

int FutureGameplayPlayerRelationship(CBaseEntity *pPlayer, CBaseEntity *pTarget)
{
    if (!TeamRoundModeConfigured() || !TeamRoundTeamplayConfigured() ||
        pPlayer == NULL || pTarget == NULL || !pPlayer->IsPlayer() || !pTarget->IsPlayer())
    {
        return GR_NOTTEAMMATE;
    }

    const int playerTeam = GetAssignedRoundTeamId((CBasePlayer *)pPlayer);
    const int targetTeam = GetAssignedRoundTeamId((CBasePlayer *)pTarget);
    if (playerTeam != kExpRoundTeamNone && playerTeam == targetTeam)
    {
        return GR_TEAMMATE;
    }

    return GR_NOTTEAMMATE;
}

void FutureGameplayOnPlayerInitHUD(CBasePlayer *pPlayer)
{
    if (pPlayer == NULL)
    {
        return;
    }

    if (TeamRoundModeConfigured())
    {
        if (GetAssignedRoundTeamId(pPlayer) == kExpRoundTeamNone)
        {
            AutoAssignPlayerToRoundTeam(pPlayer);
        }
        else
        {
            ApplyRoundTeamLabel(pPlayer, GetAssignedRoundTeamId(pPlayer));
        }
    }
    else
    {
        ApplyRoundTeamLabel(pPlayer, kExpRoundTeamNone);
    }

    UpdateRoundPopulationSnapshot();
}

void FutureGameplayOnPlayerSpawn(CBasePlayer *pPlayer)
{
    if (pPlayer == NULL)
    {
        return;
    }

    if (IsRoundFakeClient(pPlayer) && pPlayer->pev != NULL)
    {
        pPlayer->pev->flags |= (FL_CLIENT | FL_FAKECLIENT);
    }

    if (!ExpRoundModeActive() || !ShouldApplyRoundResetOnSpawn())
    {
        return;
    }

    ApplyRoundResetToPlayer(pPlayer);
}

void FutureGameplayOnClientDisconnected(CBasePlayer *pPlayer)
{
    if (pPlayer == NULL)
    {
        return;
    }

    ClearRoundFakeClientRecord(pPlayer);
    ClearRoundTeamAssignment(pPlayer);
    UpdateRoundPopulationSnapshot();

    if (ExpRoundModeActive() && g_expRoundState.state == kExpRoundStateLive)
    {
        EvaluateRoundOutcome();
    }
}

void FutureGameplayOnPlayerKilled(CBasePlayer *pVictim, CBasePlayer *pKiller)
{
    if (pVictim == NULL)
    {
        return;
    }

    if (!ExpRoundModeActive() || g_expRoundState.state != kExpRoundStateLive)
    {
        return;
    }

    (void)pKiller;
    EvaluateRoundOutcome();
}

bool ExpPistolTapFireEnabled()
{
    return sv_exp_pistol_tapfire.value != 0.0f;
}

float ExpMoveSpreadScale()
{
    return sv_exp_move_spread_scale.value > 0.0f ? sv_exp_move_spread_scale.value : 0.0f;
}

bool ExpFirstShotAccuracyEnabled()
{
    return sv_exp_first_shot_accuracy.value != 0.0f;
}

float ExpSpreadRecoverySeconds()
{
    return sv_exp_spread_recovery.value > 0.0f ? sv_exp_spread_recovery.value : 0.0f;
}

const char *ExpWeaponUnderTest()
{
    return GetOptionalCvarString(sv_exp_weapon_under_test);
}

const char *ExpGlockProfileName()
{
    return GetNonEmptyCvarString(sv_exp_glock_profile_name, "default");
}

const char *ExpMP5ProfileName()
{
    return GetNonEmptyCvarString(sv_exp_mp5_profile_name, "default");
}

const char *Exp357ProfileName()
{
    return GetNonEmptyCvarString(sv_exp_357_profile_name, "default");
}

const char *ExpShotgunProfileName()
{
    return GetNonEmptyCvarString(sv_exp_shotgun_profile_name, "default");
}

const char *ExpSessionTag()
{
    return GetOptionalCvarString(sv_exp_session_tag);
}

const char *ExpMatrixName()
{
    return GetOptionalCvarString(sv_exp_matrix_name);
}

const char *ExpMatrixStep()
{
    return GetOptionalCvarString(sv_exp_matrix_step);
}

float ExpGlockPrimaryBaseSpread()
{
    return GetNonNegativeCvarValue(sv_exp_glock_primary_base_spread);
}

float ExpGlockPrimaryGroundMovePenalty()
{
    return GetNonNegativeCvarValue(sv_exp_glock_primary_ground_move_penalty);
}

float ExpGlockPrimaryAirMovePenalty()
{
    return GetNonNegativeCvarValue(sv_exp_glock_primary_air_move_penalty);
}

float ExpGlockPrimaryDuckPenaltyScale()
{
    return GetNonNegativeCvarValue(sv_exp_glock_primary_duck_penalty_scale);
}

float ExpGlockPrimaryFirstShotSpeedThreshold()
{
    return GetNonNegativeCvarValue(sv_exp_glock_primary_first_shot_speed_threshold);
}

float ExpGlockPrimaryMaxSpread()
{
    return GetNonNegativeCvarValue(sv_exp_glock_primary_max_spread);
}

float ExpGlockPrimaryDamage()
{
    return GetNonNegativeCvarValue(sv_exp_glock_primary_damage);
}

float ExpGlockPrimaryHeadshotScale()
{
    return GetNonNegativeCvarValue(sv_exp_glock_primary_headshot_scale);
}

bool ExpGlockPrimaryHeadshotLethal()
{
    return sv_exp_glock_primary_headshot_lethal.value != 0.0f;
}

bool ExpDebugWeaponLogEnabled()
{
    return sv_exp_debug_weaponlog.value != 0.0f;
}

bool ExpDebugWeaponLogRejectionsEnabled()
{
    return sv_exp_debug_weaponlog_rejections.value != 0.0f;
}

bool ExpGlockExperimentalModeEnabled()
{
    return ExpPistolTapFireEnabled() || ExpMoveSpreadScale() > 0.0f || ExpFirstShotAccuracyEnabled();
}

bool ExpMP5ExperimentalModeEnabled()
{
    return ExpMP5PrimaryEnabled();
}

bool ExpMP5PrimaryEnabled()
{
    return sv_exp_mp5_primary_enabled.value != 0.0f;
}

float ExpMP5PrimaryBaseSpread()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_base_spread);
}

float ExpMP5PrimaryGroundMovePenalty()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_ground_move_penalty);
}

float ExpMP5PrimaryAirMovePenalty()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_air_move_penalty);
}

float ExpMP5PrimaryDuckPenaltyScale()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_duck_penalty_scale);
}

float ExpMP5PrimaryBurstGrowth()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_burst_growth);
}

float ExpMP5PrimaryBurstMaxAdditionalSpread()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_burst_max_additional_spread);
}

float ExpMP5PrimarySpreadRecoverySeconds()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_spread_recovery);
}

float ExpMP5PrimaryDamage()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_damage);
}

float ExpMP5PrimaryHeadshotScale()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_headshot_scale);
}

bool ExpMP5PrimaryHeadshotLethal()
{
    return sv_exp_mp5_primary_headshot_lethal.value != 0.0f;
}

bool ExpMP5PrimaryFirstShotAccuracyEnabled()
{
    return sv_exp_mp5_primary_first_shot_accuracy.value != 0.0f;
}

float ExpMP5PrimaryFirstShotSpeedThreshold()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_first_shot_speed_threshold);
}

float ExpMP5PrimaryMaxSpread()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_primary_max_spread);
}

bool ExpMP5LabLoadoutEnabled()
{
    return sv_exp_mp5_lab_loadout.value != 0.0f;
}

float ExpMP5LabAmmo()
{
    return GetNonNegativeCvarValue(sv_exp_mp5_lab_ammo);
}

bool ExpMP5LabAutoswitch()
{
    return sv_exp_mp5_lab_autoswitch.value != 0.0f;
}

bool Exp357ExperimentalModeEnabled()
{
    return Exp357PrimaryEnabled();
}

bool ExpShotgunExperimentalModeEnabled()
{
    return ExpShotgunPrimaryEnabled();
}

bool Exp357PrimaryEnabled()
{
    return sv_exp_357_primary_enabled.value != 0.0f;
}

bool ExpShotgunPrimaryEnabled()
{
    return sv_exp_shotgun_primary_enabled.value != 0.0f;
}

float Exp357PrimaryBaseSpread()
{
    return GetNonNegativeCvarValue(sv_exp_357_primary_base_spread);
}

float ExpShotgunPrimaryBaseSpread()
{
    return GetNonNegativeCvarValue(sv_exp_shotgun_primary_base_spread);
}

float Exp357PrimaryGroundMovePenalty()
{
    return GetNonNegativeCvarValue(sv_exp_357_primary_ground_move_penalty);
}

float ExpShotgunPrimaryGroundMovePenalty()
{
    return GetNonNegativeCvarValue(sv_exp_shotgun_primary_ground_move_penalty);
}

float Exp357PrimaryAirMovePenalty()
{
    return GetNonNegativeCvarValue(sv_exp_357_primary_air_move_penalty);
}

float ExpShotgunPrimaryAirMovePenalty()
{
    return GetNonNegativeCvarValue(sv_exp_shotgun_primary_air_move_penalty);
}

float Exp357PrimaryDuckPenaltyScale()
{
    return GetNonNegativeCvarValue(sv_exp_357_primary_duck_penalty_scale);
}

float ExpShotgunPrimaryDuckPenaltyScale()
{
    return GetNonNegativeCvarValue(sv_exp_shotgun_primary_duck_penalty_scale);
}

float Exp357PrimarySpreadRecoverySeconds()
{
    return GetNonNegativeCvarValue(sv_exp_357_primary_spread_recovery);
}

float ExpShotgunPrimarySpreadRecoverySeconds()
{
    return GetNonNegativeCvarValue(sv_exp_shotgun_primary_spread_recovery);
}

float Exp357PrimaryDamage()
{
    return GetNonNegativeCvarValue(sv_exp_357_primary_damage);
}

float ExpShotgunPrimaryDamagePerPellet()
{
    return GetNonNegativeCvarValue(sv_exp_shotgun_primary_damage_per_pellet);
}

int ExpShotgunPrimaryPelletCount()
{
    const float pelletCount = GetNonNegativeCvarValue(sv_exp_shotgun_primary_pellet_count);
    return pelletCount >= 1.0f ? (int)(pelletCount + 0.5f) : 1;
}

float Exp357PrimaryHeadshotScale()
{
    return GetNonNegativeCvarValue(sv_exp_357_primary_headshot_scale);
}

float ExpShotgunPrimaryHeadshotScale()
{
    return GetNonNegativeCvarValue(sv_exp_shotgun_primary_headshot_scale);
}

bool Exp357PrimaryHeadshotLethal()
{
    return sv_exp_357_primary_headshot_lethal.value != 0.0f;
}

bool ExpShotgunPrimaryHeadshotLethal()
{
    return sv_exp_shotgun_primary_headshot_lethal.value != 0.0f;
}

bool Exp357PrimaryFirstShotAccuracyEnabled()
{
    return sv_exp_357_primary_first_shot_accuracy.value != 0.0f;
}

bool ExpShotgunPrimaryFirstShotAccuracyEnabled()
{
    return sv_exp_shotgun_primary_first_shot_accuracy.value != 0.0f;
}

float Exp357PrimaryFirstShotSpeedThreshold()
{
    return GetNonNegativeCvarValue(sv_exp_357_primary_first_shot_speed_threshold);
}

float ExpShotgunPrimaryFirstShotSpeedThreshold()
{
    return GetNonNegativeCvarValue(sv_exp_shotgun_primary_first_shot_speed_threshold);
}

float Exp357PrimaryMaxSpread()
{
    return GetNonNegativeCvarValue(sv_exp_357_primary_max_spread);
}

float ExpShotgunPrimaryMaxSpread()
{
    return GetNonNegativeCvarValue(sv_exp_shotgun_primary_max_spread);
}

bool Exp357LabLoadoutEnabled()
{
    return sv_exp_357_lab_loadout.value != 0.0f;
}

float Exp357LabAmmo()
{
    return GetNonNegativeCvarValue(sv_exp_357_lab_ammo);
}

bool Exp357LabAutoswitch()
{
    return sv_exp_357_lab_autoswitch.value != 0.0f;
}

bool ExpShotgunLabLoadoutEnabled()
{
    return sv_exp_shotgun_lab_loadout.value != 0.0f;
}

float ExpShotgunLabAmmo()
{
    return GetNonNegativeCvarValue(sv_exp_shotgun_lab_ammo);
}

bool ExpShotgunLabAutoswitch()
{
    return sv_exp_shotgun_lab_autoswitch.value != 0.0f;
}

bool ExpRoundModeEnabled()
{
    return sv_exp_round_mode.value != 0.0f;
}

float ExpRoundFreezeTimeSeconds()
{
    return GetPositiveOrDefaultCvarValue(sv_exp_round_freeze_time, kDefaultRoundFreezeTime);
}

float ExpRoundRestartDelaySeconds()
{
    return GetPositiveOrDefaultCvarValue(sv_exp_round_restart_delay, kDefaultRoundRestartDelay);
}

float ExpRoundStartHealth()
{
    return GetPositiveOrDefaultCvarValue(sv_exp_round_start_health, kDefaultRoundStartHealth);
}

float ExpRoundStartArmor()
{
    return GetNonNegativeCvarValue(sv_exp_round_start_armor);
}

bool ExpRoundNoRespawn()
{
    return sv_exp_round_no_respawn.value != 0.0f;
}

bool ExpRoundFriendlyFireEnabled()
{
    return sv_exp_round_friendlyfire.value != 0.0f;
}

const char *ExpRoundWeaponProfile()
{
    return GetOptionalCvarString(sv_exp_round_weapon_profile);
}

const char *ExpRoundLoadoutMode()
{
    return GetConfiguredRoundLoadoutMode();
}

bool ExpTeamRoundModeEnabled()
{
    return TeamRoundModeConfigured();
}

bool ExpTeamRoundTeamplayEnabled()
{
    return TeamRoundTeamplayConfigured();
}

const char *ExpTeamRoundSpawnMode()
{
    return GetConfiguredTeamRoundSpawnMode();
}

const char *ExpTeamRoundTeam1Name()
{
    return GetConfiguredTeamRoundName(kExpRoundTeam1);
}

const char *ExpTeamRoundTeam2Name()
{
    return GetConfiguredTeamRoundName(kExpRoundTeam2);
}

const char *ExpTeamRoundTeam1Loadout()
{
    return GetResolvedRoundLoadoutModeForTeam(kExpRoundTeam1);
}

const char *ExpTeamRoundTeam2Loadout()
{
    return GetResolvedRoundLoadoutModeForTeam(kExpRoundTeam2);
}

float ExpTeamRoundTeam1Health()
{
    return GetResolvedRoundStartHealthForTeam(kExpRoundTeam1);
}

float ExpTeamRoundTeam2Health()
{
    return GetResolvedRoundStartHealthForTeam(kExpRoundTeam2);
}

float ExpTeamRoundTeam1Armor()
{
    return GetResolvedRoundStartArmorForTeam(kExpRoundTeam1);
}

float ExpTeamRoundTeam2Armor()
{
    return GetResolvedRoundStartArmorForTeam(kExpRoundTeam2);
}

int ExpRoundConnectedPlayersForTeam(int teamId)
{
    if (teamId == kExpRoundTeam1)
    {
        return g_expRoundState.team1ConnectedPlayers;
    }

    if (teamId == kExpRoundTeam2)
    {
        return g_expRoundState.team2ConnectedPlayers;
    }

    return 0;
}

int ExpRoundAlivePlayersForTeam(int teamId)
{
    if (teamId == kExpRoundTeam1)
    {
        return g_expRoundState.team1AlivePlayers;
    }

    if (teamId == kExpRoundTeam2)
    {
        return g_expRoundState.team2AlivePlayers;
    }

    return 0;
}

int ExpRoundUnassignedConnectedPlayers()
{
    return g_expRoundState.unassignedConnectedPlayers;
}

int ExpRoundUnassignedAlivePlayers()
{
    return g_expRoundState.unassignedAlivePlayers;
}

const char *ExpRoundLastWinnerTeamName()
{
    return g_expRoundState.lastWinnerTeamName;
}

bool ExpRoundModeActive()
{
    return ExpRoundModeEnabled() || g_expRoundState.state != kExpRoundStateDisabled;
}

bool ExpRoundLive()
{
    return g_expRoundState.state == kExpRoundStateLive;
}

bool ExpCfgDrivenModeActive()
{
    return g_liveCfgState.cfgDrivenModeActive;
}

const char *ExpActiveCfgProfile()
{
    return g_liveCfgState.hasActiveCfg ? g_liveCfgState.activeExecPath : "";
}

const char *ExpLastSuccessfulCfgProfile()
{
    return g_liveCfgState.hasLastSuccessfulCfg ? g_liveCfgState.lastSuccessfulExecPath : "";
}

const char *ExpLastCfgAction()
{
    return g_liveCfgState.lastAction;
}

const char *ExpLastCfgAppliedAt()
{
    return g_liveCfgState.lastAppliedAt;
}

bool ExpGlockLabDummyEnabled()
{
    return sv_exp_glock_lab_dummy.value != 0.0f;
}

const char *ExpGlockLabDummyDisplayName()
{
    return kGlockLabDummyDisplayName;
}

const char *ExpGlockLabTargetProfileName()
{
    return GetNonEmptyCvarString(sv_exp_glock_lab_target_profile_name, kDefaultGlockLabTargetProfileName);
}

float ExpGlockLabDummyHealth()
{
    return GetPositiveOrDefaultCvarValue(sv_exp_glock_lab_dummy_health, kDefaultGlockLabDummyHealth);
}

float ExpGlockLabDummyArmor()
{
    return GetNonNegativeCvarValue(sv_exp_glock_lab_dummy_armor);
}

bool ExpGlockLabDummyHeadProtected()
{
    return sv_exp_glock_lab_dummy_head_protected.value != 0.0f;
}

float ExpGlockLabDummyArmorHealthFraction()
{
    return ClampFloat(sv_exp_glock_lab_dummy_armor_health_fraction.value, 0.0f, 1.0f);
}

float ExpGlockLabDummyArmorDrainScale()
{
    return GetNonNegativeCvarValue(sv_exp_glock_lab_dummy_armor_drain_scale);
}

bool ExpGlockLabDummyAutoRespawnEnabled()
{
    return sv_exp_glock_lab_dummy_autorespawn.value != 0.0f;
}

float ExpGlockLabDummyRespawnDelaySeconds()
{
    return GetNonNegativeCvarValue(sv_exp_glock_lab_dummy_respawn_delay);
}

float ExpGlockLabDummySpawnDistance()
{
    return GetPositiveOrDefaultCvarValue(sv_exp_glock_lab_dummy_spawn_distance, kDefaultGlockLabDummySpawnDistance);
}

const char *ExpGlockLabDummyModel()
{
    return ResolveGlockLabDummyModel();
}

bool ExpGlockLabDummyFacePlayer()
{
    return sv_exp_glock_lab_dummy_face_player.value != 0.0f;
}

float ExpGlockLabDummyOffsetRight()
{
    return sv_exp_glock_lab_dummy_offset_right.value;
}

float ExpGlockLabDummyOffsetUp()
{
    return sv_exp_glock_lab_dummy_offset_up.value;
}

bool IsExpGlockLabDummyEntity(CBaseEntity *pEntity)
{
    return IsLabDummyEntityInternal(pEntity);
}
