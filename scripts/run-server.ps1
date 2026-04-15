[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$Map = "crossfire",
    [int]$Port = 27015,
    [int]$MaxPlayers = 4,
    [string[]]$SetCvar = @(),
    [hashtable]$Cvars,
    [switch]$Detached,
    [string]$TemplateRoot,
    [string]$HldsExe,
    [string]$HlExe,
    [string]$SteamCmdExe,
    [switch]$AllowSteamCmdDownload
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

$configuration = Get-ValidatedConfiguration -Configuration $Configuration
$runtimeHldsExe = Join-RepoPath "testbed\runtime\hlds.exe"
$runtimeDll = Join-RepoPath "testbed\runtime\valve\dlls\hl.dll"

if ((-not (Test-LeafPath -Path $runtimeHldsExe)) -or (-not (Test-LeafPath -Path $runtimeDll))) {
    & "$PSScriptRoot\install-testbed.ps1" -Configuration $configuration -TemplateRoot $TemplateRoot -HldsExe $HldsExe -HlExe $HlExe -SteamCmdExe $SteamCmdExe -AllowSteamCmdDownload:$AllowSteamCmdDownload
}

$runtimeRoot = Join-RepoPath "testbed\runtime"
Assert-UdpPortAvailable -Port $Port

if ($Detached) {
    Write-Step "Launching HLDS in detached mode"
    $launchInfo = Start-HldsDetached -RuntimeRoot $runtimeRoot -Map $Map -Port $Port -MaxPlayers $MaxPlayers -SetCvar $SetCvar -Cvars $Cvars
    Write-Host "PID: $($launchInfo.Process.Id)"
    Write-Host "stdout log: $($launchInfo.StdOutLog)"
    Write-Host "stderr log: $($launchInfo.StdErrLog)"
}
else {
    Write-Step "Launching HLDS in the foreground"
    $logPath = Invoke-HldsForeground -RuntimeRoot $runtimeRoot -Map $Map -Port $Port -MaxPlayers $MaxPlayers -SetCvar $SetCvar -Cvars $Cvars
    Write-Host "Log: $logPath"
}
