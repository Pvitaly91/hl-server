[CmdletBinding()]
param(
    [ValidateSet("glock", "mp5")]
    [string]$Mode = "glock",

    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$Map = "crossfire",
    [int]$Port = 27015,
    [int]$MaxPlayers = 16,
    [string]$HlExe,
    [string]$GlockProfile = "cs_tight",
    [string]$Mp5Profile = "cs_burst",
    [string]$TargetProfile = "vest_headprotected",
    [switch]$NoClient,
    [switch]$NoAutoConnect
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

function Get-PreferredLanIpv4 {
    try {
        return @(
            Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
            Where-Object {
                $_.IPAddress -and
                $_.IPAddress -notlike "127.*" -and
                $_.IPAddress -notlike "169.254*"
            } |
            Select-Object -ExpandProperty IPAddress -Unique
        ) | Select-Object -First 1
    }
    catch {
        return $null
    }
}

function Get-DemoLaunchAssignments {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Mode,
        [Parameter(Mandatory = $true)]
        [string]$GlockProfile,
        [Parameter(Mandatory = $true)]
        [string]$Mp5Profile,
        [Parameter(Mandatory = $true)]
        [string]$TargetProfile
    )

    $assignments = @(
        "sv_exp_debug_weaponlog=1",
        "sv_exp_debug_weaponlog_rejections=1",
        "sv_exp_session_tag=direct_manual_demo"
    )

    switch ($Mode) {
        "glock" {
            $assignments += @("sv_exp_weapon_under_test=glock")
            $assignments += @(Get-ExperimentalGlockLaunchAssignments)
            $assignments += @(Get-GlockProfileLaunchAssignments -Name $GlockProfile)
            $assignments += @(Get-GlockLabDummyLaunchAssignments)
            $assignments += @(Get-GlockLabTargetLaunchAssignments -Name $TargetProfile)
        }
        "mp5" {
            $assignments += @("sv_exp_weapon_under_test=mp5")
            $assignments += @(Get-ExperimentalMp5LaunchAssignments)
            $assignments += @(Get-Mp5ProfileLaunchAssignments -Name $Mp5Profile)
            $assignments += @(Get-Mp5LabLoadoutLaunchAssignments)
            $assignments += @(Get-GlockLabDummyLaunchAssignments)
            $assignments += @(Get-GlockLabTargetLaunchAssignments -Name $TargetProfile)
        }
    }

    return $assignments
}

$configuration = Get-ValidatedConfiguration -Configuration $Configuration
$gameDirName = Get-TestbedLiveModName
$connectAddress = "127.0.0.1:$Port"
$lanAddress = Get-PreferredLanIpv4
$effectiveProfile = if ($Mode -eq "glock") { $GlockProfile } else { $Mp5Profile }

Write-Step "Preparing the direct Half-Life live mod"
& "$PSScriptRoot\install-testbed.ps1" -Configuration $configuration -HlExe $HlExe -PreferClientMatchedRuntime -BuildIfMissing

$clientInstall = Resolve-TestbedClientInstall -ExplicitHlExe $HlExe
if (-not $clientInstall) {
    throw "No stock Half-Life client was resolved. Set HL_EXE in .env or pass -HlExe <path>."
}

$doctorReport = Get-TestbedDoctorReport -Configuration $configuration -ExplicitHlExe $HlExe -PreferClientMatchedRuntime -IgnoreLatestLaunchFailure
if (-not $doctorReport.IsHealthy) {
    throw (Get-TestbedDoctorFailureMessage -Report $doctorReport)
}

$clientRoot = $clientInstall.Root
$hldsExe = $clientInstall.Probe.HldsExe
$hlExe = $clientInstall.HlExe
$modRoot = Get-TestbedLiveModRoot -ClientRoot $clientRoot -GameDirName $gameDirName

$setCvar = @(Get-DemoLaunchAssignments -Mode $Mode -GlockProfile $GlockProfile -Mp5Profile $Mp5Profile -TargetProfile $TargetProfile)
$launchCvars = Get-HldsLaunchCvars -SetCvar $setCvar
$serverCfgName = Write-HldsLaunchCvarConfig -RuntimeRoot $clientRoot -Cvars $launchCvars -GameDirName $gameDirName
$serverArgs = Get-HldsArgumentList -Map $Map -Port $Port -MaxPlayers $MaxPlayers -ServerCfgName $serverCfgName -GameDirName $gameDirName
$launchTimestamp = Get-Date -Format "yyyyMMdd-HHmmss"

Write-Host ""
Write-Host "Direct hlserver_testbed launcher"
Write-Host "  mode           : $Mode"
Write-Host "  profile        : $effectiveProfile"
Write-Host "  target profile : $TargetProfile"
Write-Host "  game dir       : $gameDirName"
Write-Host "  client root    : $clientRoot"
Write-Host "  mod root       : $modRoot"
Write-Host "  launch hlds    : $hldsExe"
Write-Host "  launch hl      : $(if ($NoClient) { 'disabled by -NoClient' } else { $hlExe })"
Write-Host "  map            : $Map"
Write-Host "  port           : $Port"
Write-Host "  logs           : $(Get-TestbedLogsRoot)"
Write-Host "  connect local  : connect $connectAddress"
if (-not [string]::IsNullOrWhiteSpace($lanAddress)) {
    Write-Host "  connect lan    : connect ${lanAddress}:$Port"
}
Write-Host "  server command : hlds.exe $($serverArgs -join ' ')"
Write-Host "  client command : hl.exe -game $gameDirName -console$(if ($NoAutoConnect -or $NoClient) { '' } else { ' +connect ' + $connectAddress })"

$environmentState = Push-HldsRuntimeEnvironment -RuntimeRoot $clientRoot
try {
    Write-HldsLaunchMetadata -RuntimeRoot $clientRoot -HldsExe $hldsExe -ArgumentList $serverArgs -Timestamp $launchTimestamp -ServerCfgName $serverCfgName -SteamAppId $environmentState.EffectiveSteamAppId | Out-Null
    $serverProcess = Start-Process -FilePath $hldsExe -WorkingDirectory $clientRoot -ArgumentList $serverArgs -PassThru
}
finally {
    Pop-HldsRuntimeEnvironment -State $environmentState
}

$launchInfo = [PSCustomObject]@{
    Process = $serverProcess
    RuntimeRoot = $clientRoot
    GameDirName = $gameDirName
    StdOutLog = Join-Path (Get-TestbedLogsRoot) ("hlds-" + $launchTimestamp + "-stdout.log")
    StdErrLog = Join-Path (Get-TestbedLogsRoot) ("hlds-" + $launchTimestamp + "-stderr.log")
    RuntimeLogsRoot = Join-Path $clientRoot "logs"
    QConsoleLog = Join-Path $clientRoot "qconsole.log"
    ValveQConsoleLog = Join-Path $clientRoot ($gameDirName + "\qconsole.log")
    LaunchMetadataPath = Join-Path (Get-TestbedLogsRoot) ("hlds-" + $launchTimestamp + "-launch.txt")
}

if (-not (Wait-ForHldsReady -LaunchInfo $launchInfo -Map $Map -TimeoutSeconds 20)) {
    throw "HLDS did not become ready within 20 seconds. Check $($launchInfo.LaunchMetadataPath) and Half-Life\\qconsole.log."
}

if (-not $NoClient) {
    $clientArgs = @("-game", $gameDirName, "-console")
    if (-not $NoAutoConnect) {
        $clientArgs += @("+connect", $connectAddress)
    }

    $environmentState = Push-HldsRuntimeEnvironment -RuntimeRoot $clientRoot
    try {
        Start-Process -FilePath $hlExe -WorkingDirectory $clientRoot -ArgumentList $clientArgs | Out-Null
    }
    finally {
        Pop-HldsRuntimeEnvironment -State $environmentState
    }
}

Write-Host ""
Write-Host "Server started from: $clientRoot"
Write-Host "HLDS PID          : $($serverProcess.Id)"
Write-Host "Manual connect    : connect $connectAddress"
if (-not [string]::IsNullOrWhiteSpace($lanAddress)) {
    Write-Host "Manual LAN connect: connect ${lanAddress}:$Port"
}
Write-Host ""
switch ($Mode) {
    "glock" {
        Write-Host "What to check in-game:"
        Write-Host "  1. A dummy target should appear in front of the player."
        Write-Host "  2. Glock first-shot and movement spread should feel tighter than stock."
        Write-Host "  3. Headshots against the default armored dummy should be much stronger."
    }
    "mp5" {
        Write-Host "What to check in-game:"
        Write-Host "  1. The player should receive MP5 plus ammo automatically."
        Write-Host "  2. A dummy target should appear in front of the player."
        Write-Host "  3. Short bursts should feel tighter than long sprays."
    }
}
