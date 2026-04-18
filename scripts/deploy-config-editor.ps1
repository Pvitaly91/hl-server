[CmdletBinding()]
param(
    [string]$BuiltExe,
    [string]$ProjectDir
)

$ErrorActionPreference = "Stop"

function Write-DeployInfo {
    param([string]$Message)
    Write-Host "[HlConfigEditorCpp deploy] $Message"
}

function Write-DeployWarning {
    param([string]$Message)
    Write-Warning "[HlConfigEditorCpp deploy] $Message"
}

function Get-RepoRoot {
    param([string]$ProjectDirectory)

    if ([string]::IsNullOrWhiteSpace($ProjectDirectory)) {
        return $null
    }

    try {
        return (Resolve-Path (Join-Path $ProjectDirectory "..\\..")).Path
    }
    catch {
        return $null
    }
}

function Get-EnvFileValues {
    param([string]$RepoRoot)

    $values = @{}
    if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
        return $values
    }

    $envPath = Join-Path $RepoRoot ".env"
    if (-not (Test-Path -LiteralPath $envPath)) {
        return $values
    }

    foreach ($line in Get-Content -LiteralPath $envPath) {
        $trimmed = $line.Trim()
        if ([string]::IsNullOrWhiteSpace($trimmed) -or $trimmed.StartsWith("#")) {
            continue
        }

        $separatorIndex = $trimmed.IndexOf("=")
        if ($separatorIndex -le 0) {
            continue
        }

        $key = $trimmed.Substring(0, $separatorIndex).Trim()
        $value = $trimmed.Substring($separatorIndex + 1).Trim().Trim("'").Trim('"')
        $values[$key] = $value
    }

    return $values
}

function Get-RootFromExePath {
    param([string]$ExePath)

    if ([string]::IsNullOrWhiteSpace($ExePath) -or -not (Test-Path -LiteralPath $ExePath)) {
        return $null
    }

    return Split-Path -Parent (Resolve-Path -LiteralPath $ExePath).Path
}

function Resolve-HalfLifeRoot {
    param([hashtable]$EnvFileValues)

    $candidateRoots = @(
        (Get-RootFromExePath -ExePath $env:HL_EXE),
        (Get-RootFromExePath -ExePath $env:HLDS_EXE),
        (Get-RootFromExePath -ExePath $EnvFileValues["HL_EXE"]),
        (Get-RootFromExePath -ExePath $EnvFileValues["HLDS_EXE"]),
        "D:\\Steam\\steamapps\\common\\Half-Life",
        "D:\\SteamLibrary\\steamapps\\common\\Half-Life",
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Half-Life",
        "C:\\Steam\\steamapps\\common\\Half-Life"
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }

    foreach ($candidate in $candidateRoots) {
        if ((Test-Path -LiteralPath (Join-Path $candidate "hl.exe")) -or
            (Test-Path -LiteralPath (Join-Path $candidate "hlds.exe"))) {
            return $candidate
        }
    }

    return $null
}

if ([string]::IsNullOrWhiteSpace($BuiltExe) -or -not (Test-Path -LiteralPath $BuiltExe)) {
    Write-DeployWarning "Built executable was not found. Deployment was skipped."
    exit 0
}

$resolvedBuiltExe = (Resolve-Path -LiteralPath $BuiltExe).Path
$repoRoot = Get-RepoRoot -ProjectDirectory $ProjectDir
$envFileValues = Get-EnvFileValues -RepoRoot $repoRoot
$halfLifeRoot = Resolve-HalfLifeRoot -EnvFileValues $envFileValues

if ([string]::IsNullOrWhiteSpace($halfLifeRoot)) {
    Write-DeployWarning "Half-Life root could not be resolved. Built executable remains at $resolvedBuiltExe. Set HL_EXE or HLDS_EXE in .env to enable live-mod deployment."
    exit 0
}

$liveModRoot = Join-Path $halfLifeRoot "hlserver_testbed"
try {
    New-Item -ItemType Directory -Force -Path $liveModRoot | Out-Null
}
catch {
    Write-DeployWarning "Live mod folder could not be prepared at $liveModRoot. Built executable remains at $resolvedBuiltExe."
    exit 0
}

$destinationPath = Join-Path $liveModRoot "HlConfigEditorCpp.exe"
try {
    Copy-Item -LiteralPath $resolvedBuiltExe -Destination $destinationPath -Force
}
catch {
    Write-DeployWarning "Failed to copy the editor into $destinationPath. Built executable remains at $resolvedBuiltExe."
    exit 0
}

Write-DeployInfo "Built executable  : $resolvedBuiltExe"
Write-DeployInfo "Deployed executable: $destinationPath"
exit 0
