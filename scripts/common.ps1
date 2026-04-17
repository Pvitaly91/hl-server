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

function Get-TestbedModsRoot {
    return (Join-Path (Get-TestbedRoot) "mods")
}

function Get-TestbedLiveModName {
    return "hlserver_testbed"
}

function Get-TestbedLiveModRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ClientRoot,
        [string]$GameDirName = (Get-TestbedLiveModName)
    )

    return (Join-Path (Get-FullPath -Path $ClientRoot) $GameDirName)
}

function Get-TestbedLiveModStageRoot {
    param(
        [string]$GameDirName = (Get-TestbedLiveModName)
    )

    return (Join-Path (Get-TestbedModsRoot) $GameDirName)
}

function Get-TestbedLiveModLinkPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ClientRoot,
        [string]$GameDirName = (Get-TestbedLiveModName)
    )

    return (Get-TestbedLiveModRoot -ClientRoot $ClientRoot -GameDirName $GameDirName)
}

function Get-TestbedLiveModManifestPath {
    param(
        [string]$ModRoot,
        [string]$StageRoot
    )

    $root = if (-not [string]::IsNullOrWhiteSpace($ModRoot)) {
        $ModRoot
    }
    elseif (-not [string]::IsNullOrWhiteSpace($StageRoot)) {
        $StageRoot
    }
    else {
        throw "Live mod manifest path requires -ModRoot or -StageRoot."
    }

    return (Join-Path (Get-FullPath -Path $root) ".hl-server-live-mod.json")
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
        [string]$RuntimeRoot,
        [string]$ExplicitHlExe,
        [switch]$PreferClientMatchedRuntime
    )

    if ($PreferClientMatchedRuntime) {
        $clientInstall = Resolve-TestbedClientInstall -ExplicitHlExe $ExplicitHlExe
        if ($clientInstall) {
            return $clientInstall.HlExe
        }

        return $null
    }

    $runtimeClient = Join-Path $RuntimeRoot "hl.exe"
    if (Test-LeafPath -Path $runtimeClient) {
        return $runtimeClient
    }

    return (Resolve-HlExe)
}

function Resolve-TestbedClientInstall {
    param(
        [string]$ExplicitHlExe
    )

    $clientExe = Resolve-HlExe -ExplicitPath $ExplicitHlExe
    if (-not $clientExe) {
        return $null
    }

    $clientRoot = Split-Path -Parent $clientExe
    return [PSCustomObject]@{
        HlExe = $clientExe
        Root = $clientRoot
        Probe = Get-HldsRuntimeProbe -Root $clientRoot
    }
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
        [string]$SteamCmdExe,
        [switch]$ForceRefresh
    )

    $installDir = Join-RepoPath "testbed\cache\hlds-template"
    $hldsExe = Join-Path $installDir "hlds.exe"

    $cachedProbe = if (Test-Path -LiteralPath $installDir -PathType Container) {
        Get-HldsRuntimeProbe -Root $installDir
    }
    else {
        $null
    }

    if ($cachedProbe -and $cachedProbe.IsRunnable -and (-not $ForceRefresh)) {
        return $installDir
    }

    if ($ForceRefresh -and $cachedProbe) {
        Write-Step "Refreshing Half-Life Dedicated Server template in testbed/cache"
    }
    else {
        Write-Step "Installing Half-Life Dedicated Server template into testbed/cache"
    }

    Ensure-Directory -Path $installDir
    Ensure-Directory -Path (Get-TestbedLogsRoot)

    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $steamCmdLog = Join-Path (Get-TestbedLogsRoot) ("steamcmd-hlds-template-" + $timestamp + ".log")

    $arguments = @(
        "+force_install_dir", $installDir,
        "+login", "anonymous",
        "+app_update", "90", "validate",
        "+quit"
    )

    & $SteamCmdExe @arguments 2>&1 | Tee-Object -FilePath $steamCmdLog | Out-Null
    $steamCmdExitCode = $LASTEXITCODE
    $steamCmdReportedSuccess = Select-String -Path $steamCmdLog -Pattern "Success! App '90' fully installed." -SimpleMatch -Quiet -ErrorAction SilentlyContinue

    if (($steamCmdExitCode -ne 0) -and (-not $steamCmdReportedSuccess) -and (-not (Test-LeafPath -Path $hldsExe))) {
        throw "SteamCMD failed while installing Half-Life Dedicated Server. Exit code: $steamCmdExitCode. Log: $steamCmdLog"
    }

    if (-not (Test-LeafPath -Path $hldsExe)) {
        throw "SteamCMD completed but no hlds.exe was installed to $installDir. Log: $steamCmdLog"
    }

    Write-Host "SteamCMD template log: $steamCmdLog"
    return $installDir
}

function Test-PathsEqual {
    param(
        [string]$Left,
        [string]$Right
    )

    if ([string]::IsNullOrWhiteSpace($Left) -or [string]::IsNullOrWhiteSpace($Right)) {
        return $false
    }

    return (Get-FullPath -Path $Left).Equals((Get-FullPath -Path $Right), [System.StringComparison]::OrdinalIgnoreCase)
}

function Get-HldsTemplateRootKind {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    $fullRoot = Get-FullPath -Path $Root
    $runtimeRoot = Get-TestbedRuntimeRoot
    $cachedTemplateRoot = Join-RepoPath "testbed\cache\hlds-template"
    $leafName = Split-Path -Leaf $fullRoot

    if (Test-PathsEqual -Left $fullRoot -Right $runtimeRoot) {
        return "disposable_runtime"
    }

    if (Test-PathsEqual -Left $fullRoot -Right $cachedTemplateRoot) {
        return "cached_dedicated_server"
    }

    if ($leafName.Equals("Half-Life Dedicated Server", [System.StringComparison]::OrdinalIgnoreCase)) {
        return "dedicated_server_install"
    }

    if (Test-LeafPath -Path (Join-Path $fullRoot "hl.exe")) {
        return "half_life_client_install"
    }

    return "generic_hlds_runtime"
}

function Get-HldsTemplateRootKindLabel {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Kind
    )

    switch ($Kind) {
        "cached_dedicated_server" { return "cached dedicated HLDS template" }
        "dedicated_server_install" { return "installed Half-Life Dedicated Server runtime" }
        "half_life_client_install" { return "Half-Life client install with embedded hlds.exe" }
        "generic_hlds_runtime" { return "generic HLDS runtime" }
        "disposable_runtime" { return "existing disposable runtime" }
        default { return $Kind }
    }
}

function Get-TestbedRuntimeKeyEntryDefinitions {
    return @(
        [PSCustomObject]@{ RelativePath = "hlds.exe"; PathType = "Leaf"; AlwaysRequired = $true; Description = "selected hlds.exe" }
        [PSCustomObject]@{ RelativePath = "hl.exe"; PathType = "Leaf"; AlwaysRequired = $false; Description = "stock hl.exe for client-attached sessions" }
        [PSCustomObject]@{ RelativePath = "steam_appid.txt"; PathType = "Leaf"; AlwaysRequired = $false; Description = "Steam AppID helper" }
        [PSCustomObject]@{ RelativePath = "SDL3.dll"; PathType = "Leaf"; AlwaysRequired = $false; Description = "current SDL runtime sidecar" }
        [PSCustomObject]@{ RelativePath = "SDL2.dll"; PathType = "Leaf"; AlwaysRequired = $false; Description = "legacy SDL runtime sidecar" }
        [PSCustomObject]@{ RelativePath = "steam_api.dll"; PathType = "Leaf"; AlwaysRequired = $true; Description = "Steam API sidecar" }
        [PSCustomObject]@{ RelativePath = "tier0.dll"; PathType = "Leaf"; AlwaysRequired = $true; Description = "Valve tier0 sidecar" }
        [PSCustomObject]@{ RelativePath = "vstdlib.dll"; PathType = "Leaf"; AlwaysRequired = $true; Description = "Valve vstdlib sidecar" }
        [PSCustomObject]@{ RelativePath = "swds.dll"; PathType = "Leaf"; AlwaysRequired = $false; Description = "software renderer sidecar" }
        [PSCustomObject]@{ RelativePath = "platform"; PathType = "Container"; AlwaysRequired = $false; Description = "platform sidecar directory" }
        [PSCustomObject]@{ RelativePath = "bin"; PathType = "Container"; AlwaysRequired = $false; Description = "runtime sidecar directory" }
        [PSCustomObject]@{ RelativePath = "valve"; PathType = "Container"; AlwaysRequired = $true; Description = "stock valve game content" }
    )
}

function Get-HldsRuntimeProbe {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    $fullRoot = Get-FullPath -Path $Root
    $exists = Test-Path -LiteralPath $fullRoot -PathType Container
    $entryStates = New-Object System.Collections.Generic.List[object]

    $steamAppIdPath = Join-Path $fullRoot "steam_appid.txt"
    $steamAppId = $null
    if ($exists -and (Test-LeafPath -Path $steamAppIdPath)) {
        $steamAppId = ((Get-Content -LiteralPath $steamAppIdPath | Select-Object -First 1) -as [string])
        if (-not [string]::IsNullOrWhiteSpace($steamAppId)) {
            $steamAppId = $steamAppId.Trim()
        }
    }

    $kind = if ($exists) { Get-HldsTemplateRootKind -Root $fullRoot } else { "missing_root" }
    $hldsExe = Join-Path $fullRoot "hlds.exe"
    $hlExe = Join-Path $fullRoot "hl.exe"
    $hasClientExe = $exists -and (Test-LeafPath -Path $hlExe)

    foreach ($definition in Get-TestbedRuntimeKeyEntryDefinitions) {
        $entryPath = Join-Path $fullRoot $definition.RelativePath
        $present = $false

        if ($exists) {
            if ($definition.PathType -eq "Container") {
                $present = Test-Path -LiteralPath $entryPath -PathType Container
            }
            else {
                $present = Test-LeafPath -Path $entryPath
            }
        }

        $entryStates.Add([PSCustomObject]@{
            RelativePath = $definition.RelativePath
            PathType = $definition.PathType
            Description = $definition.Description
            AlwaysRequired = $definition.AlwaysRequired
            Path = $entryPath
            Present = $present
        })
    }

    $missingBaseRequired = @(
        $entryStates |
        Where-Object { $_.AlwaysRequired -and (-not $_.Present) } |
        Select-Object -ExpandProperty RelativePath
    )

    $suggestedSteamAppId = if (-not [string]::IsNullOrWhiteSpace($steamAppId)) {
        $steamAppId
    }
    elseif ($kind -in @("cached_dedicated_server", "dedicated_server_install")) {
        "90"
    }
    elseif ($hasClientExe) {
        "70"
    }
    else {
        $null
    }

    return [PSCustomObject]@{
        Root = $fullRoot
        Exists = $exists
        Kind = $kind
        KindLabel = if ($exists) { Get-HldsTemplateRootKindLabel -Kind $kind } else { "missing runtime root" }
        IsDedicatedPreferred = $kind -in @("cached_dedicated_server", "dedicated_server_install")
        HldsExe = if ($exists -and (Test-LeafPath -Path $hldsExe)) { $hldsExe } else { $null }
        HlExe = if ($exists -and (Test-LeafPath -Path $hlExe)) { $hlExe } else { $null }
        SteamAppIdPath = if ($exists) { $steamAppIdPath } else { $null }
        SteamAppId = $steamAppId
        SuggestedSteamAppId = $suggestedSteamAppId
        EntryStates = $entryStates.ToArray()
        MissingBaseRequired = $missingBaseRequired
        IsRunnable = $exists -and ($missingBaseRequired.Count -eq 0)
    }
}

function New-HldsTemplateCandidate {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root,
        [Parameter(Mandatory = $true)]
        [string]$SelectionSource,
        [Parameter(Mandatory = $true)]
        [string]$Reason,
        [Parameter(Mandatory = $true)]
        [int]$Priority
    )

    $probe = Get-HldsRuntimeProbe -Root $Root

    return [PSCustomObject]@{
        Root = $probe.Root
        SelectionSource = $SelectionSource
        Reason = $Reason
        Priority = $Priority
        Probe = $probe
    }
}

function Add-HldsTemplateCandidate {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [System.Collections.Generic.List[object]]$Candidates,
        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [System.Collections.Generic.HashSet[string]]$SeenRoots,
        [string]$Root,
        [Parameter(Mandatory = $true)]
        [string]$SelectionSource,
        [Parameter(Mandatory = $true)]
        [string]$Reason,
        [Parameter(Mandatory = $true)]
        [int]$Priority
    )

    if ([string]::IsNullOrWhiteSpace($Root)) {
        return
    }

    $fullRoot = Get-FullPath -Path $Root
    if (-not (Test-Path -LiteralPath $fullRoot -PathType Container)) {
        return
    }

    if ($SeenRoots.Contains($fullRoot)) {
        return
    }

    $candidate = New-HldsTemplateCandidate -Root $fullRoot -SelectionSource $SelectionSource -Reason $Reason -Priority $Priority
    if ($candidate.Probe.Kind -eq "disposable_runtime") {
        return
    }

    [void]$SeenRoots.Add($fullRoot)
    $Candidates.Add($candidate)
}

function Get-HldsTemplateSelection {
    param(
        [string]$ExplicitTemplateRoot,
        [string]$ExplicitHldsExe,
        [string]$ExplicitHlExe,
        [string]$ExplicitSteamCmdExe,
        [switch]$AllowSteamCmdDownload,
        [switch]$ForceRefreshSteamCmdTemplate
    )

    Import-HLServerEnv

    $candidates = New-Object System.Collections.Generic.List[object]
    $seenRoots = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
    $selectionWarnings = New-Object System.Collections.Generic.List[string]

    $envTemplateRoot = [System.Environment]::GetEnvironmentVariable("HL_RUNTIME_TEMPLATE", "Process")
    $envHldsExe = [System.Environment]::GetEnvironmentVariable("HLDS_EXE", "Process")
    $envHlExe = [System.Environment]::GetEnvironmentVariable("HL_EXE", "Process")

    if (-not [string]::IsNullOrWhiteSpace($ExplicitTemplateRoot)) {
        $fullPath = Get-FullPath -Path $ExplicitTemplateRoot
        if (-not (Test-Path -LiteralPath $fullPath -PathType Container)) {
            throw "Runtime template root was provided explicitly but does not exist: $fullPath"
        }

        Add-HldsTemplateCandidate -Candidates $candidates -SeenRoots $seenRoots -Root $fullPath -SelectionSource "parameter:TemplateRoot" -Reason "Using the explicit -TemplateRoot override." -Priority 1000
    }
    elseif (-not [string]::IsNullOrWhiteSpace($envTemplateRoot)) {
        $fullPath = Get-FullPath -Path $envTemplateRoot
        if (-not (Test-Path -LiteralPath $fullPath -PathType Container)) {
            throw "HL_RUNTIME_TEMPLATE is set but does not exist: $fullPath"
        }

        Add-HldsTemplateCandidate -Candidates $candidates -SeenRoots $seenRoots -Root $fullPath -SelectionSource "env:HL_RUNTIME_TEMPLATE" -Reason "Using HL_RUNTIME_TEMPLATE from the process environment." -Priority 950
    }

    if (-not [string]::IsNullOrWhiteSpace($ExplicitHldsExe)) {
        $fullPath = Get-FullPath -Path $ExplicitHldsExe
        if (-not (Test-LeafPath -Path $fullPath)) {
            throw "HLDS executable was provided explicitly but does not exist: $fullPath"
        }

        Add-HldsTemplateCandidate -Candidates $candidates -SeenRoots $seenRoots -Root (Split-Path -Parent $fullPath) -SelectionSource "parameter:HldsExe" -Reason "Using the explicit -HldsExe override." -Priority 940
    }
    elseif (-not [string]::IsNullOrWhiteSpace($envHldsExe)) {
        $fullPath = Get-FullPath -Path $envHldsExe
        if (-not (Test-LeafPath -Path $fullPath)) {
            throw "HLDS_EXE is set but does not exist: $fullPath"
        }

        Add-HldsTemplateCandidate -Candidates $candidates -SeenRoots $seenRoots -Root (Split-Path -Parent $fullPath) -SelectionSource "env:HLDS_EXE" -Reason "Using HLDS_EXE from the process environment." -Priority 930
    }

    if (-not [string]::IsNullOrWhiteSpace($ExplicitHlExe)) {
        $fullPath = Get-FullPath -Path $ExplicitHlExe
        if (-not (Test-LeafPath -Path $fullPath)) {
            throw "Half-Life executable was provided explicitly but does not exist: $fullPath"
        }

        $root = Split-Path -Parent $fullPath
        Add-HldsTemplateCandidate -Candidates $candidates -SeenRoots $seenRoots -Root $root -SelectionSource "parameter:HlExe" -Reason "Using the explicit -HlExe override because it points at a runtime root." -Priority 920
    }
    elseif (-not [string]::IsNullOrWhiteSpace($envHlExe)) {
        $fullPath = Get-FullPath -Path $envHlExe
        if (-not (Test-LeafPath -Path $fullPath)) {
            throw "HL_EXE is set but does not exist: $fullPath"
        }

        $root = Split-Path -Parent $fullPath
        Add-HldsTemplateCandidate -Candidates $candidates -SeenRoots $seenRoots -Root $root -SelectionSource "env:HL_EXE" -Reason "Using HL_EXE from the process environment because it points at a runtime root." -Priority 910
    }

    $cachedTemplateRoot = Join-RepoPath "testbed\cache\hlds-template"
    if (Test-Path -LiteralPath $cachedTemplateRoot -PathType Container) {
        Add-HldsTemplateCandidate -Candidates $candidates -SeenRoots $seenRoots -Root $cachedTemplateRoot -SelectionSource "cache:hlds-template" -Reason "Using the cached dedicated HLDS template under testbed/cache." -Priority 900
    }

    foreach ($root in Get-SteamInstallRoots) {
        Add-HldsTemplateCandidate -Candidates $candidates -SeenRoots $seenRoots -Root (Join-Path $root "steamapps\common\Half-Life Dedicated Server") -SelectionSource "auto:steam-dedicated" -Reason "Detected an installed Half-Life Dedicated Server runtime under Steam." -Priority 850
    }

    $steamCmdExe = Resolve-SteamCmdExe -ExplicitPath $ExplicitSteamCmdExe -AllowDownload:$AllowSteamCmdDownload
    if ($steamCmdExe) {
        $steamCmdTemplateRoot = Install-HldsTemplateFromSteamCmd -SteamCmdExe $steamCmdExe -ForceRefresh:$ForceRefreshSteamCmdTemplate
        Add-HldsTemplateCandidate -Candidates $candidates -SeenRoots $seenRoots -Root $steamCmdTemplateRoot -SelectionSource "steamcmd" -Reason "Provisioned a dedicated HLDS template through SteamCMD into testbed/cache." -Priority 875
    }

    foreach ($candidate in Get-HldsAutoCandidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }

        $fullCandidate = Get-FullPath -Path $candidate
        if (-not (Test-LeafPath -Path $fullCandidate)) {
            continue
        }

        $root = Split-Path -Parent $fullCandidate
        $priority = if ($root -like "*Half-Life Dedicated Server") { 825 } else { 650 }
        Add-HldsTemplateCandidate -Candidates $candidates -SeenRoots $seenRoots -Root $root -SelectionSource "auto:hlds" -Reason "Auto-detected hlds.exe in a local runtime root." -Priority $priority
    }

    foreach ($candidate in Get-HlAutoCandidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }

        $fullCandidate = Get-FullPath -Path $candidate
        if (-not (Test-LeafPath -Path $fullCandidate)) {
            continue
        }

        Add-HldsTemplateCandidate -Candidates $candidates -SeenRoots $seenRoots -Root (Split-Path -Parent $fullCandidate) -SelectionSource "auto:hl" -Reason "Auto-detected hl.exe in a local Half-Life client install that also carries hlds.exe." -Priority 620
    }

    $viableCandidates = @(
        $candidates |
        Where-Object { $_.Probe.IsRunnable -and $_.Probe.HldsExe } |
        Sort-Object @{ Expression = "Priority"; Descending = $true }, @{ Expression = { if ($_.Probe.IsDedicatedPreferred) { 1 } else { 0 } }; Descending = $true }, @{ Expression = "Root"; Descending = $false }
    )

    $selectedCandidate = $viableCandidates | Select-Object -First 1
    if (-not $selectedCandidate) {
        $details = if ($candidates.Count -gt 0) {
            ($candidates | ForEach-Object {
                $missing = if ($_.Probe.MissingBaseRequired.Count -gt 0) { "missing " + ($_.Probe.MissingBaseRequired -join ", ") } else { "not runnable" }
                '{0} ({1}; {2})' -f $_.Root, $_.Probe.KindLabel, $missing
            }) -join "; "
        }
        else {
            "No candidate roots were discovered."
        }

        throw "Unable to resolve a viable HLDS runtime template. $details Set HL_RUNTIME_TEMPLATE or HLDS_EXE, install Half-Life Dedicated Server, or run doctor-testbed.ps1 -Repair."
    }

    if (($selectedCandidate.Probe.Kind -eq "half_life_client_install") -and (-not ($viableCandidates | Where-Object { $_.Probe.IsDedicatedPreferred }))) {
        $selectionWarnings.Add("No dedicated HLDS template was available, so the selection fell back to a regular Half-Life install that happens to contain hlds.exe.")
    }

    return [PSCustomObject]@{
        SelectedCandidate = $selectedCandidate
        Candidates = $candidates.ToArray()
        ViableCandidates = $viableCandidates
        HasDedicatedCandidate = [bool]($viableCandidates | Where-Object { $_.Probe.IsDedicatedPreferred } | Select-Object -First 1)
        Warnings = $selectionWarnings.ToArray()
        SteamCmdExe = $steamCmdExe
    }
}

function Resolve-HldsTemplateRoot {
    param(
        [string]$ExplicitTemplateRoot,
        [string]$ExplicitHldsExe,
        [string]$ExplicitHlExe,
        [string]$ExplicitSteamCmdExe,
        [switch]$AllowSteamCmdDownload,
        [switch]$ForceRefreshSteamCmdTemplate
    )

    $selection = Get-HldsTemplateSelection -ExplicitTemplateRoot $ExplicitTemplateRoot -ExplicitHldsExe $ExplicitHldsExe -ExplicitHlExe $ExplicitHlExe -ExplicitSteamCmdExe $ExplicitSteamCmdExe -AllowSteamCmdDownload:$AllowSteamCmdDownload -ForceRefreshSteamCmdTemplate:$ForceRefreshSteamCmdTemplate
    return $selection.SelectedCandidate.Root
}

function Get-TestbedRuntimeManifestPath {
    param(
        [string]$RuntimeRoot = (Get-TestbedRuntimeRoot)
    )

    return (Join-Path (Get-FullPath -Path $RuntimeRoot) ".hl-server-runtime.json")
}

function Read-TestbedRuntimeManifest {
    param(
        [string]$RuntimeRoot = (Get-TestbedRuntimeRoot)
    )

    $manifestPath = Get-TestbedRuntimeManifestPath -RuntimeRoot $RuntimeRoot
    if (-not (Test-LeafPath -Path $manifestPath)) {
        return $null
    }

    try {
        return (Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json)
    }
    catch {
        return [PSCustomObject]@{
            Path = $manifestPath
            ParseError = $_.Exception.Message
        }
    }
}

function Write-TestbedRuntimeManifest {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RuntimeRoot,
        [Parameter(Mandatory = $true)]
        [string]$Configuration,
        [Parameter(Mandatory = $true)]
        $TemplateSelection,
        [Parameter(Mandatory = $true)]
        [string]$MirrorRoot,
        [Parameter(Mandatory = $true)]
        $MirrorResult,
        [Parameter(Mandatory = $true)]
        [string]$InstalledDllPath,
        [string]$InstalledPdbPath,
        [bool]$CreatedSteamAppIdFile = $false,
        $ClientInstall,
        [bool]$PreferClientMatchedRuntime = $false
    )

    $manifestPath = Get-TestbedRuntimeManifestPath -RuntimeRoot $RuntimeRoot
    $selectedCandidate = $TemplateSelection.SelectedCandidate
    $contentSourceRoot = if ($ClientInstall) { $ClientInstall.Root } else { $selectedCandidate.Root }
    $contentSourceHlExe = if ($ClientInstall) { $ClientInstall.HlExe } else { $selectedCandidate.Probe.HlExe }

    $manifest = [ordered]@{
        runtimeRoot = (Get-FullPath -Path $RuntimeRoot)
        configuration = $Configuration
        sourceRoot = $selectedCandidate.Root
        sourceKind = $selectedCandidate.Probe.Kind
        sourceKindLabel = $selectedCandidate.Probe.KindLabel
        selectionSource = $selectedCandidate.SelectionSource
        selectionReason = $selectedCandidate.Reason
        sourceHldsExe = $selectedCandidate.Probe.HldsExe
        sourceHlExe = $selectedCandidate.Probe.HlExe
        sourceSteamAppId = $selectedCandidate.Probe.SteamAppId
        runtimeMode = if ($PreferClientMatchedRuntime) { "client_matched_live" } else { "default" }
        preferClientMatchedRuntime = $PreferClientMatchedRuntime
        mirrorRoot = $MirrorRoot
        contentSourceRoot = $contentSourceRoot
        contentSourceHlExe = $contentSourceHlExe
        suggestedSteamAppId = $selectedCandidate.Probe.SuggestedSteamAppId
        createdSteamAppIdFile = $CreatedSteamAppIdFile
        installedDllPath = $InstalledDllPath
        installedPdbPath = $InstalledPdbPath
        mirroredTopLevelEntries = @($MirrorResult.TopLevelEntries)
        createdAt = (Get-Date -Format o)
    }

    $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding ASCII
    return $manifestPath
}

function Read-TestbedLiveModManifest {
    param(
        [string]$ModRoot,
        [string]$StageRoot
    )

    $manifestPath = Get-TestbedLiveModManifestPath -ModRoot $ModRoot -StageRoot $StageRoot
    if (-not (Test-LeafPath -Path $manifestPath)) {
        return $null
    }

    try {
        return (Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json)
    }
    catch {
        return [PSCustomObject]@{
            Path = $manifestPath
            ParseError = $_.Exception.Message
        }
    }
}

function Get-PathLinkTarget {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return $null
    }

    $item = Get-Item -LiteralPath $Path -Force
    $targetValue = $item.PSObject.Properties["Target"]
    if (-not $targetValue -or $null -eq $targetValue.Value) {
        return $null
    }

    $targets = @($targetValue.Value)
    if ($targets.Count -eq 0) {
        return $null
    }

    return (Get-FullPath -Path ([string]$targets[0]))
}

function Ensure-JunctionPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Target
    )

    $fullPath = Get-FullPath -Path $Path
    $fullTarget = Get-FullPath -Path $Target

    if (Test-Path -LiteralPath $fullPath) {
        $item = Get-Item -LiteralPath $fullPath -Force
        $currentTarget = Get-PathLinkTarget -Path $fullPath

        if ($item.LinkType -eq "Junction" -and (Test-PathsEqual -Left $currentTarget -Right $fullTarget)) {
            return $fullPath
        }

        $existingType = if ($item.PSIsContainer) { "directory" } else { "file" }
        throw "Expected a managed junction at $fullPath pointing to $fullTarget, but found an existing $existingType that is not reusable."
    }

    Ensure-Directory -Path (Split-Path -Parent $fullPath)
    New-Item -ItemType Junction -Path $fullPath -Target $fullTarget | Out-Null
    return $fullPath
}

function Reset-ManagedLiveModDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$ClientRoot,
        [Parameter(Mandatory = $true)]
        [string]$GameDirName
    )

    $fullPath = Get-FullPath -Path $Path
    $fullClientRoot = Get-FullPath -Path $ClientRoot
    Assert-PathWithinRoot -Path $fullPath -Root $fullClientRoot -Description "Managed live mod path"

    if (Test-Path -LiteralPath $fullPath) {
        $item = Get-Item -LiteralPath $fullPath -Force
        $manifestPath = Join-Path $fullPath ".hl-server-live-mod.json"
        $markerPath = Join-Path $fullPath ".hl-server-live-mod.txt"
        $isManaged = ($item.LinkType -eq "Junction") -or (Test-LeafPath -Path $manifestPath) -or (Test-LeafPath -Path $markerPath)

        if (-not $isManaged) {
            throw "Half-Life mod root $fullPath already exists and is not managed by hl-server. Move or rename it manually before retrying."
        }

        Remove-Item -LiteralPath $fullPath -Recurse -Force
    }

    New-Item -ItemType Directory -Path $fullPath -Force | Out-Null
    return $fullPath
}

function Write-TestbedLiveModLibList {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ModRoot,
        [Parameter(Mandatory = $true)]
        [string]$GameDirName
    )

    $libListPath = Join-Path (Get-FullPath -Path $ModRoot) "liblist.gam"
    $content = @(
        ('game "{0}"' -f "Half-Life Server Testbed")
        'startmap "crossfire"'
        'mpentity "info_player_deathmatch"'
        'gamedll "dlls\hl.dll"'
        'gamedll_linux "dlls/hl.so"'
        'gamedll_osx "dlls/hl.dylib"'
        'secure "0"'
        'type "multiplayer_only"'
        'fallback_dir "valve"'
        'hlversion "1111"'
    )

    Set-Content -LiteralPath $libListPath -Value $content -Encoding ASCII
    return $libListPath
}

function Get-TestbedLiveModContentLinks {
    return @()
}

function Write-TestbedLiveModManifest {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ModRoot,
        [Parameter(Mandatory = $true)]
        [string]$Configuration,
        [Parameter(Mandatory = $true)]
        [string]$ClientRoot,
        [Parameter(Mandatory = $true)]
        [string]$GameDirName,
        [Parameter(Mandatory = $true)]
        [string]$ClientHlExe,
        [Parameter(Mandatory = $true)]
        [string]$ClientHldsExe,
        [Parameter(Mandatory = $true)]
        [string]$InstalledDllPath,
        [string]$InstalledPdbPath
    )

    $manifestPath = Get-TestbedLiveModManifestPath -ModRoot $ModRoot
    $manifest = [ordered]@{
        modRoot = (Get-FullPath -Path $ModRoot)
        clientRoot = (Get-FullPath -Path $ClientRoot)
        gameDirName = $GameDirName
        modRootPath = (Get-TestbedLiveModRoot -ClientRoot $ClientRoot -GameDirName $GameDirName)
        configuration = $Configuration
        sourceHlExe = (Get-FullPath -Path $ClientHlExe)
        sourceHldsExe = (Get-FullPath -Path $ClientHldsExe)
        contentSourceRoot = (Get-FullPath -Path $ClientRoot)
        installedDllPath = (Get-FullPath -Path $InstalledDllPath)
        installedPdbPath = if ([string]::IsNullOrWhiteSpace($InstalledPdbPath)) { $null } else { (Get-FullPath -Path $InstalledPdbPath) }
        contentStrategy = "fallback_dir_valve"
        createdAt = (Get-Date -Format o)
    }

    $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding ASCII
    return $manifestPath
}

function Get-TestbedLiveModState {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ClientRoot,
        [string]$GameDirName = (Get-TestbedLiveModName)
    )

    $fullClientRoot = Get-FullPath -Path $ClientRoot
    $legacyStageRoot = Get-TestbedLiveModStageRoot -GameDirName $GameDirName
    $modRoot = Get-TestbedLiveModRoot -ClientRoot $fullClientRoot -GameDirName $GameDirName
    $modExists = Test-Path -LiteralPath $modRoot -PathType Container
    $modItem = if ($modExists) { Get-Item -LiteralPath $modRoot -Force } else { $null }
    $linkType = if ($modItem -and $modItem.PSObject.Properties["LinkType"]) { [string]$modItem.LinkType } else { $null }
    $manifest = Read-TestbedLiveModManifest -ModRoot $modRoot
    $manifestPath = Get-TestbedLiveModManifestPath -ModRoot $modRoot
    $entryStates = New-Object System.Collections.Generic.List[object]

    foreach ($entry in @(
        @{ RelativePath = "dlls\hl.dll"; Path = (Join-Path $modRoot "dlls\hl.dll"); Description = "server GameDLL for the live mod" }
        @{ RelativePath = "dlls\hl.pdb"; Path = (Join-Path $modRoot "dlls\hl.pdb"); Description = "debug symbols for the live mod" }
        @{ RelativePath = "liblist.gam"; Path = (Join-Path $modRoot "liblist.gam"); Description = "live mod game info file" }
        @{ RelativePath = ".hl-server-live-mod.json"; Path = $manifestPath; Description = "managed live mod manifest" }
        @{ RelativePath = $GameDirName; Path = $modRoot; Description = "managed live mod directory under the client root" }
    )) {
        $present = if ($entry.RelativePath -in @("dlls\hl.dll", "dlls\hl.pdb", "liblist.gam", ".hl-server-live-mod.json")) {
            Test-LeafPath -Path $entry.Path
        }
        else {
            Test-Path -LiteralPath $entry.Path -PathType Container
        }

        $entryStates.Add([PSCustomObject]@{
            RelativePath = $entry.RelativePath
            Description = $entry.Description
            Path = $entry.Path
            Present = $present
            AlwaysRequired = ($entry.RelativePath -ne "dlls\hl.pdb")
        })
    }

    $missingRequired = @(
        $entryStates |
        Where-Object { $_.AlwaysRequired -and (-not $_.Present) } |
        Select-Object -ExpandProperty RelativePath
    )

    $clientHlExe = Join-Path $fullClientRoot "hl.exe"
    $clientHldsExe = Join-Path $fullClientRoot "hlds.exe"
    $runtimeMap = Join-Path $fullClientRoot "valve\maps\crossfire.bsp"
    $sourceMap = Join-Path $fullClientRoot "valve\maps\crossfire.bsp"
    $rootIsDirectDirectory = $modExists -and [string]::IsNullOrWhiteSpace($linkType)

    return [PSCustomObject]@{
        ClientRoot = $fullClientRoot
        GameDirName = $GameDirName
        ModRoot = $modRoot
        StageRoot = $modRoot
        LinkPath = $modRoot
        LinkExists = $modExists
        LinkType = $linkType
        LinkTarget = $null
        LinkTargetMatchesStageRoot = $rootIsDirectDirectory
        RootIsDirectDirectory = $rootIsDirectDirectory
        LegacyStageRoot = $legacyStageRoot
        ManifestPath = $manifestPath
        Manifest = $manifest
        HlExe = if (Test-LeafPath -Path $clientHlExe) { $clientHlExe } else { $null }
        HldsExe = if (Test-LeafPath -Path $clientHldsExe) { $clientHldsExe } else { $null }
        RuntimeMapPath = $runtimeMap
        SourceMapPath = $sourceMap
        EntryStates = $entryStates.ToArray()
        MissingRequired = $missingRequired
        IsReady = ($missingRequired.Count -eq 0) -and $rootIsDirectDirectory -and (Test-LeafPath -Path $clientHlExe) -and (Test-LeafPath -Path $clientHldsExe)
    }
}

function Get-TestbedRuntimeEntryComparison {
    param(
        [Parameter(Mandatory = $true)]
        $SourceProbe,
        [Parameter(Mandatory = $true)]
        $RuntimeProbe
    )

    $comparisons = New-Object System.Collections.Generic.List[object]

    foreach ($definition in Get-TestbedRuntimeKeyEntryDefinitions) {
        $sourceState = $SourceProbe.EntryStates | Where-Object { $_.RelativePath -eq $definition.RelativePath } | Select-Object -First 1
        $runtimeState = $RuntimeProbe.EntryStates | Where-Object { $_.RelativePath -eq $definition.RelativePath } | Select-Object -First 1
        $sourcePresent = if ($sourceState) { [bool]$sourceState.Present } else { $false }
        $runtimePresent = if ($runtimeState) { [bool]$runtimeState.Present } else { $false }
        $expected = $definition.AlwaysRequired -or $sourcePresent

        $comparisons.Add([PSCustomObject]@{
            RelativePath = $definition.RelativePath
            Description = $definition.Description
            PathType = $definition.PathType
            SourcePresent = $sourcePresent
            RuntimePresent = $runtimePresent
            Expected = $expected
            MissingExpected = $expected -and (-not $runtimePresent)
        })
    }

    return $comparisons.ToArray()
}

function Get-LatestHldsLaunchFailure {
    $logsRoot = Get-TestbedLogsRoot
    if (-not (Test-Path -LiteralPath $logsRoot -PathType Container)) {
        return $null
    }

    $knownPatterns = @(
        "Unable to initialize Steam",
        'Failed to load "SDL3.dll"',
        'Failed to load "SDL2.dll"'
    )

    foreach ($logFile in (Get-ChildItem -LiteralPath $logsRoot -File -Filter "hlds-*.log" | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 20)) {
        $matches = New-Object System.Collections.Generic.List[string]

        foreach ($pattern in $knownPatterns) {
            if (Select-String -Path $logFile.FullName -Pattern $pattern -SimpleMatch -Quiet -ErrorAction SilentlyContinue) {
                $matches.Add($pattern)
            }
        }

        if ($matches.Count -gt 0) {
            return [PSCustomObject]@{
                Path = $logFile.FullName
                LastWriteTimeUtc = $logFile.LastWriteTimeUtc
                Matches = $matches.ToArray()
                Summary = ($matches -join "; ")
                Tail = @(Get-Content -LiteralPath $logFile.FullName -Tail 12)
            }
        }
    }

    return $null
}

function New-TestbedContentSignature {
    param(
        [string]$Root,
        [Parameter(Mandatory = $true)]
        [string]$RelativePath
    )

    $path = $null
    $exists = $false
    $hash = $null

    if (-not [string]::IsNullOrWhiteSpace($Root)) {
        $path = Join-Path (Get-FullPath -Path $Root) $RelativePath
        $exists = Test-LeafPath -Path $path
        if ($exists) {
            $hash = Get-FileSha256HashString -Path $path
        }
    }

    return [PSCustomObject]@{
        Root = if ([string]::IsNullOrWhiteSpace($Root)) { $null } else { Get-FullPath -Path $Root }
        RelativePath = $RelativePath
        Path = $path
        Exists = $exists
        Hash = $hash
    }
}

function Get-TestbedLiveContentStatus {
    param(
        [string]$RuntimeRoot,
        $SelectedCandidate,
        $ClientInstall,
        $RuntimeManifest,
        $LiveModState,
        [bool]$PreferClientMatchedRuntime = $false
    )

    $runtimeRootFull = Get-FullPath -Path $RuntimeRoot
    $clientRoot = if ($ClientInstall) { $ClientInstall.Root } else { $null }
    $runtimeSourceRoot = if ($SelectedCandidate) { $SelectedCandidate.Root } else { $null }
    $runtimeSourceKind = if ($SelectedCandidate) { $SelectedCandidate.Probe.Kind } else { $null }
    $effectiveContentRoot = $runtimeSourceRoot
    $manifestContentRoot = $null
    $runtimeRelativePath = "valve\maps\crossfire.bsp"
    $clientRelativePath = "valve\maps\crossfire.bsp"
    $sourceRelativePath = "valve\maps\crossfire.bsp"
    $runtimeHldsExe = Join-Path $runtimeRootFull "hlds.exe"
    $runtimeHlExe = Join-Path $runtimeRootFull "hl.exe"
    $runtimeHldsExists = Test-LeafPath -Path $runtimeHldsExe
    $runtimeHlExists = Test-LeafPath -Path $runtimeHlExe
    $clientLaunchExe = if ($runtimeHlExists) { $runtimeHlExe } elseif ($ClientInstall) { $ClientInstall.HlExe } else { $null }
    $serverWorkingDirectory = if ($runtimeHldsExists) { $runtimeRootFull } else { $null }
    $clientWorkingDirectory = if ($clientLaunchExe) { Split-Path -Parent $clientLaunchExe } else { $null }
    $liveModeKind = "disposable_runtime"

    if ($PreferClientMatchedRuntime -and $LiveModState -and $clientRoot) {
        $liveModeKind = "same_root_live_mod"
        $runtimeRootFull = $LiveModState.ModRoot
        $runtimeSourceRoot = $clientRoot
        $runtimeSourceKind = "half_life_client_install"
        $effectiveContentRoot = $clientRoot
        $manifestContentRoot = if ($LiveModState.Manifest) {
            Get-ObjectPropertyValue -InputObject $LiveModState.Manifest -Names @("contentSourceRoot", "clientRoot")
        }
        else {
            $null
        }
        $runtimeRelativePath = "valve\maps\crossfire.bsp"
        $clientRelativePath = "valve\maps\crossfire.bsp"
        $sourceRelativePath = "valve\maps\crossfire.bsp"
        $runtimeHldsExe = $LiveModState.HldsExe
        $runtimeHlExe = $LiveModState.HlExe
        $runtimeHldsExists = -not [string]::IsNullOrWhiteSpace($runtimeHldsExe)
        $runtimeHlExists = -not [string]::IsNullOrWhiteSpace($runtimeHlExe)
        $clientLaunchExe = $runtimeHlExe
        $serverWorkingDirectory = $clientRoot
        $clientWorkingDirectory = $clientRoot
    }
    elseif ($RuntimeManifest) {
        $manifestContentRoot = Get-ObjectPropertyValue -InputObject $RuntimeManifest -Names @("contentSourceRoot", "mirrorRoot", "sourceRoot")
        if ($PreferClientMatchedRuntime -and $clientRoot) {
            $effectiveContentRoot = $clientRoot
        }
    }

    $runtimeSignature = New-TestbedContentSignature -Root $(if ($liveModeKind -eq "same_root_live_mod") { $clientRoot } else { $runtimeRootFull }) -RelativePath $runtimeRelativePath
    $sourceSignature = New-TestbedContentSignature -Root $runtimeSourceRoot -RelativePath $sourceRelativePath
    $clientSignature = New-TestbedContentSignature -Root $clientRoot -RelativePath $clientRelativePath

    $runtimeMatchesClientHash = $runtimeSignature.Exists -and $clientSignature.Exists -and ($runtimeSignature.Hash -eq $clientSignature.Hash)
    $sourceMatchesClientHash = $sourceSignature.Exists -and $clientSignature.Exists -and ($sourceSignature.Hash -eq $clientSignature.Hash)
    $sourceMatchesClientRoot = Test-PathsEqual -Left $runtimeSourceRoot -Right $clientRoot
    $expectedContentMatchesClientRoot = Test-PathsEqual -Left $effectiveContentRoot -Right $clientRoot
    $manifestContentMatchesClientRoot = Test-PathsEqual -Left $manifestContentRoot -Right $clientRoot
    $expectedLaunchRoot = if ($liveModeKind -eq "same_root_live_mod") { $clientRoot } else { $runtimeRootFull }
    $serverLaunchUsesRuntimeRoot = Test-PathsEqual -Left $serverWorkingDirectory -Right $expectedLaunchRoot
    $clientLaunchUsesRuntimeRoot = Test-PathsEqual -Left $clientWorkingDirectory -Right $expectedLaunchRoot
    $sameRootLaunch = $serverLaunchUsesRuntimeRoot -and $clientLaunchUsesRuntimeRoot
    $contentMatch = $runtimeMatchesClientHash
    $contentMatchLabel = if ($runtimeSignature.Exists -and $clientSignature.Exists) {
        if ($contentMatch) { "yes" } else { "no" }
    }
    else {
        "unknown"
    }
    $rootMatchLabel = if ($clientRoot) {
        if ($expectedContentMatchesClientRoot) { "yes" } else { "no" }
    }
    else {
        "unknown"
    }
    $sameRootLaunchLabel = if ($clientLaunchExe -and $runtimeHldsExists) {
        if ($sameRootLaunch) { "yes" } else { "no" }
    }
    else {
        "unknown"
    }
    $diagnosisKind = if (-not $clientRoot) {
        "missing_client_root"
    }
    elseif ($PreferClientMatchedRuntime -and $liveModeKind -eq "same_root_live_mod" -and $LiveModState -and (-not $LiveModState.IsReady)) {
        "stale_live_mod"
    }
    elseif (-not $runtimeSignature.Exists -or -not $clientSignature.Exists) {
        "content_layout_mismatch"
    }
    elseif (-not $serverLaunchUsesRuntimeRoot -or ($PreferClientMatchedRuntime -and (-not $clientLaunchUsesRuntimeRoot))) {
        "wrong_working_directory"
    }
    elseif (-not $runtimeMatchesClientHash) {
        "different_map_checksums"
    }
    elseif ($PreferClientMatchedRuntime -and $liveModeKind -eq "same_root_live_mod" -and $sameRootLaunch -and $expectedContentMatchesClientRoot) {
        "same_root_live_mod"
    }
    elseif ($PreferClientMatchedRuntime -and $sameRootLaunch -and $expectedContentMatchesClientRoot) {
        "same_root_client_matched"
    }
    elseif (-not $expectedContentMatchesClientRoot) {
        "different_source_roots"
    }
    else {
        "content_match"
    }

    $verdict = if (-not $clientRoot) {
        "no stock client root was resolved"
    }
    elseif (-not $runtimeSourceRoot) {
        "runtime source was not resolved"
    }
    elseif ($PreferClientMatchedRuntime -and $liveModeKind -eq "same_root_live_mod" -and $LiveModState -and (-not $LiveModState.IsReady)) {
        "the managed same-root live mod is missing required files in the Half-Life mod directory"
    }
    elseif ($runtimeMatchesClientHash) {
        if ($PreferClientMatchedRuntime -and $liveModeKind -eq "same_root_live_mod" -and $sameRootLaunch -and $expectedContentMatchesClientRoot) {
            "server and client launch from the same Half-Life root through the managed live mod"
        }
        elseif ($PreferClientMatchedRuntime -and $expectedContentMatchesClientRoot) {
            if ($sourceMatchesClientRoot) {
                "runtime and client share the same live content root"
            }
            else {
                "runtime crossfire matches the client after live content mirroring"
            }
        }
        elseif ($sourceMatchesClientRoot) {
            "runtime and client share the same live content root"
        }
        elseif ($sourceMatchesClientHash) {
            "different roots, but crossfire matches between runtime source and client"
        }
        else {
            "runtime crossfire matches the client after live content mirroring"
        }
    }
    elseif ($sourceMatchesClientHash) {
        "selected source matches the client, but the disposable runtime is stale"
    }
    else {
        "crossfire differs between the runtime/client content roots"
    }

    return [PSCustomObject]@{
        PreferClientMatchedRuntime = $PreferClientMatchedRuntime
        ClientHlExe = if ($ClientInstall) { $ClientInstall.HlExe } else { $null }
        ClientRoot = $clientRoot
        RuntimeSourceRoot = $runtimeSourceRoot
        RuntimeSourceKind = $runtimeSourceKind
        RuntimeRoot = $runtimeRootFull
        RuntimeHldsExe = if ($runtimeHldsExists) { $runtimeHldsExe } else { $null }
        RuntimeHlExe = if ($runtimeHlExists) { $runtimeHlExe } else { $null }
        ClientLaunchExe = $clientLaunchExe
        ServerWorkingDirectory = $serverWorkingDirectory
        ClientWorkingDirectory = $clientWorkingDirectory
        EffectiveContentRoot = $effectiveContentRoot
        ManifestContentRoot = $manifestContentRoot
        RuntimeMap = $runtimeSignature
        SourceMap = $sourceSignature
        ClientMap = $clientSignature
        SourceMatchesClientRoot = $sourceMatchesClientRoot
        SourceMatchesClientHash = $sourceMatchesClientHash
        ExpectedContentMatchesClientRoot = $expectedContentMatchesClientRoot
        ManifestContentMatchesClientRoot = $manifestContentMatchesClientRoot
        RuntimeMatchesClientHash = $runtimeMatchesClientHash
        ServerLaunchUsesRuntimeRoot = $serverLaunchUsesRuntimeRoot
        ClientLaunchUsesRuntimeRoot = $clientLaunchUsesRuntimeRoot
        SameRootLaunch = $sameRootLaunch
        SameRootLaunchLabel = $sameRootLaunchLabel
        ContentMatch = $contentMatch
        ContentMatchLabel = $contentMatchLabel
        RootMatchLabel = $rootMatchLabel
        DiagnosisKind = $diagnosisKind
        Verdict = $verdict
        LiveModeKind = $liveModeKind
        GameDirName = if ($PreferClientMatchedRuntime -and $liveModeKind -eq "same_root_live_mod") { $LiveModState.GameDirName } else { "valve" }
        LiveModStageRoot = if ($LiveModState -and (Test-Path -LiteralPath $LiveModState.LegacyStageRoot -PathType Container)) { $LiveModState.LegacyStageRoot } else { $null }
        LiveModLinkPath = if ($LiveModState) { $LiveModState.ModRoot } else { $null }
        LiveModManifestPath = if ($LiveModState) { $LiveModState.ManifestPath } else { $null }
    }
}

function Get-FileSha256HashString {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $resolvedPath = Get-FullPath -Path $Path
    $stream = [System.IO.File]::Open($resolvedPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    try {
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        try {
            return ([System.BitConverter]::ToString($sha256.ComputeHash($stream))).Replace("-", "")
        }
        finally {
            $sha256.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
}

function Get-TestbedDoctorReport {
    param(
        [ValidateSet("Debug", "Release")]
        [string]$Configuration = "Debug",
        [string]$ExplicitTemplateRoot,
        [string]$ExplicitHldsExe,
        [string]$ExplicitHlExe,
        [string]$ExplicitSteamCmdExe,
        [switch]$AllowSteamCmdDownload,
        [switch]$ForceRefreshSteamCmdTemplate,
        [switch]$PreferClientMatchedRuntime,
        [switch]$IgnoreLatestLaunchFailure
    )

    $configuration = Get-ValidatedConfiguration -Configuration $Configuration
    $runtimeRoot = Get-TestbedRuntimeRoot
    $runtimeProbe = Get-HldsRuntimeProbe -Root $runtimeRoot
    $runtimeManifest = Read-TestbedRuntimeManifest -RuntimeRoot $runtimeRoot
    $buildDllPath = Get-HlDllPath -Configuration $configuration
    $buildDllExists = Test-LeafPath -Path $buildDllPath
    $clientInstall = $null
    $clientInstallError = $null
    $liveModState = $null

    $selection = $null
    $selectionError = $null

    try {
        $selection = Get-HldsTemplateSelection -ExplicitTemplateRoot $ExplicitTemplateRoot -ExplicitHldsExe $ExplicitHldsExe -ExplicitHlExe $ExplicitHlExe -ExplicitSteamCmdExe $ExplicitSteamCmdExe -AllowSteamCmdDownload:$AllowSteamCmdDownload -ForceRefreshSteamCmdTemplate:$ForceRefreshSteamCmdTemplate
    }
    catch {
        $selectionError = $_.Exception.Message
    }

    try {
        $clientInstall = Resolve-TestbedClientInstall -ExplicitHlExe $ExplicitHlExe
    }
    catch {
        $clientInstallError = $_.Exception.Message
    }

    if ($clientInstall) {
        $liveModState = Get-TestbedLiveModState -ClientRoot $clientInstall.Root
    }

    $entryComparison = @()
    if ((-not $PreferClientMatchedRuntime) -and $selection -and $selection.SelectedCandidate -and $runtimeProbe.Exists) {
        $entryComparison = @(Get-TestbedRuntimeEntryComparison -SourceProbe $selection.SelectedCandidate.Probe -RuntimeProbe $runtimeProbe)
    }

    $liveContentStatus = Get-TestbedLiveContentStatus -RuntimeRoot $runtimeRoot -SelectedCandidate $(if ($selection) { $selection.SelectedCandidate } else { $null }) -ClientInstall $clientInstall -RuntimeManifest $runtimeManifest -LiveModState $liveModState -PreferClientMatchedRuntime:$PreferClientMatchedRuntime

    $issues = New-Object System.Collections.Generic.List[string]
    $warnings = New-Object System.Collections.Generic.List[string]
    $recommendations = New-Object System.Collections.Generic.List[string]
    $classification = "healthy"
    $needsRepair = $false

    if (-not $buildDllExists) {
        $warnings.Add("Build artifact not found at $buildDllPath.")
        $recommendations.Add("Run .\scripts\build.ps1 -Configuration $configuration before attempting a repair.")
    }

    if ($selectionError) {
        if ($PreferClientMatchedRuntime) {
            $warnings.Add($selectionError)
        }
        else {
            $classification = "insufficient template source"
            $issues.Add($selectionError)
            $recommendations.Add("Set HL_RUNTIME_TEMPLATE or HLDS_EXE to a valid runtime root, or run .\scripts\doctor-testbed.ps1 -Repair to provision a dedicated template into testbed/cache.")
        }
    }
    elseif ($selection -and $selection.SelectedCandidate) {
        foreach ($warning in @($selection.Warnings)) {
            if (-not [string]::IsNullOrWhiteSpace($warning)) {
                $warnings.Add($warning)
            }
        }

        if ((-not $PreferClientMatchedRuntime) -and (Test-PathsEqual -Left $selection.SelectedCandidate.Root -Right $runtimeRoot)) {
            $classification = "insufficient template source"
            $issues.Add("The selected runtime template resolves to the disposable runtime itself. Choose a real install root or a cached template instead.")
            $recommendations.Add("Clear HL_RUNTIME_TEMPLATE and HLDS_EXE if they point at testbed/runtime, then rerun the doctor.")
        }
    }

    if ($clientInstallError) {
        if ($PreferClientMatchedRuntime -and ($classification -eq "healthy")) {
            $classification = "missing stock client"
            $issues.Add($clientInstallError)
            $recommendations.Add("Set HL_EXE in .env or pass -HlExe <path> so same-root live sessions can use the real Half-Life root.")
        }
        else {
            $warnings.Add($clientInstallError)
        }
    }
    elseif ($PreferClientMatchedRuntime -and (-not $clientInstall)) {
        if ($classification -eq "healthy") {
            $classification = "missing stock client"
        }

        $issues.Add("Same-root live mode was requested, but no stock hl.exe could be resolved.")
        $recommendations.Add("Set HL_EXE in .env or pass -HlExe <path> so live client-attached sessions can use the real Half-Life root.")
    }

    if ($classification -eq "healthy") {
        if ($PreferClientMatchedRuntime) {
            if ($clientInstall -and (-not $clientInstall.Probe.HldsExe)) {
                $classification = "missing same-root hlds"
                $needsRepair = $false
                $issues.Add("Same-root live mode requires hlds.exe in the same Half-Life root as $($clientInstall.HlExe).")
                $recommendations.Add("Use a Half-Life install that already contains hlds.exe, or repoint HL_EXE to one that does.")
            }
            elseif ($liveModState -and $liveModState.Manifest -and $liveModState.Manifest.PSObject.Properties["ParseError"]) {
                $classification = "stale live mod"
                $needsRepair = $true
                $issues.Add("Managed live mod manifest could not be parsed: $($liveModState.Manifest.ParseError)")
                $recommendations.Add("Run .\scripts\doctor-testbed.ps1 -Repair -PreferClientMatchedRuntime to rebuild the managed live mod.")
            }
            elseif (-not $liveModState) {
                $classification = "stale live mod"
                $needsRepair = $true
                $issues.Add("Managed live mod state could not be resolved from the current Half-Life root.")
                $recommendations.Add("Run .\scripts\doctor-testbed.ps1 -Repair -PreferClientMatchedRuntime to recreate the managed live mod.")
            }
            elseif (-not $liveModState.Manifest) {
                $classification = "stale live mod"
                $needsRepair = $true
                $issues.Add("Managed live mod manifest is missing from $($liveModState.ModRoot).")
                $recommendations.Add("Run .\scripts\doctor-testbed.ps1 -Repair -PreferClientMatchedRuntime to recreate the managed live mod.")
            }
            elseif ($liveModState.Manifest.configuration -and (-not $liveModState.Manifest.configuration.Equals($configuration, [System.StringComparison]::OrdinalIgnoreCase))) {
                $classification = "stale live mod"
                $needsRepair = $true
                $issues.Add("Managed live mod was prepared for configuration $($liveModState.Manifest.configuration), not the requested $configuration build.")
                $recommendations.Add("Run .\scripts\doctor-testbed.ps1 -Repair -PreferClientMatchedRuntime to reinstall the live mod for $configuration.")
            }
            elseif (-not $liveModState.RootIsDirectDirectory) {
                $classification = "stale live mod"
                $needsRepair = $true
                $issues.Add("Half-Life live mod root $($liveModState.ModRoot) is not a direct managed mod directory.")
                $recommendations.Add("Run .\scripts\doctor-testbed.ps1 -Repair -PreferClientMatchedRuntime to recreate the managed mod directly under the Half-Life root.")
            }
            elseif (-not $liveContentStatus.ManifestContentMatchesClientRoot) {
                $classification = "stale live mod"
                $needsRepair = $true
                $issues.Add("Managed live mod content root is $($liveContentStatus.ManifestContentRoot), but same-root live mode expects $($liveContentStatus.ClientRoot).")
                $recommendations.Add("Run .\scripts\doctor-testbed.ps1 -Repair -PreferClientMatchedRuntime to rebuild the live mod from the current Half-Life root.")
            }
        }
        elseif (-not $runtimeProbe.Exists) {
            $classification = "stale disposable runtime"
            $needsRepair = $true
            $issues.Add("Disposable runtime root does not exist: $runtimeRoot")
            $recommendations.Add("Run .\scripts\doctor-testbed.ps1 -Repair to rebuild testbed/runtime from the selected source.")
        }
        elseif ($runtimeManifest -and $runtimeManifest.PSObject.Properties["ParseError"]) {
            $classification = "stale disposable runtime"
            $needsRepair = $true
            $issues.Add("Disposable runtime manifest could not be parsed: $($runtimeManifest.ParseError)")
            $recommendations.Add("Run .\scripts\doctor-testbed.ps1 -Repair to rebuild the disposable runtime and rewrite the manifest.")
        }
        elseif (-not $runtimeManifest) {
            $classification = "stale disposable runtime"
            $needsRepair = $true
            $issues.Add("Disposable runtime manifest is missing from testbed/runtime.")
            $recommendations.Add("Run .\scripts\doctor-testbed.ps1 -Repair to refresh the disposable runtime.")
        }
        elseif ($runtimeManifest.sourceRoot -and $selection -and $selection.SelectedCandidate -and (-not (Test-PathsEqual -Left $runtimeManifest.sourceRoot -Right $selection.SelectedCandidate.Root))) {
            $classification = "stale disposable runtime"
            $needsRepair = $true
            $issues.Add("Disposable runtime was mirrored from $($runtimeManifest.sourceRoot), but the current source selection resolves to $($selection.SelectedCandidate.Root).")
            $recommendations.Add("Run .\scripts\doctor-testbed.ps1 -Repair to refresh testbed/runtime from the newly selected source.")
        }
        elseif ($runtimeManifest.configuration -and (-not $runtimeManifest.configuration.Equals($configuration, [System.StringComparison]::OrdinalIgnoreCase))) {
            $classification = "stale disposable runtime"
            $needsRepair = $true
            $issues.Add("Disposable runtime was prepared for configuration $($runtimeManifest.configuration), not the requested $configuration build.")
            $recommendations.Add("Run .\scripts\install-testbed.ps1 -Configuration $configuration to refresh the disposable runtime.")
        }
    }

    if (($classification -eq "healthy") -and (-not $PreferClientMatchedRuntime) -and $runtimeProbe.Exists -and (-not $runtimeProbe.IsRunnable)) {
        $classification = "missing executable dependency"
        $needsRepair = $true
        $issues.Add("Disposable runtime is missing required base entries: $($runtimeProbe.MissingBaseRequired -join ', ').")
        $recommendations.Add("Run .\scripts\doctor-testbed.ps1 -Repair to refresh the mirrored executable-side dependencies.")
    }

    $missingExpectedEntries = @($entryComparison | Where-Object { $_.MissingExpected } | Select-Object -ExpandProperty RelativePath)
    if (($classification -eq "healthy") -and (-not $PreferClientMatchedRuntime) -and $missingExpectedEntries.Count -gt 0) {
        $classification = "missing executable dependency"
        $needsRepair = $true
        $issues.Add("Disposable runtime is missing entries that exist in the selected source: $($missingExpectedEntries -join ', ').")
        $recommendations.Add("Run .\scripts\doctor-testbed.ps1 -Repair to refresh the mirrored executable-side dependencies.")
    }

    if ($classification -eq "healthy" -and $liveContentStatus.ClientRoot) {
        if ($PreferClientMatchedRuntime -and (-not $liveContentStatus.RuntimeMatchesClientHash)) {
            $classification = "live content mismatch"
            $needsRepair = $true
            $issues.Add("Managed live mod map $($liveContentStatus.RuntimeMap.Path) does not match the live client map $($liveContentStatus.ClientMap.Path).")
            $recommendations.Add("Run .\scripts\doctor-testbed.ps1 -Repair -PreferClientMatchedRuntime to rebuild the same-root live mod.")
        }
        elseif ($PreferClientMatchedRuntime -and $liveModState -and (-not $liveModState.IsReady)) {
            $classification = "stale live mod"
            $needsRepair = $true
            $issues.Add("Managed live mod is missing required entries: $($liveModState.MissingRequired -join ', ').")
            $recommendations.Add("Run .\scripts\doctor-testbed.ps1 -Repair -PreferClientMatchedRuntime to rebuild the same-root live mod.")
        }
        elseif ((-not $PreferClientMatchedRuntime) -and (-not $liveContentStatus.SourceMatchesClientRoot)) {
            if ($liveContentStatus.SourceMatchesClientHash) {
                $warnings.Add("Live sessions currently resolve the runtime source to $($liveContentStatus.RuntimeSourceRoot) while the stock client root is $($liveContentStatus.ClientRoot). crossfire.bsp matches today, but the roots differ.")
            }
            else {
                $warnings.Add("Live sessions currently resolve the runtime source to $($liveContentStatus.RuntimeSourceRoot) while the stock client root is $($liveContentStatus.ClientRoot), and crossfire.bsp hashes differ. Use -PreferClientMatchedRuntime for live play to avoid 'different map'.")
                $recommendations.Add("Use .\scripts\doctor-testbed.ps1 -PreferClientMatchedRuntime -Repair before a live client-attached session.")
            }
        }
    }

    $latestLaunchFailure = if ($IgnoreLatestLaunchFailure) { $null } else { Get-LatestHldsLaunchFailure }
    $manifestTimestampSource = if ($PreferClientMatchedRuntime -and $liveModState) { $liveModState.Manifest } else { $runtimeManifest }
    $manifestCreatedAtUtc = $null
    if ($manifestTimestampSource -and $manifestTimestampSource.PSObject.Properties["createdAt"] -and (-not [string]::IsNullOrWhiteSpace([string]$manifestTimestampSource.createdAt))) {
        try {
            $manifestCreatedAtUtc = ([DateTimeOffset]::Parse([string]$manifestTimestampSource.createdAt)).UtcDateTime
        }
        catch {
            $manifestCreatedAtUtc = $null
        }
    }

    $latestFailureAppliesToCurrentRuntime = $latestLaunchFailure -and (($null -eq $manifestCreatedAtUtc) -or ($latestLaunchFailure.LastWriteTimeUtc -ge $manifestCreatedAtUtc))
    if ($latestFailureAppliesToCurrentRuntime) {
        if ($classification -eq "healthy") {
            $classification = "unknown external Steam/runtime issue"
            $issues.Add("Latest HLDS launch log still reports: $($latestLaunchFailure.Summary)")
            if ($selection -and $selection.SelectedCandidate -and $selection.SelectedCandidate.Probe.Kind -eq "half_life_client_install" -and (-not $selection.HasDedicatedCandidate)) {
                $recommendations.Add("Run .\scripts\doctor-testbed.ps1 -Repair to cache a dedicated HLDS template instead of relying on the regular Half-Life client install.")
            }
            else {
                $recommendations.Add("Verify the selected runtime still launches outside the repo and that the local Steam install is healthy.")
            }
        }
        else {
            $warnings.Add("Latest HLDS launch log reports: $($latestLaunchFailure.Summary)")
        }
    }
    elseif ($latestLaunchFailure) {
        $warnings.Add("Ignoring older HLDS launch failure from $($latestLaunchFailure.Path) because the disposable runtime has been refreshed since then.")
    }

    return [PSCustomObject]@{
        Configuration = $configuration
        RuntimeRoot = $runtimeRoot
        RuntimeProbe = $runtimeProbe
        RuntimeManifest = $runtimeManifest
        BuildDllPath = $buildDllPath
        BuildDllExists = $buildDllExists
        SourceSelection = $selection
        SourceSelectionError = $selectionError
        ClientInstall = $clientInstall
        ClientInstallError = $clientInstallError
        LiveModState = $liveModState
        LiveContentStatus = $liveContentStatus
        EntryComparison = $entryComparison
        Classification = $classification
        Issues = $issues.ToArray()
        Warnings = $warnings.ToArray()
        Recommendations = $recommendations.ToArray()
        LatestLaunchFailure = $latestLaunchFailure
        LatestLaunchFailureApplies = $latestFailureAppliesToCurrentRuntime
        NeedsRepair = $needsRepair
        IsHealthy = ($classification -eq "healthy")
    }
}

function Write-TestbedDoctorSummary {
    param(
        [Parameter(Mandatory = $true)]
        $Report
    )

    $selection = if ($Report.SourceSelection) { $Report.SourceSelection.SelectedCandidate } else { $null }

    Write-Step "Testbed runtime doctor"
    Write-Host "Diagnosis           : $($Report.Classification)"
    Write-Host "Configuration       : $($Report.Configuration)"
    Write-Host "Disposable runtime  : $($Report.RuntimeRoot)"
    Write-Host "Build hl.dll        : $(if ($Report.BuildDllExists) { $Report.BuildDllPath } else { 'missing' })"

    if ($selection) {
        Write-Host "Selected source     : $($selection.Root)"
        Write-Host "Source kind         : $($selection.Probe.KindLabel)"
        Write-Host "Selection reason    : $($selection.Reason)"
        Write-Host "Selected hlds.exe   : $(if ($selection.Probe.HldsExe) { $selection.Probe.HldsExe } else { 'missing' })"
        Write-Host "Selected hl.exe     : $(if ($selection.Probe.HlExe) { $selection.Probe.HlExe } else { 'not found' })"
    }
    elseif ($Report.SourceSelectionError) {
        Write-Host "Selected source     : unavailable"
        Write-Host "Selection reason    : $($Report.SourceSelectionError)"
    }

    $manifestPath = Get-TestbedRuntimeManifestPath -RuntimeRoot $Report.RuntimeRoot
    if ($Report.LiveContentStatus.PreferClientMatchedRuntime -and $Report.LiveModState) {
        Write-Host "Live mod root       : $($Report.LiveModState.ModRoot)"
        Write-Host "Live mod manifest   : $(if (Test-LeafPath -Path $Report.LiveModState.ManifestPath) { $Report.LiveModState.ManifestPath } else { 'missing' })"
        Write-Host "Live game dir       : $($Report.LiveContentStatus.GameDirName)"
    }
    else {
        Write-Host "Runtime manifest    : $(if (Test-LeafPath -Path $manifestPath) { $manifestPath } else { 'missing' })"
    }
    Write-Host "Runtime hlds.exe    : $(if ($Report.LiveContentStatus.RuntimeHldsExe) { $Report.LiveContentStatus.RuntimeHldsExe } else { 'missing' })"
    Write-Host "Runtime hl.exe      : $(if ($Report.LiveContentStatus.RuntimeHlExe) { $Report.LiveContentStatus.RuntimeHlExe } else { 'not found' })"
    Write-Host "Client hl.exe       : $(if ($Report.LiveContentStatus.ClientHlExe) { $Report.LiveContentStatus.ClientHlExe } else { 'not found' })"
    Write-Host "Launch client exe   : $(if ($Report.LiveContentStatus.ClientLaunchExe) { $Report.LiveContentStatus.ClientLaunchExe } else { 'not found' })"
    Write-Host "Client root         : $(if ($Report.LiveContentStatus.ClientRoot) { $Report.LiveContentStatus.ClientRoot } else { 'not found' })"
    Write-Host "Live content mode   : $(if ($Report.LiveContentStatus.PreferClientMatchedRuntime) { 'same-root live mod requested' } else { 'default selection' })"
    Write-Host "Content source root : $(if ($Report.LiveContentStatus.EffectiveContentRoot) { $Report.LiveContentStatus.EffectiveContentRoot } else { 'unavailable' })"
    Write-Host "Manifest content    : $(if ($Report.LiveContentStatus.ManifestContentRoot) { $Report.LiveContentStatus.ManifestContentRoot } else { 'missing' })"
    Write-Host "Server work dir     : $(if ($Report.LiveContentStatus.ServerWorkingDirectory) { $Report.LiveContentStatus.ServerWorkingDirectory } else { 'unknown' })"
    Write-Host "Client work dir     : $(if ($Report.LiveContentStatus.ClientWorkingDirectory) { $Report.LiveContentStatus.ClientWorkingDirectory } else { 'unknown' })"
    Write-Host "Same-root launch    : $($Report.LiveContentStatus.SameRootLaunchLabel)"
    Write-Host "content_match       : $($Report.LiveContentStatus.ContentMatchLabel)"
    Write-Host "root_match          : $($Report.LiveContentStatus.RootMatchLabel)"
    Write-Host "Live diagnosis kind : $($Report.LiveContentStatus.DiagnosisKind)"
    Write-Host "Live content verdict: $($Report.LiveContentStatus.Verdict)"

    if ($Report.LiveContentStatus.RuntimeMap.Path) {
        Write-Host "Runtime crossfire   : $($Report.LiveContentStatus.RuntimeMap.Path)"
    }

    if ($Report.LiveContentStatus.ClientMap.Path) {
        Write-Host "Client crossfire    : $($Report.LiveContentStatus.ClientMap.Path)"
    }

    if ($Report.LiveContentStatus.SourceMap.Path) {
        Write-Host "Source crossfire    : $($Report.LiveContentStatus.SourceMap.Path)"
    }

    if ($Report.LiveContentStatus.RuntimeMap.Hash) {
        Write-Host "Runtime crossfire SHA256: $($Report.LiveContentStatus.RuntimeMap.Hash)"
    }

    if ($Report.LiveContentStatus.ClientMap.Hash) {
        Write-Host "Client crossfire SHA256 : $($Report.LiveContentStatus.ClientMap.Hash)"
    }

    if ($Report.LiveContentStatus.SourceMap.Hash) {
        Write-Host "Source crossfire SHA256 : $($Report.LiveContentStatus.SourceMap.Hash)"
    }

    if ($Report.LatestLaunchFailure -and $Report.LatestLaunchFailureApplies) {
        Write-Host "Latest blocker      : $($Report.LatestLaunchFailure.Summary)"
        Write-Host "Latest blocker log  : $($Report.LatestLaunchFailure.Path)"
    }

    $entriesToPrint = if ($Report.LiveContentStatus.PreferClientMatchedRuntime -and $Report.LiveModState) {
        @(
            $Report.LiveModState.EntryStates | ForEach-Object {
                [PSCustomObject]@{
                    RelativePath = $_.RelativePath
                    Description = $_.Description
                    SourcePresent = $null
                    RuntimePresent = $_.Present
                    Expected = $_.AlwaysRequired
                    MissingExpected = $_.AlwaysRequired -and (-not $_.Present)
                }
            }
        )
    }
    elseif ($Report.EntryComparison.Count -gt 0) {
        $Report.EntryComparison
    }
    else {
        @(
            $Report.RuntimeProbe.EntryStates | ForEach-Object {
                [PSCustomObject]@{
                    RelativePath = $_.RelativePath
                    Description = $_.Description
                    SourcePresent = $null
                    RuntimePresent = $_.Present
                    Expected = $_.AlwaysRequired
                    MissingExpected = $_.AlwaysRequired -and (-not $_.Present)
                }
            }
        )
    }

    foreach ($entry in $entriesToPrint) {
        $status = if ($entry.RuntimePresent) {
            "found"
        }
        elseif ($entry.Expected) {
            "missing"
        }
        else {
            "not present"
        }

        $sourceHint = if ($null -ne $entry.SourcePresent) {
            "; source=" + $(if ($entry.SourcePresent) { "found" } else { "not present" })
        }
        else {
            ""
        }

        Write-Host ("Key entry           : {0} -> {1}{2}" -f $entry.RelativePath, $status, $sourceHint)
    }

    foreach ($warning in @($Report.Warnings)) {
        Write-Host "Warning             : $warning"
    }

    foreach ($issue in @($Report.Issues)) {
        Write-Host "Issue               : $issue"
    }

    foreach ($recommendation in @($Report.Recommendations)) {
        Write-Host "Remediation         : $recommendation"
    }
}

function Get-TestbedDoctorFailureMessage {
    param(
        [Parameter(Mandatory = $true)]
        $Report
    )

    $issue = if ($Report.Issues.Count -gt 0) { $Report.Issues[0] } else { "The testbed runtime doctor reported '$($Report.Classification)'." }
    return ("Testbed runtime diagnosis '{0}': {1}" -f $Report.Classification, $issue)
}

function Copy-RuntimeTemplateToDisposable {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceRoot,
        [Parameter(Mandatory = $true)]
        [string]$RuntimeRoot
    )

    $topLevelEntries = New-Object System.Collections.Generic.List[string]

    foreach ($item in (Get-ChildItem -LiteralPath $SourceRoot -Force | Sort-Object Name)) {
        $topLevelEntries.Add($item.Name)
        Copy-Item -LiteralPath $item.FullName -Destination $RuntimeRoot -Recurse -Force
    }

    return [PSCustomObject]@{
        SourceRoot = $SourceRoot
        RuntimeRoot = $RuntimeRoot
        TopLevelEntries = $topLevelEntries.ToArray()
    }
}

function Ensure-DisposableRuntimeKeyEntries {
    param(
        [Parameter(Mandatory = $true)]
        $SourceProbe,
        [Parameter(Mandatory = $true)]
        [string]$RuntimeRoot
    )

    $createdSteamAppIdFile = $false

    foreach ($definition in Get-TestbedRuntimeKeyEntryDefinitions) {
        $sourcePath = Join-Path $SourceProbe.Root $definition.RelativePath
        $runtimePath = Join-Path $RuntimeRoot $definition.RelativePath
        $sourceState = $SourceProbe.EntryStates | Where-Object { $_.RelativePath -eq $definition.RelativePath } | Select-Object -First 1
        $sourcePresent = if ($sourceState) { [bool]$sourceState.Present } else { $false }

        if (-not $sourcePresent) {
            continue
        }

        if ($definition.PathType -eq "Container") {
            if (-not (Test-Path -LiteralPath $runtimePath -PathType Container)) {
                Copy-Item -LiteralPath $sourcePath -Destination $RuntimeRoot -Recurse -Force
            }
        }
        elseif (-not (Test-LeafPath -Path $runtimePath)) {
            Ensure-Directory -Path (Split-Path -Parent $runtimePath)
            Copy-Item -LiteralPath $sourcePath -Destination $runtimePath -Force
        }
    }

    $runtimeSteamAppIdPath = Join-Path $RuntimeRoot "steam_appid.txt"
    if ((-not (Test-LeafPath -Path $runtimeSteamAppIdPath)) -and (-not [string]::IsNullOrWhiteSpace($SourceProbe.SuggestedSteamAppId))) {
        Set-Content -LiteralPath $runtimeSteamAppIdPath -Value $SourceProbe.SuggestedSteamAppId -Encoding ASCII
        $createdSteamAppIdFile = $true
    }

    return [PSCustomObject]@{
        CreatedSteamAppIdFile = $createdSteamAppIdFile
    }
}

function Get-TestbedRuntimeProcesses {
    param(
        [string]$RuntimeRoot = (Get-TestbedRuntimeRoot)
    )

    $fullRuntimeRoot = Get-FullPath -Path $RuntimeRoot
    $processes = New-Object System.Collections.Generic.List[object]

    foreach ($process in (Get-Process -ErrorAction SilentlyContinue)) {
        $processPath = $null

        try {
            $processPath = $process.Path
        }
        catch {
            $processPath = $null
        }

        if ([string]::IsNullOrWhiteSpace($processPath)) {
            continue
        }

        $fullProcessPath = Get-FullPath -Path $processPath
        if ($fullProcessPath.StartsWith($fullRuntimeRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            $processes.Add([PSCustomObject]@{
                Id = $process.Id
                ProcessName = $process.ProcessName
                Path = $fullProcessPath
            })
        }
    }

    return $processes.ToArray()
}

function Get-TestbedLiveModProcesses {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ClientRoot
    )

    $fullClientRoot = Get-FullPath -Path $ClientRoot
    $processes = New-Object System.Collections.Generic.List[object]

    foreach ($process in (Get-Process -ErrorAction SilentlyContinue)) {
        $processPath = $null

        try {
            $processPath = $process.Path
        }
        catch {
            $processPath = $null
        }

        if ([string]::IsNullOrWhiteSpace($processPath)) {
            continue
        }

        $fullProcessPath = Get-FullPath -Path $processPath
        if (($process.ProcessName -in @("hl", "hlds")) -and $fullProcessPath.StartsWith($fullClientRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            $processes.Add([PSCustomObject]@{
                Id = $process.Id
                ProcessName = $process.ProcessName
                Path = $fullProcessPath
            })
        }
    }

    return $processes.ToArray()
}

function Install-TestbedLiveMod {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Configuration,
        [Parameter(Mandatory = $true)]
        [string]$BuiltDllPath,
        [string]$BuiltPdbPath,
        [string]$ExplicitHlExe
    )

    $configuration = Get-ValidatedConfiguration -Configuration $Configuration
    $clientInstall = Resolve-TestbedClientInstall -ExplicitHlExe $ExplicitHlExe
    if (-not $clientInstall) {
        throw "Same-root live mod mode requires a stock hl.exe. Set HL_EXE in .env or pass -HlExe <path>."
    }

    if (-not $clientInstall.Probe.HldsExe) {
        throw "Same-root live mod mode requires hlds.exe in the same Half-Life root as hl.exe. Resolved client root: $($clientInstall.Root)"
    }

    $gameDirName = Get-TestbedLiveModName
    $testbedRoot = Get-TestbedRoot
    $clientRoot = $clientInstall.Root
    $legacyStageRoot = Get-TestbedLiveModStageRoot -GameDirName $gameDirName
    $modRoot = Get-TestbedLiveModRoot -ClientRoot $clientRoot -GameDirName $gameDirName
    $liveProcesses = @(Get-TestbedLiveModProcesses -ClientRoot $clientRoot)

    if ($liveProcesses.Count -gt 0) {
        $details = $liveProcesses | ForEach-Object { '{0} (PID {1})' -f $_.Path, $_.Id }
        throw "Cannot refresh the same-root live mod while Half-Life is still running from ${clientRoot}: $($details -join '; '). Stop the existing HL/HLDS processes and retry."
    }

    Write-Step "Preparing same-root live mod under $clientRoot"
    Write-Host "Live game dir       : $gameDirName"
    Write-Host "Client root         : $clientRoot"
    Write-Host "Launch hlds         : $($clientInstall.Probe.HldsExe)"
    Write-Host "Launch hl           : $($clientInstall.HlExe)"
    Write-Host "Live mod root       : $modRoot"

    if (Test-Path -LiteralPath $legacyStageRoot) {
        Assert-PathWithinRoot -Path $legacyStageRoot -Root $testbedRoot -Description "Legacy live mod stage"
        Remove-Item -LiteralPath $legacyStageRoot -Recurse -Force
    }

    Reset-ManagedLiveModDirectory -Path $modRoot -ClientRoot $clientRoot -GameDirName $gameDirName | Out-Null
    Ensure-Directory -Path (Join-Path $modRoot "dlls")
    Ensure-Directory -Path (Join-Path $modRoot "logs")

    Copy-Item -LiteralPath $BuiltDllPath -Destination (Join-Path $modRoot "dlls\hl.dll") -Force
    if (-not [string]::IsNullOrWhiteSpace($BuiltPdbPath) -and (Test-LeafPath -Path $BuiltPdbPath)) {
        Copy-Item -LiteralPath $BuiltPdbPath -Destination (Join-Path $modRoot "dlls\hl.pdb") -Force
    }

    $libListPath = Write-TestbedLiveModLibList -ModRoot $modRoot -GameDirName $gameDirName

    $markerPath = Join-Path $modRoot ".hl-server-live-mod.txt"
    @"
Managed same-root live mod prepared by hl-server.
Client root: $clientRoot
Game dir: $gameDirName
Installed hl.dll: $BuiltDllPath
Content strategy: fallback_dir valve
Timestamp: $(Get-Date -Format o)
"@ | Set-Content -LiteralPath $markerPath -Encoding ASCII

    $manifestPath = Write-TestbedLiveModManifest -ModRoot $modRoot -Configuration $configuration -ClientRoot $clientRoot -GameDirName $gameDirName -ClientHlExe $clientInstall.HlExe -ClientHldsExe $clientInstall.Probe.HldsExe -InstalledDllPath (Join-Path $modRoot "dlls\hl.dll") -InstalledPdbPath $(if (Test-LeafPath -Path (Join-Path $modRoot "dlls\hl.pdb")) { Join-Path $modRoot "dlls\hl.pdb" } else { $null })
    $liveModState = Get-TestbedLiveModState -ClientRoot $clientRoot -GameDirName $gameDirName

    if (-not $liveModState.IsReady) {
        throw "Same-root live mod is incomplete after refresh: $($liveModState.MissingRequired -join ', ')"
    }

    return [PSCustomObject]@{
        ClientInstall = $clientInstall
        GameDirName = $gameDirName
        StageRoot = $null
        LinkPath = $modRoot
        ModRoot = $modRoot
        ManifestPath = $manifestPath
        MarkerPath = $markerPath
        LibListPath = $libListPath
        InstalledDllPath = (Join-Path $modRoot "dlls\hl.dll")
        InstalledPdbPath = if (Test-LeafPath -Path (Join-Path $modRoot "dlls\hl.pdb")) { Join-Path $modRoot "dlls\hl.pdb" } else { $null }
        LiveModState = $liveModState
    }
}

function Install-TestbedRuntime {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Configuration,
        [Parameter(Mandatory = $true)]
        [string]$BuiltDllPath,
        [string]$BuiltPdbPath,
        [string]$ExplicitTemplateRoot,
        [string]$ExplicitHldsExe,
        [string]$ExplicitHlExe,
        [string]$ExplicitSteamCmdExe,
        [switch]$AllowSteamCmdDownload,
        [switch]$PreferClientMatchedRuntime,
        [switch]$ForceTemplateRefresh
    )

    $configuration = Get-ValidatedConfiguration -Configuration $Configuration
    $selection = Get-HldsTemplateSelection -ExplicitTemplateRoot $ExplicitTemplateRoot -ExplicitHldsExe $ExplicitHldsExe -ExplicitHlExe $ExplicitHlExe -ExplicitSteamCmdExe $ExplicitSteamCmdExe -AllowSteamCmdDownload:$AllowSteamCmdDownload -ForceRefreshSteamCmdTemplate:$ForceTemplateRefresh
    $selectedCandidate = $selection.SelectedCandidate
    $clientInstall = $null
    $mirrorRoot = $selectedCandidate.Root

    if ($PreferClientMatchedRuntime) {
        $clientInstall = Resolve-TestbedClientInstall -ExplicitHlExe $ExplicitHlExe
        if (-not $clientInstall) {
            throw "Same-root live mod mode was requested, but no stock hl.exe could be resolved. Set HL_EXE in .env or pass -HlExe <path>."
        }

        $mirrorRoot = $clientInstall.Root
    }

    $runtimeRoot = Get-TestbedRuntimeRoot
    $testbedRoot = Get-TestbedRoot

    if (Test-PathsEqual -Left $selectedCandidate.Root -Right $runtimeRoot) {
        throw "The selected runtime template resolves to the disposable runtime itself: $runtimeRoot"
    }

    Write-Step "Preparing disposable runtime from $mirrorRoot"
    Write-Host "Template source kind: $($selectedCandidate.Probe.KindLabel)"
    Write-Host "Template selection  : $($selectedCandidate.Reason)"
    Write-Host "Selected hlds.exe   : $($selectedCandidate.Probe.HldsExe)"
    Write-Host "Selected hl.exe     : $(if ($selectedCandidate.Probe.HlExe) { $selectedCandidate.Probe.HlExe } else { 'not found' })"
    Write-Host "Mirror base root    : $mirrorRoot"
    if ($clientInstall) {
        Write-Host "Client root         : $($clientInstall.Root)"
        Write-Host "Client hl.exe       : $($clientInstall.HlExe)"
    }

    foreach ($warning in @($selection.Warnings)) {
        Write-Host "Template warning    : $warning"
    }

    $runtimeProcesses = @(Get-TestbedRuntimeProcesses -RuntimeRoot $runtimeRoot)
    if ($runtimeProcesses.Count -gt 0) {
        $details = $runtimeProcesses | ForEach-Object { '{0} (PID {1})' -f $_.Path, $_.Id }
        throw "Cannot refresh the disposable runtime while it is still in use: $($details -join '; '). Stop the existing testbed HLDS/HL processes and retry."
    }

    Reset-DisposableDirectory -Path $runtimeRoot -AllowedRoot $testbedRoot
    $mirrorResult = Copy-RuntimeTemplateToDisposable -SourceRoot $mirrorRoot -RuntimeRoot $runtimeRoot
    $runtimeFixups = Ensure-DisposableRuntimeKeyEntries -SourceProbe $selectedCandidate.Probe -RuntimeRoot $runtimeRoot

    $runtimeValveDlls = Join-Path $runtimeRoot "valve\dlls"
    Ensure-Directory -Path $runtimeValveDlls
    Copy-Item -LiteralPath $BuiltDllPath -Destination (Join-Path $runtimeValveDlls "hl.dll") -Force

    if (-not [string]::IsNullOrWhiteSpace($BuiltPdbPath) -and (Test-LeafPath -Path $BuiltPdbPath)) {
        Copy-Item -LiteralPath $BuiltPdbPath -Destination (Join-Path $runtimeValveDlls "hl.pdb") -Force
    }

    $markerPath = Join-Path $runtimeRoot ".hl-server-testbed.txt"
    @"
Disposable runtime prepared by hl-server.
Template root: $($selectedCandidate.Root)
Mirror root: $mirrorRoot
Template reason: $($selectedCandidate.Reason)
Installed hl.dll: $BuiltDllPath
Timestamp: $(Get-Date -Format o)
"@ | Set-Content -LiteralPath $markerPath -Encoding ASCII

    $manifestPath = Write-TestbedRuntimeManifest -RuntimeRoot $runtimeRoot -Configuration $configuration -TemplateSelection $selection -MirrorRoot $mirrorRoot -MirrorResult $mirrorResult -InstalledDllPath $BuiltDllPath -InstalledPdbPath $BuiltPdbPath -CreatedSteamAppIdFile:$runtimeFixups.CreatedSteamAppIdFile -ClientInstall $clientInstall -PreferClientMatchedRuntime:$PreferClientMatchedRuntime
    $runtimeProbe = Get-HldsRuntimeProbe -Root $runtimeRoot
    $entryComparison = Get-TestbedRuntimeEntryComparison -SourceProbe $selectedCandidate.Probe -RuntimeProbe $runtimeProbe
    $missingExpectedEntries = @($entryComparison | Where-Object { $_.MissingExpected } | Select-Object -ExpandProperty RelativePath)
    $liveContentStatus = Get-TestbedLiveContentStatus -RuntimeRoot $runtimeRoot -SelectedCandidate $selectedCandidate -ClientInstall $clientInstall -RuntimeManifest (Read-TestbedRuntimeManifest -RuntimeRoot $runtimeRoot) -PreferClientMatchedRuntime:$PreferClientMatchedRuntime

    if (-not $runtimeProbe.IsRunnable) {
        throw "Disposable runtime is missing required entries after mirroring: $($runtimeProbe.MissingBaseRequired -join ', ')"
    }

    if ($missingExpectedEntries.Count -gt 0) {
        throw "Disposable runtime is missing entries that exist in the selected source after mirroring: $($missingExpectedEntries -join ', ')"
    }

    if ($PreferClientMatchedRuntime -and (-not $liveContentStatus.RuntimeMatchesClientHash)) {
        throw "Same-root live mod mode expected $($liveContentStatus.RuntimeMap.Path) to match $($liveContentStatus.ClientMap.Path), but the hashes still differ."
    }

    return [PSCustomObject]@{
        RuntimeRoot = $runtimeRoot
        TemplateSelection = $selection
        ClientInstall = $clientInstall
        LiveContentStatus = $liveContentStatus
        RuntimeProbe = $runtimeProbe
        ManifestPath = $manifestPath
        MarkerPath = $markerPath
        InstalledDllPath = (Join-Path $runtimeValveDlls "hl.dll")
        InstalledPdbPath = if (Test-LeafPath -Path (Join-Path $runtimeValveDlls "hl.pdb")) { Join-Path $runtimeValveDlls "hl.pdb" } else { $null }
        CreatedSteamAppIdFile = $runtimeFixups.CreatedSteamAppIdFile
    }
}

function Get-HldsArgumentList {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Map,

        [Parameter(Mandatory = $true)]
        [int]$Port,

        [Parameter(Mandatory = $true)]
        [int]$MaxPlayers,

        [string]$ServerCfgName,
        [string]$GameDirName = "valve"
    )

    $arguments = @(
        "-console",
        "-game", $GameDirName,
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

function Get-WeaponComparisonMatricesRoot {
    return (Join-RepoPath "configs\weapon-comparison-matrices")
}

function Get-WeaponComparisonMatrixProfileName {
    param(
        [Parameter(Mandatory = $true)]
        [string]$WeaponUnderTest,
        $InputObject
    )

    $names = if ($WeaponUnderTest -eq "mp5") {
        @("weaponProfile", "weaponProfileName", "mp5Profile", "mp5ProfileName", "profile")
    }
    else {
        @("weaponProfile", "weaponProfileName", "glockProfile", "glockProfileName", "profile")
    }

    return (Get-OptionalObjectString -InputObject $InputObject -Names $names)
}

function ConvertTo-WeaponComparisonMatrixStep {
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
        throw "Weapon comparison matrix '$MatrixPath' step #$Index must define a non-empty 'name'."
    }

    Assert-FilesystemFriendlyName -Name $stepName -Label ("Weapon comparison matrix step name in '{0}'" -f $MatrixName)

    $weaponUnderTest = Get-OptionalObjectString -InputObject $StepData -Names @("weapon", "weaponUnderTest")
    if ([string]::IsNullOrWhiteSpace($weaponUnderTest)) {
        $weaponUnderTest = Get-OptionalObjectString -InputObject $Defaults -Names @("weapon", "weaponUnderTest")
    }

    if ([string]::IsNullOrWhiteSpace($weaponUnderTest)) {
        throw "Weapon comparison matrix '$MatrixPath' step '$stepName' must define 'weapon' as 'glock' or 'mp5' directly or via defaults."
    }

    $weaponUnderTest = $weaponUnderTest.Trim().ToLowerInvariant()
    if (@("glock", "mp5") -notcontains $weaponUnderTest) {
        throw "Weapon comparison matrix '$MatrixPath' step '$stepName' must use weapon 'glock' or 'mp5', not '$weaponUnderTest'."
    }

    $profileName = Get-WeaponComparisonMatrixProfileName -WeaponUnderTest $weaponUnderTest -InputObject $StepData
    if ([string]::IsNullOrWhiteSpace($profileName)) {
        $profileName = Get-WeaponComparisonMatrixProfileName -WeaponUnderTest $weaponUnderTest -InputObject $Defaults
    }

    if ([string]::IsNullOrWhiteSpace($profileName)) {
        throw "Weapon comparison matrix '$MatrixPath' step '$stepName' must define a profile for '$weaponUnderTest' directly or via defaults."
    }

    $labTargetProfileName = Get-OptionalObjectString -InputObject $StepData -Names @("labTargetProfile", "labTargetProfileName", "targetProfile")
    if ([string]::IsNullOrWhiteSpace($labTargetProfileName)) {
        $labTargetProfileName = Get-OptionalObjectString -InputObject $Defaults -Names @("labTargetProfile", "labTargetProfileName", "targetProfile")
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
        foreach ($assignment in (ConvertTo-LaunchAssignments -Cvars $defaultExtraCvars -SourceLabel "Weapon comparison matrix defaults")) {
            $assignments.Add($assignment)
        }
    }

    $stepExtraCvars = Get-ObjectPropertyValue -InputObject $StepData -Names @("extraCvars", "cvars")
    if ($null -ne $stepExtraCvars) {
        foreach ($assignment in (ConvertTo-LaunchAssignments -Cvars $stepExtraCvars -SourceLabel ("Weapon comparison matrix step '{0}'" -f $stepName))) {
            $assignments.Add($assignment)
        }
    }

    $profile = if ($weaponUnderTest -eq "mp5") {
        Get-Mp5Profile -Name $profileName
    }
    else {
        Get-GlockProfile -Name $profileName
    }

    $target = if ([string]::IsNullOrWhiteSpace($labTargetProfileName)) {
        $null
    }
    else {
        Get-GlockLabTarget -Name $labTargetProfileName
    }

    return [PSCustomObject]@{
        Name = $stepName
        WeaponUnderTest = $weaponUnderTest
        Map = $map
        OperatorNote = $operatorNote
        WeaponProfileName = $profile.Name
        WeaponProfile = $profile
        GlockProfileName = if ($weaponUnderTest -eq "glock") { $profile.Name } else { $null }
        GlockProfile = if ($weaponUnderTest -eq "glock") { $profile } else { $null }
        Mp5ProfileName = if ($weaponUnderTest -eq "mp5") { $profile.Name } else { $null }
        Mp5Profile = if ($weaponUnderTest -eq "mp5") { $profile } else { $null }
        LabTargetProfileName = if ($target) { $target.Name } else { $null }
        LabTargetProfile = $target
        Assignments = $assignments.ToArray()
    }
}

function Get-WeaponComparisonMatrices {
    $matricesRoot = Get-WeaponComparisonMatricesRoot
    if (-not (Test-Path -LiteralPath $matricesRoot -PathType Container)) {
        throw "Weapon comparison matrices directory was not found: $matricesRoot"
    }

    $matrices = New-Object System.Collections.Generic.List[object]

    foreach ($matrixFile in (Get-ChildItem -LiteralPath $matricesRoot -File -Filter "*.json" | Sort-Object BaseName, Name)) {
        try {
            $rawJson = Get-Content -LiteralPath $matrixFile.FullName -Raw
            $matrixData = $rawJson | ConvertFrom-Json
        }
        catch {
            throw "Failed to parse weapon comparison matrix '$($matrixFile.FullName)': $($_.Exception.Message)"
        }

        $matrixName = Get-OptionalObjectString -InputObject $matrixData -Names @("name")
        $description = Get-OptionalObjectString -InputObject $matrixData -Names @("description")

        if ([string]::IsNullOrWhiteSpace($matrixName)) {
            throw "Weapon comparison matrix '$($matrixFile.FullName)' must define a non-empty 'name'."
        }

        Assert-FilesystemFriendlyName -Name $matrixName -Label "Weapon comparison matrix name"

        if ([string]::IsNullOrWhiteSpace($description)) {
            throw "Weapon comparison matrix '$($matrixFile.FullName)' must define a non-empty 'description'."
        }

        if ($matrixFile.BaseName -ne $matrixName) {
            throw "Weapon comparison matrix filename '$($matrixFile.Name)' must match matrix name '$matrixName'."
        }

        $defaults = Get-ObjectPropertyValue -InputObject $matrixData -Names @("defaults")
        $stepData = @(Get-ObjectPropertyValue -InputObject $matrixData -Names @("steps"))
        if ($stepData.Count -eq 0) {
            throw "Weapon comparison matrix '$($matrixFile.FullName)' must define a non-empty 'steps' array."
        }

        $stepNames = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::OrdinalIgnoreCase)
        $steps = New-Object System.Collections.Generic.List[object]
        $stepIndex = 0

        foreach ($entry in $stepData) {
            $stepIndex += 1
            $step = ConvertTo-WeaponComparisonMatrixStep -MatrixName $matrixName -MatrixPath $matrixFile.FullName -StepData $entry -Defaults $defaults -Index $stepIndex
            if (-not $stepNames.Add($step.Name)) {
                throw "Weapon comparison matrix '$($matrixFile.FullName)' contains a duplicate step name '$($step.Name)'."
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

function Get-WeaponComparisonMatrix {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $matrices = @(Get-WeaponComparisonMatrices)
    $match = $matrices | Where-Object { $_.Name.Equals($Name, [System.StringComparison]::OrdinalIgnoreCase) } | Select-Object -First 1
    if ($match) {
        return $match
    }

    $available = $matrices | Select-Object -ExpandProperty Name
    $availableText = if ($available.Count -gt 0) { ($available -join ", ") } else { "none" }
    throw "Unknown weapon comparison matrix '$Name'. Available matrices: $availableText"
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

function Get-WeaponComparisonReportsRoot {
    return (Join-Path (Get-WeaponDebugReportsRoot) "weapon-comparison-matrices")
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

function Get-LatestWeaponComparisonReportDirectory {
    $reportsRoot = Get-WeaponComparisonReportsRoot
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

        [System.Collections.IDictionary]$Cvars,
        [string]$GameDirName = "valve"
    )

    if (-not $Cvars -or $Cvars.Count -eq 0) {
        return $null
    }

    $modRoot = Join-Path (Get-FullPath -Path $RuntimeRoot) $GameDirName
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

function Get-HldsRuntimeSteamAppId {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RuntimeRoot
    )

    $steamAppIdPath = Join-Path (Get-FullPath -Path $RuntimeRoot) "steam_appid.txt"
    if (-not (Test-LeafPath -Path $steamAppIdPath)) {
        return $null
    }

    $value = ((Get-Content -LiteralPath $steamAppIdPath | Select-Object -First 1) -as [string])
    if ([string]::IsNullOrWhiteSpace($value)) {
        return $null
    }

    return $value.Trim()
}

function Push-HldsRuntimeEnvironment {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RuntimeRoot
    )

    $steamAppId = Get-HldsRuntimeSteamAppId -RuntimeRoot $RuntimeRoot
    $state = [PSCustomObject]@{
        SteamAppId = [System.Environment]::GetEnvironmentVariable("SteamAppId", "Process")
        SteamGameId = [System.Environment]::GetEnvironmentVariable("SteamGameId", "Process")
        EffectiveSteamAppId = $steamAppId
    }

    if (-not [string]::IsNullOrWhiteSpace($steamAppId)) {
        [System.Environment]::SetEnvironmentVariable("SteamAppId", $steamAppId, "Process")
        [System.Environment]::SetEnvironmentVariable("SteamGameId", $steamAppId, "Process")
    }

    return $state
}

function Pop-HldsRuntimeEnvironment {
    param(
        [Parameter(Mandatory = $true)]
        $State
    )

    [System.Environment]::SetEnvironmentVariable("SteamAppId", $State.SteamAppId, "Process")
    [System.Environment]::SetEnvironmentVariable("SteamGameId", $State.SteamGameId, "Process")
}

function Write-HldsLaunchMetadata {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RuntimeRoot,
        [Parameter(Mandatory = $true)]
        [string]$HldsExe,
        [Parameter(Mandatory = $true)]
        [string[]]$ArgumentList,
        [Parameter(Mandatory = $true)]
        [string]$Timestamp,
        [string]$ServerCfgName,
        [string]$SteamAppId
    )

    $logsRoot = Get-TestbedLogsRoot
    Ensure-Directory -Path $logsRoot

    $metadataPath = Join-Path $logsRoot ("hlds-" + $Timestamp + "-launch.txt")
    $lines = New-Object System.Collections.Generic.List[string]

    $lines.Add(("Timestamp: {0}" -f (Get-Date -Format o)))
    $lines.Add(("Executable: {0}" -f $HldsExe))
    $lines.Add(("Working directory: {0}" -f $RuntimeRoot))
    $lines.Add(("Runtime root: {0}" -f $RuntimeRoot))
    $lines.Add(("SteamAppId: {0}" -f $(if ([string]::IsNullOrWhiteSpace($SteamAppId)) { "<unset>" } else { $SteamAppId })))
    if (-not [string]::IsNullOrWhiteSpace($ServerCfgName)) {
        $lines.Add(("Server cfg: {0}" -f $ServerCfgName))
    }

    $lines.Add(("Arguments: {0}" -f ($ArgumentList -join " ")))
    Set-Content -LiteralPath $metadataPath -Value $lines -Encoding ASCII
    return $metadataPath
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
        [hashtable]$Cvars,
        [string]$GameDirName = "valve",
        [string]$HldsExePath,
        [string]$WorkingDirectory,
        [string]$SteamAppIdRoot
    )

    $runtimeRoot = Get-FullPath -Path $RuntimeRoot
    $hldsExe = if ([string]::IsNullOrWhiteSpace($HldsExePath)) { Join-Path $runtimeRoot "hlds.exe" } else { Get-FullPath -Path $HldsExePath }
    $workingDirectory = if ([string]::IsNullOrWhiteSpace($WorkingDirectory)) { $runtimeRoot } else { Get-FullPath -Path $WorkingDirectory }
    $steamAppIdRoot = if ([string]::IsNullOrWhiteSpace($SteamAppIdRoot)) { $runtimeRoot } else { Get-FullPath -Path $SteamAppIdRoot }

    if (-not (Test-LeafPath -Path $hldsExe)) {
        throw "No hlds.exe was found in the disposable runtime: $hldsExe"
    }

    $logsRoot = Get-TestbedLogsRoot
    Ensure-Directory -Path $logsRoot

    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $stdoutLog = Join-Path $logsRoot ("hlds-" + $timestamp + "-stdout.log")
    $stderrLog = Join-Path $logsRoot ("hlds-" + $timestamp + "-stderr.log")
    $serverCfgName = Write-HldsLaunchCvarConfig -RuntimeRoot $runtimeRoot -Cvars (Get-HldsLaunchCvars -SetCvar $SetCvar -Cvars $Cvars) -GameDirName $GameDirName
    $argumentList = Get-HldsArgumentList -Map $Map -Port $Port -MaxPlayers $MaxPlayers -ServerCfgName $serverCfgName -GameDirName $GameDirName
    $environmentState = Push-HldsRuntimeEnvironment -RuntimeRoot $steamAppIdRoot

    try {
        $launchMetadataPath = Write-HldsLaunchMetadata -RuntimeRoot $workingDirectory -HldsExe $hldsExe -ArgumentList $argumentList -Timestamp $timestamp -ServerCfgName $serverCfgName -SteamAppId $environmentState.EffectiveSteamAppId
        $process = Start-Process -FilePath $hldsExe -WorkingDirectory $workingDirectory -WindowStyle Hidden -ArgumentList $argumentList -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog -PassThru
    }
    finally {
        Pop-HldsRuntimeEnvironment -State $environmentState
    }

    return [PSCustomObject]@{
        Process = $process
        RuntimeRoot = $workingDirectory
        GameDirName = $GameDirName
        StdOutLog = $stdoutLog
        StdErrLog = $stderrLog
        RuntimeLogsRoot = Join-Path $workingDirectory "logs"
        QConsoleLog = Join-Path $workingDirectory "qconsole.log"
        ValveQConsoleLog = Join-Path $workingDirectory ($GameDirName + "\qconsole.log")
        LaunchMetadataPath = $launchMetadataPath
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
        [hashtable]$Cvars,
        [string]$GameDirName = "valve",
        [string]$HldsExePath,
        [string]$WorkingDirectory,
        [string]$SteamAppIdRoot
    )

    $runtimeRoot = Get-FullPath -Path $RuntimeRoot
    $hldsExe = if ([string]::IsNullOrWhiteSpace($HldsExePath)) { Join-Path $runtimeRoot "hlds.exe" } else { Get-FullPath -Path $HldsExePath }
    $workingDirectory = if ([string]::IsNullOrWhiteSpace($WorkingDirectory)) { $runtimeRoot } else { Get-FullPath -Path $WorkingDirectory }
    $steamAppIdRoot = if ([string]::IsNullOrWhiteSpace($SteamAppIdRoot)) { $runtimeRoot } else { Get-FullPath -Path $SteamAppIdRoot }
    if (-not (Test-LeafPath -Path $hldsExe)) {
        throw "No hlds.exe was found in the disposable runtime: $hldsExe"
    }

    $logsRoot = Get-TestbedLogsRoot
    Ensure-Directory -Path $logsRoot

    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $stdoutLog = Join-Path $logsRoot ("hlds-" + $timestamp + "-stdout.log")
    $serverCfgName = Write-HldsLaunchCvarConfig -RuntimeRoot $runtimeRoot -Cvars (Get-HldsLaunchCvars -SetCvar $SetCvar -Cvars $Cvars) -GameDirName $GameDirName
    $argumentList = Get-HldsArgumentList -Map $Map -Port $Port -MaxPlayers $MaxPlayers -ServerCfgName $serverCfgName -GameDirName $GameDirName
    $environmentState = Push-HldsRuntimeEnvironment -RuntimeRoot $steamAppIdRoot

    Push-Location $workingDirectory
    try {
        Write-HldsLaunchMetadata -RuntimeRoot $workingDirectory -HldsExe $hldsExe -ArgumentList $argumentList -Timestamp $timestamp -ServerCfgName $serverCfgName -SteamAppId $environmentState.EffectiveSteamAppId | Out-Null
        & $hldsExe @argumentList 2>&1 | Tee-Object -FilePath $stdoutLog
        return $stdoutLog
    }
    finally {
        Pop-HldsRuntimeEnvironment -State $environmentState
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
        $LaunchInfo.RuntimeLogsRoot,
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

    return (Wait-ForAnyLogPattern -Paths (Get-HldsLogCandidates -LaunchInfo $LaunchInfo) -Patterns @(
            ('Started map "' + $Map + '"'),
            "Server IP address",
            "Server logging data to file"
        ) -TimeoutSeconds $TimeoutSeconds -Process $LaunchInfo.Process)
}

function Wait-ForAnyLogPattern {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Paths,

        [Parameter(Mandatory = $true)]
        [string[]]$Patterns,

        [Parameter(Mandatory = $true)]
        [int]$TimeoutSeconds,

        [System.Diagnostics.Process]$Process
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)

    while ((Get-Date) -lt $deadline) {
        foreach ($pattern in $Patterns) {
            if (Wait-ForLogPattern -Paths $Paths -Pattern $pattern -TimeoutSeconds 1 -Process $Process) {
                return $true
            }
        }

        if ($Process) {
            $Process.Refresh()
            if ($Process.HasExited) {
                return $false
            }
        }
    }

    return $false
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
            if ([string]::IsNullOrWhiteSpace($path)) {
                continue
            }

            if (Test-Path -LiteralPath $path -PathType Container) {
                $candidatePaths = @(Get-ChildItem -LiteralPath $path -File -Filter "L*.log" | Select-Object -ExpandProperty FullName)
            }
            elseif (Test-LeafPath -Path $path) {
                $candidatePaths = @($path)
            }
            else {
                $candidatePaths = @()
            }

            foreach ($candidatePath in $candidatePaths) {
                if (Select-String -Path $candidatePath -Pattern $Pattern -SimpleMatch -Quiet -ErrorAction SilentlyContinue) {
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
