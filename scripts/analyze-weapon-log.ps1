[CmdletBinding(DefaultParameterSetName='Latest')]
param(
 [Parameter(ParameterSetName='Path',Mandatory=$true)][string]$Path,
 [Parameter(ParameterSetName='Latest')][switch]$Latest,
 [ValidateSet('glock','mp5','357','shotgun','all')][string]$Weapon='all',
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
function T([object[]]$v){$f=@($v|?{$_ -ne $null});if($f.Count -eq 0){return $null};[double](($f|measure -Sum).Sum)}
function M($e){
 if($Weapon -eq 'all'){return $true}
 if(-not $e){return $false}
 $weaponName=[string]$e.Weapon
 if([string]::IsNullOrWhiteSpace($weaponName)){return $false}
 return $weaponName.ToLowerInvariant() -eq $Weapon
}
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
  Profile357Name=FF $h @('357_profile','profile357','sv_exp_357_profile_name')
  ShotgunProfileName=FF $h @('shotgun_profile','shotgunProfile','sv_exp_shotgun_profile_name')
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
  SpotName=FF $h @('spot_name','spotName')
  SpotStorage=FF $h @('spot_storage','spotStorage')
  FailureCode=FF $h @('code')
  Reason=F $h 'reason'
  Details=F $h 'details'
  HitGroup=FF $h @('hitgroup','hitGroup')
  FirstShot=B(F $h 'firstshot')
  Spread=N(F $h 'spread')
  MovementPenalty=N(F $h 'move_penalty')
  AdditionalSpread=N(FF $h @('additional_spread','burst_additional_spread','additionalSpread','burstAddedSpread'))
  BurstAddedSpread=N(FF $h @('burst_additional_spread','burstAddedSpread'))
  PatternMode=B(FF $h @('pattern_mode','patternMode'))
  PatternIndex=I(FF $h @('pattern_index','patternIndex'))
  PatternOffsetX=N(FF $h @('pattern_offset_x','patternOffsetX'))
  PatternOffsetY=N(FF $h @('pattern_offset_y','patternOffsetY'))
  PatternReset=B(FF $h @('pattern_reset','patternReset'))
  BurstReset=B(FF $h @('burst_reset','burstReset'))
  TotalAdditionalSpread=N(FF $h @('total_additional_spread','totalAdditionalSpread'))
  MovementContribution=N(FF $h @('movement_contribution','movementContribution'))
  AirContribution=N(FF $h @('air_contribution','airContribution'))
  CrouchBonus=N(FF $h @('crouch_bonus','crouchBonus'))
  CadenceGrowthContribution=N(FF $h @('cadence_growth_contribution','cadenceGrowthContribution'))
  CadenceMode=B(FF $h @('cadence_mode','cadenceMode'))
  CadenceInterval=N(FF $h @('cadence_interval','cadenceInterval'))
  CadencePenalty=N(FF $h @('cadence_penalty','cadencePenalty'))
  CadenceReset=B(FF $h @('cadence_reset','cadenceReset'))
  HoldPenalty=N(FF $h @('hold_penalty','holdPenalty'))
  NextAdditionalSpread=N(FF $h @('next_additional_spread','nextAdditionalSpread'))
  PatternContribution=N(FF $h @('pattern_contribution','patternContribution'))
  CadenceContribution=N(FF $h @('cadence_contribution','cadenceContribution'))
  RecoveryApplied=N(FF $h @('recovery_applied','recoveryApplied'))
  ShotGrowth=N(FF $h @('shot_growth','burst_growth','shotGrowth'))
  ShotIndex=I(FF $h @('shot_index','shotIndex','burst_index','burstIndex'))
  BurstIndex=I(FF $h @('burst_index','burstIndex'))
  SpeedRatio=N(FF $h @('speed_ratio','speedRatio'))
  PelletsPlanned=I(FF $h @('pellets_planned','pellets'))
  PelletsHit=I(F $h 'pellets_hit')
  HeadshotPellets=I(F $h 'headshot_pellets')
  HorizontalSpeed=N(F $h 'speed2d')
  MaxSpeed=N(F $h 'maxspeed')
  Grounded=B(F $h 'grounded')
  Ducking=B(F $h 'ducking')
  HealthBefore=N(F $h 'health_before')
  HealthAfter=N(F $h 'health_after')
  AppliedDamage=N(F $h 'applied_damage')
  ArmorBefore=N(F $h 'armor_before')
  ArmorAfter=N(F $h 'armor_after')
  ArmorDamage=N(F $h 'armor_damage')
  DamageRaw=N(F $h 'damage_raw')
  DamageToHealth=N(F $h 'damage_to_health')
  DamageAbsorbed=N(F $h 'damage_absorbed')
  ArmorDrain=N(F $h 'armor_drain')
  Headshot=B(F $h 'headshot')
  HeadshotLethalApplied=B(F $h 'headshot_lethal_applied')
  VictimKind=FF $h @('victim_kind','victimKind')
  VictimClass=FF $h @('victim_class','victimClass')
  VictimIsPlayer=B(FF $h @('victim_is_player','victimIsPlayer'))
  VictimIsFake=B(FF $h @('victim_is_fake','victimIsFake'))
  ArmorApplied=B(FF $h @('armor_applied','armorApplied'))
  HelmetEquipped=B(FF $h @('helmet_equipped','helmetEquipped'))
  HeadProtectionActive=B(FF $h @('head_protection_active','head_protection','headProtected','head_protected'))
  ArmorHitProtected=B(FF $h @('armor_hit_protected','armorHitProtected'))
  ArmorModel=FF $h @('armor_model','armorModel')
  HeadProtected=B(FF $h @('head_protected','headProtectionActive','headProtected'))
  DummyArmorBefore=N(FF $h @('dummy_armor_before','dummyArmorBefore'))
  Raw=$l
 }}
$log=Resolve-LogFile;$events=@();$warnings=New-Object Collections.ArrayList;$obs=New-Object Collections.ArrayList;$assert=New-Object Collections.ArrayList;$i=0;foreach($line in Get-Content -LiteralPath $log.FullName){$i++;$e=P $line $i;if($e -and $e.Type -ne 'unknown'){$events+=$e}elseif($line.Trim()){[void]$warnings.Add("Skipped non-telemetry line $i because it did not match the expected single-line key=value format.")}}
$session=@($events|?{$_.Type -eq 'session'}|select -first 1);$session=if($session.Count){$session[0]}else{$null};$effectiveWeapon=if($Weapon -ne 'all'){$Weapon}else{$session.WeaponUnderTest}
$accepted=@($events|?{$_.Type -eq 'accepted' -and (M $_)})
$rejected=@($events|?{$_.Type -eq 'rejected' -and (M $_)})
$hits=@($events|?{$_.Type -eq 'hit' -and (M $_)})
$kills=@($events|?{$_.Type -eq 'kill' -and (M $_)})
$dummySpawns=@($events|?{$_.Type -eq 'dummy_spawn'})
$dummyRespawns=@($events|?{$_.Type -eq 'dummy_respawn'})
$dummySpawnFailures=@($events|?{$_.Type -eq 'dummy_spawn_failed'})
$targetMarks=@($events|?{$_.Type -eq 'target_mark'})
$targetUnmarks=@($events|?{$_.Type -eq 'target_unmark'})
$targetUseSaved=@($events|?{$_.Type -eq 'target_use_saved'})
$dummyClears=@($events|?{$_.Type -eq 'dummy_clear'})
$dummyRepositions=@($events|?{$_.Type -eq 'dummy_reposition'})
$headshotHits=@($hits|?{$_.Headshot -eq $true})
$headshotKills=@($kills|?{$_.Headshot -eq $true})
$lethal=@($hits|?{$_.HeadshotLethalApplied -eq $true})
if($lethal.Count -eq 0){$lethal=@($kills|?{$_.HeadshotLethalApplied -eq $true})}
$dummyHits=@($hits|?{D $_})
$dummyKills=@($kills|?{D $_})
$dummyHeadshotHits=@($dummyHits|?{$_.Headshot -eq $true})
$dummyHeadshotKills=@($dummyKills|?{$_.Headshot -eq $true})
$dummyLethal=@($dummyHits|?{$_.HeadshotLethalApplied -eq $true})
if($dummyLethal.Count -eq 0){$dummyLethal=@($dummyKills|?{$_.HeadshotLethalApplied -eq $true})}
$armoredDummyHits=@($dummyHits|?{A $_})
$protectedDummyHeadshotHits=@($dummyHits|?{$_.Headshot -eq $true -and $_.HeadProtected -eq $true})
$protectedDummyHeadshotKills=@($dummyKills|?{$_.Headshot -eq $true -and $_.HeadProtected -eq $true})
$firstShotAccepted=@($accepted|?{$_.FirstShot -eq $true})
$movePenaltyAccepted=@($accepted|?{$_.MovementPenalty -gt 0})
$burstGrowthAccepted=@($accepted|?{$_.AdditionalSpread -gt 0 -or $_.ShotIndex -gt 1 -or $_.RecoveryApplied -gt 0})
$burstResetAccepted=@($accepted|?{$_.BurstReset -eq $true})
$cadenceAccepted=@($accepted|?{$_.CadenceMode -eq $true})
$cadencePenaltyAccepted=@($accepted|?{$_.CadenceMode -eq $true -and (($_.CadencePenalty -gt 0) -or ($_.CadenceReset -eq $true) -or ($_.ShotIndex -gt 1))})
$cadenceResetAccepted=@($accepted|?{$_.CadenceReset -eq $true})
$patternAccepted=@($accepted|?{$_.PatternMode -eq $true})
$patternResetAccepted=@($accepted|?{$_.PatternReset -eq $true})
$standing=@($accepted|?{$_.Grounded -eq $true -and $_.Ducking -eq $false -and $_.MovementPenalty -ne $null -and $_.HorizontalSpeed -gt 0 -and $_.MaxSpeed -gt 0}|%{$_.MovementPenalty/([Math]::Min([Math]::Max(($_.HorizontalSpeed/$_.MaxSpeed),0.0),1.0))})
$crouch=@($accepted|?{$_.Grounded -eq $true -and $_.Ducking -eq $true -and $_.MovementPenalty -ne $null -and $_.HorizontalSpeed -gt 0 -and $_.MaxSpeed -gt 0}|%{$_.MovementPenalty/([Math]::Min([Math]::Max(($_.HorizontalSpeed/$_.MaxSpeed),0.0),1.0))})
$crouchMoveEvidence=($standing.Count -gt 0 -and $crouch.Count -gt 0 -and ($crouch|measure -Average).Average -lt ($standing|measure -Average).Average)
$recoveryReturnedFirstShot=$false
if($effectiveWeapon -ne 'mp5'){
 $seen=$false
 foreach($a in $accepted){
  if($a.FirstShot -eq $false){$seen=$true;continue}
  if($seen -and $a.FirstShot -eq $true){$recoveryReturnedFirstShot=$true;break}
 }
}
$targetSpawnLifecycle=@($dummySpawns+$dummyRespawns)
$savedSpotUsage=@($targetSpawnLifecycle|?{$_.Source -eq 'saved_spot'})
$targetSpawnBySource=@($targetSpawnLifecycle|Group-Object Source|Sort-Object Name|ForEach-Object{[ordered]@{Name=$(if([string]::IsNullOrWhiteSpace($_.Name)){'unknown'}else{$_.Name});Count=$_.Count}})
$playerHits=@($hits|?{$_.VictimKind -eq 'player' -or $_.VictimKind -eq 'fake_client'})
$playerKills=@($kills|?{$_.VictimKind -eq 'player' -or $_.VictimKind -eq 'fake_client'})
$fakePlayerHits=@($playerHits|?{$_.VictimKind -eq 'fake_client' -or $_.VictimIsFake -eq $true})
$realPlayerHits=@($playerHits|?{$_.VictimKind -eq 'player' -and $_.VictimIsFake -ne $true})
$playerHeadshotHits=@($playerHits|?{$_.Headshot -eq $true})
$playerHeadshotKills=@($playerKills|?{$_.Headshot -eq $true})
$helmetProtectedPlayerHeadshotHits=@($playerHeadshotHits|?{$_.HeadProtectionActive -eq $true})
$unprotectedPlayerHeadshotHits=@($playerHeadshotHits|?{$_.HeadProtectionActive -ne $true})
$directPlayerArmorEvidenceHits=@($playerHits|?{$_.ArmorBefore -ne $null -and $_.ArmorAfter -ne $null -and $_.DamageRaw -ne $null -and $_.DamageToHealth -ne $null})
$playerArmorAbsorbedTotal=T($playerHits|%{$_.DamageAbsorbed})
$playerArmorDrainTotal=T($playerHits|%{$_.ArmorDrain})
$spreadStats=S($accepted|%{$_.Spread})
$moveStats=S($accepted|%{$_.MovementPenalty})
$speedStats=S($accepted|%{$_.HorizontalSpeed})
$additionalStats=S($accepted|%{$_.AdditionalSpread})
$recoveryStats=S($accepted|%{$_.RecoveryApplied})
$shotGrowthStats=S($accepted|%{$_.ShotGrowth})
$burstStats=S($accepted|%{$_.BurstAddedSpread})
$nextAdditionalSpreadStats=S($accepted|%{$_.NextAdditionalSpread})
$cadenceIntervalStats=S($cadenceAccepted|%{$_.CadenceInterval})
$cadencePenaltyStats=S($cadenceAccepted|%{$_.CadencePenalty})
$holdPenaltyStats=S($accepted|%{$_.HoldPenalty})
$movementContributionStats=S($accepted|%{$_.MovementContribution})
$airContributionStats=S($accepted|%{$_.AirContribution})
$crouchBonusStats=S($accepted|%{$_.CrouchBonus})
$patternContributionStats=S($accepted|%{$_.PatternContribution})
$cadenceContributionStats=S($accepted|%{$_.CadenceContribution})
$patternIndexStats=S($patternAccepted|%{$_.PatternIndex})
$patternOffsetXStats=S($patternAccepted|%{$_.PatternOffsetX})
$patternOffsetYStats=S($patternAccepted|%{$_.PatternOffsetY})
$mp5BurstAccepted=@($accepted|?{$_.Weapon -eq 'mp5' -and $_.BurstIndex -ne $null})
$mp5ShortBurstAccepted=@($mp5BurstAccepted|?{$_.BurstIndex -ge 2 -and $_.BurstIndex -le 5})
$mp5LongBurstAccepted=@($mp5BurstAccepted|?{$_.BurstIndex -ge 6})
$mp5ShortBurstSpreadStats=S($mp5ShortBurstAccepted|%{$_.NextAdditionalSpread})
$mp5LongBurstSpreadStats=S($mp5LongBurstAccepted|%{$_.NextAdditionalSpread})
$appliedStats=S($hits|%{$_.AppliedDamage})
$damageToHealthStats=S($hits|%{$_.DamageToHealth})
$damageAbsorbedStats=S($hits|%{$_.DamageAbsorbed})
$armorDrainStats=S($hits|%{$_.ArmorDrain})
$pelletPlanStats=S($accepted|%{$_.PelletsPlanned})
$pelletHitStats=S($hits|%{$_.PelletsHit})
$headshotPelletStats=S($hits|%{$_.HeadshotPellets})
$healthDeltaConsistentHits=@($hits|?{$_.HealthBefore -ne $null -and $_.HealthAfter -ne $null -and $_.AppliedDamage -ne $null -and [Math]::Abs((($_.HealthBefore - $_.HealthAfter) - $_.AppliedDamage)) -le 0.11})
$damageModelConsistentHits=@($hits|?{$_.AppliedDamage -ne $null -and $_.DamageToHealth -ne $null -and [Math]::Abs(($_.AppliedDamage - $_.DamageToHealth)) -le 0.11})
$fullyConsistentHits=@($hits|?{$_.HealthBefore -ne $null -and $_.HealthAfter -ne $null -and $_.AppliedDamage -ne $null -and $_.DamageToHealth -ne $null -and [Math]::Abs((($_.HealthBefore - $_.HealthAfter) - $_.AppliedDamage)) -le 0.11 -and [Math]::Abs(($_.AppliedDamage - $_.DamageToHealth)) -le 0.11})
$healthDeltaConsistentKills=@($kills|?{$_.HealthBefore -ne $null -and $_.HealthAfter -ne $null -and $_.AppliedDamage -ne $null -and [Math]::Abs((($_.HealthBefore - $_.HealthAfter) - $_.AppliedDamage)) -le 0.11})
$damageModelConsistentKills=@($kills|?{$_.AppliedDamage -ne $null -and $_.DamageToHealth -ne $null -and [Math]::Abs(($_.AppliedDamage - $_.DamageToHealth)) -le 0.11})
$fullyConsistentKills=@($kills|?{$_.HealthBefore -ne $null -and $_.HealthAfter -ne $null -and $_.AppliedDamage -ne $null -and $_.DamageToHealth -ne $null -and [Math]::Abs((($_.HealthBefore - $_.HealthAfter) - $_.AppliedDamage)) -le 0.11 -and [Math]::Abs(($_.AppliedDamage - $_.DamageToHealth)) -le 0.11})
$recentTargetFailureReasons=@($dummySpawnFailures|Select-Object -Last 3|ForEach-Object{if($_.FailureCode -and $_.Reason){'{0}: {1}' -f $_.FailureCode,$_.Reason}elseif($_.Reason){$_.Reason}elseif($_.FailureCode){$_.FailureCode}else{'unknown target spawn failure'}})
$firstTarget=@($dummySpawns+$dummyHits+$dummyKills|?{$_.TargetProfileName}|select -first 1)
$targetProfile=if($session.TargetProfileName){$session.TargetProfileName}elseif($firstTarget.Count){$firstTarget[0].TargetProfileName}else{$null}
$isGlockSession=(($session.WeaponUnderTest -eq 'glock') -or $effectiveWeapon -eq 'glock')
$isMp5Session=(($session.WeaponUnderTest -eq 'mp5') -or $effectiveWeapon -eq 'mp5')
$is357Session=(($session.WeaponUnderTest -eq '357') -or $effectiveWeapon -eq '357')
$isShotgunSession=(($session.WeaponUnderTest -eq 'shotgun') -or $effectiveWeapon -eq 'shotgun')
$glockAccepted=@($accepted|?{$_.Weapon -eq 'glock' -and $_.ProfileName}|select -first 1)
$glockProfile=if($isGlockSession -and $session.GlockProfileName){$session.GlockProfileName}elseif($isGlockSession -and $session.ProfileName){$session.ProfileName}elseif((-not $isMp5Session) -and $glockAccepted.Count){$glockAccepted[0].ProfileName}else{$null}
$mp5Accepted=@($accepted|?{$_.Weapon -eq 'mp5' -and $_.ProfileName}|select -first 1)
$mp5Profile=if($isMp5Session -and $session.Mp5ProfileName){$session.Mp5ProfileName}elseif($isMp5Session -and $session.ProfileName){$session.ProfileName}elseif((-not $isGlockSession) -and $mp5Accepted.Count){$mp5Accepted[0].ProfileName}else{$null}
$accepted357=@($accepted|?{$_.Weapon -eq '357' -and $_.ProfileName}|select -first 1)
$profile357=if($is357Session -and $session.Profile357Name){$session.Profile357Name}elseif($is357Session -and $session.ProfileName){$session.ProfileName}elseif($accepted357.Count){$accepted357[0].ProfileName}else{$null}
$shotgunAccepted=@($accepted|?{$_.Weapon -eq 'shotgun' -and $_.ProfileName}|select -first 1)
$shotgunProfile=if($isShotgunSession -and $session.ShotgunProfileName){$session.ShotgunProfileName}elseif($isShotgunSession -and $session.ProfileName){$session.ProfileName}elseif($shotgunAccepted.Count){$shotgunAccepted[0].ProfileName}else{$null}
$weaponProfile=if($effectiveWeapon -eq 'mp5'){if($mp5Profile){$mp5Profile}else{$glockProfile}}elseif($effectiveWeapon -eq 'glock'){if($glockProfile){$glockProfile}else{$mp5Profile}}elseif($effectiveWeapon -eq '357'){if($profile357){$profile357}else{$session.ProfileName}}elseif($effectiveWeapon -eq 'shotgun'){if($shotgunProfile){$shotgunProfile}else{$session.ProfileName}}elseif($shotgunProfile){$shotgunProfile}elseif($profile357){$profile357}elseif($mp5Profile){$mp5Profile}elseif($glockProfile){$glockProfile}elseif($session.ProfileName){$session.ProfileName}else{$null}
if(-not $session){[void]$warnings.Add('No session header line was parsed. The analyzer can still summarize event telemetry, but launch metadata is missing.')} ; if($accepted.Count){[void]$obs.Add("Parsed $($accepted.Count) accepted event(s).")}else{[void]$warnings.Add('No accepted weapon telemetry matched the current filter.')}; if($hits.Count){[void]$obs.Add("Parsed $($hits.Count) hit event(s).")}else{[void]$warnings.Add('No hit telemetry matched the current filter.')}; if($movePenaltyAccepted.Count){[void]$obs.Add("Movement-penalty evidence exists ($($movePenaltyAccepted.Count)).")}else{[void]$warnings.Add('No accepted event with move_penalty > 0 matched the current filter.')}; if(($effectiveWeapon -eq 'glock' -or $effectiveWeapon -eq 'mp5') -and -not $burstGrowthAccepted.Count){[void]$warnings.Add('No cadence-growth or recovery evidence was logged for this selection.')} ; if(($effectiveWeapon -eq 'glock' -or $effectiveWeapon -eq '357') -and -not $cadencePenaltyAccepted.Count){[void]$warnings.Add('No cadence penalty or cadence reset evidence was logged for this pistol selection.')} ; if($cadenceAccepted.Count){[void]$obs.Add("Cadence telemetry was present ($($cadenceAccepted.Count)).")} ; if($dummySpawns.Count -or $dummyRespawns.Count -or $dummyClears.Count -or $dummyRepositions.Count -or $dummySpawnFailures.Count -or $targetMarks.Count -or $targetUnmarks.Count -or $targetUseSaved.Count){[void]$obs.Add("Target lifecycle telemetry was present ($($dummySpawns.Count) spawn, $($dummyRespawns.Count) respawn, $($dummySpawnFailures.Count) spawn_failed, $($dummyClears.Count) clear, $($dummyRepositions.Count) reposition, $($targetMarks.Count) mark, $($targetUnmarks.Count) unmark, $($targetUseSaved.Count) use_saved).")} ; if($dummySpawnFailures.Count){[void]$obs.Add("Target spawn failures were logged ($($dummySpawnFailures.Count)).")} ; if($savedSpotUsage.Count){[void]$obs.Add("Saved-spot spawn usage was logged ($($savedSpotUsage.Count)).")} ; if($dummyHeadshotKills.Count){[void]$obs.Add("Target headshot kill evidence exists ($($dummyHeadshotKills.Count)).")}
if($playerHits.Count){[void]$obs.Add("Parsed $($playerHits.Count) direct player/fake-player hit event(s).")}
if($realPlayerHits.Count){[void]$obs.Add("Real-player hit telemetry was present ($($realPlayerHits.Count)).")}
if($fakePlayerHits.Count){[void]$obs.Add("Fake verification client hit telemetry was present ($($fakePlayerHits.Count)).")}
if($playerHeadshotHits.Count){[void]$obs.Add("Player/fake-player headshot hit evidence exists ($($playerHeadshotHits.Count)).")}
if($playerHeadshotKills.Count){[void]$obs.Add("Player/fake-player headshot kill evidence exists ($($playerHeadshotKills.Count)).")}
if($helmetProtectedPlayerHeadshotHits.Count){[void]$obs.Add("Helmet-protected headshot evidence exists ($($helmetProtectedPlayerHeadshotHits.Count)).")}
if($unprotectedPlayerHeadshotHits.Count){[void]$obs.Add("Unprotected headshot evidence exists ($($unprotectedPlayerHeadshotHits.Count)).")}
if($directPlayerArmorEvidenceHits.Count){
 [void]$obs.Add("Direct player armor telemetry exists ($($directPlayerArmorEvidenceHits.Count)) with absorbed total $(if($playerArmorAbsorbedTotal -ne $null){('{0:F4}' -f $playerArmorAbsorbedTotal)}else{'n/a'}).")
}elseif($playerHits.Count){
 [void]$warnings.Add('Player/fake-player hit telemetry exists, but no hit carried full direct armor-before/after and damage breakdown fields.')
}
if(($effectiveWeapon -eq 'mp5' -or $Weapon -eq 'mp5') -and $mp5ShortBurstSpreadStats -and $mp5LongBurstSpreadStats){
 if($mp5LongBurstSpreadStats.Average -gt $mp5ShortBurstSpreadStats.Average){
  [void]$obs.Add(("MP5 spray progression is visible: longer sprays carried higher next additional spread than short bursts ({0:F4} -> {1:F4})." -f $mp5ShortBurstSpreadStats.Average,$mp5LongBurstSpreadStats.Average))
 }else{
  [void]$warnings.Add(("MP5 short-burst vs long-spray next additional spread did not separate cleanly in this log ({0:F4} vs {1:F4})." -f $mp5ShortBurstSpreadStats.Average,$mp5LongBurstSpreadStats.Average))
 }
}
if($hits.Count -gt 0){
 if($fullyConsistentHits.Count -eq $hits.Count){
  [void]$obs.Add("Hit telemetry consistency check passed ($($fullyConsistentHits.Count)/$($hits.Count)).")
 }else{
  [void]$warnings.Add("Some hit lines have mismatched health delta vs applied_damage or damage_to_health ($($fullyConsistentHits.Count)/$($hits.Count) fully consistent).")
 }
}
if($kills.Count -gt 0){
 if($fullyConsistentKills.Count -eq $kills.Count){
  [void]$obs.Add("Kill telemetry consistency check passed ($($fullyConsistentKills.Count)/$($kills.Count)).")
 }else{
  [void]$warnings.Add("Some kill lines have mismatched health delta vs applied_damage or damage_to_health ($($fullyConsistentKills.Count)/$($kills.Count) fully consistent).")
 }
}
$signals=[ordered]@{acceptedShots=($accepted.Count -gt 0);tapFireHoldRejections=($rejected.Count -gt 0);firstShotAccepted=($firstShotAccepted.Count -gt 0);movementPenaltyPositive=($movePenaltyAccepted.Count -gt 0);recoveryReturnedFirstShot=$recoveryReturnedFirstShot;crouchMovePenaltyReductionCandidate=$crouchMoveEvidence;hitsPresent=($hits.Count -gt 0);killsPresent=($kills.Count -gt 0);headshotHitsPresent=($headshotHits.Count -gt 0);headshotKillsPresent=($headshotKills.Count -gt 0);lethalHeadshotEvidencePresent=($lethal.Count -gt 0);dummySpawnsPresent=($dummySpawns.Count -gt 0);dummyRespawnsPresent=($dummyRespawns.Count -gt 0);dummyHitsPresent=($dummyHits.Count -gt 0);dummyKillsPresent=($dummyKills.Count -gt 0);dummyHeadshotHitsPresent=($dummyHeadshotHits.Count -gt 0);dummyHeadshotKillsPresent=($dummyHeadshotKills.Count -gt 0);armoredDummyHitsPresent=($armoredDummyHits.Count -gt 0);protectedDummyHeadshotHitsPresent=($protectedDummyHeadshotHits.Count -gt 0);protectedDummyHeadshotKillsPresent=($protectedDummyHeadshotKills.Count -gt 0);dummyLethalHeadshotEvidencePresent=($dummyLethal.Count -gt 0);weaponAcceptedPresent=($accepted.Count -gt 0);weaponHitsPresent=($hits.Count -gt 0);weaponKillsPresent=($kills.Count -gt 0);weaponHeadshotKillsPresent=($headshotKills.Count -gt 0);burstGrowthEvidencePresent=($burstGrowthAccepted.Count -gt 0);cadenceModeEvidencePresent=($cadenceAccepted.Count -gt 0);cadencePenaltyEvidencePresent=($cadencePenaltyAccepted.Count -gt 0);cadenceResetEvidencePresent=($cadenceResetAccepted.Count -gt 0);movementPenaltyEvidencePresent=($movePenaltyAccepted.Count -gt 0);patternEvidencePresent=($patternAccepted.Count -gt 0);patternResetEvidencePresent=($patternResetAccepted.Count -gt 0)}
$signals.playerHitsPresent=($playerHits.Count -gt 0)
$signals.realPlayerHitsPresent=($realPlayerHits.Count -gt 0)
$signals.fakePlayerHitsPresent=($fakePlayerHits.Count -gt 0)
$signals.playerKillsPresent=($playerKills.Count -gt 0)
$signals.playerHeadshotHitsPresent=($playerHeadshotHits.Count -gt 0)
$signals.playerHeadshotKillsPresent=($playerHeadshotKills.Count -gt 0)
$signals.helmetProtectedPlayerHeadshotHitsPresent=($helmetProtectedPlayerHeadshotHits.Count -gt 0)
$signals.unprotectedPlayerHeadshotHitsPresent=($unprotectedPlayerHeadshotHits.Count -gt 0)
$signals.directPlayerArmorEvidencePresent=($directPlayerArmorEvidenceHits.Count -gt 0)
$missing=New-Object Collections.ArrayList; if(-not $signals.movementPenaltyPositive){[void]$missing.Add('No accepted shot with move_penalty > 0 was logged for this step.')}; if(($effectiveWeapon -eq 'glock' -or $effectiveWeapon -eq 'mp5') -and -not $signals.burstGrowthEvidencePresent){[void]$missing.Add('No cadence-growth or recovery evidence was logged for this weapon step.')}; if(-not $signals.dummyHeadshotHitsPresent){[void]$missing.Add('No dummy headshot hit evidence was logged for this step.')}; if(-not $signals.armoredDummyHitsPresent){[void]$missing.Add('No armored dummy hit evidence was logged for this step.')}; if(-not $signals.protectedDummyHeadshotHitsPresent){[void]$missing.Add('No protected-head dummy headshot evidence was logged for this step.')}; if(-not ($signals.dummyLethalHeadshotEvidencePresent -or $signals.lethalHeadshotEvidencePresent)){[void]$missing.Add('No lethal-headshot evidence was logged for this step.')}
if($RequireAccepted -and -not $signals.acceptedShots){[void]$assert.Add('Required signal missing: accepted weapon telemetry lines.')}; if($RequireRejections -and -not $signals.tapFireHoldRejections){[void]$assert.Add('Required signal missing: rejection telemetry lines.')}; if($RequireFirstShot -and -not $signals.firstShotAccepted){[void]$assert.Add('Required signal missing: accepted events with firstshot=1.')}; if(($RequireMovePenalty -or $RequireMovementPenaltyEvidence) -and -not $signals.movementPenaltyPositive){[void]$assert.Add('Required signal missing: accepted events with move_penalty > 0.')}; if($RequireCrouchMoveEvidence -and -not $signals.crouchMovePenaltyReductionCandidate){[void]$assert.Add('Required signal missing: crouch-moving accepted shots suggesting lower normalized movement penalty than standing movement.')}; if(($RequireHits -or $RequireWeaponHits) -and -not $signals.hitsPresent){[void]$assert.Add('Required signal missing: weapon hit telemetry lines.')}; if(($RequireKills -or $RequireWeaponKills) -and -not $signals.killsPresent){[void]$assert.Add('Required signal missing: weapon kill telemetry lines.')}; if(($RequireHeadshotKills -or $RequireWeaponHeadshotKills) -and -not $signals.headshotKillsPresent){[void]$assert.Add('Required signal missing: weapon headshot kill telemetry lines.')}; if($RequireLethalHeadshotEvidence -and -not $signals.lethalHeadshotEvidencePresent){[void]$assert.Add('Required signal missing: explicit lethal-headshot evidence via headshot_lethal_applied=1.')}; if($RequireDummySpawns -and -not $signals.dummySpawnsPresent){[void]$assert.Add('Required signal missing: lab dummy spawn lifecycle lines.')}; if($RequireDummyHits -and -not $signals.dummyHitsPresent){[void]$assert.Add('Required signal missing: hit telemetry against the lab dummy.')}; if($RequireDummyHeadshotHits -and -not $signals.dummyHeadshotHitsPresent){[void]$assert.Add('Required signal missing: dummy headshot hit telemetry.')}; if($RequireDummyHeadshotKills -and -not $signals.dummyHeadshotKillsPresent){[void]$assert.Add('Required signal missing: dummy headshot kill telemetry.')}; if($RequireArmoredDummyHits -and -not $signals.armoredDummyHitsPresent){[void]$assert.Add('Required signal missing: armored dummy hit telemetry.')}; if($RequireProtectedDummyHeadshotHits -and -not $signals.protectedDummyHeadshotHitsPresent){[void]$assert.Add('Required signal missing: protected-head dummy headshot hit telemetry.')}; if($RequireProtectedDummyHeadshotKills -and -not $signals.protectedDummyHeadshotKillsPresent){[void]$assert.Add('Required signal missing: protected-head dummy headshot kill telemetry.')}; if($RequireDummyLethalHeadshotEvidence -and -not $signals.dummyLethalHeadshotEvidencePresent){[void]$assert.Add('Required signal missing: explicit lethal-headshot evidence against the dummy via headshot_lethal_applied=1.')}; if($RequireWeaponAccepted -and -not $signals.weaponAcceptedPresent){[void]$assert.Add('Required signal missing: accepted telemetry for the selected weapon filter.')}; if($RequireBurstGrowthEvidence -and -not $signals.burstGrowthEvidencePresent){[void]$assert.Add('Required signal missing: cadence-growth or recovery evidence for the selected weapon filter.')}
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
 profile357=$profile357
 shotgunProfile=$shotgunProfile
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
 profile357=$meta.profile357
 shotgunProfile=$meta.shotgunProfile
 labTargetProfile=$meta.labTargetProfile
 acceptedShotCount=$accepted.Count
 rejectedShotCount=$rejected.Count
 hitCount=$hits.Count
 killCount=$kills.Count
 targetMarkCount=$targetMarks.Count
 targetUnmarkCount=$targetUnmarks.Count
 targetUseSavedCount=$targetUseSaved.Count
 targetSpawnCount=$dummySpawns.Count
 targetSpawnFailureCount=$dummySpawnFailures.Count
 targetClearCount=$dummyClears.Count
 targetRespawnCount=$dummyRespawns.Count
 targetRepositionCount=$dummyRepositions.Count
 targetSavedSpotUsageCount=$savedSpotUsage.Count
 targetSpawnBySource=@($targetSpawnBySource)
 targetHitCount=$dummyHits.Count
 targetKillCount=$dummyKills.Count
 targetHeadshotHitCount=$dummyHeadshotHits.Count
 targetHeadshotKillCount=$dummyHeadshotKills.Count
 playerHitCount=$playerHits.Count
 realPlayerHitCount=$realPlayerHits.Count
 fakePlayerHitCount=$fakePlayerHits.Count
 playerKillCount=$playerKills.Count
 playerHeadshotHitCount=$playerHeadshotHits.Count
 playerHeadshotKillCount=$playerHeadshotKills.Count
 helmetProtectedPlayerHeadshotHitCount=$helmetProtectedPlayerHeadshotHits.Count
 unprotectedPlayerHeadshotHitCount=$unprotectedPlayerHeadshotHits.Count
 directPlayerArmorEvidenceCount=$directPlayerArmorEvidenceHits.Count
 inferredPlayerArmorEvidenceCount=([Math]::Max($playerHits.Count - $directPlayerArmorEvidenceHits.Count,0))
 playerArmorAbsorbedDamageTotal=$playerArmorAbsorbedTotal
 playerArmorDrainTotal=$playerArmorDrainTotal
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
 burstResetCount=$burstResetAccepted.Count
 cadenceGrowthEvidenceCount=$burstGrowthAccepted.Count
 cadenceEvidenceCount=$cadencePenaltyAccepted.Count
 cadenceResetCount=$cadenceResetAccepted.Count
 movementPenaltyEvidenceCount=$movePenaltyAccepted.Count
 patternEvidenceCount=$patternAccepted.Count
 patternResetCount=$patternResetAccepted.Count
 hitHealthDeltaConsistentCount=$healthDeltaConsistentHits.Count
 hitDamageModelConsistentCount=$damageModelConsistentHits.Count
 hitFullyConsistentCount=$fullyConsistentHits.Count
 killHealthDeltaConsistentCount=$healthDeltaConsistentKills.Count
 killDamageModelConsistentCount=$damageModelConsistentKills.Count
 killFullyConsistentCount=$fullyConsistentKills.Count
 additionalSpread=[ordered]@{
  min=$(if($additionalStats){$additionalStats.Min}else{$null})
  average=$(if($additionalStats){$additionalStats.Average}else{$null})
  max=$(if($additionalStats){$additionalStats.Max}else{$null})
 }
 recoveryApplied=[ordered]@{
  min=$(if($recoveryStats){$recoveryStats.Min}else{$null})
  average=$(if($recoveryStats){$recoveryStats.Average}else{$null})
  max=$(if($recoveryStats){$recoveryStats.Max}else{$null})
 }
 shotGrowth=[ordered]@{
  min=$(if($shotGrowthStats){$shotGrowthStats.Min}else{$null})
  average=$(if($shotGrowthStats){$shotGrowthStats.Average}else{$null})
  max=$(if($shotGrowthStats){$shotGrowthStats.Max}else{$null})
 }
 nextAdditionalSpread=[ordered]@{
  min=$(if($nextAdditionalSpreadStats){$nextAdditionalSpreadStats.Min}else{$null})
  average=$(if($nextAdditionalSpreadStats){$nextAdditionalSpreadStats.Average}else{$null})
  max=$(if($nextAdditionalSpreadStats){$nextAdditionalSpreadStats.Max}else{$null})
 }
  cadenceInterval=[ordered]@{
  min=$(if($cadenceIntervalStats){$cadenceIntervalStats.Min}else{$null})
  average=$(if($cadenceIntervalStats){$cadenceIntervalStats.Average}else{$null})
  max=$(if($cadenceIntervalStats){$cadenceIntervalStats.Max}else{$null})
 }
 cadencePenalty=[ordered]@{
  min=$(if($cadencePenaltyStats){$cadencePenaltyStats.Min}else{$null})
  average=$(if($cadencePenaltyStats){$cadencePenaltyStats.Average}else{$null})
  max=$(if($cadencePenaltyStats){$cadencePenaltyStats.Max}else{$null})
 }
 holdPenalty=[ordered]@{
  min=$(if($holdPenaltyStats){$holdPenaltyStats.Min}else{$null})
  average=$(if($holdPenaltyStats){$holdPenaltyStats.Average}else{$null})
  max=$(if($holdPenaltyStats){$holdPenaltyStats.Max}else{$null})
 }
 movementContribution=[ordered]@{
  min=$(if($movementContributionStats){$movementContributionStats.Min}else{$null})
  average=$(if($movementContributionStats){$movementContributionStats.Average}else{$null})
  max=$(if($movementContributionStats){$movementContributionStats.Max}else{$null})
 }
 airContribution=[ordered]@{
  min=$(if($airContributionStats){$airContributionStats.Min}else{$null})
  average=$(if($airContributionStats){$airContributionStats.Average}else{$null})
  max=$(if($airContributionStats){$airContributionStats.Max}else{$null})
 }
 crouchBonus=[ordered]@{
  min=$(if($crouchBonusStats){$crouchBonusStats.Min}else{$null})
  average=$(if($crouchBonusStats){$crouchBonusStats.Average}else{$null})
  max=$(if($crouchBonusStats){$crouchBonusStats.Max}else{$null})
 }
 patternIndex=[ordered]@{
  min=$(if($patternIndexStats){$patternIndexStats.Min}else{$null})
  average=$(if($patternIndexStats){$patternIndexStats.Average}else{$null})
  max=$(if($patternIndexStats){$patternIndexStats.Max}else{$null})
 }
 patternContribution=[ordered]@{
  min=$(if($patternContributionStats){$patternContributionStats.Min}else{$null})
  average=$(if($patternContributionStats){$patternContributionStats.Average}else{$null})
  max=$(if($patternContributionStats){$patternContributionStats.Max}else{$null})
 }
 cadenceContribution=[ordered]@{
  min=$(if($cadenceContributionStats){$cadenceContributionStats.Min}else{$null})
  average=$(if($cadenceContributionStats){$cadenceContributionStats.Average}else{$null})
  max=$(if($cadenceContributionStats){$cadenceContributionStats.Max}else{$null})
 }
 patternOffsetX=[ordered]@{
  min=$(if($patternOffsetXStats){$patternOffsetXStats.Min}else{$null})
  average=$(if($patternOffsetXStats){$patternOffsetXStats.Average}else{$null})
  max=$(if($patternOffsetXStats){$patternOffsetXStats.Max}else{$null})
 }
 patternOffsetY=[ordered]@{
  min=$(if($patternOffsetYStats){$patternOffsetYStats.Min}else{$null})
  average=$(if($patternOffsetYStats){$patternOffsetYStats.Average}else{$null})
  max=$(if($patternOffsetYStats){$patternOffsetYStats.Max}else{$null})
 }
 appliedDamage=[ordered]@{
  min=$(if($appliedStats){$appliedStats.Min}else{$null})
  average=$(if($appliedStats){$appliedStats.Average}else{$null})
  max=$(if($appliedStats){$appliedStats.Max}else{$null})
 }
 damageToHealth=[ordered]@{
  min=$(if($damageToHealthStats){$damageToHealthStats.Min}else{$null})
  average=$(if($damageToHealthStats){$damageToHealthStats.Average}else{$null})
  max=$(if($damageToHealthStats){$damageToHealthStats.Max}else{$null})
 }
 damageAbsorbed=[ordered]@{
  min=$(if($damageAbsorbedStats){$damageAbsorbedStats.Min}else{$null})
  average=$(if($damageAbsorbedStats){$damageAbsorbedStats.Average}else{$null})
  max=$(if($damageAbsorbedStats){$damageAbsorbedStats.Max}else{$null})
 }
 armorDrain=[ordered]@{
  min=$(if($armorDrainStats){$armorDrainStats.Min}else{$null})
  average=$(if($armorDrainStats){$armorDrainStats.Average}else{$null})
  max=$(if($armorDrainStats){$armorDrainStats.Max}else{$null})
 }
 hitTelemetryConsistency=[ordered]@{
  hitHealthDeltaConsistent=$healthDeltaConsistentHits.Count
  hitDamageModelConsistent=$damageModelConsistentHits.Count
  hitFullyConsistent=$fullyConsistentHits.Count
  killHealthDeltaConsistent=$healthDeltaConsistentKills.Count
  killDamageModelConsistent=$damageModelConsistentKills.Count
  killFullyConsistent=$fullyConsistentKills.Count
 }
 pelletsPlanned=[ordered]@{
  min=$(if($pelletPlanStats){$pelletPlanStats.Min}else{$null})
  average=$(if($pelletPlanStats){$pelletPlanStats.Average}else{$null})
  max=$(if($pelletPlanStats){$pelletPlanStats.Max}else{$null})
 }
 pelletsHit=[ordered]@{
  min=$(if($pelletHitStats){$pelletHitStats.Min}else{$null})
  average=$(if($pelletHitStats){$pelletHitStats.Average}else{$null})
  max=$(if($pelletHitStats){$pelletHitStats.Max}else{$null})
 }
 headshotPellets=[ordered]@{
  min=$(if($headshotPelletStats){$headshotPelletStats.Min}else{$null})
  average=$(if($headshotPelletStats){$headshotPelletStats.Average}else{$null})
  max=$(if($headshotPelletStats){$headshotPelletStats.Max}else{$null})
 }
 mp5ShortBurstNextAdditionalSpread=[ordered]@{
  min=$(if($mp5ShortBurstSpreadStats){$mp5ShortBurstSpreadStats.Min}else{$null})
  average=$(if($mp5ShortBurstSpreadStats){$mp5ShortBurstSpreadStats.Average}else{$null})
  max=$(if($mp5ShortBurstSpreadStats){$mp5ShortBurstSpreadStats.Max}else{$null})
 }
 mp5LongBurstNextAdditionalSpread=[ordered]@{
  min=$(if($mp5LongBurstSpreadStats){$mp5LongBurstSpreadStats.Min}else{$null})
  average=$(if($mp5LongBurstSpreadStats){$mp5LongBurstSpreadStats.Average}else{$null})
  max=$(if($mp5LongBurstSpreadStats){$mp5LongBurstSpreadStats.Max}else{$null})
 }
 evidence=[ordered]@{
  tapFireRejection=$signals.tapFireHoldRejections
  tapFireRejectionEvidence=$signals.tapFireHoldRejections
  movementPenalty=$signals.movementPenaltyPositive
  movementPenaltyEvidence=$signals.movementPenaltyPositive
  cadenceGrowth=$signals.burstGrowthEvidencePresent
  cadenceGrowthEvidence=$signals.burstGrowthEvidencePresent
  recoveryEvidence=$signals.burstGrowthEvidencePresent
  burstGrowth=$signals.burstGrowthEvidencePresent
  burstGrowthEvidence=$signals.burstGrowthEvidencePresent
  cadenceMode=$signals.cadenceModeEvidencePresent
  cadenceModeEvidence=$signals.cadenceModeEvidencePresent
  cadencePenaltyEvidence=$signals.cadencePenaltyEvidencePresent
  cadenceResetEvidence=$signals.cadenceResetEvidencePresent
  patternMode=$signals.patternEvidencePresent
  patternModeEvidence=$signals.patternEvidencePresent
  patternResetEvidence=$signals.patternResetEvidencePresent
  dummyHeadshotPath=$signals.dummyHeadshotHitsPresent
  dummyHeadshotPathEvidence=$signals.dummyHeadshotHitsPresent
  armoredDummyPath=$signals.armoredDummyHitsPresent
  armoredDummyEvidence=$signals.armoredDummyHitsPresent
  protectedHeadDummyPath=$signals.protectedDummyHeadshotHitsPresent
  protectedHeadDummyEvidence=$signals.protectedDummyHeadshotHitsPresent
  lethalHeadshotPath=($signals.dummyLethalHeadshotEvidencePresent -or $signals.lethalHeadshotEvidencePresent)
  lethalHeadshotEvidence=($signals.dummyLethalHeadshotEvidencePresent -or $signals.lethalHeadshotEvidencePresent)
  directPlayerHitTelemetry=$signals.playerHitsPresent
  directPlayerHitTelemetryEvidence=$signals.playerHitsPresent
  fakePlayerHitTelemetry=$signals.fakePlayerHitsPresent
  fakePlayerHitTelemetryEvidence=$signals.fakePlayerHitsPresent
  helmetProtectedPlayerHeadshotEvidence=$signals.helmetProtectedPlayerHeadshotHitsPresent
  unprotectedPlayerHeadshotEvidence=$signals.unprotectedPlayerHeadshotHitsPresent
  directPlayerArmorEvidence=$signals.directPlayerArmorEvidencePresent
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
  profile357Name=$profile357
  shotgunProfileName=$shotgunProfile
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
  targetUseSaved=$targetUseSaved.Count
  targetSpawns=$dummySpawns.Count
  targetSpawnFailures=$dummySpawnFailures.Count
  targetRespawns=$dummyRespawns.Count
  targetSavedSpotUsage=$savedSpotUsage.Count
  targetClears=$dummyClears.Count
 targetRepositions=$dummyRepositions.Count
 targetHits=$dummyHits.Count
 targetKills=$dummyKills.Count
 targetHeadshotHits=$dummyHeadshotHits.Count
 targetHeadshotKills=$dummyHeadshotKills.Count
  playerHits=$playerHits.Count
  realPlayerHits=$realPlayerHits.Count
  fakePlayerHits=$fakePlayerHits.Count
  playerKills=$playerKills.Count
  playerHeadshotHits=$playerHeadshotHits.Count
  playerHeadshotKills=$playerHeadshotKills.Count
  helmetProtectedPlayerHeadshotHits=$helmetProtectedPlayerHeadshotHits.Count
  unprotectedPlayerHeadshotHits=$unprotectedPlayerHeadshotHits.Count
  directPlayerArmorEvidenceHits=$directPlayerArmorEvidenceHits.Count
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
  cadenceEvidence=$cadencePenaltyAccepted.Count
  cadenceResets=$cadenceResetAccepted.Count
  patternAccepted=$patternAccepted.Count
  patternResets=$patternResetAccepted.Count
  acceptedGrounded=@($accepted|?{$_.Grounded -eq $true}).Count
  acceptedAirborne=@($accepted|?{$_.Grounded -eq $false}).Count
  acceptedDucking=@($accepted|?{$_.Ducking -eq $true}).Count
 }
 stats=[ordered]@{
  spread=$spreadStats
  movementPenalty=$moveStats
  horizontalSpeed=$speedStats
  additionalSpread=$additionalStats
  recoveryApplied=$recoveryStats
  shotGrowth=$shotGrowthStats
  burstAddedSpread=$burstStats
  cadenceInterval=$cadenceIntervalStats
  cadencePenalty=$cadencePenaltyStats
  holdPenalty=$holdPenaltyStats
  patternIndex=$patternIndexStats
  patternOffsetX=$patternOffsetXStats
  patternOffsetY=$patternOffsetYStats
  patternContribution=$patternContributionStats
  cadenceContribution=$cadenceContributionStats
  appliedDamage=$appliedStats
  damageToHealth=$damageToHealthStats
  damageAbsorbed=$damageAbsorbedStats
  armorDrain=$armorDrainStats
  hitTelemetryConsistency=[ordered]@{
   hitHealthDeltaConsistent=$healthDeltaConsistentHits.Count
   hitDamageModelConsistent=$damageModelConsistentHits.Count
   hitFullyConsistent=$fullyConsistentHits.Count
   killHealthDeltaConsistent=$healthDeltaConsistentKills.Count
   killDamageModelConsistent=$damageModelConsistentKills.Count
   killFullyConsistent=$fullyConsistentKills.Count
  }
  pelletsPlanned=$pelletPlanStats
  pelletsHit=$pelletHitStats
  headshotPellets=$headshotPelletStats
  playerArmorAbsorbedTotal=$playerArmorAbsorbedTotal
  playerArmorDrainTotal=$playerArmorDrainTotal
  targetSpawnBySource=@($targetSpawnBySource)
  hitgroupCounts=(($hits|group HitGroup|?{$_.Name}|sort Name|%{[ordered]@{Name=$_.Name;Count=$_.Count}}))
  dummyHitgroupCounts=(($dummyHits|group HitGroup|?{$_.Name}|sort Name|%{[ordered]@{Name=$_.Name;Count=$_.Count}}))
  playerHitgroupCounts=(($playerHits|group HitGroup|?{$_.Name}|sort Name|%{[ordered]@{Name=$_.Name;Count=$_.Count}}))
  fakePlayerHitgroupCounts=(($fakePlayerHits|group HitGroup|?{$_.Name}|sort Name|%{[ordered]@{Name=$_.Name;Count=$_.Count}}))
 }
 signals=$signals
 comparisonSummary=$summary
 observations=@($obs)
 warnings=@($warnings)
}
$exports=[ordered]@{};$exportEvents=if($Weapon -eq 'all'){$events}else{@($events|?{$_ -and ($_.Type -eq 'session' -or $_.Type -like 'dummy_*' -or $_.Type -like 'target_*' -or (M $_))})};if($ExportJson -or $ExportCsv){$out=if([string]::IsNullOrWhiteSpace($OutputDir)){Get-WeaponDebugReportsRoot}else{Get-FullPath -Path $OutputDir};Ensure-Directory -Path $out;$stamp=Get-Date -Format 'yyyyMMdd-HHmmss';if($ExportJson){$j=Join-Path $out ('weapon-report-'+$stamp+'.json');$report|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $j -Encoding UTF8;$exports.json=$j};if($ExportCsv){$c=Join-Path $out ('weapon-events-'+$stamp+'.csv');$exportEvents|Export-Csv -LiteralPath $c -NoTypeInformation -Encoding UTF8;$exports.csv=$c}}
Write-Host 'Weapon log analysis'
Write-Host "  log path                 : $($log.FullName)"
Write-Host "  weapon filter            : $Weapon"
if($session){
 Write-Host "  session                  : ts=$($session.Timestamp) map=$($session.Map) game=$($session.Game) status=$($session.Status)"
 if($effectiveWeapon){Write-Host "  weapon under test        : $effectiveWeapon"}
 if($glockProfile){Write-Host "  glock profile            : $glockProfile"}
 if($mp5Profile){Write-Host "  mp5 profile              : $mp5Profile"}
 if($profile357){Write-Host "  357 profile              : $profile357"}
 if($shotgunProfile){Write-Host "  shotgun profile          : $shotgunProfile"}
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
Write-Host "  target use_saved         : $($targetUseSaved.Count)"
Write-Host "  target spawns            : $($dummySpawns.Count)"
Write-Host "  target spawn failures    : $($dummySpawnFailures.Count)"
Write-Host "  target clears            : $($dummyClears.Count)"
Write-Host "  target respawns          : $($dummyRespawns.Count)"
Write-Host "  saved-spot spawns        : $($savedSpotUsage.Count)"
if($dummyRepositions.Count -gt 0){Write-Host "  target repositions       : $($dummyRepositions.Count)"}
if($targetSpawnBySource.Count -gt 0){Write-Host "  target spawn by source   : $(($targetSpawnBySource|ForEach-Object{'{0}={1}' -f $_.Name,$_.Count}) -join ', ')"}
if($recentTargetFailureReasons.Count -gt 0){Write-Host "  recent target failures   : $($recentTargetFailureReasons -join ' | ')"}
Write-Host "  target hits              : $($dummyHits.Count)"
Write-Host "  target kills             : $($dummyKills.Count)"
Write-Host "  target hs hits           : $($dummyHeadshotHits.Count)"
Write-Host "  target hs kills          : $($dummyHeadshotKills.Count)"
Write-Host "  armored target hits      : $($armoredDummyHits.Count)"
Write-Host "  protected hs hits        : $($protectedDummyHeadshotHits.Count)"
Write-Host "  protected hs kills       : $($protectedDummyHeadshotKills.Count)"
Write-Host "  target lethal hs evid.   : $($dummyLethal.Count)"
Write-Host "  player hits              : $($playerHits.Count)"
Write-Host "  real-player hits         : $($realPlayerHits.Count)"
Write-Host "  fake-player hits         : $($fakePlayerHits.Count)"
Write-Host "  player kills             : $($playerKills.Count)"
Write-Host "  player hs hits           : $($playerHeadshotHits.Count)"
Write-Host "  player hs kills          : $($playerHeadshotKills.Count)"
Write-Host "  helmeted hs hits         : $($helmetProtectedPlayerHeadshotHits.Count)"
Write-Host "  unprotected hs hits      : $($unprotectedPlayerHeadshotHits.Count)"
Write-Host "  direct player armor evid.: $($directPlayerArmorEvidenceHits.Count)"
Write-Host "  inferred player armor    : $([Math]::Max($playerHits.Count - $directPlayerArmorEvidenceHits.Count,0))"
Write-Host "  player armor absorbed    : $(if($playerArmorAbsorbedTotal -ne $null){('{0:F4}' -f $playerArmorAbsorbedTotal)}else{'n/a'})"
Write-Host "  player armor drain total : $(if($playerArmorDrainTotal -ne $null){('{0:F4}' -f $playerArmorDrainTotal)}else{'n/a'})"
Write-Host "  accepted move_penalty>0  : $($movePenaltyAccepted.Count)"
if($effectiveWeapon -eq 'glock' -or $effectiveWeapon -eq 'mp5' -or $Weapon -eq 'glock' -or $Weapon -eq 'mp5'){Write-Host "  cadence growth evidence  : $($burstGrowthAccepted.Count)"}
if($effectiveWeapon -eq 'mp5' -or $Weapon -eq 'mp5'){Write-Host "  burst resets            : $($burstResetAccepted.Count)"}
if($effectiveWeapon -eq 'glock' -or $effectiveWeapon -eq '357' -or $Weapon -eq 'glock' -or $Weapon -eq '357'){Write-Host "  cadence evidence         : $($cadencePenaltyAccepted.Count)"}
if($effectiveWeapon -eq 'glock' -or $effectiveWeapon -eq '357' -or $Weapon -eq 'glock' -or $Weapon -eq '357'){Write-Host "  cadence resets           : $($cadenceResetAccepted.Count)"}
if($effectiveWeapon -eq 'glock' -or $effectiveWeapon -eq 'mp5' -or $effectiveWeapon -eq '357' -or $effectiveWeapon -eq 'shotgun' -or $Weapon -eq 'glock' -or $Weapon -eq 'mp5' -or $Weapon -eq '357' -or $Weapon -eq 'shotgun'){Write-Host "  pattern evidence         : $($patternAccepted.Count)"}
if($effectiveWeapon -eq 'glock' -or $effectiveWeapon -eq 'mp5' -or $effectiveWeapon -eq '357' -or $effectiveWeapon -eq 'shotgun' -or $Weapon -eq 'glock' -or $Weapon -eq 'mp5' -or $Weapon -eq '357' -or $Weapon -eq 'shotgun'){Write-Host "  pattern resets           : $($patternResetAccepted.Count)"}
if($effectiveWeapon -ne 'mp5'){Write-Host "  first-shot accepted      : $($firstShotAccepted.Count)"}
Write-Host "  applied dmg min/avg/max  : $(if($appliedStats){('{0:F4} / {1:F4} / {2:F4}' -f $appliedStats.Min,$appliedStats.Average,$appliedStats.Max)}else{'n/a / n/a / n/a'})"
Write-Host "  dmg->hp min/avg/max      : $(if($damageToHealthStats){('{0:F4} / {1:F4} / {2:F4}' -f $damageToHealthStats.Min,$damageToHealthStats.Average,$damageToHealthStats.Max)}else{'n/a / n/a / n/a'})"
Write-Host "  absorbed min/avg/max     : $(if($damageAbsorbedStats){('{0:F4} / {1:F4} / {2:F4}' -f $damageAbsorbedStats.Min,$damageAbsorbedStats.Average,$damageAbsorbedStats.Max)}else{'n/a / n/a / n/a'})"
Write-Host "  armor drain min/avg/max  : $(if($armorDrainStats){('{0:F4} / {1:F4} / {2:F4}' -f $armorDrainStats.Min,$armorDrainStats.Average,$armorDrainStats.Max)}else{'n/a / n/a / n/a'})"
Write-Host "  consistent hit lines     : $($fullyConsistentHits.Count) / $($hits.Count)"
Write-Host "  consistent kill lines    : $($fullyConsistentKills.Count) / $($kills.Count)"
Write-Host "  hitgroups                : $(FHitgroups $report.stats.hitgroupCounts)"
if($playerHits.Count -gt 0){Write-Host "  player hitgroups         : $(FHitgroups $report.stats.playerHitgroupCounts)"}
if($fakePlayerHits.Count -gt 0){Write-Host "  fake-player hitgroups    : $(FHitgroups $report.stats.fakePlayerHitgroupCounts)"}
Write-Host "  spread min/avg/max       : $(if($spreadStats){('{0:F4} / {1:F4} / {2:F4}' -f $spreadStats.Min,$spreadStats.Average,$spreadStats.Max)}else{'n/a / n/a / n/a'})"
Write-Host "  move penalty min/avg/max : $(if($moveStats){('{0:F4} / {1:F4} / {2:F4}' -f $moveStats.Min,$moveStats.Average,$moveStats.Max)}else{'n/a / n/a / n/a'})"
if($effectiveWeapon -eq 'glock' -or $effectiveWeapon -eq 'mp5' -or $effectiveWeapon -eq '357' -or $effectiveWeapon -eq 'shotgun' -or $Weapon -eq 'glock' -or $Weapon -eq 'mp5' -or $Weapon -eq '357' -or $Weapon -eq 'shotgun'){Write-Host "  extra spread min/avg/max : $(if($additionalStats){('{0:F4} / {1:F4} / {2:F4}' -f $additionalStats.Min,$additionalStats.Average,$additionalStats.Max)}else{'n/a / n/a / n/a'})"}
if($effectiveWeapon -eq 'glock' -or $effectiveWeapon -eq 'mp5' -or $effectiveWeapon -eq '357' -or $effectiveWeapon -eq 'shotgun' -or $Weapon -eq 'glock' -or $Weapon -eq 'mp5' -or $Weapon -eq '357' -or $Weapon -eq 'shotgun'){Write-Host "  recovery min/avg/max     : $(if($recoveryStats){('{0:F4} / {1:F4} / {2:F4}' -f $recoveryStats.Min,$recoveryStats.Average,$recoveryStats.Max)}else{'n/a / n/a / n/a'})"}
if($effectiveWeapon -eq 'glock' -or $effectiveWeapon -eq 'mp5' -or $effectiveWeapon -eq 'shotgun' -or $Weapon -eq 'glock' -or $Weapon -eq 'mp5' -or $Weapon -eq 'shotgun'){Write-Host "  shot growth min/avg/max  : $(if($shotGrowthStats){('{0:F4} / {1:F4} / {2:F4}' -f $shotGrowthStats.Min,$shotGrowthStats.Average,$shotGrowthStats.Max)}else{'n/a / n/a / n/a'})"}
if($effectiveWeapon -eq 'glock' -or $effectiveWeapon -eq '357' -or $Weapon -eq 'glock' -or $Weapon -eq '357'){Write-Host "  cadence int min/avg/max  : $(if($cadenceIntervalStats){('{0:F4} / {1:F4} / {2:F4}' -f $cadenceIntervalStats.Min,$cadenceIntervalStats.Average,$cadenceIntervalStats.Max)}else{'n/a / n/a / n/a'})"}
if($effectiveWeapon -eq 'glock' -or $effectiveWeapon -eq '357' -or $Weapon -eq 'glock' -or $Weapon -eq '357'){Write-Host "  cadence pen min/avg/max  : $(if($cadencePenaltyStats){('{0:F4} / {1:F4} / {2:F4}' -f $cadencePenaltyStats.Min,$cadencePenaltyStats.Average,$cadencePenaltyStats.Max)}else{'n/a / n/a / n/a'})"}
if($effectiveWeapon -eq 'glock' -or $effectiveWeapon -eq '357' -or $Weapon -eq 'glock' -or $Weapon -eq '357'){Write-Host "  hold pen min/avg/max     : $(if($holdPenaltyStats){('{0:F4} / {1:F4} / {2:F4}' -f $holdPenaltyStats.Min,$holdPenaltyStats.Average,$holdPenaltyStats.Max)}else{'n/a / n/a / n/a'})"}
if($effectiveWeapon -eq 'glock' -or $effectiveWeapon -eq 'mp5' -or $effectiveWeapon -eq '357' -or $effectiveWeapon -eq 'shotgun' -or $Weapon -eq 'glock' -or $Weapon -eq 'mp5' -or $Weapon -eq '357' -or $Weapon -eq 'shotgun'){Write-Host "  pattern index min/avg/max: $(if($patternIndexStats){('{0:N0} / {1:F2} / {2:N0}' -f $patternIndexStats.Min,$patternIndexStats.Average,$patternIndexStats.Max)}else{'n/a / n/a / n/a'})"}
if($effectiveWeapon -eq 'glock' -or $effectiveWeapon -eq 'mp5' -or $effectiveWeapon -eq '357' -or $effectiveWeapon -eq 'shotgun' -or $Weapon -eq 'glock' -or $Weapon -eq 'mp5' -or $Weapon -eq '357' -or $Weapon -eq 'shotgun'){Write-Host "  pattern x min/avg/max    : $(if($patternOffsetXStats){('{0:F4} / {1:F4} / {2:F4}' -f $patternOffsetXStats.Min,$patternOffsetXStats.Average,$patternOffsetXStats.Max)}else{'n/a / n/a / n/a'})"}
if($effectiveWeapon -eq 'glock' -or $effectiveWeapon -eq 'mp5' -or $effectiveWeapon -eq '357' -or $effectiveWeapon -eq 'shotgun' -or $Weapon -eq 'glock' -or $Weapon -eq 'mp5' -or $Weapon -eq '357' -or $Weapon -eq 'shotgun'){Write-Host "  pattern y min/avg/max    : $(if($patternOffsetYStats){('{0:F4} / {1:F4} / {2:F4}' -f $patternOffsetYStats.Min,$patternOffsetYStats.Average,$patternOffsetYStats.Max)}else{'n/a / n/a / n/a'})"}
if($effectiveWeapon -eq 'glock' -or $effectiveWeapon -eq '357' -or $Weapon -eq 'glock' -or $Weapon -eq '357'){Write-Host "  cadence contrib min/avg/max: $(if($cadenceContributionStats){('{0:F4} / {1:F4} / {2:F4}' -f $cadenceContributionStats.Min,$cadenceContributionStats.Average,$cadenceContributionStats.Max)}else{'n/a / n/a / n/a'})"}
if($effectiveWeapon -eq '357' -or $effectiveWeapon -eq 'mp5' -or $Weapon -eq '357' -or $Weapon -eq 'mp5'){Write-Host "  next add min/avg/max     : $(if($nextAdditionalSpreadStats){('{0:F4} / {1:F4} / {2:F4}' -f $nextAdditionalSpreadStats.Min,$nextAdditionalSpreadStats.Average,$nextAdditionalSpreadStats.Max)}else{'n/a / n/a / n/a'})"}
if($effectiveWeapon -eq 'mp5' -or $Weapon -eq 'mp5'){Write-Host "  burst add min/avg/max    : $(if($burstStats){('{0:F4} / {1:F4} / {2:F4}' -f $burstStats.Min,$burstStats.Average,$burstStats.Max)}else{'n/a / n/a / n/a'})"}
if($effectiveWeapon -eq 'mp5' -or $Weapon -eq 'mp5'){Write-Host "  move contrib min/avg/max : $(if($movementContributionStats){('{0:F4} / {1:F4} / {2:F4}' -f $movementContributionStats.Min,$movementContributionStats.Average,$movementContributionStats.Max)}else{'n/a / n/a / n/a'})"}
if($effectiveWeapon -eq 'mp5' -or $Weapon -eq 'mp5'){Write-Host "  air contrib min/avg/max  : $(if($airContributionStats){('{0:F4} / {1:F4} / {2:F4}' -f $airContributionStats.Min,$airContributionStats.Average,$airContributionStats.Max)}else{'n/a / n/a / n/a'})"}
if($effectiveWeapon -eq 'mp5' -or $Weapon -eq 'mp5'){Write-Host "  crouch bonus min/avg/max : $(if($crouchBonusStats){('{0:F4} / {1:F4} / {2:F4}' -f $crouchBonusStats.Min,$crouchBonusStats.Average,$crouchBonusStats.Max)}else{'n/a / n/a / n/a'})"}
if($effectiveWeapon -eq 'mp5' -or $Weapon -eq 'mp5'){Write-Host "  hold pen min/avg/max     : $(if($holdPenaltyStats){('{0:F4} / {1:F4} / {2:F4}' -f $holdPenaltyStats.Min,$holdPenaltyStats.Average,$holdPenaltyStats.Max)}else{'n/a / n/a / n/a'})"}
if($effectiveWeapon -eq 'mp5' -or $Weapon -eq 'mp5'){Write-Host "  short burst next avg     : $(if($mp5ShortBurstSpreadStats){('{0:F4}' -f $mp5ShortBurstSpreadStats.Average)}else{'n/a'})"}
if($effectiveWeapon -eq 'mp5' -or $Weapon -eq 'mp5'){Write-Host "  long spray next avg      : $(if($mp5LongBurstSpreadStats){('{0:F4}' -f $mp5LongBurstSpreadStats.Average)}else{'n/a'})"}
if($effectiveWeapon -eq 'shotgun' -or $Weapon -eq 'shotgun'){Write-Host "  pellets min/avg/max      : $(if($pelletPlanStats){('{0:N0} / {1:F2} / {2:N0}' -f $pelletPlanStats.Min,$pelletPlanStats.Average,$pelletPlanStats.Max)}else{'n/a / n/a / n/a'})"}
if($effectiveWeapon -eq 'shotgun' -or $Weapon -eq 'shotgun'){Write-Host "  pellet hits min/avg/max  : $(if($pelletHitStats){('{0:N0} / {1:F2} / {2:N0}' -f $pelletHitStats.Min,$pelletHitStats.Average,$pelletHitStats.Max)}else{'n/a / n/a / n/a'})"}
if($effectiveWeapon -eq 'shotgun' -or $Weapon -eq 'shotgun'){Write-Host "  hs pellets min/avg/max   : $(if($headshotPelletStats){('{0:N0} / {1:F2} / {2:N0}' -f $headshotPelletStats.Min,$headshotPelletStats.Average,$headshotPelletStats.Max)}else{'n/a / n/a / n/a'})"}
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
