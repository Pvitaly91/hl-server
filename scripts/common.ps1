Set-StrictMode -Version 3.0
$ErrorActionPreference = "Stop"

$script:ValveSdkUpstreamUrl = "https://github.com/ValveSoftware/halflife.git"
$script:ValveSdkUpstreamRef = "master"
$script:ValveSdkUpstreamCommit = "b1b5cf5892918535619b2937bb927e46cb097ba1"
$script:SteamCmdDownloadUrls = @(
    "https://steamcdn-a.akamaihd.net/client/installer/steamcmd.zip",
    "https://media.steampowered.com/installer/steamcmd.zip"
)
$script:EnvLoaded = $false

function Write-Step {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    Write-Host "==> $Message"
}

function Get-RepoRoot {
    return (Split-Path -Parent $PSScriptRoot)
}

function Join-RepoPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RelativePath
    )

    return (Join-Path (Get-RepoRoot) $RelativePath)
}

function Get-TestbedRoot {
    return (Join-RepoPath "testbed")
}

function Get-TestbedRuntimeRoot {
    return (Join-Path (Get-TestbedRoot) "runtime")
}

function Get-TestbedLogsRoot {
    return (Join-Path (Get-TestbedRoot) "logs")
}

function Get-FullPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    return [System.IO.Path]::GetFullPath($Path.Trim('"'))
}

function Ensure-Directory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
    }
}

function Assert-PathWithinRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Root,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    $fullPath = Get-FullPath -Path $Path
    $fullRoot = Get-FullPath -Path $Root

    if (-not $fullRoot.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
        $fullRoot = $fullRoot + [System.IO.Path]::DirectorySeparatorChar
    }

    if (-not $fullPath.StartsWith($fullRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay under $fullRoot. Refusing to operate on $fullPath."
    }
}

function Reset-DisposableDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$AllowedRoot
    )

    Assert-PathWithinRoot -Path $Path -Root $AllowedRoot -Description "Disposable path"

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }

    New-Item -ItemType Directory -Path $Path -Force | Out-Null
}

function Copy-DirectoryContents {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,

        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    Ensure-Directory -Path $Destination

    Get-ChildItem -LiteralPath $Source -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $Destination -Recurse -Force
    }
}

function Import-HLServerEnv {
    if ($script:EnvLoaded) {
        return
    }

    $envFile = Join-RepoPath ".env"

    if (Test-Path -LiteralPath $envFile -PathType Leaf) {
        foreach ($line in Get-Content -LiteralPath $envFile) {
            $trimmed = $line.Trim()

            if ([string]::IsNullOrWhiteSpace($trimmed) -or $trimmed.StartsWith("#")) {
                continue
            }

            $separatorIndex = $trimmed.IndexOf("=")
            if ($separatorIndex -lt 1) {
                continue
            }

            $name = $trimmed.Substring(0, $separatorIndex).Trim()
            $value = $trimmed.Substring($separatorIndex + 1).Trim()

            if (($value.StartsWith('"') -and $value.EndsWith('"')) -or ($value.StartsWith("'") -and $value.EndsWith("'"))) {
                $value = $value.Substring(1, $value.Length - 2)
            }

            [System.Environment]::SetEnvironmentVariable($name, $value, "Process")
        }
    }

    $script:EnvLoaded = $true
}

function Get-ValidatedConfiguration {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Configuration
    )

    switch ($Configuration.ToLowerInvariant()) {
        "debug" { return "Debug" }
        "release" { return "Release" }
        default { throw "Unsupported configuration '$Configuration'. Use Debug or Release." }
    }
}

function Get-BuildPresetName {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Configuration
    )

    switch ((Get-ValidatedConfiguration -Configuration $Configuration)) {
        "Debug" { return "debug" }
        "Release" { return "release" }
    }
}

function Get-BuildDirectory {
    return (Join-RepoPath "build\vs2022-win32")
}

function Get-SolutionPath {
    return (Join-Path (Get-BuildDirectory) "hl_server.sln")
}

function Get-ArtifactDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Configuration
    )

    return (Join-RepoPath ("artifacts\" + (Get-ValidatedConfiguration -Configuration $Configuration)))
}

function Get-HlDllPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Configuration
    )

    return (Join-Path (Get-ArtifactDirectory -Configuration $Configuration) "hl.dll")
}

function Get-HlPdbPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Configuration
    )

    return (Join-Path (Get-ArtifactDirectory -Configuration $Configuration) "hl.pdb")
}

function Test-LeafPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    return (Test-Path -LiteralPath $Path -PathType Leaf)
}

function Resolve-ExecutablePath {
    param(
        [string]$ExplicitPath,
        [Parameter(Mandatory = $true)]
        [string]$EnvName,
        [Parameter(Mandatory = $true)]
        [string[]]$AutoCandidates,
        [Parameter(Mandatory = $true)]
        [string]$DisplayName
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        $fullPath = Get-FullPath -Path $ExplicitPath
        if (Test-LeafPath -Path $fullPath) {
            return $fullPath
        }

        throw "$DisplayName was provided explicitly but does not exist: $fullPath"
    }

    $envPath = [System.Environment]::GetEnvironmentVariable($EnvName, "Process")
    if (-not [string]::IsNullOrWhiteSpace($envPath)) {
        $fullPath = Get-FullPath -Path $envPath
        if (Test-LeafPath -Path $fullPath) {
            return $fullPath
        }

        throw "$EnvName is set but does not exist: $fullPath"
    }

    foreach ($candidate in $AutoCandidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }

        $fullCandidate = Get-FullPath -Path $candidate
        if (Test-LeafPath -Path $fullCandidate) {
            return $fullCandidate
        }
    }

    return $null
}

function Resolve-DirectoryPath {
    param(
        [string]$ExplicitPath,
        [Parameter(Mandatory = $true)]
        [string]$EnvName,
        [Parameter(Mandatory = $true)]
        [string]$DisplayName
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        $fullPath = Get-FullPath -Path $ExplicitPath
        if (Test-Path -LiteralPath $fullPath -PathType Container) {
            return $fullPath
        }

        throw "$DisplayName was provided explicitly but does not exist: $fullPath"
    }

    $envPath = [System.Environment]::GetEnvironmentVariable($EnvName, "Process")
    if (-not [string]::IsNullOrWhiteSpace($envPath)) {
        $fullPath = Get-FullPath -Path $envPath
        if (Test-Path -LiteralPath $fullPath -PathType Container) {
            return $fullPath
        }

        throw "$EnvName is set but does not exist: $fullPath"
    }

    return $null
}

function Get-SteamInstallRoots {
    $roots = New-Object System.Collections.Generic.List[string]

    foreach ($registryPath in @(
        "HKCU:\Software\Valve\Steam",
        "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam",
        "HKLM:\SOFTWARE\Valve\Steam"
    )) {
        if (Test-Path -LiteralPath $registryPath) {
            $properties = Get-ItemProperty -LiteralPath $registryPath -ErrorAction SilentlyContinue
            foreach ($name in @("SteamPath", "InstallPath")) {
                $property = $properties.PSObject.Properties[$name]
                $value = if ($property) { $property.Value } else { $null }
                if (-not [string]::IsNullOrWhiteSpace($value)) {
                    $roots.Add($value.Replace("/", "\"))
                }
            }
        }
    }

    foreach ($candidate in @(
        (Join-Path ([System.Environment]::GetFolderPath("ProgramFilesX86")) "Steam"),
        (Join-Path ([System.Environment]::GetFolderPath("ProgramFiles")) "Steam"),
        "C:\Steam"
    )) {
        if (-not [string]::IsNullOrWhiteSpace($candidate)) {
            $roots.Add($candidate)
        }
    }

    return ($roots | Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and (Test-Path -LiteralPath $_ -PathType Container) } | Sort-Object -Unique)
}

function Get-HldsAutoCandidates {
    $candidates = New-Object System.Collections.Generic.List[string]

    foreach ($root in Get-SteamInstallRoots) {
        $candidates.Add((Join-Path $root "steamapps\common\Half-Life Dedicated Server\hlds.exe"))
        $candidates.Add((Join-Path $root "steamapps\common\Half-Life\hlds.exe"))
    }

    $candidates.Add((Join-RepoPath "testbed\cache\hlds-template\hlds.exe"))
    return $candidates.ToArray()
}

function Get-HlAutoCandidates {
    $candidates = New-Object System.Collections.Generic.List[string]

    foreach ($root in Get-SteamInstallRoots) {
        $candidates.Add((Join-Path $root "steamapps\common\Half-Life\hl.exe"))
    }

    return $candidates.ToArray()
}

function Get-SteamCmdAutoCandidates {
    $candidates = New-Object System.Collections.Generic.List[string]

    foreach ($root in Get-SteamInstallRoots) {
        $candidates.Add((Join-Path $root "steamcmd.exe"))
    }

    $candidates.Add("C:\steamcmd\steamcmd.exe")
    $candidates.Add((Join-RepoPath "testbed\cache\steamcmd\steamcmd.exe"))
    return $candidates.ToArray()
}

function Resolve-HldsExe {
    param(
        [string]$ExplicitPath
    )

    Import-HLServerEnv
    return (Resolve-ExecutablePath -ExplicitPath $ExplicitPath -EnvName "HLDS_EXE" -AutoCandidates (Get-HldsAutoCandidates) -DisplayName "HLDS executable")
}

function Resolve-HlExe {
    param(
        [string]$ExplicitPath
    )

    Import-HLServerEnv
    return (Resolve-ExecutablePath -ExplicitPath $ExplicitPath -EnvName "HL_EXE" -AutoCandidates (Get-HlAutoCandidates) -DisplayName "Half-Life executable")
}

function Get-GitPath {
    $git = Get-Command git.exe -ErrorAction SilentlyContinue
    if (-not $git) {
        $git = Get-Command git -ErrorAction SilentlyContinue
    }

    if (-not $git) {
        throw "Unable to locate git. Install Git for Windows and retry."
    }

    return $git.Source
}

function Get-VSWherePath {
    $vswhere = Join-Path ([System.Environment]::GetFolderPath("ProgramFilesX86")) "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-LeafPath -Path $vswhere) {
        return $vswhere
    }

    return $null
}

function Get-VSInstallationPath {
    $vswhere = Get-VSWherePath
    if (-not $vswhere) {
        return $null
    }

    $installationPath = & $vswhere -latest -products * -version "[17.0,18.0)" -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0) {
        return $null
    }

    $path = ($installationPath | Select-Object -First 1).Trim()
    if (-not [string]::IsNullOrWhiteSpace($path) -and (Test-Path -LiteralPath $path -PathType Container)) {
        return $path
    }

    return $null
}

function Get-CMakePath {
    $cmake = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if (-not $cmake) {
        $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    }

    if ($cmake) {
        return $cmake.Source
    }

    $vsInstall = Get-VSInstallationPath
    if ($vsInstall) {
        $vsCMake = Join-Path $vsInstall "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if (Test-LeafPath -Path $vsCMake) {
            return $vsCMake
        }
    }

    throw "Unable to locate cmake.exe. Install Visual Studio 2022 Desktop development with C++."
}

function Assert-BuildPrerequisites {
    Get-GitPath | Out-Null

    if (-not (Get-VSInstallationPath)) {
        throw "Visual Studio 2022 with x86/x64 C++ tools was not found."
    }

    Get-CMakePath | Out-Null
}

function Install-SteamCmd {
    $steamCmdRoot = Join-RepoPath "testbed\cache\steamcmd"
    $steamCmdExe = Join-Path $steamCmdRoot "steamcmd.exe"

    if (Test-LeafPath -Path $steamCmdExe) {
        return $steamCmdExe
    }

    Write-Step "Downloading SteamCMD into testbed/cache"

    Ensure-Directory -Path (Split-Path -Parent $steamCmdRoot)
    Ensure-Directory -Path $steamCmdRoot

    $zipPath = Join-RepoPath "testbed\cache\steamcmd.zip"

    $downloaded = $false
    foreach ($url in $script:SteamCmdDownloadUrls) {
        try {
            Invoke-WebRequest -Uri $url -OutFile $zipPath -UseBasicParsing
            $downloaded = $true
            break
        }
        catch {
            Write-Host "SteamCMD download attempt failed: $url"
        }
    }

    if (-not $downloaded) {
        throw "Unable to download SteamCMD from Valve's public distribution URLs."
    }

    if (Test-Path -LiteralPath $steamCmdRoot) {
        Remove-Item -LiteralPath $steamCmdRoot -Recurse -Force
    }

    Expand-Archive -LiteralPath $zipPath -DestinationPath $steamCmdRoot -Force

    if (-not (Test-LeafPath -Path $steamCmdExe)) {
        throw "SteamCMD download completed but steamcmd.exe was not found at $steamCmdExe."
    }

    return $steamCmdExe
}

function Resolve-SteamCmdExe {
    param(
        [string]$ExplicitPath,
        [switch]$AllowDownload
    )

    Import-HLServerEnv

    $resolved = Resolve-ExecutablePath -ExplicitPath $ExplicitPath -EnvName "STEAMCMD_EXE" -AutoCandidates (Get-SteamCmdAutoCandidates) -DisplayName "SteamCMD executable"
    if ($resolved) {
        return $resolved
    }

    if ($AllowDownload) {
        return (Install-SteamCmd)
    }

    return $null
}

function Install-HldsTemplateFromSteamCmd {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SteamCmdExe
    )

    $installDir = Join-RepoPath "testbed\cache\hlds-template"
    $hldsExe = Join-Path $installDir "hlds.exe"

    if (Test-LeafPath -Path $hldsExe) {
        return $installDir
    }

    Write-Step "Installing Half-Life Dedicated Server template into testbed/cache"

    Ensure-Directory -Path $installDir

    $arguments = @(
        "+login", "anonymous",
        "+force_install_dir", $installDir,
        "+app_update", "90", "validate",
        "+quit"
    )

    & $SteamCmdExe @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "SteamCMD failed while installing Half-Life Dedicated Server."
    }

    if (-not (Test-LeafPath -Path $hldsExe)) {
        throw "SteamCMD completed but no hlds.exe was installed to $installDir."
    }

    return $installDir
}

function Resolve-HldsTemplateRoot {
    param(
        [string]$ExplicitTemplateRoot,
        [string]$ExplicitHldsExe,
        [string]$ExplicitHlExe,
        [string]$ExplicitSteamCmdExe,
        [switch]$AllowSteamCmdDownload
    )

    Import-HLServerEnv

    $templateRoot = Resolve-DirectoryPath -ExplicitPath $ExplicitTemplateRoot -EnvName "HL_RUNTIME_TEMPLATE" -DisplayName "Runtime template root"
    if ($templateRoot) {
        return $templateRoot
    }

    $hldsExe = Resolve-HldsExe -ExplicitPath $ExplicitHldsExe
    if ($hldsExe) {
        return (Split-Path -Parent $hldsExe)
    }

    $hlExe = Resolve-HlExe -ExplicitPath $ExplicitHlExe
    if ($hlExe) {
        $hlRoot = Split-Path -Parent $hlExe
        $embeddedHlds = Join-Path $hlRoot "hlds.exe"
        if (Test-LeafPath -Path $embeddedHlds) {
            return $hlRoot
        }
    }

    $steamCmdExe = Resolve-SteamCmdExe -ExplicitPath $ExplicitSteamCmdExe -AllowDownload:$AllowSteamCmdDownload
    if ($steamCmdExe) {
        return (Install-HldsTemplateFromSteamCmd -SteamCmdExe $steamCmdExe)
    }

    throw "Unable to resolve an HLDS runtime template. Set HLDS_EXE or HL_RUNTIME_TEMPLATE, or provide STEAMCMD_EXE, or pass -AllowSteamCmdDownload."
}

function Get-HldsArgumentList {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Map,

        [Parameter(Mandatory = $true)]
        [int]$Port,

        [Parameter(Mandatory = $true)]
        [int]$MaxPlayers,

        [string]$ServerCfgName
    )

    $arguments = @(
        "-console",
        "-game", "valve",
        "-norestart",
        "-condebug",
        "-port", $Port,
        "+maxplayers", $MaxPlayers,
        "+sv_lan", "1",
        "+log", "on",
        "+mp_logecho", "1"
    )

    if (-not [string]::IsNullOrWhiteSpace($ServerCfgName)) {
        $arguments += @("+servercfgfile", $ServerCfgName)
    }

    $arguments += @("+map", $Map)

    return $arguments
}

function Get-ExperimentalGlockLaunchAssignments {
    return @(
        "sv_exp_pistol_tapfire=1",
        "sv_exp_move_spread_scale=1.0",
        "sv_exp_first_shot_accuracy=1",
        "sv_exp_spread_recovery=0.3",
        "sv_exp_glock_profile_name=default"
    )
}

function Get-ExperimentalGlockDebugLaunchAssignments {
    return @(
        "sv_exp_debug_weaponlog=1",
        "sv_exp_debug_weaponlog_rejections=1"
    )
}

function Get-GlockLabDummyLaunchAssignments {
    return @(
        "sv_exp_glock_lab_dummy=1"
    )
}

function Get-GlockProfilesRoot {
    return (Join-RepoPath "configs\glock-presets")
}

function ConvertTo-GlockProfileAssignments {
    param(
        [Parameter(Mandatory = $true)]
        $Cvars
    )

    $assignments = New-Object System.Collections.Generic.List[string]

    foreach ($property in $Cvars.PSObject.Properties) {
        $name = [string]$property.Name
        $value = [string]$property.Value

        if ([string]::IsNullOrWhiteSpace($name) -or [string]::IsNullOrWhiteSpace($value)) {
            throw "Glock preset cvar names and values must be non-empty."
        }

        $assignments.Add(('{0}={1}' -f $name, $value))
    }

    return $assignments.ToArray()
}

function Get-GlockProfiles {
    $profilesRoot = Get-GlockProfilesRoot
    if (-not (Test-Path -LiteralPath $profilesRoot -PathType Container)) {
        throw "Glock profiles directory was not found: $profilesRoot"
    }

    $profiles = New-Object System.Collections.Generic.List[object]

    foreach ($profileFile in (Get-ChildItem -LiteralPath $profilesRoot -File -Filter "*.json" | Sort-Object BaseName, Name)) {
        try {
            $rawJson = Get-Content -LiteralPath $profileFile.FullName -Raw
            $profileData = $rawJson | ConvertFrom-Json
        }
        catch {
            throw "Failed to parse Glock profile '$($profileFile.FullName)': $($_.Exception.Message)"
        }

        $profileName = [string]$profileData.name
        $description = [string]$profileData.description

        if ([string]::IsNullOrWhiteSpace($profileName)) {
            throw "Glock profile '$($profileFile.FullName)' must define a non-empty 'name'."
        }

        if ([string]::IsNullOrWhiteSpace($description)) {
            throw "Glock profile '$($profileFile.FullName)' must define a non-empty 'description'."
        }

        if ($profileFile.BaseName -ne $profileName) {
            throw "Glock profile filename '$($profileFile.Name)' must match preset name '$profileName'."
        }

        if ($null -eq $profileData.cvars) {
            throw "Glock profile '$($profileFile.FullName)' must define a 'cvars' object."
        }

        $assignments = @(ConvertTo-GlockProfileAssignments -Cvars $profileData.cvars)

        $profiles.Add([PSCustomObject]@{
            Name = $profileName
            Description = $description
            Path = $profileFile.FullName
            Assignments = $assignments
            Cvars = $profileData.cvars
        })
    }

    return $profiles.ToArray()
}

function Get-GlockProfile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $profiles = @(Get-GlockProfiles)
    $match = $profiles | Where-Object { $_.Name.Equals($Name, [System.StringComparison]::OrdinalIgnoreCase) } | Select-Object -First 1
    if ($match) {
        return $match
    }

    $available = $profiles | Select-Object -ExpandProperty Name
    $availableText = if ($available.Count -gt 0) { ($available -join ", ") } else { "none" }
    throw "Unknown Glock profile '$Name'. Available profiles: $availableText"
}

function Get-GlockProfileLaunchAssignments {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $profile = Get-GlockProfile -Name $Name
    return @("sv_exp_glock_profile_name=$($profile.Name)") + @($profile.Assignments)
}

function Get-WeaponDebugLogsRoot {
    return (Get-TestbedLogsRoot)
}

function Get-WeaponDebugReportsRoot {
    return (Join-Path (Get-WeaponDebugLogsRoot) "reports")
}

function Get-LatestWeaponDebugLog {
    $logsRoot = Get-WeaponDebugLogsRoot
    if (-not (Test-Path -LiteralPath $logsRoot -PathType Container)) {
        return $null
    }

    return (
        Get-ChildItem -LiteralPath $logsRoot -File -Filter "weapon-debug-*.log" |
        Sort-Object LastWriteTimeUtc, Name |
        Select-Object -Last 1
    )
}

function Get-HldsLaunchCvars {
    param(
        [string[]]$SetCvar = @(),
        [hashtable]$Cvars
    )

    $resolved = [ordered]@{}

    foreach ($assignment in $SetCvar) {
        if ([string]::IsNullOrWhiteSpace($assignment)) {
            continue
        }

        $separatorIndex = $assignment.IndexOf("=")
        if ($separatorIndex -lt 1 -or $separatorIndex -ge ($assignment.Length - 1)) {
            throw "Invalid -SetCvar value '$assignment'. Use Name=Value."
        }

        $name = $assignment.Substring(0, $separatorIndex).Trim()
        $value = $assignment.Substring($separatorIndex + 1).Trim()
        if ([string]::IsNullOrWhiteSpace($name) -or [string]::IsNullOrWhiteSpace($value)) {
            throw "Invalid -SetCvar value '$assignment'. Use Name=Value."
        }

        $resolved[$name] = $value
    }

    if ($Cvars) {
        foreach ($name in $Cvars.Keys | Sort-Object) {
            if ([string]::IsNullOrWhiteSpace([string]$name)) {
                throw "Cvar names cannot be empty."
            }

            $resolved[[string]$name] = [string]$Cvars[$name]
        }
    }

    return $resolved
}

function Write-HldsLaunchCvarConfig {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RuntimeRoot,

        [System.Collections.IDictionary]$Cvars
    )

    if (-not $Cvars -or $Cvars.Count -eq 0) {
        return $null
    }

    $modRoot = Join-Path (Get-FullPath -Path $RuntimeRoot) "valve"
    Ensure-Directory -Path $modRoot

    $configName = "hlserver_launch.cfg"
    $configPath = Join-Path $modRoot $configName
    $lines = New-Object System.Collections.Generic.List[string]

    foreach ($name in $Cvars.Keys) {
        $value = [string]$Cvars[$name]
        $lines.Add(('{0} "{1}"' -f $name, $value.Replace('"', '\"')))
    }

    Set-Content -LiteralPath $configPath -Value $lines -Encoding ASCII
    return $configName
}

function Test-UdpPortAvailable {
    param(
        [Parameter(Mandatory = $true)]
        [int]$Port
    )

    $client = $null

    try {
        $client = New-Object System.Net.Sockets.UdpClient($Port)
        return $true
    }
    catch {
        return $false
    }
    finally {
        if ($client) {
            $client.Close()
        }
    }
}

function Get-AvailableUdpPort {
    param(
        [Parameter(Mandatory = $true)]
        [int]$PreferredPort
    )

    if (Test-UdpPortAvailable -Port $PreferredPort) {
        return $PreferredPort
    }

    $client = New-Object System.Net.Sockets.UdpClient(0)
    try {
        return ([System.Net.IPEndPoint]$client.Client.LocalEndPoint).Port
    }
    finally {
        $client.Close()
    }
}

function Assert-UdpPortAvailable {
    param(
        [Parameter(Mandatory = $true)]
        [int]$Port
    )

    if (-not (Test-UdpPortAvailable -Port $Port)) {
        throw "UDP port $Port is already in use. Pass -Port with a free port."
    }
}

function Start-HldsDetached {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RuntimeRoot,

        [Parameter(Mandatory = $true)]
        [string]$Map,

        [Parameter(Mandatory = $true)]
        [int]$Port,

        [Parameter(Mandatory = $true)]
        [int]$MaxPlayers,

        [string[]]$SetCvar = @(),
        [hashtable]$Cvars
    )

    $runtimeRoot = Get-FullPath -Path $RuntimeRoot
    $hldsExe = Join-Path $runtimeRoot "hlds.exe"

    if (-not (Test-LeafPath -Path $hldsExe)) {
        throw "No hlds.exe was found in the disposable runtime: $hldsExe"
    }

    $logsRoot = Get-TestbedLogsRoot
    Ensure-Directory -Path $logsRoot

    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $stdoutLog = Join-Path $logsRoot ("hlds-" + $timestamp + "-stdout.log")
    $stderrLog = Join-Path $logsRoot ("hlds-" + $timestamp + "-stderr.log")
    $serverCfgName = Write-HldsLaunchCvarConfig -RuntimeRoot $runtimeRoot -Cvars (Get-HldsLaunchCvars -SetCvar $SetCvar -Cvars $Cvars)

    $process = Start-Process -FilePath $hldsExe -WorkingDirectory $runtimeRoot -ArgumentList (Get-HldsArgumentList -Map $Map -Port $Port -MaxPlayers $MaxPlayers -ServerCfgName $serverCfgName) -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog -PassThru

    return [PSCustomObject]@{
        Process = $process
        RuntimeRoot = $runtimeRoot
        StdOutLog = $stdoutLog
        StdErrLog = $stderrLog
        QConsoleLog = Join-Path $runtimeRoot "qconsole.log"
        ValveQConsoleLog = Join-Path $runtimeRoot "valve\qconsole.log"
    }
}

function Invoke-HldsForeground {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RuntimeRoot,

        [Parameter(Mandatory = $true)]
        [string]$Map,

        [Parameter(Mandatory = $true)]
        [int]$Port,

        [Parameter(Mandatory = $true)]
        [int]$MaxPlayers,

        [string[]]$SetCvar = @(),
        [hashtable]$Cvars
    )

    $runtimeRoot = Get-FullPath -Path $RuntimeRoot
    $hldsExe = Join-Path $runtimeRoot "hlds.exe"
    if (-not (Test-LeafPath -Path $hldsExe)) {
        throw "No hlds.exe was found in the disposable runtime: $hldsExe"
    }

    $logsRoot = Get-TestbedLogsRoot
    Ensure-Directory -Path $logsRoot

    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $stdoutLog = Join-Path $logsRoot ("hlds-" + $timestamp + "-stdout.log")
    $serverCfgName = Write-HldsLaunchCvarConfig -RuntimeRoot $runtimeRoot -Cvars (Get-HldsLaunchCvars -SetCvar $SetCvar -Cvars $Cvars)

    Push-Location $runtimeRoot
    try {
        & $hldsExe @(Get-HldsArgumentList -Map $Map -Port $Port -MaxPlayers $MaxPlayers -ServerCfgName $serverCfgName) 2>&1 | Tee-Object -FilePath $stdoutLog
        return $stdoutLog
    }
    finally {
        Pop-Location
    }
}

function Get-HldsLogCandidates {
    param(
        [Parameter(Mandatory = $true)]
        $LaunchInfo
    )

    return @(
        $LaunchInfo.StdOutLog,
        $LaunchInfo.StdErrLog,
        $LaunchInfo.QConsoleLog,
        $LaunchInfo.ValveQConsoleLog
    )
}

function Wait-ForHldsReady {
    param(
        [Parameter(Mandatory = $true)]
        $LaunchInfo,

        [Parameter(Mandatory = $true)]
        [string]$Map,

        [Parameter(Mandatory = $true)]
        [int]$TimeoutSeconds
    )

    return (Wait-ForLogPattern -Paths (Get-HldsLogCandidates -LaunchInfo $LaunchInfo) -Pattern ('Started map "' + $Map + '"') -TimeoutSeconds $TimeoutSeconds -Process $LaunchInfo.Process)
}

function Wait-ForLogPattern {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Paths,

        [Parameter(Mandatory = $true)]
        [string]$Pattern,

        [Parameter(Mandatory = $true)]
        [int]$TimeoutSeconds,

        [System.Diagnostics.Process]$Process
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)

    while ((Get-Date) -lt $deadline) {
        foreach ($path in $Paths) {
            if (-not [string]::IsNullOrWhiteSpace($path) -and (Test-LeafPath -Path $path)) {
                if (Select-String -Path $path -Pattern $Pattern -SimpleMatch -Quiet -ErrorAction SilentlyContinue) {
                    return $true
                }
            }
        }

        if ($Process) {
            $Process.Refresh()
            if ($Process.HasExited) {
                return $false
            }
        }

        Start-Sleep -Seconds 1
    }

    return $false
}

function Stop-ProcessIfRunning {
    param(
        [System.Diagnostics.Process]$Process
    )

    if ($Process -and -not $Process.HasExited) {
        Stop-Process -Id $Process.Id -Force
        $Process.WaitForExit()
    }
}
