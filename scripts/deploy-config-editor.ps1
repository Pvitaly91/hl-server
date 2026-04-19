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

function Test-CanMarkManagedLiveModFolder {
    param([string]$LiveModRoot)

    if ([string]::IsNullOrWhiteSpace($LiveModRoot) -or -not (Test-Path -LiteralPath $LiveModRoot)) {
        return $false
    }

    $allowedFileNames = @(
        "HlConfigEditorCpp.exe",
        "HlConfigEditorCpp.pdb",
        "hlserver_launch.cfg",
        "liblist.gam",
        ".hl-server-live-mod.txt",
        ".hl-server-live-mod.json"
    )

    $allowedDirectories = @(
        "cfg_profiles",
        "logs"
    )

    $items = Get-ChildItem -LiteralPath $LiveModRoot -Force -ErrorAction SilentlyContinue
    foreach ($item in $items) {
        if ($item.PSIsContainer) {
            if ($allowedDirectories -contains $item.Name) {
                continue
            }

            return $false
        }

        if ($allowedFileNames -contains $item.Name) {
            continue
        }

        if ($item.Name.EndsWith(".hlcfg.json", [System.StringComparison]::OrdinalIgnoreCase) -or
            $item.Extension.Equals(".cfg", [System.StringComparison]::OrdinalIgnoreCase)) {
            continue
        }

        return $false
    }

    return $true
}

function Ensure-ManagedLiveModMarker {
    param(
        [string]$LiveModRoot,
        [string]$HalfLifeRoot
    )

    if ([string]::IsNullOrWhiteSpace($LiveModRoot) -or -not (Test-Path -LiteralPath $LiveModRoot)) {
        return
    }

    $markerPath = Join-Path $LiveModRoot ".hl-server-live-mod.txt"
    if (Test-Path -LiteralPath $markerPath) {
        return
    }

    if (-not (Test-CanMarkManagedLiveModFolder -LiveModRoot $LiveModRoot)) {
        Write-DeployWarning "Skipped writing the managed live-mod marker because $LiveModRoot contains files that were not recognized as safe editor-deployment content."
        return
    }

    @"
Managed same-root live mod placeholder prepared by hl-server editor deployment.
Client root: $HalfLifeRoot
Game dir: hlserver_testbed
Purpose: preserve root-level cfg exports and allow the live launcher to refresh this folder later.
Timestamp: $(Get-Date -Format o)
"@ | Set-Content -LiteralPath $markerPath -Encoding ASCII

    Write-DeployInfo "Wrote managed live-mod marker: $markerPath"
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
    Write-DeployWarning "Failed to copy the editor into $destinationPath. Built executable remains at $resolvedBuiltExe. $($_.Exception.Message)"
    exit 0
}

try {
    Ensure-ManagedLiveModMarker -LiveModRoot $liveModRoot -HalfLifeRoot $halfLifeRoot
}
catch {
    Write-DeployWarning "The editor was copied, but the managed live-mod marker could not be updated in $liveModRoot. $($_.Exception.Message)"
}

Write-DeployInfo "Built executable  : $resolvedBuiltExe"
Write-DeployInfo "Deployed executable: $destinationPath"
exit 0
