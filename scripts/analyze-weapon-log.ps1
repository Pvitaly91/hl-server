[CmdletBinding(DefaultParameterSetName='Latest')]
param(
 [Parameter(ParameterSetName='Path',Mandatory=$true)][string]$Path,
 [Parameter(ParameterSetName='Latest')][switch]$Latest,
 [ValidateSet('glock','mp5','all')][string]$Weapon='all',
 [switch]$ExportJson,[switch]$ExportCsv,[string]$OutputDir,[switch]$PassThru,
 [switch]$RequireAccepted,[switch]$RequireRejections,[switch]$RequireFirstShot,[switch]$RequireMovePenalty,[switch]$RequireCrouchMoveEvidence,[switch]$RequireHits,[switch]$RequireKills,[switch]$RequireHeadshotKills,[switch]$RequireLethalHeadshotEvidence,[switch]$RequireDummySpawns,[switch]$RequireDummyHits,[switch]$RequireDummyHeadshotHits,[switch]$RequireDummyHeadshotKills,[switch]$RequireArmoredDummyHits,[switch]$RequireProtectedDummyHeadshotHits,[switch]$RequireProtectedDummyHeadshotKills,[switch]$RequireDummyLethalHeadshotEvidence,[switch]$RequireWeaponAccepted,[switch]$RequireWeaponHits,[switch]$RequireWeaponKills,[switch]$RequireWeaponHeadshotKills,[switch]$RequireBurstGrowthEvidence,[switch]$RequireMovementPenaltyEvidence)
$ErrorActionPreference='Stop'; . "$PSScriptRoot\common.ps1"; Import-HLServerEnv
function Resolve-LogFile{if($PSCmdlet.ParameterSetName -eq 'Path'){$p=Get-FullPath -Path $Path; if(-not(Test-LeafPath $p)){throw "Weapon debug log was not found: $p"}; return Get-Item -LiteralPath $p}; $l=Get-LatestWeaponDebugLog; if(-not $l){throw "No weapon debug log was found under $(Get-WeaponDebugLogsRoot). Generate telemetry first or pass -Path."}; $l}
function F($h,[string]$n){if($h -and $h.Contains($n)){[string]$h[$n]}}
function FF($h,[string[]]$n){foreach($x in $n){$v=F $h $x;if(-not [string]::IsNullOrWhiteSpace($v)){return $v}}}
function I([string]$v){if([string]::IsNullOrWhiteSpace($v)-or $v -eq 'na'){return $null};[int]::Parse($v,[Globalization.CultureInfo]::InvariantCulture)}
function N([string]$v){if([string]::IsNullOrWhiteSpace($v)-or $v -eq 'na'){return $null};[double]::Parse($v,[Globalization.CultureInfo]::InvariantCulture)}
function B([string]$v){if([string]::IsNullOrWhiteSpace($v)-or $v -eq 'na'){return $null};switch($v.ToLowerInvariant()){'1'{$true}'0'{$false}'true'{$true}'false'{$false}default{[bool]::Parse($v)}}}
function S([object[]]$v){$f=@($v|?{$_ -ne $null});if($f.Count -eq 0){return $null};$m=$f|measure -Minimum -Maximum -Average;[pscustomobject]@{Count=[int]$m.Count;Min=[double]$m.Minimum;Average=[double]$m.Average;Max=[double]$m.Maximum}}
function M($e){$Weapon -eq 'all' -or (([string]$e.Weapon).ToLowerInvariant() -eq $Weapon)}
function FHitgroups($values){$items=@();foreach($value in @($values)){if($null -eq $value){continue};$name=[string]$value.Name;$count=$value.Count;if([string]::IsNullOrWhiteSpace($name)){continue};$items+=('{0}={1}' -f $name,$count)};if($items.Count -eq 0){return 'n/a'};return ($items -join ', ')}
function D($e){$e.VictimKind -eq 'dummy' -or $e.VictimClass -eq 'glock_lab_dummy'}
function A($e){(D $e) -and ($e.ArmorApplied -eq $true -or (($e.DummyArmorBefore) -gt 0))}
function P([string]$l,[int]$n){
 $t=$l.Trim();if(-not $t){return $null}
 if($t.StartsWith('[')){$i=$t.IndexOf(']');if($i -gt 0){$t=$t.Substring($i+1).TrimStart()}}
 $m=[regex]::Matches($t,'(?<k>[A-Za-z0-9_]+)=(?:"(?<q>[^"]*)"|(?<v>\S+))');if($m.Count -eq 0){return $null}
 $h=[ordered]@{};foreach($x in $m){$h[$x.Groups['k'].Value]=if($x.Groups['q'].Success){$x.Groups['q'].Value}else{$x.Groups['v'].Value}}
 $type=(F $h 'type');if(-not $type){if((F $h 'event') -eq 'weapon_debug_session'){$type='session'}elseif(F $h 'reason'){$type='rejected'}elseif(F $h 'weapon'){$type='accepted'}else{$type='unknown'}}
 $w=(F $h 'weapon');if($w){$w=$w.ToLowerInvariant()}
 [pscustomobject]@{
  Line=$n
  Type=$type.ToLowerInvariant()
  Timestamp=F $h 'ts'
  Map=F $h 'map'
  Game=F $h 'game'
  Status=F $h 'status'
  LogFile=F $h 'file'
  Weapon=$w
  WeaponUnderTest=FF $h @('weapon_under_test','weaponUnderTest','sv_exp_weapon_under_test')
  ProfileName=F $h 'profile'
  GlockProfileName=FF $h @('glock_profile','glockProfile','sv_exp_glock_profile_name')
  Mp5ProfileName=FF $h @('mp5_profile','mp5Profile','sv_exp_mp5_profile_name')
  TargetProfileName=FF $h @('target_profile','targetProfile','sv_exp_glock_lab_target_profile_name')
  SessionTag=FF $h @('session_tag','sv_exp_session_tag')
  MatrixName=FF $h @('matrix_name','sv_exp_matrix_name')
  MatrixStep=FF $h @('matrix_step','sv_exp_matrix_step')
  CfgMode=B(F $h 'cfg_mode')
  CfgActiveProfile=FF $h @('cfg_active_profile','cfg_active')
  CfgLastSuccessfulProfile=FF $h @('cfg_last_successful_profile','cfg_last_success')
  CfgLastAction=FF $h @('cfg_last_action')
  CfgLastAppliedAt=FF $h @('cfg_last_applied_at')
  Source=F $h 'source'
  Candidate=FF $h @('candidate','details')
  FailureCode=FF $h @('code')
  Reason=F $h 'reason'
  Details=F $h 'details'
  HitGroup=FF $h @('hitgroup','hitGroup')
  FirstShot=B(F $h 'firstshot')
  Spread=N(F $h 'spread')
  MovementPenalty=N(F $h 'move_penalty')
  BurstAddedSpread=N(FF $h @('burst_additional_spread','burstAddedSpread'))
  BurstIndex=I(FF $h @('burst_index','burstIndex'))
  HorizontalSpeed=N(F $h 'speed2d')
  MaxSpeed=N(F $h 'maxspeed')
  Grounded=B(F $h 'grounded')
  Ducking=B(F $h 'ducking')
  AppliedDamage=N(F $h 'applied_damage')
  Headshot=B(F $h 'headshot')
  HeadshotLethalApplied=B(F $h 'headshot_lethal_applied')
  VictimKind=FF $h @('victim_kind','victimKind')
  VictimClass=FF $h @('victim_class','victimClass')
  ArmorApplied=B(FF $h @('armor_applied','armorApplied'))
  HeadProtected=B(FF $h @('head_protected','headProtected'))
  DummyArmorBefore=N(FF $h @('dummy_armor_before','dummyArmorBefore'))
  Raw=$l
 }}
$log=Resolve-LogFile;$events=@();$warnings=New-Object Collections.ArrayList;$obs=New-Object Collections.ArrayList;$assert=New-Object Collections.ArrayList;$i=0;foreach($line in Get-Content -LiteralPath $log.FullName){$i++;$e=P $line $i;if($e -and $e.Type -ne 'unknown'){$events+=$e}elseif($line.Trim()){[void]$warnings.Add("Skipped non-telemetry line $i because it did not match the expected single-line key=value format.")}}
$session=@($events|?{$_.Type -eq 'session'}|select -first 1);$session=if($session.Count){$session[0]}else{$null};$effectiveWeapon=if($Weapon -ne 'all'){$Weapon}else{$session.WeaponUnderTest}
$accepted=@($events|?{$_.Type -eq 'accepted' -and (M $_)});$rejected=@($events|?{$_.Type -eq 'rejected' -and (M $_)});$hits=@($events|?{$_.Type -eq 'hit' -and (M $_)});$kills=@($events|?{$_.Type -eq 'kill' -and (M $_)});$dummySpawns=@($events|?{$_.Type -eq 'dummy_spawn'});$dummyRespawns=@($events|?{$_.Type -eq 'dummy_respawn'});$dummySpawnFailures=@($events|?{$_.Type -eq 'dummy_spawn_failed'});$targetMarks=@($events|?{$_.Type -eq 'target_mark'});$targetUnmarks=@($events|?{$_.Type -eq 'target_unmark'});$dummyClears=@($events|?{$_.Type -eq 'dummy_clear'});$dummyRepositions=@($events|?{$_.Type -eq 'dummy_reposition'});$headshotHits=@($hits|?{$_.Headshot -eq $true});$headshotKills=@($kills|?{$_.Headshot -eq $true});$lethal=@($hits|?{$_.HeadshotLethalApplied -eq $true});if($lethal.Count -eq 0){$lethal=@($kills|?{$_.HeadshotLethalApplied -eq $true})};$dummyHits=@($hits|?{D $_});$dummyKills=@($kills|?{D $_});$dummyHeadshotHits=@($dummyHits|?{$_.Headshot -eq $true});$dummyHeadshotKills=@($dummyKills|?{$_.Headshot -eq $true});$dummyLethal=@($dummyHits|?{$_.HeadshotLethalApplied -eq $true});if($dummyLethal.Count -eq 0){$dummyLethal=@($dummyKills|?{$_.HeadshotLethalApplied -eq $true})};$armoredDummyHits=@($dummyHits|?{A $_});$protectedDummyHeadshotHits=@($dummyHits|?{$_.Headshot -eq $true -and $_.HeadProtected -eq $true});$protectedDummyHeadshotKills=@($dummyKills|?{$_.Headshot -eq $true -and $_.HeadProtected -eq $true});$firstShotAccepted=@($accepted|?{$_.FirstShot -eq $true});$movePenaltyAccepted=@($accepted|?{$_.MovementPenalty -gt 0});$burstGrowthAccepted=@($accepted|?{$_.BurstAddedSpread -gt 0 -or $_.BurstIndex -gt 1});$standing=@($accepted|?{$_.Grounded -eq $true -and $_.Ducking -eq $false -and $_.MovementPenalty -ne $null -and $_.HorizontalSpeed -gt 0 -and $_.MaxSpeed -gt 0}|%{$_.MovementPenalty/([Math]::Min([Math]::Max(($_.HorizontalSpeed/$_.MaxSpeed),0.0),1.0))});$crouch=@($accepted|?{$_.Grounded -eq $true -and $_.Ducking -eq $true -and $_.MovementPenalty -ne $null -and $_.HorizontalSpeed -gt 0 -and $_.MaxSpeed -gt 0}|%{$_.MovementPenalty/([Math]::Min([Math]::Max(($_.HorizontalSpeed/$_.MaxSpeed),0.0),1.0))});$crouchMoveEvidence=($standing.Count -gt 0 -and $crouch.Count -gt 0 -and ($crouch|measure -Average).Average -lt ($standing|measure -Average).Average);$recoveryReturnedFirstShot=$false;if($effectiveWeapon -ne 'mp5'){$seen=$false;foreach($a in $accepted){if($a.FirstShot -eq $false){$seen=$true;continue};if($seen -and $a.FirstShot -eq $true){$recoveryReturnedFirstShot=$true;break}}}
$spreadStats=S($accepted|%{$_.Spread})
$moveStats=S($accepted|%{$_.MovementPenalty})
$speedStats=S($accepted|%{$_.HorizontalSpeed})
$burstStats=S($accepted|%{$_.BurstAddedSpread})
$appliedStats=S($hits|%{$_.AppliedDamage})
$recentTargetFailureReasons=@($dummySpawnFailures|Select-Object -Last 3|ForEach-Object{if($_.FailureCode -and $_.Reason){'{0}: {1}' -f $_.FailureCode,$_.Reason}elseif($_.Reason){$_.Reason}elseif($_.FailureCode){$_.FailureCode}else{'unknown target spawn failure'}})
$firstTarget=@($dummySpawns+$dummyHits+$dummyKills|?{$_.TargetProfileName}|select -first 1)
$targetProfile=if($session.TargetProfileName){$session.TargetProfileName}elseif($firstTarget.Count){$firstTarget[0].TargetProfileName}else{$null}
$isGlockSession=(($session.WeaponUnderTest -eq 'glock') -or $effectiveWeapon -eq 'glock')
$isMp5Session=(($session.WeaponUnderTest -eq 'mp5') -or $effectiveWeapon -eq 'mp5')
$glockAccepted=@($accepted|?{$_.Weapon -eq 'glock' -and $_.ProfileName}|select -first 1)
$glockProfile=if($isGlockSession -and $session.GlockProfileName){$session.GlockProfileName}elseif($isGlockSession -and $session.ProfileName){$session.ProfileName}elseif((-not $isMp5Session) -and $glockAccepted.Count){$glockAccepted[0].ProfileName}else{$null}
$mp5Accepted=@($accepted|?{$_.Weapon -eq 'mp5' -and $_.ProfileName}|select -first 1)
$mp5Profile=if($isMp5Session -and $session.Mp5ProfileName){$session.Mp5ProfileName}elseif($isMp5Session -and $session.ProfileName){$session.ProfileName}elseif((-not $isGlockSession) -and $mp5Accepted.Count){$mp5Accepted[0].ProfileName}else{$null}
$weaponProfile=if($effectiveWeapon -eq 'mp5'){if($mp5Profile){$mp5Profile}else{$glockProfile}}elseif($effectiveWeapon -eq 'glock'){if($glockProfile){$glockProfile}else{$mp5Profile}}elseif($mp5Profile){$mp5Profile}elseif($glockProfile){$glockProfile}elseif($session.ProfileName){$session.ProfileName}else{$null}
if(-not $session){[void]$warnings.Add('No session header line was parsed. The analyzer can still summarize event telemetry, but launch metadata is missing.')} ; if($accepted.Count){[void]$obs.Add("Parsed $($accepted.Count) accepted event(s).")}else{[void]$warnings.Add('No accepted weapon telemetry matched the current filter.')}; if($hits.Count){[void]$obs.Add("Parsed $($hits.Count) hit event(s).")}else{[void]$warnings.Add('No hit telemetry matched the current filter.')}; if($movePenaltyAccepted.Count){[void]$obs.Add("Movement-penalty evidence exists ($($movePenaltyAccepted.Count)).")}else{[void]$warnings.Add('No accepted event with move_penalty > 0 matched the current filter.')}; if($effectiveWeapon -eq 'glock' -and -not $rejected.Count){[void]$warnings.Add('No Glock tap-fire rejection evidence was logged for this selection.')}; if($effectiveWeapon -eq 'mp5' -and -not $burstGrowthAccepted.Count){[void]$warnings.Add('No MP5 burst-growth evidence was logged for this selection.')} ; if($dummySpawns.Count -or $dummyRespawns.Count -or $dummyClears.Count -or $dummyRepositions.Count -or $dummySpawnFailures.Count -or $targetMarks.Count -or $targetUnmarks.Count){[void]$obs.Add("Target lifecycle telemetry was present ($($dummySpawns.Count) spawn, $($dummyRespawns.Count) respawn, $($dummySpawnFailures.Count) spawn_failed, $($dummyClears.Count) clear, $($dummyRepositions.Count) reposition, $($targetMarks.Count) mark, $($targetUnmarks.Count) unmark).")} ; if($dummySpawnFailures.Count){[void]$obs.Add("Target spawn failures were logged ($($dummySpawnFailures.Count)).")} ; if($dummyHeadshotKills.Count){[void]$obs.Add("Target headshot kill evidence exists ($($dummyHeadshotKills.Count)).")}
$signals=[ordered]@{acceptedShots=($accepted.Count -gt 0);tapFireHoldRejections=($rejected.Count -gt 0);firstShotAccepted=($firstShotAccepted.Count -gt 0);movementPenaltyPositive=($movePenaltyAccepted.Count -gt 0);recoveryReturnedFirstShot=$recoveryReturnedFirstShot;crouchMovePenaltyReductionCandidate=$crouchMoveEvidence;hitsPresent=($hits.Count -gt 0);killsPresent=($kills.Count -gt 0);headshotHitsPresent=($headshotHits.Count -gt 0);headshotKillsPresent=($headshotKills.Count -gt 0);lethalHeadshotEvidencePresent=($lethal.Count -gt 0);dummySpawnsPresent=($dummySpawns.Count -gt 0);dummyRespawnsPresent=($dummyRespawns.Count -gt 0);dummyHitsPresent=($dummyHits.Count -gt 0);dummyKillsPresent=($dummyKills.Count -gt 0);dummyHeadshotHitsPresent=($dummyHeadshotHits.Count -gt 0);dummyHeadshotKillsPresent=($dummyHeadshotKills.Count -gt 0);armoredDummyHitsPresent=($armoredDummyHits.Count -gt 0);protectedDummyHeadshotHitsPresent=($protectedDummyHeadshotHits.Count -gt 0);protectedDummyHeadshotKillsPresent=($protectedDummyHeadshotKills.Count -gt 0);dummyLethalHeadshotEvidencePresent=($dummyLethal.Count -gt 0);weaponAcceptedPresent=($accepted.Count -gt 0);weaponHitsPresent=($hits.Count -gt 0);weaponKillsPresent=($kills.Count -gt 0);weaponHeadshotKillsPresent=($headshotKills.Count -gt 0);burstGrowthEvidencePresent=($burstGrowthAccepted.Count -gt 0);movementPenaltyEvidencePresent=($movePenaltyAccepted.Count -gt 0)}
$missing=New-Object Collections.ArrayList; if($effectiveWeapon -eq 'glock' -and -not $signals.tapFireHoldRejections){[void]$missing.Add('No tap-fire rejection evidence was logged for this step.')}; if(-not $signals.movementPenaltyPositive){[void]$missing.Add('No accepted shot with move_penalty > 0 was logged for this step.')}; if($effectiveWeapon -eq 'mp5' -and -not $signals.burstGrowthEvidencePresent){[void]$missing.Add('No burst-growth evidence was logged for this MP5 step.')}; if(-not $signals.dummyHeadshotHitsPresent){[void]$missing.Add('No dummy headshot hit evidence was logged for this step.')}; if(-not $signals.armoredDummyHitsPresent){[void]$missing.Add('No armored dummy hit evidence was logged for this step.')}; if(-not $signals.protectedDummyHeadshotHitsPresent){[void]$missing.Add('No protected-head dummy headshot evidence was logged for this step.')}; if(-not ($signals.dummyLethalHeadshotEvidencePresent -or $signals.lethalHeadshotEvidencePresent)){[void]$missing.Add('No lethal-headshot evidence was logged for this step.')}
if($RequireAccepted -and -not $signals.acceptedShots){[void]$assert.Add('Required signal missing: accepted weapon telemetry lines.')}; if($RequireRejections -and -not $signals.tapFireHoldRejections){[void]$assert.Add('Required signal missing: rejection telemetry lines.')}; if($RequireFirstShot -and -not $signals.firstShotAccepted){[void]$assert.Add('Required signal missing: accepted events with firstshot=1.')}; if(($RequireMovePenalty -or $RequireMovementPenaltyEvidence) -and -not $signals.movementPenaltyPositive){[void]$assert.Add('Required signal missing: accepted events with move_penalty > 0.')}; if($RequireCrouchMoveEvidence -and -not $signals.crouchMovePenaltyReductionCandidate){[void]$assert.Add('Required signal missing: crouch-moving accepted shots suggesting lower normalized movement penalty than standing movement.')}; if(($RequireHits -or $RequireWeaponHits) -and -not $signals.hitsPresent){[void]$assert.Add('Required signal missing: weapon hit telemetry lines.')}; if(($RequireKills -or $RequireWeaponKills) -and -not $signals.killsPresent){[void]$assert.Add('Required signal missing: weapon kill telemetry lines.')}; if(($RequireHeadshotKills -or $RequireWeaponHeadshotKills) -and -not $signals.headshotKillsPresent){[void]$assert.Add('Required signal missing: weapon headshot kill telemetry lines.')}; if($RequireLethalHeadshotEvidence -and -not $signals.lethalHeadshotEvidencePresent){[void]$assert.Add('Required signal missing: explicit lethal-headshot evidence via headshot_lethal_applied=1.')}; if($RequireDummySpawns -and -not $signals.dummySpawnsPresent){[void]$assert.Add('Required signal missing: lab dummy spawn lifecycle lines.')}; if($RequireDummyHits -and -not $signals.dummyHitsPresent){[void]$assert.Add('Required signal missing: hit telemetry against the lab dummy.')}; if($RequireDummyHeadshotHits -and -not $signals.dummyHeadshotHitsPresent){[void]$assert.Add('Required signal missing: dummy headshot hit telemetry.')}; if($RequireDummyHeadshotKills -and -not $signals.dummyHeadshotKillsPresent){[void]$assert.Add('Required signal missing: dummy headshot kill telemetry.')}; if($RequireArmoredDummyHits -and -not $signals.armoredDummyHitsPresent){[void]$assert.Add('Required signal missing: armored dummy hit telemetry.')}; if($RequireProtectedDummyHeadshotHits -and -not $signals.protectedDummyHeadshotHitsPresent){[void]$assert.Add('Required signal missing: protected-head dummy headshot hit telemetry.')}; if($RequireProtectedDummyHeadshotKills -and -not $signals.protectedDummyHeadshotKillsPresent){[void]$assert.Add('Required signal missing: protected-head dummy headshot kill telemetry.')}; if($RequireDummyLethalHeadshotEvidence -and -not $signals.dummyLethalHeadshotEvidencePresent){[void]$assert.Add('Required signal missing: explicit lethal-headshot evidence against the dummy via headshot_lethal_applied=1.')}; if($RequireWeaponAccepted -and -not $signals.weaponAcceptedPresent){[void]$assert.Add('Required signal missing: accepted telemetry for the selected weapon filter.')}; if($RequireBurstGrowthEvidence -and -not $signals.burstGrowthEvidencePresent){[void]$assert.Add('Required signal missing: burst-growth evidence for the selected weapon filter.')}
$meta=[ordered]@{
 weaponUnderTest=$effectiveWeapon
 weaponProfile=$weaponProfile
 sessionTag=$session.SessionTag
 matrixName=$session.MatrixName
 matrixStep=$session.MatrixStep
 cfgMode=$session.CfgMode
 cfgActiveProfile=$session.CfgActiveProfile
 cfgLastSuccessfulProfile=$session.CfgLastSuccessfulProfile
 cfgLastAction=$session.CfgLastAction
 cfgLastAppliedAt=$session.CfgLastAppliedAt
 glockProfile=$glockProfile
 mp5Profile=$mp5Profile
 labTargetProfile=$targetProfile
}
$summary=[ordered]@{
 weaponUnderTest=$meta.weaponUnderTest
 weaponProfile=$meta.weaponProfile
 sessionTag=$meta.sessionTag
 matrixName=$meta.matrixName
 matrixStep=$meta.matrixStep
 cfgMode=$meta.cfgMode
 cfgActiveProfile=$meta.cfgActiveProfile
 cfgLastSuccessfulProfile=$meta.cfgLastSuccessfulProfile
 cfgLastAction=$meta.cfgLastAction
 cfgLastAppliedAt=$meta.cfgLastAppliedAt
 glockProfile=$meta.glockProfile
 mp5Profile=$meta.mp5Profile
 labTargetProfile=$meta.labTargetProfile
 acceptedShotCount=$accepted.Count
 rejectedShotCount=$rejected.Count
 hitCount=$hits.Count
 killCount=$kills.Count
 targetMarkCount=$targetMarks.Count
 targetUnmarkCount=$targetUnmarks.Count
 targetSpawnCount=$dummySpawns.Count
 targetSpawnFailureCount=$dummySpawnFailures.Count
 targetClearCount=$dummyClears.Count
 targetRespawnCount=$dummyRespawns.Count
 targetRepositionCount=$dummyRepositions.Count
 targetHitCount=$dummyHits.Count
 targetKillCount=$dummyKills.Count
 targetHeadshotHitCount=$dummyHeadshotHits.Count
 targetHeadshotKillCount=$dummyHeadshotKills.Count
 dummyHitCount=$dummyHits.Count
 dummyKillCount=$dummyKills.Count
 dummyHeadshotHitCount=$dummyHeadshotHits.Count
 dummyHeadshotKillCount=$dummyHeadshotKills.Count
 lethalHeadshotEvidenceCount=$lethal.Count
 dummyLethalHeadshotEvidenceCount=$dummyLethal.Count
 armoredDummyHitCount=$armoredDummyHits.Count
 protectedDummyHeadshotHitCount=$protectedDummyHeadshotHits.Count
 protectedDummyHeadshotKillCount=$protectedDummyHeadshotKills.Count
 targetRecentFailureReasons=@($recentTargetFailureReasons)
 burstGrowthEvidenceCount=$burstGrowthAccepted.Count
 movementPenaltyEvidenceCount=$movePenaltyAccepted.Count
 appliedDamage=[ordered]@{
  min=$(if($appliedStats){$appliedStats.Min}else{$null})
  average=$(if($appliedStats){$appliedStats.Average}else{$null})
  max=$(if($appliedStats){$appliedStats.Max}else{$null})
 }
 evidence=[ordered]@{
  tapFireRejection=$signals.tapFireHoldRejections
  tapFireRejectionEvidence=$signals.tapFireHoldRejections
  movementPenalty=$signals.movementPenaltyPositive
  movementPenaltyEvidence=$signals.movementPenaltyPositive
  burstGrowth=$signals.burstGrowthEvidencePresent
  burstGrowthEvidence=$signals.burstGrowthEvidencePresent
  dummyHeadshotPath=$signals.dummyHeadshotHitsPresent
  dummyHeadshotPathEvidence=$signals.dummyHeadshotHitsPresent
  armoredDummyPath=$signals.armoredDummyHitsPresent
  armoredDummyEvidence=$signals.armoredDummyHitsPresent
  protectedHeadDummyPath=$signals.protectedDummyHeadshotHitsPresent
  protectedHeadDummyEvidence=$signals.protectedDummyHeadshotHitsPresent
  lethalHeadshotPath=($signals.dummyLethalHeadshotEvidencePresent -or $signals.lethalHeadshotEvidencePresent)
  lethalHeadshotEvidence=($signals.dummyLethalHeadshotEvidencePresent -or $signals.lethalHeadshotEvidencePresent)
 }
 missingSignalNotes=@($missing)
}
$report=[ordered]@{
 analyzedLogPath=$log.FullName
 session=$(if($session){[ordered]@{
  timestamp=$session.Timestamp
  map=$session.Map
  game=$session.Game
  status=$session.Status
  file=$session.LogFile
  weaponUnderTest=$effectiveWeapon
  weaponProfileName=$weaponProfile
  profileName=$session.ProfileName
  glockProfileName=$glockProfile
  mp5ProfileName=$mp5Profile
  targetProfileName=$targetProfile
  sessionTag=$session.SessionTag
  matrixName=$session.MatrixName
  matrixStep=$session.MatrixStep
 }}else{$null})
 metadata=$meta
 counters=[ordered]@{
  parsedEventCount=$events.Count
  acceptedShots=$accepted.Count
  rejectedShots=$rejected.Count
  hitEvents=$hits.Count
 killEvents=$kills.Count
 headshotHits=$headshotHits.Count
 headshotKills=$headshotKills.Count
  lethalHeadshotEvidence=$lethal.Count
  targetMarks=$targetMarks.Count
  targetUnmarks=$targetUnmarks.Count
  targetSpawns=$dummySpawns.Count
  targetSpawnFailures=$dummySpawnFailures.Count
  targetRespawns=$dummyRespawns.Count
  targetClears=$dummyClears.Count
  targetRepositions=$dummyRepositions.Count
  targetHits=$dummyHits.Count
  targetKills=$dummyKills.Count
  targetHeadshotHits=$dummyHeadshotHits.Count
  targetHeadshotKills=$dummyHeadshotKills.Count
  dummySpawns=$dummySpawns.Count
  dummyRespawns=$dummyRespawns.Count
  dummyClears=$dummyClears.Count
  dummyHits=$dummyHits.Count
  dummyKills=$dummyKills.Count
  dummyHeadshotHits=$dummyHeadshotHits.Count
  dummyHeadshotKills=$dummyHeadshotKills.Count
  armoredDummyHits=$armoredDummyHits.Count
  protectedDummyHeadshotHits=$protectedDummyHeadshotHits.Count
  protectedDummyHeadshotKills=$protectedDummyHeadshotKills.Count
  dummyLethalHeadshotEvidence=$dummyLethal.Count
  firstShotAccepted=$firstShotAccepted.Count
  acceptedMovePenaltyPositive=$movePenaltyAccepted.Count
  burstGrowthEvidence=$burstGrowthAccepted.Count
  acceptedGrounded=@($accepted|?{$_.Grounded -eq $true}).Count
  acceptedAirborne=@($accepted|?{$_.Grounded -eq $false}).Count
  acceptedDucking=@($accepted|?{$_.Ducking -eq $true}).Count
 }
 stats=[ordered]@{
  spread=$spreadStats
  movementPenalty=$moveStats
  horizontalSpeed=$speedStats
  burstAddedSpread=$burstStats
  appliedDamage=$appliedStats
  hitgroupCounts=(($hits|group HitGroup|?{$_.Name}|sort Name|%{[ordered]@{Name=$_.Name;Count=$_.Count}}))
  dummyHitgroupCounts=(($dummyHits|group HitGroup|?{$_.Name}|sort Name|%{[ordered]@{Name=$_.Name;Count=$_.Count}}))
 }
 signals=$signals
 comparisonSummary=$summary
 observations=@($obs)
 warnings=@($warnings)
}
$exports=[ordered]@{};$exportEvents=if($Weapon -eq 'all'){$events}else{@($events|?{$_.Type -eq 'session' -or $_.Type -like 'dummy_*' -or $_.Type -like 'target_*' -or (M $_)})};if($ExportJson -or $ExportCsv){$out=if([string]::IsNullOrWhiteSpace($OutputDir)){Get-WeaponDebugReportsRoot}else{Get-FullPath -Path $OutputDir};Ensure-Directory -Path $out;$stamp=Get-Date -Format 'yyyyMMdd-HHmmss';if($ExportJson){$j=Join-Path $out ('weapon-report-'+$stamp+'.json');$report|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $j -Encoding UTF8;$exports.json=$j};if($ExportCsv){$c=Join-Path $out ('weapon-events-'+$stamp+'.csv');$exportEvents|Export-Csv -LiteralPath $c -NoTypeInformation -Encoding UTF8;$exports.csv=$c}}
Write-Host 'Weapon log analysis'
Write-Host "  log path                 : $($log.FullName)"
Write-Host "  weapon filter            : $Weapon"
if($session){
 Write-Host "  session                  : ts=$($session.Timestamp) map=$($session.Map) game=$($session.Game) status=$($session.Status)"
 if($effectiveWeapon){Write-Host "  weapon under test        : $effectiveWeapon"}
 if($glockProfile){Write-Host "  glock profile            : $glockProfile"}
 if($mp5Profile){Write-Host "  mp5 profile              : $mp5Profile"}
 Write-Host "  target profile           : $(if($targetProfile){$targetProfile}else{'n/a'})"
}else{
 Write-Host '  session                  : missing'
}
Write-Host "  accepted shots           : $($accepted.Count)"
Write-Host "  rejected shots           : $($rejected.Count)"
Write-Host "  hit events               : $($hits.Count)"
Write-Host "  kill events              : $($kills.Count)"
Write-Host "  headshot hits            : $($headshotHits.Count)"
Write-Host "  headshot kills           : $($headshotKills.Count)"
Write-Host "  target marks             : $($targetMarks.Count)"
Write-Host "  target unmarks           : $($targetUnmarks.Count)"
Write-Host "  target spawns            : $($dummySpawns.Count)"
Write-Host "  target spawn failures    : $($dummySpawnFailures.Count)"
Write-Host "  target clears            : $($dummyClears.Count)"
Write-Host "  target respawns          : $($dummyRespawns.Count)"
if($dummyRepositions.Count -gt 0){Write-Host "  target repositions       : $($dummyRepositions.Count)"}
if($recentTargetFailureReasons.Count -gt 0){Write-Host "  recent target failures   : $($recentTargetFailureReasons -join ' | ')"}
Write-Host "  target hits              : $($dummyHits.Count)"
Write-Host "  target kills             : $($dummyKills.Count)"
Write-Host "  target hs hits           : $($dummyHeadshotHits.Count)"
Write-Host "  target hs kills          : $($dummyHeadshotKills.Count)"
Write-Host "  armored target hits      : $($armoredDummyHits.Count)"
Write-Host "  protected hs hits        : $($protectedDummyHeadshotHits.Count)"
Write-Host "  protected hs kills       : $($protectedDummyHeadshotKills.Count)"
Write-Host "  target lethal hs evid.   : $($dummyLethal.Count)"
Write-Host "  accepted move_penalty>0  : $($movePenaltyAccepted.Count)"
if($effectiveWeapon -eq 'mp5' -or $Weapon -eq 'mp5'){Write-Host "  burst growth evidence    : $($burstGrowthAccepted.Count)"}
if($effectiveWeapon -ne 'mp5'){Write-Host "  first-shot accepted      : $($firstShotAccepted.Count)"}
Write-Host "  applied dmg min/avg/max  : $(if($appliedStats){('{0:F4} / {1:F4} / {2:F4}' -f $appliedStats.Min,$appliedStats.Average,$appliedStats.Max)}else{'n/a / n/a / n/a'})"
Write-Host "  hitgroups                : $(FHitgroups $report.stats.hitgroupCounts)"
Write-Host "  spread min/avg/max       : $(if($spreadStats){('{0:F4} / {1:F4} / {2:F4}' -f $spreadStats.Min,$spreadStats.Average,$spreadStats.Max)}else{'n/a / n/a / n/a'})"
Write-Host "  move penalty min/avg/max : $(if($moveStats){('{0:F4} / {1:F4} / {2:F4}' -f $moveStats.Min,$moveStats.Average,$moveStats.Max)}else{'n/a / n/a / n/a'})"
if($effectiveWeapon -eq 'mp5' -or $Weapon -eq 'mp5'){Write-Host "  burst add min/avg/max    : $(if($burstStats){('{0:F4} / {1:F4} / {2:F4}' -f $burstStats.Min,$burstStats.Average,$burstStats.Max)}else{'n/a / n/a / n/a'})"}
Write-Host "  speed2d min/avg/max      : $(if($speedStats){('{0:F1} / {1:F1} / {2:F1}' -f $speedStats.Min,$speedStats.Average,$speedStats.Max)}else{'n/a / n/a / n/a'})"
Write-Host ''
Write-Host 'Observations'
if($obs.Count -eq 0){Write-Host '  - No notable observations were derived from the parsed telemetry.'}else{foreach($o in $obs){Write-Host "  - $o"}}
Write-Host ''
Write-Host 'Warnings'
if($warnings.Count -eq 0){Write-Host '  - none'}else{foreach($w in $warnings){Write-Host "  - $w"}}
if($exports.Count -gt 0){
 Write-Host ''
 Write-Host 'Exports'
 foreach($k in $exports.Keys){Write-Host "  $k : $($exports[$k])"}
}
Write-Host ''
Write-Host 'Evidence only: this summarizes logged server-authoritative telemetry. It does not validate client-side recoil feel, prediction quality, or exact PvP parity.'
if($assert.Count -gt 0){
 Write-Host ''
 Write-Host 'Assertion failures'
 foreach($a in $assert){Write-Host "  - $a"}
 exit 2
}
if($PassThru){[pscustomobject]@{LogPath=$log.FullName;Report=$report;Exports=[pscustomobject]$exports}}
