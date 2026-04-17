[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [int]$Port = 27015,
    [string]$Map = "crossfire",
    [string]$Mp5Profile,
    [string]$LabTargetProfile,
    [string[]]$SetCvar = @(),
    [string]$SessionTag,
    [string]$MatrixName,
    [string]$MatrixStep,
    [switch]$LabDummy,
    [switch]$NoClient,
    [switch]$NoTail,
    [switch]$AnalyzeLatestOnExit,
    [switch]$PassThru,
    [switch]$SkipChecklist,
    [switch]$DetachedServer = $true,
    [int]$TimeoutSeconds = 45,
    [string]$TemplateRoot,
    [string]$HldsExe,
    [string]$HlExe,
    [string]$SteamCmdExe,
    [switch]$AllowSteamCmdDownload,
    [switch]$PreferClientMatchedRuntime
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

function Wait-ForSessionWeaponDebugLog {
    param(
        [System.IO.FileInfo]$PreviousLatestLog,
        [Parameter(Mandatory = $true)]
        [int]$TimeoutSeconds,
        [System.Diagnostics.Process]$Process
    )

    $previousPath = if ($PreviousLatestLog) { $PreviousLatestLog.FullName } else { $null }
    $previousWriteTimeUtc = if ($PreviousLatestLog) { $PreviousLatestLog.LastWriteTimeUtc } else { [DateTime]::MinValue }
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)

    while ((Get-Date) -lt $deadline) {
        $latestLog = Get-LatestWeaponDebugLog
        if ($latestLog) {
            $isNewLog = [string]::IsNullOrWhiteSpace($previousPath) -or
                (-not $latestLog.FullName.Equals($previousPath, [System.StringComparison]::OrdinalIgnoreCase)) -or
                ($latestLog.LastWriteTimeUtc -gt $previousWriteTimeUtc)

            if ($isNewLog -and (Select-String -Path $latestLog.FullName -Pattern "event=weapon_debug_session status=ready" -SimpleMatch -Quiet -ErrorAction SilentlyContinue)) {
                return $latestLog
            }
        }

        if ($Process) {
            $Process.Refresh()
            if ($Process.HasExited) {
                return $null
            }
        }

        Start-Sleep -Milliseconds 500
    }

    return $null
}

function Start-WeaponLogTailWindow {
    param(
        [Parameter(Mandatory = $true)]
        [string]$WeaponLogPath
    )

    $tailScript = Join-Path $PSScriptRoot "tail-weapon-log.ps1"
    $arguments = @(
        "-NoExit",
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $tailScript,
        "-Path", $WeaponLogPath
    )

    Start-Process -FilePath "powershell.exe" -WorkingDirectory (Get-RepoRoot) -ArgumentList $arguments | Out-Null
}

function Write-Checklist {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Configuration,
        [Parameter(Mandatory = $true)]
        [int]$Port,
        [Parameter(Mandatory = $true)]
        [string]$Map,
        [Parameter(Mandatory = $true)]
        [string]$ConnectAddress,
        [Parameter(Mandatory = $true)]
        [string]$RuntimeRoot,
        [Parameter(Mandatory = $true)]
        [string]$ServerLogPath,
        [string]$Mp5Profile,
        [string]$LabTargetProfile,
        [string]$WeaponLogPath,
        [bool]$TailWindowRequested,
        [string]$ClientLaunchStatus,
        [bool]$LabDummy,
        [string]$GameDirName = "valve"
    )

    Write-Host ""
    Write-Host $(if ($LabDummy) { "Manual MP5 lab session ready" } else { "Manual MP5 session ready" })
    Write-Host "  configuration : $Configuration"
    Write-Host "  map           : $Map"
    Write-Host "  port          : $Port"
    Write-Host "  connect       : $ConnectAddress"
    Write-Host "  game          : $GameDirName"
    Write-Host "  runtime root  : $RuntimeRoot"
    Write-Host "  server log    : $ServerLogPath"
    Write-Host "  mp5 profile   : $(if ([string]::IsNullOrWhiteSpace($Mp5Profile)) { 'baseline' } else { $Mp5Profile })"
    if ($LabDummy) {
        Write-Host "  target profile: $(if ([string]::IsNullOrWhiteSpace($LabTargetProfile)) { 'default' } else { $LabTargetProfile })"
    }

    if ([string]::IsNullOrWhiteSpace($WeaponLogPath)) {
        Write-Host "  weapon log    : pending; run .\\scripts\\tail-weapon-log.ps1 once the first telemetry session line is created"
    }
    else {
        Write-Host "  weapon log    : $WeaponLogPath"
    }

    if ($TailWindowRequested) {
        Write-Host "  tailing       : requested in a separate PowerShell window"
    }
    else {
        Write-Host "  tailing       : disabled"
    }

    Write-Host "  client        : $ClientLaunchStatus"
    Write-Host ""
    Write-Host "Manual MP5 checklist"
    Write-Host "1. Confirm the stock client connected to the disposable server at $ConnectAddress."
    Write-Host "2. Confirm the player received weapon_9mmAR and usable 9mm ammo."
    if ($LabDummy) {
        Write-Host "3. Confirm the lab dummy appeared in front of the player."
        Write-Host "4. Confirm the active MP5 profile is $(if ([string]::IsNullOrWhiteSpace($Mp5Profile)) { 'baseline' } else { $Mp5Profile }) and the target profile is $(if ([string]::IsNullOrWhiteSpace($LabTargetProfile)) { 'default' } else { $LabTargetProfile })."
        Write-Host "5. Fire a short controlled burst after settling."
        Write-Host "6. Fire a longer burst to exercise burst-growth evidence."
        Write-Host "7. Fire while moving to exercise movement-spread evidence."
        Write-Host "8. Attempt at least one headshot and confirm at least one dummy kill."
        Write-Host "9. If the target is armored or head-protected, compare torso and head attempts."
        Write-Host "10. Analyze afterward with .\\scripts\\analyze-weapon-log.ps1 -Latest -Weapon mp5."
    }
    else {
        Write-Host "3. Fire a short controlled burst after settling."
        Write-Host "4. Fire a longer burst to exercise burst-growth evidence."
        Write-Host "5. Fire while moving to exercise movement-spread evidence."
        Write-Host "6. Attempt at least one headshot."
        Write-Host "7. Analyze afterward with .\\scripts\\analyze-weapon-log.ps1 -Latest -Weapon mp5."
    }

    Write-Host ""
    Write-Host "This is server-authoritative experimentation for stock clients. It does not validate client-side recoil feel or exact Counter-Strike parity."
}

function Invoke-AnalyzeLatestWeaponLog {
    param(
        [string]$WeaponLogPath
    )

    $analysisScript = Join-Path $PSScriptRoot "analyze-weapon-log.ps1"
    $analysisArguments = @("-Weapon", "mp5")

    if (-not [string]::IsNullOrWhiteSpace($WeaponLogPath) -and (Test-LeafPath -Path $WeaponLogPath)) {
        $analysisArguments += @("-Path", $WeaponLogPath)
    }
    else {
        $analysisArguments += "-Latest"
    }

    Write-Host ""
    Write-Host "Press Enter after the manual firing pass to analyze the latest MP5 telemetry log."
    [void](Read-Host)

    & $analysisScript @analysisArguments
}

$configuration = Get-ValidatedConfiguration -Configuration $Configuration

if (-not $DetachedServer) {
    throw "run-mp5-test-session.ps1 always uses a detached HLDS process so it can wait for readiness, open log tailing, and print the manual checklist."
}

if ($TimeoutSeconds -lt 1) {
    throw "TimeoutSeconds must be at least 1."
}

$resolvedPort = Get-AvailableUdpPort -PreferredPort $Port
if ($resolvedPort -ne $Port) {
    Write-Step "Requested port $Port was busy, using free UDP port $resolvedPort instead"
}

$runtimeRoot = Get-TestbedRuntimeRoot
$sessionGameDir = if ($PreferClientMatchedRuntime) { Get-TestbedLiveModName } else { "valve" }
$previousWeaponLog = Get-LatestWeaponDebugLog
$useLabDummy = $LabDummy -or (-not [string]::IsNullOrWhiteSpace($LabTargetProfile))
$maxPlayers = 1

Write-Step $(if ($useLabDummy) { "Installing disposable runtime for the MP5 lab session" } else { "Installing disposable runtime for the MP5 manual session" })
& "$PSScriptRoot\install-testbed.ps1" -Configuration $configuration -TemplateRoot $TemplateRoot -HldsExe $HldsExe -HlExe $HlExe -SteamCmdExe $SteamCmdExe -AllowSteamCmdDownload:$AllowSteamCmdDownload -PreferClientMatchedRuntime:$PreferClientMatchedRuntime

if ($PreferClientMatchedRuntime) {
    $clientInstall = Resolve-TestbedClientInstall -ExplicitHlExe $HlExe
    if ($clientInstall) {
        $runtimeRoot = Get-TestbedLiveModLinkPath -ClientRoot $clientInstall.Root -GameDirName $sessionGameDir
    }
}

Write-Step $(if ($useLabDummy) { "Launching disposable HLDS in MP5 experimental lab mode" } else { "Launching disposable HLDS in MP5 experimental mode" })
$sessionMetadataCvars = @(Get-SessionMetadataLaunchAssignments -SessionTag $SessionTag -MatrixName $MatrixName -MatrixStep $MatrixStep -WeaponUnderTest mp5)
$effectiveSetCvars = @($sessionMetadataCvars) + @($SetCvar)
$launchInfo = & "$PSScriptRoot\run-server.ps1" `
    -Configuration $configuration `
    -Map $Map `
    -Port $resolvedPort `
    -MaxPlayers $maxPlayers `
    -Detached `
    -EnableExperimentalMp5 `
    -EnableExperimentalWeaponDebug `
    -EnableMp5LabLoadout `
    -EnableGlockLabDummy:$useLabDummy `
    -Mp5Profile $Mp5Profile `
    -LabTargetProfile $LabTargetProfile `
    -SetCvar $effectiveSetCvars `
    -TemplateRoot $TemplateRoot `
    -HldsExe $HldsExe `
    -HlExe $HlExe `
    -SteamCmdExe $SteamCmdExe `
    -AllowSteamCmdDownload:$AllowSteamCmdDownload `
    -PreferClientMatchedRuntime:$PreferClientMatchedRuntime `
    -PassThru

$weaponLog = Wait-ForSessionWeaponDebugLog -PreviousLatestLog $previousWeaponLog -TimeoutSeconds $TimeoutSeconds -Process $launchInfo.Process
$serverReady = $true

if (-not $weaponLog) {
    $serverReady = Wait-ForHldsReady -LaunchInfo $launchInfo -Map $Map -TimeoutSeconds $TimeoutSeconds
    if (-not $serverReady) {
        throw "Timed out waiting for either the MP5 session-ready weapon log or the HLDS map-start marker for '$Map'. Check $($launchInfo.StdOutLog) and $($launchInfo.StdErrLog)."
    }
}

$tailWindowRequested = $false

if (-not $NoTail) {
    if ($weaponLog) {
        try {
            Start-WeaponLogTailWindow -WeaponLogPath $weaponLog.FullName
            $tailWindowRequested = $true
        }
        catch {
            Write-Host "Unable to open a separate tail window automatically. Run .\\scripts\\tail-weapon-log.ps1 -Path `"$($weaponLog.FullName)`" in another PowerShell window."
        }
    }
    else {
        Write-Host "Weapon debug log path was not ready before the timeout. Run .\\scripts\\tail-weapon-log.ps1 once the session header appears."
    }
}

$connectAddress = "127.0.0.1:$resolvedPort"
$clientLaunchStatus = "disabled by -NoClient"

if (-not $NoClient) {
    $clientExe = Get-TestbedSessionClientExe -RuntimeRoot $runtimeRoot -ExplicitHlExe $HlExe -PreferClientMatchedRuntime:$PreferClientMatchedRuntime
    if ($clientExe) {
        $clientWorkingDirectory = if ($PreferClientMatchedRuntime) { Split-Path -Parent $clientExe } else { $null }
        & "$PSScriptRoot\run-client.ps1" -ConnectAddress $connectAddress -HlExe $clientExe -Game $sessionGameDir -WorkingDirectory $clientWorkingDirectory
        $clientLaunchStatus = "launch requested via $clientExe (-game $sessionGameDir)"
    }
    else {
        $clientLaunchStatus = "skipped because no stock hl.exe was found for the live session"
        Write-Host "No stock Half-Life client executable was found. Set HL_EXE in .env if your Half-Life root is not auto-detected."
    }
}

if (-not $SkipChecklist) {
    Write-Checklist -Configuration $configuration -Port $resolvedPort -Map $Map -ConnectAddress $connectAddress -RuntimeRoot $runtimeRoot -ServerLogPath $launchInfo.StdOutLog -Mp5Profile $Mp5Profile -LabTargetProfile $LabTargetProfile -WeaponLogPath $(if ($weaponLog) { $weaponLog.FullName } else { $null }) -TailWindowRequested $tailWindowRequested -ClientLaunchStatus $clientLaunchStatus -LabDummy:$useLabDummy -GameDirName $sessionGameDir
}

if ($AnalyzeLatestOnExit) {
    Invoke-AnalyzeLatestWeaponLog -WeaponLogPath $(if ($weaponLog) { $weaponLog.FullName } else { $null })
}

if ($PassThru) {
    return [PSCustomObject]@{
        Configuration = $configuration
        Map = $Map
        Port = $resolvedPort
        ConnectAddress = $connectAddress
        RuntimeRoot = $runtimeRoot
        ServerLogPath = $launchInfo.StdOutLog
        ServerErrorLogPath = $launchInfo.StdErrLog
        WeaponLogPath = if ($weaponLog) { $weaponLog.FullName } else { $null }
        TailWindowRequested = $tailWindowRequested
        ClientLaunchStatus = $clientLaunchStatus
        WeaponUnderTest = "mp5"
        WeaponProfile = if ([string]::IsNullOrWhiteSpace($Mp5Profile)) { "baseline" } else { $Mp5Profile }
        Mp5Profile = if ([string]::IsNullOrWhiteSpace($Mp5Profile)) { "baseline" } else { $Mp5Profile }
        LabTargetProfile = if ([string]::IsNullOrWhiteSpace($LabTargetProfile)) { "default" } else { $LabTargetProfile }
        LabDummy = $useLabDummy
        SessionTag = $SessionTag
        MatrixName = $MatrixName
        MatrixStep = $MatrixStep
        LaunchInfo = $launchInfo
        Process = $launchInfo.Process
    }
}
