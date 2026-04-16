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

function Get-TestbedSessionClientExe {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RuntimeRoot
    )

    $runtimeClient = Join-Path $RuntimeRoot "hl.exe"
    if (Test-LeafPath -Path $runtimeClient) {
        return $runtimeClient
    }

    return (Resolve-HlExe)
}

function Get-ObjectPropertyValue {
    param(
        $InputObject,
        [Parameter(Mandatory = $true)]
        [string[]]$Names
    )

    if ($null -eq $InputObject) {
        return $null
    }

    foreach ($name in $Names) {
        $property = $InputObject.PSObject.Properties[$name]
        if ($property) {
            return $property.Value
        }
    }

    return $null
}

function Get-OptionalObjectString {
    param(
        $InputObject,
        [Parameter(Mandatory = $true)]
        [string[]]$Names
    )

    $value = Get-ObjectPropertyValue -InputObject $InputObject -Names $Names
    if ($null -eq $value) {
        return $null
    }

    $text = [string]$value
    if ([string]::IsNullOrWhiteSpace($text)) {
        return $null
    }

    return $text.Trim()
}

function Test-FilesystemFriendlyName {
    param(
        [string]$Name
    )

    return (-not [string]::IsNullOrWhiteSpace($Name)) -and ($Name -match '^[A-Za-z0-9][A-Za-z0-9_-]*$')
}

function Assert-FilesystemFriendlyName {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [string]$Label
    )

    if (-not (Test-FilesystemFriendlyName -Name $Name)) {
        throw "$Label '$Name' must match ^[A-Za-z0-9][A-Za-z0-9_-]*$."
    }
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

function Get-ExperimentalWeaponDebugLaunchAssignments {
    return @(Get-ExperimentalGlockDebugLaunchAssignments)
}

function Get-ExperimentalMp5LaunchAssignments {
    return @(
        "sv_exp_mp5_primary_enabled=1",
        "sv_exp_mp5_profile_name=default"
    )
}

function Get-GlockLabDummyLaunchAssignments {
    return @(
        "sv_exp_glock_lab_dummy=1"
    )
}

function Get-Mp5LabLoadoutLaunchAssignments {
    return @(
        "sv_exp_mp5_lab_loadout=1",
        "sv_exp_mp5_lab_ammo=250",
        "sv_exp_mp5_lab_autoswitch=1"
    )
}

function Get-SessionMetadataLaunchAssignments {
    param(
        [string]$SessionTag,
        [string]$MatrixName,
        [string]$MatrixStep,
        [string]$WeaponUnderTest
    )

    $assignments = New-Object System.Collections.Generic.List[string]

    if (-not [string]::IsNullOrWhiteSpace($SessionTag)) {
        $assignments.Add(("sv_exp_session_tag={0}" -f $SessionTag.Trim()))
    }

    if (-not [string]::IsNullOrWhiteSpace($MatrixName)) {
        $assignments.Add(("sv_exp_matrix_name={0}" -f $MatrixName.Trim()))
    }

    if (-not [string]::IsNullOrWhiteSpace($MatrixStep)) {
        $assignments.Add(("sv_exp_matrix_step={0}" -f $MatrixStep.Trim()))
    }

    if (-not [string]::IsNullOrWhiteSpace($WeaponUnderTest)) {
        $assignments.Add(("sv_exp_weapon_under_test={0}" -f $WeaponUnderTest.Trim()))
    }

    return $assignments.ToArray()
}

function ConvertTo-LaunchAssignments {
    param(
        [Parameter(Mandatory = $true)]
        $Cvars,

        [Parameter(Mandatory = $true)]
        [string]$SourceLabel
    )

    $assignments = New-Object System.Collections.Generic.List[string]

    foreach ($property in $Cvars.PSObject.Properties) {
        $name = [string]$property.Name
        $value = [string]$property.Value

        if ([string]::IsNullOrWhiteSpace($name) -or [string]::IsNullOrWhiteSpace($value)) {
            throw "$SourceLabel cvar names and values must be non-empty."
        }

        $assignments.Add(('{0}={1}' -f $name, $value))
    }

    return $assignments.ToArray()
}

function Get-GlockProfilesRoot {
    return (Join-RepoPath "configs\glock-presets")
}

function ConvertTo-GlockProfileAssignments {
    param(
        [Parameter(Mandatory = $true)]
        $Cvars
    )

    return @(ConvertTo-LaunchAssignments -Cvars $Cvars -SourceLabel "Glock preset")
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

function Get-Mp5ProfilesRoot {
    return (Join-RepoPath "configs\mp5-presets")
}

function Get-Mp5Profiles {
    $profilesRoot = Get-Mp5ProfilesRoot
    if (-not (Test-Path -LiteralPath $profilesRoot -PathType Container)) {
        throw "MP5 profiles directory was not found: $profilesRoot"
    }

    $profiles = New-Object System.Collections.Generic.List[object]

    foreach ($profileFile in (Get-ChildItem -LiteralPath $profilesRoot -File -Filter "*.json" | Sort-Object BaseName, Name)) {
        try {
            $rawJson = Get-Content -LiteralPath $profileFile.FullName -Raw
            $profileData = $rawJson | ConvertFrom-Json
        }
        catch {
            throw "Failed to parse MP5 profile '$($profileFile.FullName)': $($_.Exception.Message)"
        }

        $profileName = [string]$profileData.name
        $description = [string]$profileData.description

        if ([string]::IsNullOrWhiteSpace($profileName)) {
            throw "MP5 profile '$($profileFile.FullName)' must define a non-empty 'name'."
        }

        if ([string]::IsNullOrWhiteSpace($description)) {
            throw "MP5 profile '$($profileFile.FullName)' must define a non-empty 'description'."
        }

        if ($profileFile.BaseName -ne $profileName) {
            throw "MP5 profile filename '$($profileFile.Name)' must match profile name '$profileName'."
        }

        if ($null -eq $profileData.cvars) {
            throw "MP5 profile '$($profileFile.FullName)' must define a 'cvars' object."
        }

        $assignments = @(ConvertTo-LaunchAssignments -Cvars $profileData.cvars -SourceLabel "MP5 preset")

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

function Get-Mp5Profile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $profiles = @(Get-Mp5Profiles)
    $match = $profiles | Where-Object { $_.Name.Equals($Name, [System.StringComparison]::OrdinalIgnoreCase) } | Select-Object -First 1
    if ($match) {
        return $match
    }

    $available = $profiles | Select-Object -ExpandProperty Name
    $availableText = if ($available.Count -gt 0) { ($available -join ", ") } else { "none" }
    throw "Unknown MP5 profile '$Name'. Available profiles: $availableText"
}

function Get-Mp5ProfileLaunchAssignments {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $profile = Get-Mp5Profile -Name $Name
    return @("sv_exp_mp5_profile_name=$($profile.Name)") + @($profile.Assignments)
}

function Get-GlockLabTargetsRoot {
    return (Join-RepoPath "configs\glock-lab-targets")
}

function Get-GlockLabTargets {
    $targetsRoot = Get-GlockLabTargetsRoot
    if (-not (Test-Path -LiteralPath $targetsRoot -PathType Container)) {
        throw "Glock lab target profiles directory was not found: $targetsRoot"
    }

    $targets = New-Object System.Collections.Generic.List[object]

    foreach ($targetFile in (Get-ChildItem -LiteralPath $targetsRoot -File -Filter "*.json" | Sort-Object BaseName, Name)) {
        try {
            $rawJson = Get-Content -LiteralPath $targetFile.FullName -Raw
            $targetData = $rawJson | ConvertFrom-Json
        }
        catch {
            throw "Failed to parse Glock lab target profile '$($targetFile.FullName)': $($_.Exception.Message)"
        }

        $targetName = [string]$targetData.name
        $description = [string]$targetData.description

        if ([string]::IsNullOrWhiteSpace($targetName)) {
            throw "Glock lab target profile '$($targetFile.FullName)' must define a non-empty 'name'."
        }

        if ([string]::IsNullOrWhiteSpace($description)) {
            throw "Glock lab target profile '$($targetFile.FullName)' must define a non-empty 'description'."
        }

        if ($targetFile.BaseName -ne $targetName) {
            throw "Glock lab target profile filename '$($targetFile.Name)' must match target name '$targetName'."
        }

        if ($null -eq $targetData.cvars) {
            throw "Glock lab target profile '$($targetFile.FullName)' must define a 'cvars' object."
        }

        $assignments = @(ConvertTo-LaunchAssignments -Cvars $targetData.cvars -SourceLabel "Glock lab target profile")

        $targets.Add([PSCustomObject]@{
            Name = $targetName
            Description = $description
            Path = $targetFile.FullName
            Assignments = $assignments
            Cvars = $targetData.cvars
        })
    }

    return $targets.ToArray()
}

function Get-GlockLabTarget {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $targets = @(Get-GlockLabTargets)
    $match = $targets | Where-Object { $_.Name.Equals($Name, [System.StringComparison]::OrdinalIgnoreCase) } | Select-Object -First 1
    if ($match) {
        return $match
    }

    $available = $targets | Select-Object -ExpandProperty Name
    $availableText = if ($available.Count -gt 0) { ($available -join ", ") } else { "none" }
    throw "Unknown Glock lab target profile '$Name'. Available profiles: $availableText"
}

function Get-GlockLabTargetLaunchAssignments {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $target = Get-GlockLabTarget -Name $Name
    return @("sv_exp_glock_lab_target_profile_name=$($target.Name)") + @($target.Assignments)
}

function Get-GlockComparisonMatricesRoot {
    return (Join-RepoPath "configs\glock-comparison-matrices")
}

function ConvertTo-GlockComparisonMatrixStep {
    param(
        [Parameter(Mandatory = $true)]
        [string]$MatrixName,
        [Parameter(Mandatory = $true)]
        [string]$MatrixPath,
        [Parameter(Mandatory = $true)]
        $StepData,
        $Defaults,
        [Parameter(Mandatory = $true)]
        [int]$Index
    )

    $stepName = Get-OptionalObjectString -InputObject $StepData -Names @("name", "tag", "stepName", "step")
    if ([string]::IsNullOrWhiteSpace($stepName)) {
        throw "Glock comparison matrix '$MatrixPath' step #$Index must define a non-empty 'name'."
    }

    Assert-FilesystemFriendlyName -Name $stepName -Label ("Glock comparison matrix step name in '$MatrixName'")

    $profileName = Get-OptionalObjectString -InputObject $StepData -Names @("glockProfile", "glockProfileName", "profile")
    if ([string]::IsNullOrWhiteSpace($profileName)) {
        $profileName = Get-OptionalObjectString -InputObject $Defaults -Names @("glockProfile", "glockProfileName", "profile")
    }

    if ([string]::IsNullOrWhiteSpace($profileName)) {
        throw "Glock comparison matrix '$MatrixPath' step '$stepName' must define a Glock profile name directly or via defaults."
    }

    $labTargetProfileName = Get-OptionalObjectString -InputObject $StepData -Names @("labTargetProfile", "labTargetProfileName", "targetProfile")
    if ([string]::IsNullOrWhiteSpace($labTargetProfileName)) {
        $labTargetProfileName = Get-OptionalObjectString -InputObject $Defaults -Names @("labTargetProfile", "labTargetProfileName", "targetProfile")
    }

    if ([string]::IsNullOrWhiteSpace($labTargetProfileName)) {
        throw "Glock comparison matrix '$MatrixPath' step '$stepName' must define a lab target profile name directly or via defaults."
    }

    $map = Get-OptionalObjectString -InputObject $StepData -Names @("map", "mapOverride")
    if ([string]::IsNullOrWhiteSpace($map)) {
        $map = Get-OptionalObjectString -InputObject $Defaults -Names @("map", "mapOverride")
    }
    if ([string]::IsNullOrWhiteSpace($map)) {
        $map = "crossfire"
    }

    $operatorNote = Get-OptionalObjectString -InputObject $StepData -Names @("operatorNote", "note", "checkFocus")
    if ([string]::IsNullOrWhiteSpace($operatorNote)) {
        $operatorNote = Get-OptionalObjectString -InputObject $Defaults -Names @("operatorNote", "note", "checkFocus")
    }

    $assignments = New-Object System.Collections.Generic.List[string]
    $defaultExtraCvars = Get-ObjectPropertyValue -InputObject $Defaults -Names @("extraCvars", "cvars")
    if ($null -ne $defaultExtraCvars) {
        foreach ($assignment in (ConvertTo-LaunchAssignments -Cvars $defaultExtraCvars -SourceLabel "Glock comparison matrix defaults")) {
            $assignments.Add($assignment)
        }
    }

    $stepExtraCvars = Get-ObjectPropertyValue -InputObject $StepData -Names @("extraCvars", "cvars")
    if ($null -ne $stepExtraCvars) {
        foreach ($assignment in (ConvertTo-LaunchAssignments -Cvars $stepExtraCvars -SourceLabel ("Glock comparison matrix step '{0}'" -f $stepName))) {
            $assignments.Add($assignment)
        }
    }

    $profile = Get-GlockProfile -Name $profileName
    $target = Get-GlockLabTarget -Name $labTargetProfileName

    return [PSCustomObject]@{
        Name = $stepName
        Map = $map
        OperatorNote = $operatorNote
        GlockProfileName = $profile.Name
        GlockProfile = $profile
        LabTargetProfileName = $target.Name
        LabTargetProfile = $target
        Assignments = $assignments.ToArray()
    }
}

function Get-GlockComparisonMatrices {
    $matricesRoot = Get-GlockComparisonMatricesRoot
    if (-not (Test-Path -LiteralPath $matricesRoot -PathType Container)) {
        throw "Glock comparison matrices directory was not found: $matricesRoot"
    }

    $matrices = New-Object System.Collections.Generic.List[object]

    foreach ($matrixFile in (Get-ChildItem -LiteralPath $matricesRoot -File -Filter "*.json" | Sort-Object BaseName, Name)) {
        try {
            $rawJson = Get-Content -LiteralPath $matrixFile.FullName -Raw
            $matrixData = $rawJson | ConvertFrom-Json
        }
        catch {
            throw "Failed to parse Glock comparison matrix '$($matrixFile.FullName)': $($_.Exception.Message)"
        }

        $matrixName = Get-OptionalObjectString -InputObject $matrixData -Names @("name")
        $description = Get-OptionalObjectString -InputObject $matrixData -Names @("description")

        if ([string]::IsNullOrWhiteSpace($matrixName)) {
            throw "Glock comparison matrix '$($matrixFile.FullName)' must define a non-empty 'name'."
        }

        Assert-FilesystemFriendlyName -Name $matrixName -Label "Glock comparison matrix name"

        if ([string]::IsNullOrWhiteSpace($description)) {
            throw "Glock comparison matrix '$($matrixFile.FullName)' must define a non-empty 'description'."
        }

        if ($matrixFile.BaseName -ne $matrixName) {
            throw "Glock comparison matrix filename '$($matrixFile.Name)' must match matrix name '$matrixName'."
        }

        $defaults = Get-ObjectPropertyValue -InputObject $matrixData -Names @("defaults")
        $stepData = @(Get-ObjectPropertyValue -InputObject $matrixData -Names @("steps"))
        if ($stepData.Count -eq 0) {
            throw "Glock comparison matrix '$($matrixFile.FullName)' must define a non-empty 'steps' array."
        }

        $stepNames = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
        $steps = New-Object System.Collections.Generic.List[object]
        $stepIndex = 0

        foreach ($entry in $stepData) {
            $stepIndex += 1
            $step = ConvertTo-GlockComparisonMatrixStep -MatrixName $matrixName -MatrixPath $matrixFile.FullName -StepData $entry -Defaults $defaults -Index $stepIndex
            if (-not $stepNames.Add($step.Name)) {
                throw "Glock comparison matrix '$($matrixFile.FullName)' contains a duplicate step name '$($step.Name)'."
            }

            $steps.Add($step)
        }

        $matrices.Add([PSCustomObject]@{
            Name = $matrixName
            Description = $description
            Path = $matrixFile.FullName
            Defaults = $defaults
            Steps = $steps.ToArray()
            StepCount = $steps.Count
        })
    }

    return $matrices.ToArray()
}

function Get-GlockComparisonMatrix {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $matrices = @(Get-GlockComparisonMatrices)
    $match = $matrices | Where-Object { $_.Name.Equals($Name, [System.StringComparison]::OrdinalIgnoreCase) } | Select-Object -First 1
    if ($match) {
        return $match
    }

    $available = $matrices | Select-Object -ExpandProperty Name
    $availableText = if ($available.Count -gt 0) { ($available -join ", ") } else { "none" }
    throw "Unknown Glock comparison matrix '$Name'. Available matrices: $availableText"
}

function Get-WeaponDebugLogsRoot {
    return (Get-TestbedLogsRoot)
}

function Get-WeaponDebugReportsRoot {
    return (Join-Path (Get-WeaponDebugLogsRoot) "reports")
}

function Get-GlockComparisonReportsRoot {
    return (Join-Path (Get-WeaponDebugReportsRoot) "glock-comparison-matrices")
}

function Get-LatestGlockComparisonReportDirectory {
    $reportsRoot = Get-GlockComparisonReportsRoot
    if (-not (Test-Path -LiteralPath $reportsRoot -PathType Container)) {
        return $null
    }

    return (
        Get-ChildItem -LiteralPath $reportsRoot -Directory |
        Sort-Object LastWriteTimeUtc, Name |
        Select-Object -Last 1
    )
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
