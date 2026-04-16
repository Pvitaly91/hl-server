[CmdletBinding(DefaultParameterSetName = "Latest")]
param(
    [Parameter(ParameterSetName = "Path", Mandatory = $true)][string]$Path,
    [Parameter(ParameterSetName = "Latest")][switch]$Latest,
    [switch]$ExportJson,
    [switch]$ExportCsv,
    [string]$OutputDir,
    [switch]$RequireAccepted,
    [switch]$RequireRejections,
    [switch]$RequireFirstShot,
    [switch]$RequireMovePenalty,
    [switch]$RequireCrouchMoveEvidence,
    [switch]$RequireHits,
    [switch]$RequireKills,
    [switch]$RequireHeadshotKills,
    [switch]$RequireLethalHeadshotEvidence,
    [switch]$RequireDummySpawns,
    [switch]$RequireDummyHits,
    [switch]$RequireDummyHeadshotHits,
    [switch]$RequireDummyHeadshotKills,
    [switch]$RequireArmoredDummyHits,
    [switch]$RequireProtectedDummyHeadshotHits,
    [switch]$RequireProtectedDummyHeadshotKills,
    [switch]$RequireDummyLethalHeadshotEvidence
)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

function Resolve-AnalysisTargetLog {
    if ($PSCmdlet.ParameterSetName -eq "Path") {
        $resolved = Get-FullPath -Path $Path
        if (-not (Test-LeafPath -Path $resolved)) { throw "Weapon debug log was not found: $resolved" }
        return Get-Item -LiteralPath $resolved
    }
    $latestLog = Get-LatestWeaponDebugLog
    if (-not $latestLog) { throw "No weapon debug log was found under $(Get-WeaponDebugLogsRoot). Generate telemetry first or pass -Path." }
    return $latestLog
}

function Field { param([System.Collections.IDictionary]$V,[string]$N) if ($V -and $V.Contains($N)) { return [string]$V[$N] } return $null }
function Fields { param([System.Collections.IDictionary]$V,[string[]]$Names) foreach ($n in $Names) { $x = Field $V $n; if (-not [string]::IsNullOrWhiteSpace($x)) { return $x } } return $null }
function ToInt([string]$V) { if ([string]::IsNullOrWhiteSpace($V) -or $V -eq "na") { return $null }; return [int]::Parse($V,[System.Globalization.CultureInfo]::InvariantCulture) }
function ToNum([string]$V) { if ([string]::IsNullOrWhiteSpace($V) -or $V -eq "na") { return $null }; return [double]::Parse($V,[System.Globalization.CultureInfo]::InvariantCulture) }
function ToBool([string]$V) { if ([string]::IsNullOrWhiteSpace($V) -or $V -eq "na") { return $null }; switch ($V) { "1" { $true; break } "0" { $false; break } default { [bool]::Parse($V) } } }
function FNum($V,[int]$D) { if ($null -eq $V) { return "n/a" }; return ([double]$V).ToString(("F{0}" -f $D),[System.Globalization.CultureInfo]::InvariantCulture) }
function FBool($V) { if ($null -eq $V -or [string]::IsNullOrWhiteSpace([string]$V)) { return "n/a" }; if ([bool]$V) { return "1" }; return "0" }
function Stats([object[]]$Vals) { $f=@($Vals|Where-Object{$null-ne$_}); if($f.Count -eq 0){return $null}; $m=$f|Measure-Object -Minimum -Maximum -Average; [PSCustomObject]@{Count=[int]$m.Count;Min=[double]$m.Minimum;Average=[double]$m.Average;Max=[double]$m.Maximum} }

function HitgroupCounts([object[]]$Events) {
    $counts=[ordered]@{}; if($null -eq $Events -or $Events.Count -eq 0){return $counts}
    foreach($name in @("head","chest","stomach","leftarm","rightarm","leftleg","rightleg","generic")){
        $count=@($Events|Where-Object{$_.HitGroup -eq $name}).Count
        if($count -gt 0){$counts[$name]=$count}
    }
    foreach($g in @($Events|Where-Object{-not [string]::IsNullOrWhiteSpace($_.HitGroup) -and -not $counts.Contains($_.HitGroup)}|Group-Object HitGroup|Sort-Object Name)){
        $counts[$g.Name]=[int]$g.Count
    }
    return $counts
}

function FHitgroups([System.Collections.IDictionary]$Counts) {
    if($null -eq $Counts -or $Counts.Count -eq 0){ return "n/a" }
    return (($Counts.Keys | ForEach-Object { "{0}={1}" -f $_,$Counts[$_] }) -join ", ")
}

function Parse-WeaponLogLine {
    param([string]$Line,[int]$LineNumber)
    $text=$Line.Trim(); if([string]::IsNullOrWhiteSpace($text)){ return $null }
    $prefix=$null
    if($text.StartsWith("[")){ $i=$text.IndexOf("]"); if($i -gt 0){ $prefix=$text.Substring(1,$i-1); $text=$text.Substring($i+1).TrimStart() } }
    $matches=[System.Text.RegularExpressions.Regex]::Matches($text,'(?<key>[A-Za-z0-9_]+)=(?:"(?<quoted>[^"]*)"|(?<value>\S+))')
    if($matches.Count -eq 0){ return $null }
    $values=[ordered]@{}
    foreach($m in $matches){ $values[$m.Groups["key"].Value] = if($m.Groups["quoted"].Success){$m.Groups["quoted"].Value}else{$m.Groups["value"].Value} }
    $type=Field $values "type"
    if([string]::IsNullOrWhiteSpace($type)){
        if((Field $values "event") -eq "weapon_debug_session"){ $type="session" }
        elseif(-not [string]::IsNullOrWhiteSpace((Field $values "reason"))){ $type="rejected" }
        elseif((Field $values "weapon") -eq "glock" -and (Field $values "fire") -eq "primary"){ $type="accepted" }
        else{ $type="unknown" }
    } else { $type=$type.ToLowerInvariant() }
    $speed=ToNum (Field $values "speed2d")
    $maxSpeed=ToNum (Field $values "maxspeed")
    $speedRatio=$null
    if($null -ne $speed -and $null -ne $maxSpeed -and $maxSpeed -gt 0){ $speedRatio=[Math]::Min([Math]::Max(($speed/$maxSpeed),0.0),1.0) }
    $movePenalty=ToNum (Field $values "move_penalty")
    $movePenaltyRate=$null
    if($null -ne $movePenalty -and $null -ne $speedRatio -and $speedRatio -gt 0){ $movePenaltyRate=$movePenalty/$speedRatio }
    return [PSCustomObject]@{
        LineNumber=$LineNumber; Prefix=$prefix; Type=$type; Timestamp=Field $values "ts"; Map=Field $values "map"; Event=Field $values "event"; Status=Field $values "status"; Game=Field $values "game"; LogFile=Field $values "file";
        ProfileName=Field $values "profile"; TargetProfileName=Fields $values @("target_profile","targetProfile","sv_exp_glock_lab_target_profile_name");
        TapFire=ToBool (Field $values "tapfire"); MoveSpreadScale=ToNum (Field $values "move_scale"); FirstShotEnabled=ToBool (Field $values "firstshot_enabled"); SpreadRecovery=ToNum (Field $values "recovery");
        BaseSpread=ToNum (Field $values "base"); GroundMovePenalty=ToNum (Field $values "ground_move_penalty"); AirMovePenalty=ToNum (Field $values "air_move_penalty"); DuckPenaltyScale=ToNum (Field $values "duck_penalty_scale");
        FirstShotSpeedThreshold=ToNum (Field $values "firstshot_speed"); MaxSpread=ToNum (Field $values "max_spread"); PrimaryDamageSetting=ToNum (Field $values "sv_exp_glock_primary_damage"); HeadshotScaleSetting=ToNum (Field $values "sv_exp_glock_primary_headshot_scale"); HeadshotLethalSetting=ToBool (Field $values "sv_exp_glock_primary_headshot_lethal");
        LabDummyEnabled=ToBool (Field $values "sv_exp_glock_lab_dummy"); LabDummyHealth=ToNum (Field $values "sv_exp_glock_lab_dummy_health"); LabDummyArmorSetting=ToNum (Field $values "sv_exp_glock_lab_dummy_armor"); LabDummyHeadProtectedSetting=ToBool (Field $values "sv_exp_glock_lab_dummy_head_protected"); LabDummyArmorHealthFraction=ToNum (Field $values "sv_exp_glock_lab_dummy_armor_health_fraction"); LabDummyArmorDrainScale=ToNum (Field $values "sv_exp_glock_lab_dummy_armor_drain_scale");
        LabDummyAutoRespawn=ToBool (Field $values "sv_exp_glock_lab_dummy_autorespawn"); LabDummyRespawnDelay=ToNum (Field $values "sv_exp_glock_lab_dummy_respawn_delay"); LabDummySpawnDistance=ToNum (Field $values "sv_exp_glock_lab_dummy_spawn_distance"); LabDummyModel=Field $values "sv_exp_glock_lab_dummy_model";
        Player=Field $values "player"; EntIndex=ToInt (Field $values "entindex"); UserId=ToInt (Field $values "userid"); Weapon=Field $values "weapon"; Fire=Field $values "fire"; Reason=Field $values "reason"; Experimental=ToBool (Field $values "experimental"); FirstShot=ToBool (Field $values "firstshot"); Spread=ToNum (Field $values "spread"); MovementPenalty=$movePenalty; HorizontalSpeed=$speed; MaxSpeed=$maxSpeed; SpeedRatio=$speedRatio; MovementPenaltyRate=$movePenaltyRate; Grounded=ToBool (Field $values "grounded"); Ducking=ToBool (Field $values "ducking"); DeltaPrevious=ToNum (Field $values "delta_prev"); Clip=ToInt (Field $values "clip");
        Attacker=Field $values "attacker"; AttackerEntIndex=ToInt (Field $values "attacker_entindex"); AttackerUserId=ToInt (Field $values "attacker_userid"); Victim=Field $values "victim"; VictimEntIndex=ToInt (Field $values "victim_entindex"); VictimUserId=ToInt (Field $values "victim_userid"); VictimKind=Fields $values @("victim_kind","victimKind"); VictimClass=Fields $values @("victim_class","victimClass"); VictimModel=Fields $values @("victim_model","victimModel");
        HitGroup=Field $values "hitgroup"; HitGroupId=ToInt (Field $values "hitgroup_id"); Headshot=ToBool (Field $values "headshot"); BaseDamage=ToNum (Field $values "base_damage"); HitgroupScale=ToNum (Field $values "hitgroup_scale"); TraceDamage=ToNum (Field $values "trace_damage"); AppliedDamage=ToNum (Field $values "applied_damage"); HealthBefore=ToNum (Field $values "health_before"); HealthAfter=ToNum (Field $values "health_after"); ArmorBefore=ToNum (Field $values "armor_before"); ArmorAfter=ToNum (Field $values "armor_after"); ArmorDamage=ToNum (Field $values "armor_damage");
        DamageRaw=ToNum (Fields $values @("damage_raw","damageRaw")); DamageToHealth=ToNum (Fields $values @("damage_to_health","damageToHealth")); DamageAbsorbed=ToNum (Fields $values @("damage_absorbed","damageAbsorbed")); ArmorDrain=ToNum (Fields $values @("armor_drain","armorDrain")); DummyArmorBefore=ToNum (Fields $values @("dummy_armor_before","dummyArmorBefore")); DummyArmorAfter=ToNum (Fields $values @("dummy_armor_after","dummyArmorAfter")); ArmorApplied=ToBool (Fields $values @("armor_applied","armorApplied")); HeadProtected=ToBool (Fields $values @("head_protected","headProtected")); HeadshotLethalActive=ToBool (Field $values "headshot_lethal_active"); HeadshotLethalApplied=ToBool (Field $values "headshot_lethal_applied");
        Dummy=Field $values "dummy"; DummyClass=Fields $values @("dummy_class","dummyClass"); DummyModel=Fields $values @("dummy_model","dummyModel"); DummySpawnHealth=ToNum (Fields $values @("spawn_health","spawnHealth","health")); DummySpawnArmor=ToNum (Fields $values @("spawn_armor","spawnArmor")); DummyAutoRespawn=ToBool (Field $values "autorespawn"); DummyRespawnDelay=ToNum (Field $values "respawn_delay"); DummySpawnDistance=ToNum (Field $values "spawn_distance"); DummyArmorHealthFractionEvent=ToNum (Fields $values @("armor_health_fraction","armorHealthFraction")); DummyArmorDrainScaleEvent=ToNum (Fields $values @("armor_drain_scale","armorDrainScale")); Anchor=Field $values "anchor"; AnchorEntIndex=ToInt (Field $values "anchor_entindex"); AnchorUserId=ToInt (Field $values "anchor_userid"); Origin=Field $values "origin"; Yaw=ToNum (Field $values "yaw");
        RawLine=$Line
    }
}

function IsDummyVictim($Event) {
    if ($null -eq $Event) { return $false }
    return $Event.VictimKind -eq "dummy" -or $Event.VictimClass -eq "glock_lab_dummy"
}

function IsArmoredDummy($Event) {
    if (-not (IsDummyVictim $Event)) { return $false }
    return $Event.ArmorApplied -eq $true -or (($null -ne $Event.DummyArmorBefore) -and $Event.DummyArmorBefore -gt 0.0) -or (($null -ne $Event.DummySpawnArmor) -and $Event.DummySpawnArmor -gt 0.0)
}

function FirstTargetProfile($Session,[object[]]$Spawn,[object[]]$Hit,[object[]]$Kill) {
    if ($Session -and -not [string]::IsNullOrWhiteSpace($Session.TargetProfileName)) { return $Session.TargetProfileName }
    foreach ($set in @($Spawn,$Hit,$Kill)) {
        $m = @($set | Where-Object { -not [string]::IsNullOrWhiteSpace($_.TargetProfileName) } | Select-Object -First 1)
        if ($m.Count -gt 0) { return $m[0].TargetProfileName }
    }
    return $null
}

$log = Resolve-AnalysisTargetLog
$events = @()
$warnings = New-Object System.Collections.ArrayList
$observations = New-Object System.Collections.ArrayList
$assertions = New-Object System.Collections.ArrayList
$n = 0
foreach ($line in (Get-Content -LiteralPath $log.FullName)) {
    $n += 1
    $evt = Parse-WeaponLogLine -Line $line -LineNumber $n
    if ($evt -and $evt.Type -ne "unknown") { $events += $evt }
    elseif (-not [string]::IsNullOrWhiteSpace($line)) { [void]$warnings.Add("Skipped non-telemetry line $n because it did not match the expected single-line key=value format.") }
}

$session = @($events | Where-Object { $_.Type -eq "session" } | Select-Object -First 1)
$session = if ($session.Count -gt 0) { $session[0] } else { $null }
$accepted = @($events | Where-Object { $_.Type -eq "accepted" })
$rejected = @($events | Where-Object { $_.Type -eq "rejected" })
$hits = @($events | Where-Object { $_.Type -eq "hit" })
$kills = @($events | Where-Object { $_.Type -eq "kill" })
$dummySpawns = @($events | Where-Object { $_.Type -eq "dummy_spawn" })
$dummyRespawns = @($events | Where-Object { $_.Type -eq "dummy_respawn" })
$dummyClears = @($events | Where-Object { $_.Type -eq "dummy_clear" })
$headshotHits = @($hits | Where-Object { $_.Headshot -eq $true })
$headshotKills = @($kills | Where-Object { $_.Headshot -eq $true })
$lethalEvents = @($hits | Where-Object { $_.HeadshotLethalApplied -eq $true })
if ($lethalEvents.Count -eq 0) { $lethalEvents = @($kills | Where-Object { $_.HeadshotLethalApplied -eq $true }) }
$dummyHits = @($hits | Where-Object { IsDummyVictim $_ })
$dummyKills = @($kills | Where-Object { IsDummyVictim $_ })
$dummyHeadshotHits = @($dummyHits | Where-Object { $_.Headshot -eq $true })
$dummyHeadshotKills = @($dummyKills | Where-Object { $_.Headshot -eq $true })
$dummyLethal = @($dummyHits | Where-Object { $_.HeadshotLethalApplied -eq $true })
if ($dummyLethal.Count -eq 0) { $dummyLethal = @($dummyKills | Where-Object { $_.HeadshotLethalApplied -eq $true }) }
$armoredDummyHits = @($dummyHits | Where-Object { IsArmoredDummy $_ })
$protectedDummyHeadshotHits = @($dummyHits | Where-Object { $_.Headshot -eq $true -and $_.HeadProtected -eq $true })
$protectedDummyHeadshotKills = @($dummyKills | Where-Object { $_.Headshot -eq $true -and $_.HeadProtected -eq $true })
$dummyEvidence = @($dummyHits) + @($dummyKills)
$unarmoredEvidence = @($dummyEvidence | Where-Object { $_.TargetProfileName -eq "unarmored" -or ((($null -eq $_.DummyArmorBefore) -or $_.DummyArmorBefore -le 0.0) -and $_.ArmorApplied -ne $true) })
$armoredEvidence = @($dummyEvidence | Where-Object { $_.ArmorApplied -eq $true -or (($null -ne $_.DummyArmorBefore) -and $_.DummyArmorBefore -gt 0.0) })
$firstShotAccepted = @($accepted | Where-Object { $_.FirstShot -eq $true })
$movePenaltyAccepted = @($accepted | Where-Object { $null -ne $_.MovementPenalty -and $_.MovementPenalty -gt 0.0 })
$groundedAccepted = @($accepted | Where-Object { $_.Grounded -eq $true })
$airborneAccepted = @($accepted | Where-Object { $_.Grounded -eq $false })
$duckingAccepted = @($accepted | Where-Object { $_.Ducking -eq $true })
$standingMoveAccepted = @($accepted | Where-Object { $_.Grounded -eq $true -and $_.Ducking -eq $false -and $null -ne $_.MovementPenaltyRate })
$crouchMoveAccepted = @($accepted | Where-Object { $_.Grounded -eq $true -and $_.Ducking -eq $true -and $null -ne $_.MovementPenaltyRate })
$spreadStats = Stats ($accepted | ForEach-Object { $_.Spread })
$moveStats = Stats ($accepted | ForEach-Object { $_.MovementPenalty })
$speedStats = Stats ($accepted | ForEach-Object { $_.HorizontalSpeed })
$standingRateStats = Stats ($standingMoveAccepted | ForEach-Object { $_.MovementPenaltyRate })
$crouchRateStats = Stats ($crouchMoveAccepted | ForEach-Object { $_.MovementPenaltyRate })
$appliedDamageStats = Stats ($hits | ForEach-Object { $_.AppliedDamage })
$dummyAppliedDamageStats = Stats ($dummyHits | ForEach-Object { $_.AppliedDamage })
$rawDamageStats = Stats ($dummyHits | ForEach-Object { if ($null -ne $_.DamageRaw) { $_.DamageRaw } else { $_.TraceDamage } })
$damageToHealthStats = Stats ($dummyHits | ForEach-Object { if ($null -ne $_.DamageToHealth) { $_.DamageToHealth } else { $_.AppliedDamage } })
$damageAbsorbedStats = Stats ($dummyHits | ForEach-Object { $_.DamageAbsorbed })
$armorDrainStats = Stats ($dummyHits | ForEach-Object { $_.ArmorDrain })
$hitgroupCounts = HitgroupCounts $hits
$dummyHitgroupCounts = HitgroupCounts $dummyHits
$recoveryReturnedFirstShot = $false
$recoveryEvent = $null
$sawNonFirst = $false
foreach ($a in $accepted) {
    if ($a.FirstShot -eq $false) { $sawNonFirst = $true; continue }
    if ($sawNonFirst -and $a.FirstShot -eq $true) { $recoveryReturnedFirstShot = $true; $recoveryEvent = $a; break }
}
$crouchMoveEvidence = $false
if ($standingRateStats -and $crouchRateStats) { $crouchMoveEvidence = $crouchRateStats.Average -lt $standingRateStats.Average }
$dummyDriven = ($dummySpawns.Count -gt 0) -and ($dummyHits.Count -gt 0)
$targetProfile = FirstTargetProfile $session $dummySpawns $dummyHits $dummyKills

if (-not $session) { [void]$warnings.Add("No session header line was parsed. The analyzer can still summarize event telemetry, but launch metadata is missing.") }
if ($dummySpawns.Count -gt 0) { [void]$observations.Add(("Dummy lifecycle telemetry was present ({0} spawn, {1} respawn)." -f $dummySpawns.Count,$dummyRespawns.Count)) }
elseif ($dummyHits.Count -gt 0 -or $dummyKills.Count -gt 0) { [void]$warnings.Add("Dummy hit or kill evidence was present without an explicit dummy_spawn line in this log slice.") }
if (-not [string]::IsNullOrWhiteSpace($targetProfile)) { [void]$observations.Add(("Target profile metadata was present for this analysis pass: {0}." -f $targetProfile)) }
elseif ($dummySpawns.Count -gt 0 -or $dummyHits.Count -gt 0) { [void]$warnings.Add("No lab target profile metadata was found. This reduces confidence when comparing armored versus unarmored sessions.") }
if ($dummyDriven) { [void]$observations.Add("The log contains real dummy-driven live firing evidence: a dummy spawned and later received Glock hit telemetry.") }
elseif ($dummySpawns.Count -gt 0) { [void]$warnings.Add("The log shows dummy lifecycle telemetry, but no Glock hit telemetry against the dummy yet.") }
if ($unarmoredEvidence.Count -gt 0) { [void]$observations.Add(("The log contains dummy evidence against unarmored targets ({0} hit or kill events)." -f $unarmoredEvidence.Count)) } else { [void]$warnings.Add("No dummy hit or kill evidence was clearly classified as unarmored.") }
if ($armoredEvidence.Count -gt 0) { [void]$observations.Add(("The log contains dummy evidence against armored targets ({0} hit or kill events)." -f $armoredEvidence.Count)) } else { [void]$warnings.Add("No armored dummy hit or kill evidence was found.") }
if ($protectedDummyHeadshotHits.Count -gt 0) { [void]$observations.Add(("The log contains protected-head dummy headshot hit evidence ({0})." -f $protectedDummyHeadshotHits.Count)) } elseif ($armoredDummyHits.Count -gt 0) { [void]$warnings.Add("Armored dummy hits were present, but none were protected-head dummy headshots.") }
if ($protectedDummyHeadshotKills.Count -gt 0) { [void]$observations.Add(("Protected-head dummy headshot kill evidence exists ({0})." -f $protectedDummyHeadshotKills.Count)) } elseif ($protectedDummyHeadshotHits.Count -gt 0) { [void]$warnings.Add("Protected-head dummy headshot hits were logged, but none of them were kills.") }
if ($dummyLethal.Count -gt 0) { [void]$observations.Add(("Explicit lethal-headshot evidence against the dummy was present ({0})." -f $dummyLethal.Count)) } elseif ($dummyHeadshotHits.Count -gt 0) { [void]$warnings.Add("Dummy headshot hits were logged, but no line explicitly recorded headshot_lethal_applied=1 against the dummy.") }
if ($firstShotAccepted.Count -gt 0) { [void]$observations.Add(("Accepted Glock primary shots included firstshot=1 evidence ({0})." -f $firstShotAccepted.Count)) }
if ($movePenaltyAccepted.Count -gt 0) { [void]$observations.Add(("Accepted Glock primary shots with move_penalty > 0 were present ({0})." -f $movePenaltyAccepted.Count)) }
if ($recoveryReturnedFirstShot) { $delta = if ($recoveryEvent -and $null -ne $recoveryEvent.DeltaPrevious) { " after delta_prev=$(FNum $recoveryEvent.DeltaPrevious 3)s" } else { "" }; [void]$observations.Add(("Later accepted events returned to firstshot=1 after earlier non-firstshot accepted shots{0}, which is evidence that recovery restored the first-shot gate." -f $delta)) }
elseif ($firstShotAccepted.Count -gt 0 -and $accepted.Count -gt 1) { [void]$warnings.Add("The log shows first-shot accepted events, but it does not clearly show a later return to firstshot=1 after a non-firstshot accepted event.") }
if ($standingMoveAccepted.Count -gt 0 -and $crouchMoveAccepted.Count -gt 0) {
    if ($crouchMoveEvidence) { [void]$observations.Add(("Grounded crouch-moving accepted shots showed lower normalized movement-penalty candidates than grounded standing moving shots (avg penalty-rate {0} vs {1}). This is evidence, not proof, of crouch-move penalty reduction." -f (FNum $crouchRateStats.Average 4),(FNum $standingRateStats.Average 4))) }
    else { [void]$warnings.Add(("Grounded crouch-moving accepted shots were present, but this log did not show lower crouch normalized movement-penalty candidates than grounded standing movement (avg penalty-rate {0} vs {1})." -f (FNum $crouchRateStats.Average 4),(FNum $standingRateStats.Average 4))) }
} elseif ($crouchMoveAccepted.Count -eq 0) { [void]$warnings.Add("No grounded crouch-moving accepted shots with move_penalty > 0 were found, so crouch-move reduction cannot be evaluated from this log.") }
else { [void]$warnings.Add("No comparable grounded standing moving accepted shots were found, so crouch-move reduction cannot be evaluated from this log.") }

$signals = [ordered]@{
    acceptedShots = ($accepted.Count -gt 0); tapFireHoldRejections = ($rejected.Count -gt 0); firstShotAccepted = ($firstShotAccepted.Count -gt 0); movementPenaltyPositive = ($movePenaltyAccepted.Count -gt 0); recoveryReturnedFirstShot = $recoveryReturnedFirstShot; crouchMovePenaltyReductionCandidate = $crouchMoveEvidence;
    hitsPresent = ($hits.Count -gt 0); killsPresent = ($kills.Count -gt 0); headshotHitsPresent = ($headshotHits.Count -gt 0); headshotKillsPresent = ($headshotKills.Count -gt 0); lethalHeadshotEvidencePresent = ($lethalEvents.Count -gt 0);
    dummySpawnsPresent = ($dummySpawns.Count -gt 0); dummyRespawnsPresent = ($dummyRespawns.Count -gt 0); dummyHitsPresent = ($dummyHits.Count -gt 0); dummyKillsPresent = ($dummyKills.Count -gt 0); dummyHeadshotHitsPresent = ($dummyHeadshotHits.Count -gt 0); dummyHeadshotKillsPresent = ($dummyHeadshotKills.Count -gt 0);
    armoredDummyHitsPresent = ($armoredDummyHits.Count -gt 0); protectedDummyHeadshotHitsPresent = ($protectedDummyHeadshotHits.Count -gt 0); protectedDummyHeadshotKillsPresent = ($protectedDummyHeadshotKills.Count -gt 0); dummyLethalHeadshotEvidencePresent = ($dummyLethal.Count -gt 0); dummyDrivenLiveFiringEvidencePresent = $dummyDriven; dummyUnarmoredEvidencePresent = ($unarmoredEvidence.Count -gt 0); dummyArmoredEvidencePresent = ($armoredEvidence.Count -gt 0)
}

if ($RequireAccepted -and -not $signals.acceptedShots) { [void]$assertions.Add("Required signal missing: accepted Glock primary shots.") }
if ($RequireRejections -and -not $signals.tapFireHoldRejections) { [void]$assertions.Add("Required signal missing: tap-fire hold rejection lines.") }
if ($RequireFirstShot -and -not $signals.firstShotAccepted) { [void]$assertions.Add("Required signal missing: accepted events with firstshot=1.") }
if ($RequireMovePenalty -and -not $signals.movementPenaltyPositive) { [void]$assertions.Add("Required signal missing: accepted events with move_penalty > 0.") }
if ($RequireCrouchMoveEvidence -and -not $signals.crouchMovePenaltyReductionCandidate) { [void]$assertions.Add("Required signal missing: crouch-moving accepted shots that suggest lower movement penalty than standing movement.") }
if ($RequireHits -and -not $signals.hitsPresent) { [void]$assertions.Add("Required signal missing: Glock hit telemetry lines.") }
if ($RequireKills -and -not $signals.killsPresent) { [void]$assertions.Add("Required signal missing: Glock kill telemetry lines.") }
if ($RequireHeadshotKills -and -not $signals.headshotKillsPresent) { [void]$assertions.Add("Required signal missing: Glock headshot kill telemetry lines.") }
if ($RequireLethalHeadshotEvidence -and -not $signals.lethalHeadshotEvidencePresent) { [void]$assertions.Add("Required signal missing: explicit lethal-headshot evidence via headshot_lethal_applied=1.") }
if ($RequireDummySpawns -and -not $signals.dummySpawnsPresent) { [void]$assertions.Add("Required signal missing: lab dummy spawn lifecycle lines.") }
if ($RequireDummyHits -and -not $signals.dummyHitsPresent) { [void]$assertions.Add("Required signal missing: Glock hit telemetry against the lab dummy.") }
if ($RequireDummyHeadshotHits -and -not $signals.dummyHeadshotHitsPresent) { [void]$assertions.Add("Required signal missing: dummy headshot hit telemetry.") }
if ($RequireDummyHeadshotKills -and -not $signals.dummyHeadshotKillsPresent) { [void]$assertions.Add("Required signal missing: dummy headshot kill telemetry.") }
if ($RequireArmoredDummyHits -and -not $signals.armoredDummyHitsPresent) { [void]$assertions.Add("Required signal missing: armored dummy hit telemetry.") }
if ($RequireProtectedDummyHeadshotHits -and -not $signals.protectedDummyHeadshotHitsPresent) { [void]$assertions.Add("Required signal missing: protected-head dummy headshot hit telemetry.") }
if ($RequireProtectedDummyHeadshotKills -and -not $signals.protectedDummyHeadshotKillsPresent) { [void]$assertions.Add("Required signal missing: protected-head dummy headshot kill telemetry.") }
if ($RequireDummyLethalHeadshotEvidence -and -not $signals.dummyLethalHeadshotEvidencePresent) { [void]$assertions.Add("Required signal missing: explicit lethal-headshot evidence against the dummy via headshot_lethal_applied=1.") }

$report = [ordered]@{
    analyzedLogPath = $log.FullName
    session = if ($session) { [ordered]@{ timestamp = $session.Timestamp; map = $session.Map; game = $session.Game; status = $session.Status; file = $session.LogFile; profileName = $session.ProfileName; targetProfileName = $targetProfile; featureFlags = [ordered]@{ tapFire = $session.TapFire; moveSpreadScale = $session.MoveSpreadScale; firstShotAccuracy = $session.FirstShotEnabled; spreadRecovery = $session.SpreadRecovery }; tuning = [ordered]@{ baseSpread = $session.BaseSpread; groundMovePenalty = $session.GroundMovePenalty; airMovePenalty = $session.AirMovePenalty; duckPenaltyScale = $session.DuckPenaltyScale; firstShotSpeedThreshold = $session.FirstShotSpeedThreshold; maxSpread = $session.MaxSpread; primaryDamage = $session.PrimaryDamageSetting; headshotScale = $session.HeadshotScaleSetting; headshotLethal = $session.HeadshotLethalSetting }; labDummy = [ordered]@{ enabled = $session.LabDummyEnabled; health = $session.LabDummyHealth; armor = $session.LabDummyArmorSetting; headProtected = $session.LabDummyHeadProtectedSetting; armorHealthFraction = $session.LabDummyArmorHealthFraction; armorDrainScale = $session.LabDummyArmorDrainScale; autorespawn = $session.LabDummyAutoRespawn; respawnDelay = $session.LabDummyRespawnDelay; spawnDistance = $session.LabDummySpawnDistance; model = $session.LabDummyModel } } } else { $null }
    counters = [ordered]@{ parsedEventCount = $events.Count; acceptedShots = $accepted.Count; rejectedShots = $rejected.Count; hitEvents = $hits.Count; killEvents = $kills.Count; headshotHits = $headshotHits.Count; headshotKills = $headshotKills.Count; lethalHeadshotEvidence = $lethalEvents.Count; dummySpawns = $dummySpawns.Count; dummyRespawns = $dummyRespawns.Count; dummyClears = $dummyClears.Count; dummyHits = $dummyHits.Count; dummyKills = $dummyKills.Count; dummyHeadshotHits = $dummyHeadshotHits.Count; dummyHeadshotKills = $dummyHeadshotKills.Count; armoredDummyHits = $armoredDummyHits.Count; protectedDummyHeadshotHits = $protectedDummyHeadshotHits.Count; protectedDummyHeadshotKills = $protectedDummyHeadshotKills.Count; dummyLethalHeadshotEvidence = $dummyLethal.Count; firstShotAccepted = $firstShotAccepted.Count; acceptedMovePenaltyPositive = $movePenaltyAccepted.Count; acceptedGrounded = $groundedAccepted.Count; acceptedAirborne = $airborneAccepted.Count; acceptedDucking = $duckingAccepted.Count }
    stats = [ordered]@{ spread = $spreadStats; movementPenalty = $moveStats; horizontalSpeed = $speedStats; standingMovePenaltyRate = $standingRateStats; crouchMovePenaltyRate = $crouchRateStats; appliedDamage = $appliedDamageStats; hitgroupCounts = $hitgroupCounts; dummyAppliedDamage = $dummyAppliedDamageStats; dummyHitgroupCounts = $dummyHitgroupCounts; dummyRawDamage = $rawDamageStats; dummyDamageToHealth = $damageToHealthStats; dummyDamageAbsorbed = $damageAbsorbedStats; dummyArmorDrain = $armorDrainStats }
    signals = $signals
    observations = @($observations)
    warnings = @($warnings)
}

$exports = [ordered]@{}
if ($ExportJson -or $ExportCsv) {
    $outDir = if ([string]::IsNullOrWhiteSpace($OutputDir)) { Get-WeaponDebugReportsRoot } else { Get-FullPath -Path $OutputDir }
    Ensure-Directory -Path $outDir
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    if ($ExportJson) { $json = Join-Path $outDir ("weapon-report-" + $stamp + ".json"); $report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $json -Encoding UTF8; $exports.json = $json }
    if ($ExportCsv) { $csv = Join-Path $outDir ("weapon-events-" + $stamp + ".csv"); $events | Export-Csv -LiteralPath $csv -NoTypeInformation -Encoding UTF8; $exports.csv = $csv }
}

Write-Host "Weapon log analysis"
Write-Host "  log path                 : $($log.FullName)"
if ($session) {
    Write-Host "  session                  : ts=$($session.Timestamp) map=$($session.Map) game=$($session.Game) status=$($session.Status)"
    if (-not [string]::IsNullOrWhiteSpace($session.ProfileName)) { Write-Host "  profile                  : $($session.ProfileName)" }
    Write-Host "  target profile           : $(if ([string]::IsNullOrWhiteSpace($targetProfile)) { 'n/a' } else { $targetProfile })"
    if ($null -ne $session.BaseSpread -or $null -ne $session.GroundMovePenalty -or $null -ne $session.AirMovePenalty -or $null -ne $session.DuckPenaltyScale -or $null -ne $session.FirstShotSpeedThreshold -or $null -ne $session.MaxSpread) { Write-Host "  tuning                   : base=$(FNum $session.BaseSpread 4) ground=$(FNum $session.GroundMovePenalty 4) air=$(FNum $session.AirMovePenalty 4) duck=$(FNum $session.DuckPenaltyScale 4) firstshot_speed=$(FNum $session.FirstShotSpeedThreshold 1) max_spread=$(FNum $session.MaxSpread 4)" }
    if ($null -ne $session.PrimaryDamageSetting -or $null -ne $session.HeadshotScaleSetting -or $null -ne $session.HeadshotLethalSetting) { Write-Host "  damage tuning            : damage=$(FNum $session.PrimaryDamageSetting 4) headshot_scale=$(FNum $session.HeadshotScaleSetting 4) headshot_lethal=$(FBool $session.HeadshotLethalSetting)" }
    if ($null -ne $session.TapFire -or $null -ne $session.MoveSpreadScale -or $null -ne $session.FirstShotEnabled -or $null -ne $session.SpreadRecovery) { Write-Host "  feature flags            : tapfire=$(FBool $session.TapFire) move_scale=$(FNum $session.MoveSpreadScale 4) firstshot=$(FBool $session.FirstShotEnabled) recovery=$(FNum $session.SpreadRecovery 3)" }
    if ($null -ne $session.LabDummyEnabled -or $null -ne $session.LabDummyArmorSetting -or $null -ne $session.LabDummyHeadProtectedSetting) { Write-Host "  lab dummy                : enabled=$(FBool $session.LabDummyEnabled) health=$(FNum $session.LabDummyHealth 1) armor=$(FNum $session.LabDummyArmorSetting 1) head_protected=$(FBool $session.LabDummyHeadProtectedSetting) armor_health_fraction=$(FNum $session.LabDummyArmorHealthFraction 3) armor_drain_scale=$(FNum $session.LabDummyArmorDrainScale 3)" }
} else {
    Write-Host "  session                  : missing"
    Write-Host "  target profile           : $(if ([string]::IsNullOrWhiteSpace($targetProfile)) { 'n/a' } else { $targetProfile })"
}
Write-Host "  accepted shots           : $($accepted.Count)"
Write-Host "  rejected shots           : $($rejected.Count)"
Write-Host "  hit events               : $($hits.Count)"
Write-Host "  kill events              : $($kills.Count)"
Write-Host "  headshot hits            : $($headshotHits.Count)"
Write-Host "  headshot kills           : $($headshotKills.Count)"
Write-Host "  lethal hs evidence       : $($lethalEvents.Count)"
Write-Host "  dummy spawns             : $($dummySpawns.Count)"
Write-Host "  dummy respawns           : $($dummyRespawns.Count)"
Write-Host "  dummy hits               : $($dummyHits.Count)"
Write-Host "  dummy kills              : $($dummyKills.Count)"
Write-Host "  armored dummy hits       : $($armoredDummyHits.Count)"
Write-Host "  protected hs hits        : $($protectedDummyHeadshotHits.Count)"
Write-Host "  protected hs kills       : $($protectedDummyHeadshotKills.Count)"
Write-Host "  dummy lethal hs evidence : $($dummyLethal.Count)"
Write-Host "  first-shot accepted      : $($firstShotAccepted.Count)"
Write-Host "  accepted move_penalty>0  : $($movePenaltyAccepted.Count)"
Write-Host "  applied dmg min/avg/max  : $(FNum $(if ($appliedDamageStats) { $appliedDamageStats.Min } else { $null }) 4) / $(FNum $(if ($appliedDamageStats) { $appliedDamageStats.Average } else { $null }) 4) / $(FNum $(if ($appliedDamageStats) { $appliedDamageStats.Max } else { $null }) 4)"
Write-Host "  dummy dmg min/avg/max    : $(FNum $(if ($dummyAppliedDamageStats) { $dummyAppliedDamageStats.Min } else { $null }) 4) / $(FNum $(if ($dummyAppliedDamageStats) { $dummyAppliedDamageStats.Average } else { $null }) 4) / $(FNum $(if ($dummyAppliedDamageStats) { $dummyAppliedDamageStats.Max } else { $null }) 4)"
Write-Host "  raw dmg min/avg/max      : $(FNum $(if ($rawDamageStats) { $rawDamageStats.Min } else { $null }) 4) / $(FNum $(if ($rawDamageStats) { $rawDamageStats.Average } else { $null }) 4) / $(FNum $(if ($rawDamageStats) { $rawDamageStats.Max } else { $null }) 4)"
Write-Host "  to-health min/avg/max    : $(FNum $(if ($damageToHealthStats) { $damageToHealthStats.Min } else { $null }) 4) / $(FNum $(if ($damageToHealthStats) { $damageToHealthStats.Average } else { $null }) 4) / $(FNum $(if ($damageToHealthStats) { $damageToHealthStats.Max } else { $null }) 4)"
Write-Host "  absorbed min/avg/max     : $(FNum $(if ($damageAbsorbedStats) { $damageAbsorbedStats.Min } else { $null }) 4) / $(FNum $(if ($damageAbsorbedStats) { $damageAbsorbedStats.Average } else { $null }) 4) / $(FNum $(if ($damageAbsorbedStats) { $damageAbsorbedStats.Max } else { $null }) 4)"
Write-Host "  armor drain min/avg/max  : $(FNum $(if ($armorDrainStats) { $armorDrainStats.Min } else { $null }) 4) / $(FNum $(if ($armorDrainStats) { $armorDrainStats.Average } else { $null }) 4) / $(FNum $(if ($armorDrainStats) { $armorDrainStats.Max } else { $null }) 4)"
Write-Host "  hitgroups                : $(FHitgroups $hitgroupCounts)"
Write-Host "  dummy hitgroups          : $(FHitgroups $dummyHitgroupCounts)"
Write-Host "  accepted grounded        : $($groundedAccepted.Count)"
Write-Host "  accepted airborne        : $($airborneAccepted.Count)"
Write-Host "  accepted ducking         : $($duckingAccepted.Count)"
Write-Host "  spread min/avg/max       : $(FNum $(if ($spreadStats) { $spreadStats.Min } else { $null }) 4) / $(FNum $(if ($spreadStats) { $spreadStats.Average } else { $null }) 4) / $(FNum $(if ($spreadStats) { $spreadStats.Max } else { $null }) 4)"
Write-Host "  move penalty min/avg/max : $(FNum $(if ($moveStats) { $moveStats.Min } else { $null }) 4) / $(FNum $(if ($moveStats) { $moveStats.Average } else { $null }) 4) / $(FNum $(if ($moveStats) { $moveStats.Max } else { $null }) 4)"
Write-Host "  speed2d min/avg/max      : $(FNum $(if ($speedStats) { $speedStats.Min } else { $null }) 1) / $(FNum $(if ($speedStats) { $speedStats.Average } else { $null }) 1) / $(FNum $(if ($speedStats) { $speedStats.Max } else { $null }) 1)"

Write-Host ""; Write-Host "Observations"
if ($observations.Count -eq 0) { Write-Host "  - No notable observations were derived from the parsed telemetry." } else { foreach ($o in $observations) { Write-Host "  - $o" } }
Write-Host ""; Write-Host "Warnings"
if ($warnings.Count -eq 0) { Write-Host "  - none" } else { foreach ($w in $warnings) { Write-Host "  - $w" } }
if ($exports.Count -gt 0) { Write-Host ""; Write-Host "Exports"; foreach ($k in $exports.Keys) { Write-Host "  $k : $($exports[$k])" } }
Write-Host ""; Write-Host "Evidence only: this summarizes logged server-authoritative telemetry. Armored-dummy evidence is not proof of exact PvP or exact player-armor parity."
if ($assertions.Count -gt 0) { Write-Host ""; Write-Host "Assertion failures"; foreach ($a in $assertions) { Write-Host "  - $a" }; exit 2 }
