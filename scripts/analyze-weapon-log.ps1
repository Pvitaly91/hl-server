[CmdletBinding(DefaultParameterSetName = "Latest")]
param(
    [Parameter(ParameterSetName = "Path", Mandatory = $true)]
    [string]$Path,

    [Parameter(ParameterSetName = "Latest")]
    [switch]$Latest,

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
    [switch]$RequireLethalHeadshotEvidence
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

function Resolve-AnalysisTargetLog {
    if ($PSCmdlet.ParameterSetName -eq "Path") {
        $resolvedPath = Get-FullPath -Path $Path
        if (-not (Test-LeafPath -Path $resolvedPath)) {
            throw "Weapon debug log was not found: $resolvedPath"
        }

        return (Get-Item -LiteralPath $resolvedPath)
    }

    $latestLog = Get-LatestWeaponDebugLog
    if (-not $latestLog) {
        throw "No weapon debug log was found under $(Get-WeaponDebugLogsRoot). Generate telemetry first or pass -Path."
    }

    return $latestLog
}

function Get-WeaponLogValue {
    param(
        [System.Collections.IDictionary]$Values,
        [string]$Name
    )

    if ($Values -and $Values.Contains($Name)) {
        return [string]$Values[$Name]
    }

    return $null
}

function Convert-WeaponLogNullableInt {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value) -or $Value -eq "na") {
        return $null
    }

    return [int]::Parse($Value, [System.Globalization.CultureInfo]::InvariantCulture)
}

function Convert-WeaponLogNullableDouble {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value) -or $Value -eq "na") {
        return $null
    }

    return [double]::Parse($Value, [System.Globalization.CultureInfo]::InvariantCulture)
}

function Convert-WeaponLogNullableBool {
    param([string]$Value)

    if ([string]::IsNullOrWhiteSpace($Value) -or $Value -eq "na") {
        return $null
    }

    switch ($Value) {
        "1" { return $true }
        "0" { return $false }
        default { return [bool]::Parse($Value) }
    }
}

function Format-WeaponLogNumber {
    param(
        [AllowNull()]$Value,
        [int]$Digits
    )

    if ($null -eq $Value) {
        return "n/a"
    }

    return ([double]$Value).ToString(("F{0}" -f $Digits), [System.Globalization.CultureInfo]::InvariantCulture)
}

function Format-WeaponLogBool {
    param([AllowNull()][bool]$Value)

    if ($null -eq $Value) {
        return "n/a"
    }

    if ($Value) {
        return "1"
    }

    return "0"
}

function Get-NumericStats {
    param([object[]]$Values)

    $filtered = @($Values | Where-Object { $null -ne $_ })
    if ($filtered.Count -eq 0) {
        return $null
    }

    $measure = $filtered | Measure-Object -Minimum -Maximum -Average
    return [PSCustomObject]@{
        Count   = [int]$measure.Count
        Min     = [double]$measure.Minimum
        Average = [double]$measure.Average
        Max     = [double]$measure.Maximum
    }
}

function Get-HitgroupCounts {
    param([object[]]$Events)

    $counts = [ordered]@{}
    if ($null -eq $Events -or $Events.Count -eq 0) {
        return $counts
    }

    $preferredHitgroups = @("head", "chest", "stomach", "leftarm", "rightarm", "leftleg", "rightleg", "generic")
    foreach ($hitgroup in $preferredHitgroups) {
        $count = @($Events | Where-Object { $_.HitGroup -eq $hitgroup }).Count
        if ($count -gt 0) {
            $counts[$hitgroup] = $count
        }
    }

    $otherGroups = @(
        $Events |
        Where-Object {
            -not [string]::IsNullOrWhiteSpace($_.HitGroup) -and
            ($preferredHitgroups -notcontains $_.HitGroup)
        } |
        Group-Object -Property HitGroup |
        Sort-Object -Property Name
    )

    foreach ($group in $otherGroups) {
        if (-not $counts.Contains($group.Name)) {
            $counts[$group.Name] = [int]$group.Count
        }
    }

    return $counts
}

function Format-HitgroupCounts {
    param([System.Collections.IDictionary]$Counts)

    if ($null -eq $Counts -or $Counts.Count -eq 0) {
        return "n/a"
    }

    $parts = @()
    foreach ($key in $Counts.Keys) {
        $parts += ("{0}={1}" -f $key, $Counts[$key])
    }

    return ($parts -join ", ")
}

function Parse-WeaponLogLine {
    param(
        [string]$Line,
        [int]$LineNumber
    )

    $trimmedLine = $Line.Trim()
    if ([string]::IsNullOrWhiteSpace($trimmedLine)) {
        return $null
    }

    $prefix = $null
    if ($trimmedLine.StartsWith("[")) {
        $closingIndex = $trimmedLine.IndexOf("]")
        if ($closingIndex -gt 0) {
            $prefix = $trimmedLine.Substring(1, $closingIndex - 1)
            $trimmedLine = $trimmedLine.Substring($closingIndex + 1).TrimStart()
        }
    }

    $matches = [System.Text.RegularExpressions.Regex]::Matches(
        $trimmedLine,
        '(?<key>[A-Za-z0-9_]+)=(?:"(?<quoted>[^"]*)"|(?<value>\S+))')
    if ($matches.Count -eq 0) {
        return $null
    }

    $values = [ordered]@{}
    foreach ($match in $matches) {
        $key = $match.Groups["key"].Value
        $value = if ($match.Groups["quoted"].Success) { $match.Groups["quoted"].Value } else { $match.Groups["value"].Value }
        $values[$key] = $value
    }

    $type = Get-WeaponLogValue -Values $values -Name "type"
    if ([string]::IsNullOrWhiteSpace($type)) {
        if ((Get-WeaponLogValue -Values $values -Name "event") -eq "weapon_debug_session") {
            $type = "session"
        }
        elseif (-not [string]::IsNullOrWhiteSpace((Get-WeaponLogValue -Values $values -Name "reason"))) {
            $type = "rejected"
        }
        elseif ((Get-WeaponLogValue -Values $values -Name "weapon") -eq "glock" -and (Get-WeaponLogValue -Values $values -Name "fire") -eq "primary") {
            $type = "accepted"
        }
        else {
            $type = "unknown"
        }
    }
    else {
        $type = $type.ToLowerInvariant()
    }

    $speed = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "speed2d")
    $maxSpeed = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "maxspeed")
    $speedRatio = $null
    if ($null -ne $speed -and $null -ne $maxSpeed -and $maxSpeed -gt 0.0) {
        $speedRatio = [Math]::Min([Math]::Max(($speed / $maxSpeed), 0.0), 1.0)
    }

    $movementPenalty = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "move_penalty")
    $movementPenaltyRate = $null
    if ($null -ne $movementPenalty -and $null -ne $speedRatio -and $speedRatio -gt 0.0) {
        $movementPenaltyRate = $movementPenalty / $speedRatio
    }

    return [PSCustomObject]@{
        LineNumber           = $LineNumber
        Prefix               = $prefix
        Type                 = $type
        Timestamp            = Get-WeaponLogValue -Values $values -Name "ts"
        Map                  = Get-WeaponLogValue -Values $values -Name "map"
        Event                = Get-WeaponLogValue -Values $values -Name "event"
        Status               = Get-WeaponLogValue -Values $values -Name "status"
        Game                 = Get-WeaponLogValue -Values $values -Name "game"
        LogFile              = Get-WeaponLogValue -Values $values -Name "file"
        ProfileName          = Get-WeaponLogValue -Values $values -Name "profile"
        MoveSpreadScale      = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "move_scale")
        FirstShotEnabled     = Convert-WeaponLogNullableBool (Get-WeaponLogValue -Values $values -Name "firstshot_enabled")
        SpreadRecovery       = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "recovery")
        GroundMovePenalty    = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "ground_move_penalty")
        AirMovePenalty       = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "air_move_penalty")
        DuckPenaltyScale     = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "duck_penalty_scale")
        FirstShotSpeedThreshold = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "firstshot_speed")
        MaxSpread            = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "max_spread")
        PrimaryDamageSetting = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "sv_exp_glock_primary_damage")
        HeadshotScaleSetting = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "sv_exp_glock_primary_headshot_scale")
        HeadshotLethalSetting = Convert-WeaponLogNullableBool (Get-WeaponLogValue -Values $values -Name "sv_exp_glock_primary_headshot_lethal")
        Player               = Get-WeaponLogValue -Values $values -Name "player"
        EntIndex             = Convert-WeaponLogNullableInt (Get-WeaponLogValue -Values $values -Name "entindex")
        UserId               = Convert-WeaponLogNullableInt (Get-WeaponLogValue -Values $values -Name "userid")
        Weapon               = Get-WeaponLogValue -Values $values -Name "weapon"
        Fire                 = Get-WeaponLogValue -Values $values -Name "fire"
        Reason               = Get-WeaponLogValue -Values $values -Name "reason"
        Experimental         = Convert-WeaponLogNullableBool (Get-WeaponLogValue -Values $values -Name "experimental")
        TapFire              = Convert-WeaponLogNullableBool (Get-WeaponLogValue -Values $values -Name "tapfire")
        FirstShot            = Convert-WeaponLogNullableBool (Get-WeaponLogValue -Values $values -Name "firstshot")
        Spread               = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "spread")
        BaseSpread           = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "base")
        MovementPenalty      = $movementPenalty
        HorizontalSpeed      = $speed
        MaxSpeed             = $maxSpeed
        SpeedRatio           = $speedRatio
        MovementPenaltyRate  = $movementPenaltyRate
        Grounded             = Convert-WeaponLogNullableBool (Get-WeaponLogValue -Values $values -Name "grounded")
        Ducking              = Convert-WeaponLogNullableBool (Get-WeaponLogValue -Values $values -Name "ducking")
        DeltaPrevious        = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "delta_prev")
        Clip                 = Convert-WeaponLogNullableInt (Get-WeaponLogValue -Values $values -Name "clip")
        Attacker             = Get-WeaponLogValue -Values $values -Name "attacker"
        AttackerEntIndex     = Convert-WeaponLogNullableInt (Get-WeaponLogValue -Values $values -Name "attacker_entindex")
        AttackerUserId       = Convert-WeaponLogNullableInt (Get-WeaponLogValue -Values $values -Name "attacker_userid")
        Victim               = Get-WeaponLogValue -Values $values -Name "victim"
        VictimEntIndex       = Convert-WeaponLogNullableInt (Get-WeaponLogValue -Values $values -Name "victim_entindex")
        VictimUserId         = Convert-WeaponLogNullableInt (Get-WeaponLogValue -Values $values -Name "victim_userid")
        HitGroup             = Get-WeaponLogValue -Values $values -Name "hitgroup"
        HitGroupId           = Convert-WeaponLogNullableInt (Get-WeaponLogValue -Values $values -Name "hitgroup_id")
        Headshot             = Convert-WeaponLogNullableBool (Get-WeaponLogValue -Values $values -Name "headshot")
        BaseDamage           = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "base_damage")
        HitgroupScale        = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "hitgroup_scale")
        TraceDamage          = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "trace_damage")
        AppliedDamage        = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "applied_damage")
        HealthBefore         = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "health_before")
        HealthAfter          = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "health_after")
        ArmorBefore          = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "armor_before")
        ArmorAfter           = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "armor_after")
        ArmorDamage          = Convert-WeaponLogNullableDouble (Get-WeaponLogValue -Values $values -Name "armor_damage")
        HeadshotLethalActive = Convert-WeaponLogNullableBool (Get-WeaponLogValue -Values $values -Name "headshot_lethal_active")
        HeadshotLethalApplied = Convert-WeaponLogNullableBool (Get-WeaponLogValue -Values $values -Name "headshot_lethal_applied")
        RawLine              = $Line
    }
}

$targetLog = Resolve-AnalysisTargetLog
$parsedEvents = @()
$warnings = New-Object System.Collections.ArrayList
$observations = New-Object System.Collections.ArrayList
$assertionFailures = New-Object System.Collections.ArrayList

$lineNumber = 0
foreach ($line in (Get-Content -LiteralPath $targetLog.FullName)) {
    $lineNumber += 1
    $parsedEvent = Parse-WeaponLogLine -Line $line -LineNumber $lineNumber
    if ($parsedEvent -and $parsedEvent.Type -ne "unknown") {
        $parsedEvents += $parsedEvent
    }
    elseif (-not [string]::IsNullOrWhiteSpace($line)) {
        [void]$warnings.Add("Skipped non-telemetry line $lineNumber because it did not match the expected single-line key=value format.")
    }
}

$sessionEvent = @($parsedEvents | Where-Object { $_.Type -eq "session" } | Select-Object -First 1)
$session = if ($sessionEvent.Count -gt 0) { $sessionEvent[0] } else { $null }
$acceptedEvents = @($parsedEvents | Where-Object { $_.Type -eq "accepted" })
$rejectedEvents = @($parsedEvents | Where-Object { $_.Type -eq "rejected" })
$hitEvents = @($parsedEvents | Where-Object { $_.Type -eq "hit" })
$killEvents = @($parsedEvents | Where-Object { $_.Type -eq "kill" })

$firstShotAccepted = @($acceptedEvents | Where-Object { $_.FirstShot -eq $true })
$movePenaltyAccepted = @($acceptedEvents | Where-Object { $null -ne $_.MovementPenalty -and $_.MovementPenalty -gt 0.0 })
$groundedAccepted = @($acceptedEvents | Where-Object { $_.Grounded -eq $true })
$airborneAccepted = @($acceptedEvents | Where-Object { $_.Grounded -eq $false })
$duckingAccepted = @($acceptedEvents | Where-Object { $_.Ducking -eq $true })
$standingMoveAccepted = @(
    $acceptedEvents |
    Where-Object {
        $_.Grounded -eq $true -and
        $_.Ducking -eq $false -and
        $null -ne $_.MovementPenalty -and $_.MovementPenalty -gt 0.0 -and
        $null -ne $_.MovementPenaltyRate
    }
)
$crouchMoveAccepted = @(
    $acceptedEvents |
    Where-Object {
        $_.Grounded -eq $true -and
        $_.Ducking -eq $true -and
        $null -ne $_.MovementPenalty -and $_.MovementPenalty -gt 0.0 -and
        $null -ne $_.MovementPenaltyRate
    }
)

$headshotHitEvents = @($hitEvents | Where-Object { $_.Headshot -eq $true })
$headshotKillEvents = @($killEvents | Where-Object { $_.Headshot -eq $true })
$lethalHeadshotEvidenceEvents = @($hitEvents | Where-Object { $_.HeadshotLethalApplied -eq $true })
if ($lethalHeadshotEvidenceEvents.Count -eq 0 -and $hitEvents.Count -eq 0) {
    $lethalHeadshotEvidenceEvents = @($killEvents | Where-Object { $_.HeadshotLethalApplied -eq $true })
}

$spreadStats = Get-NumericStats -Values ($acceptedEvents | Select-Object -ExpandProperty Spread)
$movementPenaltyStats = Get-NumericStats -Values ($acceptedEvents | Select-Object -ExpandProperty MovementPenalty)
$speedStats = Get-NumericStats -Values ($acceptedEvents | Select-Object -ExpandProperty HorizontalSpeed)
$standingPenaltyRateStats = Get-NumericStats -Values ($standingMoveAccepted | Select-Object -ExpandProperty MovementPenaltyRate)
$crouchPenaltyRateStats = Get-NumericStats -Values ($crouchMoveAccepted | Select-Object -ExpandProperty MovementPenaltyRate)
$appliedDamageStats = Get-NumericStats -Values ($hitEvents | Select-Object -ExpandProperty AppliedDamage)
$hitgroupCounts = Get-HitgroupCounts -Events $hitEvents

$recoveryReturnedFirstShot = $false
$recoveryEvent = $null
$seenNonFirstShotAccepted = $false
foreach ($acceptedEvent in $acceptedEvents) {
    if ($acceptedEvent.FirstShot -eq $false) {
        $seenNonFirstShotAccepted = $true
        continue
    }

    if ($acceptedEvent.FirstShot -eq $true -and $seenNonFirstShotAccepted) {
        $recoveryReturnedFirstShot = $true
        $recoveryEvent = $acceptedEvent
        break
    }
}

$crouchMoveEvidence = $false
foreach ($crouchEvent in $crouchMoveAccepted) {
    foreach ($standingEvent in $standingMoveAccepted) {
        if ($null -eq $crouchEvent.SpeedRatio -or $null -eq $standingEvent.SpeedRatio) {
            continue
        }

        if ($crouchEvent.SpeedRatio -ge $standingEvent.SpeedRatio -and $crouchEvent.MovementPenalty -lt $standingEvent.MovementPenalty) {
            $crouchMoveEvidence = $true
            break
        }
    }

    if ($crouchMoveEvidence) {
        break
    }
}

if (-not $crouchMoveEvidence -and $standingPenaltyRateStats -and $crouchPenaltyRateStats -and $crouchPenaltyRateStats.Average -lt $standingPenaltyRateStats.Average) {
    $crouchMoveEvidence = $true
}

$headshotPathEvidence = ($headshotHitEvents.Count -gt 0)
$headshotPathEvidenceSufficient = ($headshotHitEvents.Count -gt 0 -and ($headshotKillEvents.Count -gt 0 -or $lethalHeadshotEvidenceEvents.Count -gt 0))

if ($acceptedEvents.Count -gt 0) {
    [void]$observations.Add(("Accepted Glock primary shots were present ({0})." -f $acceptedEvents.Count))
}
else {
    [void]$warnings.Add("No accepted Glock primary shots were found in the analyzed log.")
}

if ($rejectedEvents.Count -gt 0) {
    [void]$observations.Add(("Tap-fire hold rejection lines were present ({0}); that is evidence the rejection path was exercised." -f $rejectedEvents.Count))
}
else {
    [void]$warnings.Add("No rejection lines were found, so this log does not show tap-fire hold blocking evidence.")
}

if ($hitEvents.Count -gt 0) {
    [void]$observations.Add(("Glock hit telemetry lines were present ({0})." -f $hitEvents.Count))
}
else {
    [void]$warnings.Add("No Glock hit telemetry lines were found.")
}

if ($killEvents.Count -gt 0) {
    [void]$observations.Add(("Glock kill telemetry lines were present ({0})." -f $killEvents.Count))
}
else {
    [void]$warnings.Add("No Glock kill telemetry lines were found.")
}

if ($firstShotAccepted.Count -gt 0) {
    [void]$observations.Add(("First-shot accepted events were present ({0})." -f $firstShotAccepted.Count))
}
else {
    [void]$warnings.Add("No accepted events with firstshot=1 were found.")
}

if ($movePenaltyAccepted.Count -gt 0) {
    [void]$observations.Add(("Accepted movement-penalty events were present ({0}) with move_penalty > 0." -f $movePenaltyAccepted.Count))
}
else {
    [void]$warnings.Add("No accepted events with move_penalty > 0 were found.")
}

if ($headshotHitEvents.Count -gt 0) {
    [void]$observations.Add(("Headshot hit telemetry was present ({0})." -f $headshotHitEvents.Count))
}
else {
    [void]$warnings.Add("No Glock headshot hit lines were found.")
}

if ($headshotKillEvents.Count -gt 0) {
    [void]$observations.Add(("Headshot kill telemetry was present ({0})." -f $headshotKillEvents.Count))
}
else {
    [void]$warnings.Add("No Glock headshot kill lines were found.")
}

if ($lethalHeadshotEvidenceEvents.Count -gt 0) {
    [void]$observations.Add(("Explicit lethal-headshot evidence was present ({0}) via headshot_lethal_applied=1." -f $lethalHeadshotEvidenceEvents.Count))
}
elseif ($session -and $session.HeadshotLethalSetting -eq $true) {
    [void]$warnings.Add("The session metadata enabled headshot-lethal tuning, but no hit recorded headshot_lethal_applied=1.")
}
else {
    [void]$warnings.Add("No explicit lethal-headshot evidence was found. Kills alone are not treated as proof that the lethal-headshot path ran.")
}

if ($recoveryReturnedFirstShot) {
    $deltaText = if ($null -ne $recoveryEvent.DeltaPrevious) { (" after delta_prev={0}s" -f (Format-WeaponLogNumber -Value $recoveryEvent.DeltaPrevious -Digits 3)) } else { "" }
    [void]$observations.Add(("Later accepted events returned to firstshot=1 after earlier non-firstshot accepted shots{0}, which is evidence that recovery restored the first-shot gate." -f $deltaText))
}
elseif ($firstShotAccepted.Count -gt 0 -and $acceptedEvents.Count -gt 1) {
    [void]$warnings.Add("The log shows first-shot accepted events, but it does not clearly show a later return to firstshot=1 after a non-firstshot accepted event.")
}

if ($standingMoveAccepted.Count -gt 0 -and $crouchMoveAccepted.Count -gt 0) {
    $standingRateText = Format-WeaponLogNumber -Value $standingPenaltyRateStats.Average -Digits 4
    $crouchRateText = Format-WeaponLogNumber -Value $crouchPenaltyRateStats.Average -Digits 4

    if ($crouchMoveEvidence) {
        [void]$observations.Add(("Grounded crouch-moving accepted shots showed lower normalized movement-penalty candidates than grounded standing moving shots (avg penalty-rate {0} vs {1}). This is evidence, not proof, of crouch-move penalty reduction." -f $crouchRateText, $standingRateText))
    }
    else {
        [void]$warnings.Add(("Grounded crouch-moving accepted shots were present, but this log did not show lower crouch normalized movement-penalty candidates than grounded standing movement (avg penalty-rate {0} vs {1})." -f $crouchRateText, $standingRateText))
    }
}
elseif ($crouchMoveAccepted.Count -eq 0) {
    [void]$warnings.Add("No grounded crouch-moving accepted shots with move_penalty > 0 were found, so crouch-move reduction cannot be evaluated from this log.")
}
else {
    [void]$warnings.Add("No comparable grounded standing moving accepted shots were found, so crouch-move reduction cannot be evaluated from this log.")
}

if (-not $session) {
    [void]$warnings.Add("No session header line was parsed. The analyzer can still summarize event telemetry, but launch metadata is missing.")
}

if ($headshotPathEvidenceSufficient) {
    [void]$observations.Add("The log contains enough evidence to say the session exercised the intended Glock headshot path: at least one headshot hit was recorded and the log also shows a headshot kill or an explicit lethal-headshot flag.")
}
elseif ($headshotPathEvidence) {
    [void]$warnings.Add("Headshot hits were logged, but the evidence stops short of a headshot kill or explicit lethal-headshot flag.")
}
else {
    [void]$warnings.Add("The log does not contain enough evidence to say the session exercised the intended Glock headshot path.")
}

$signals = [ordered]@{
    acceptedShots                        = ($acceptedEvents.Count -gt 0)
    tapFireHoldRejections                = ($rejectedEvents.Count -gt 0)
    firstShotAccepted                    = ($firstShotAccepted.Count -gt 0)
    movementPenaltyPositive              = ($movePenaltyAccepted.Count -gt 0)
    recoveryReturnedFirstShot            = $recoveryReturnedFirstShot
    crouchMovePenaltyReductionCandidate  = $crouchMoveEvidence
    hitsPresent                          = ($hitEvents.Count -gt 0)
    killsPresent                         = ($killEvents.Count -gt 0)
    headshotHitsPresent                  = ($headshotHitEvents.Count -gt 0)
    headshotKillsPresent                 = ($headshotKillEvents.Count -gt 0)
    lethalHeadshotEvidencePresent        = ($lethalHeadshotEvidenceEvents.Count -gt 0)
    headshotPathObserved                 = $headshotPathEvidence
    headshotPathEvidenceSufficient       = $headshotPathEvidenceSufficient
}

if ($RequireAccepted -and -not $signals.acceptedShots) {
    [void]$assertionFailures.Add("Required signal missing: accepted Glock primary shots.")
}

if ($RequireRejections -and -not $signals.tapFireHoldRejections) {
    [void]$assertionFailures.Add("Required signal missing: tap-fire hold rejection lines.")
}

if ($RequireFirstShot -and -not $signals.firstShotAccepted) {
    [void]$assertionFailures.Add("Required signal missing: accepted events with firstshot=1.")
}

if ($RequireMovePenalty -and -not $signals.movementPenaltyPositive) {
    [void]$assertionFailures.Add("Required signal missing: accepted events with move_penalty > 0.")
}

if ($RequireCrouchMoveEvidence -and -not $signals.crouchMovePenaltyReductionCandidate) {
    [void]$assertionFailures.Add("Required signal missing: crouch-moving accepted shots that suggest lower movement penalty than standing movement.")
}

if ($RequireHits -and -not $signals.hitsPresent) {
    [void]$assertionFailures.Add("Required signal missing: Glock hit telemetry lines.")
}

if ($RequireKills -and -not $signals.killsPresent) {
    [void]$assertionFailures.Add("Required signal missing: Glock kill telemetry lines.")
}

if ($RequireHeadshotKills -and -not $signals.headshotKillsPresent) {
    [void]$assertionFailures.Add("Required signal missing: Glock headshot kill telemetry lines.")
}

if ($RequireLethalHeadshotEvidence -and -not $signals.lethalHeadshotEvidencePresent) {
    [void]$assertionFailures.Add("Required signal missing: explicit lethal-headshot evidence via headshot_lethal_applied=1.")
}

$report = [ordered]@{
    analyzedLogPath = $targetLog.FullName
    session = if ($session) {
        [ordered]@{
            timestamp = $session.Timestamp
            map = $session.Map
            game = $session.Game
            event = $session.Event
            status = $session.Status
            file = $session.LogFile
            profileName = $session.ProfileName
            featureFlags = [ordered]@{
                tapFire = $session.TapFire
                moveSpreadScale = $session.MoveSpreadScale
                firstShotAccuracy = $session.FirstShotEnabled
                spreadRecovery = $session.SpreadRecovery
            }
            tuning = [ordered]@{
                baseSpread = $session.BaseSpread
                groundMovePenalty = $session.GroundMovePenalty
                airMovePenalty = $session.AirMovePenalty
                duckPenaltyScale = $session.DuckPenaltyScale
                firstShotSpeedThreshold = $session.FirstShotSpeedThreshold
                maxSpread = $session.MaxSpread
                primaryDamage = $session.PrimaryDamageSetting
                headshotScale = $session.HeadshotScaleSetting
                headshotLethal = $session.HeadshotLethalSetting
            }
        }
    }
    else {
        $null
    }
    counters = [ordered]@{
        parsedEventCount = $parsedEvents.Count
        acceptedShots = $acceptedEvents.Count
        rejectedShots = $rejectedEvents.Count
        hitEvents = $hitEvents.Count
        killEvents = $killEvents.Count
        headshotHits = $headshotHitEvents.Count
        headshotKills = $headshotKillEvents.Count
        lethalHeadshotEvidence = $lethalHeadshotEvidenceEvents.Count
        firstShotAccepted = $firstShotAccepted.Count
        acceptedMovePenaltyPositive = $movePenaltyAccepted.Count
        acceptedGrounded = $groundedAccepted.Count
        acceptedAirborne = $airborneAccepted.Count
        acceptedDucking = $duckingAccepted.Count
        acceptedStandingMoving = $standingMoveAccepted.Count
        acceptedCrouchMoving = $crouchMoveAccepted.Count
    }
    stats = [ordered]@{
        spread = $spreadStats
        movementPenalty = $movementPenaltyStats
        horizontalSpeed = $speedStats
        standingMovePenaltyRate = $standingPenaltyRateStats
        crouchMovePenaltyRate = $crouchPenaltyRateStats
        appliedDamage = $appliedDamageStats
        hitgroupCounts = $hitgroupCounts
    }
    signals = $signals
    observations = @($observations)
    warnings = @($warnings)
}

$exportedFiles = [ordered]@{}
if ($ExportJson -or $ExportCsv) {
    $resolvedOutputDir = if ([string]::IsNullOrWhiteSpace($OutputDir)) {
        Get-WeaponDebugReportsRoot
    }
    else {
        Get-FullPath -Path $OutputDir
    }

    Ensure-Directory -Path $resolvedOutputDir
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"

    if ($ExportJson) {
        $jsonPath = Join-Path $resolvedOutputDir ("weapon-report-" + $timestamp + ".json")
        $report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $jsonPath -Encoding UTF8
        $exportedFiles.json = $jsonPath
    }

    if ($ExportCsv) {
        $csvPath = Join-Path $resolvedOutputDir ("weapon-events-" + $timestamp + ".csv")
        $parsedEvents | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8
        $exportedFiles.csv = $csvPath
    }
}

Write-Host "Weapon log analysis"
Write-Host "  log path                 : $($targetLog.FullName)"

if ($session) {
    Write-Host "  session                  : ts=$($session.Timestamp) map=$($session.Map) game=$($session.Game) status=$($session.Status)"
    if (-not [string]::IsNullOrWhiteSpace($session.ProfileName)) {
        Write-Host "  profile                  : $($session.ProfileName)"
    }

    if ($null -ne $session.BaseSpread -or $null -ne $session.GroundMovePenalty -or $null -ne $session.AirMovePenalty -or $null -ne $session.DuckPenaltyScale -or $null -ne $session.FirstShotSpeedThreshold -or $null -ne $session.MaxSpread) {
        Write-Host "  tuning                   : base=$(Format-WeaponLogNumber -Value $session.BaseSpread -Digits 4) ground=$(Format-WeaponLogNumber -Value $session.GroundMovePenalty -Digits 4) air=$(Format-WeaponLogNumber -Value $session.AirMovePenalty -Digits 4) duck=$(Format-WeaponLogNumber -Value $session.DuckPenaltyScale -Digits 4) firstshot_speed=$(Format-WeaponLogNumber -Value $session.FirstShotSpeedThreshold -Digits 1) max_spread=$(Format-WeaponLogNumber -Value $session.MaxSpread -Digits 4)"
    }

    if ($null -ne $session.PrimaryDamageSetting -or $null -ne $session.HeadshotScaleSetting -or $null -ne $session.HeadshotLethalSetting) {
        Write-Host "  damage tuning            : damage=$(Format-WeaponLogNumber -Value $session.PrimaryDamageSetting -Digits 4) headshot_scale=$(Format-WeaponLogNumber -Value $session.HeadshotScaleSetting -Digits 4) headshot_lethal=$(Format-WeaponLogBool -Value $session.HeadshotLethalSetting)"
    }

    if ($null -ne $session.TapFire -or $null -ne $session.MoveSpreadScale -or $null -ne $session.FirstShotEnabled -or $null -ne $session.SpreadRecovery) {
        Write-Host "  feature flags            : tapfire=$(Format-WeaponLogBool -Value $session.TapFire) move_scale=$(Format-WeaponLogNumber -Value $session.MoveSpreadScale -Digits 4) firstshot=$(Format-WeaponLogBool -Value $session.FirstShotEnabled) recovery=$(Format-WeaponLogNumber -Value $session.SpreadRecovery -Digits 3)"
    }
}
else {
    Write-Host "  session                  : missing"
}

Write-Host "  accepted shots           : $($acceptedEvents.Count)"
Write-Host "  rejected shots           : $($rejectedEvents.Count)"
Write-Host "  hit events               : $($hitEvents.Count)"
Write-Host "  kill events              : $($killEvents.Count)"
Write-Host "  headshot hits            : $($headshotHitEvents.Count)"
Write-Host "  headshot kills           : $($headshotKillEvents.Count)"
Write-Host "  lethal hs evidence       : $($lethalHeadshotEvidenceEvents.Count)"
Write-Host "  first-shot accepted      : $($firstShotAccepted.Count)"
Write-Host "  accepted move_penalty>0  : $($movePenaltyAccepted.Count)"
Write-Host "  applied dmg min/avg/max  : $(Format-WeaponLogNumber -Value $(if ($appliedDamageStats) { $appliedDamageStats.Min } else { $null }) -Digits 4) / $(Format-WeaponLogNumber -Value $(if ($appliedDamageStats) { $appliedDamageStats.Average } else { $null }) -Digits 4) / $(Format-WeaponLogNumber -Value $(if ($appliedDamageStats) { $appliedDamageStats.Max } else { $null }) -Digits 4)"
Write-Host "  hitgroups                : $(Format-HitgroupCounts -Counts $hitgroupCounts)"
Write-Host "  accepted grounded        : $($groundedAccepted.Count)"
Write-Host "  accepted airborne        : $($airborneAccepted.Count)"
Write-Host "  accepted ducking         : $($duckingAccepted.Count)"
Write-Host "  spread min/avg/max       : $(Format-WeaponLogNumber -Value $(if ($spreadStats) { $spreadStats.Min } else { $null }) -Digits 4) / $(Format-WeaponLogNumber -Value $(if ($spreadStats) { $spreadStats.Average } else { $null }) -Digits 4) / $(Format-WeaponLogNumber -Value $(if ($spreadStats) { $spreadStats.Max } else { $null }) -Digits 4)"
Write-Host "  move penalty min/avg/max : $(Format-WeaponLogNumber -Value $(if ($movementPenaltyStats) { $movementPenaltyStats.Min } else { $null }) -Digits 4) / $(Format-WeaponLogNumber -Value $(if ($movementPenaltyStats) { $movementPenaltyStats.Average } else { $null }) -Digits 4) / $(Format-WeaponLogNumber -Value $(if ($movementPenaltyStats) { $movementPenaltyStats.Max } else { $null }) -Digits 4)"
Write-Host "  speed2d min/avg/max      : $(Format-WeaponLogNumber -Value $(if ($speedStats) { $speedStats.Min } else { $null }) -Digits 1) / $(Format-WeaponLogNumber -Value $(if ($speedStats) { $speedStats.Average } else { $null }) -Digits 1) / $(Format-WeaponLogNumber -Value $(if ($speedStats) { $speedStats.Max } else { $null }) -Digits 1)"

Write-Host ""
Write-Host "Observations"
if ($observations.Count -eq 0) {
    Write-Host "  - No notable observations were derived from the parsed telemetry."
}
else {
    foreach ($observation in $observations) {
        Write-Host "  - $observation"
    }
}

Write-Host ""
Write-Host "Warnings"
if ($warnings.Count -eq 0) {
    Write-Host "  - none"
}
else {
    foreach ($warning in $warnings) {
        Write-Host "  - $warning"
    }
}

if ($exportedFiles.Count -gt 0) {
    Write-Host ""
    Write-Host "Exports"
    foreach ($exportType in $exportedFiles.Keys) {
        Write-Host "  $exportType : $($exportedFiles[$exportType])"
    }
}

Write-Host ""
Write-Host "Evidence only: this summarizes logged server-authoritative telemetry, not subjective stock-client feel."

if ($assertionFailures.Count -gt 0) {
    Write-Host ""
    Write-Host "Assertion failures"
    foreach ($failure in $assertionFailures) {
        Write-Host "  - $failure"
    }

    exit 2
}
