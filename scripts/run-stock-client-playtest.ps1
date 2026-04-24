param(
    [ValidateSet("all", "glock", "mp5", "357", "shotgun")]
    [string]$Weapon = "all",

    # Legacy compatibility with the previous script shape.
    [ValidateSet("glock", "mp5", "357", "shotgun")]
    [string[]]$Weapons,

    [string]$MatchPack = "hldm_skill_default",
    [string]$TargetProfile = "vest_headprotected",
    [string]$TargetSpot = "default",
    [string]$HostName = "127.0.0.1",
    [int]$Port = 27015,
    [string]$RconPassword = "",
    [string]$HlExe = "",
    [string]$OutputRoot = "",
    [switch]$StartServer,
    [switch]$NoClient,
    [switch]$OpenEditor,
    [switch]$DryRun,
    [switch]$NoPause,
    [switch]$RequireRcon,
    [switch]$Strict
)

$ErrorActionPreference = "Stop"

$script:StartedAt = Get-Date
$script:RepoRoot = Split-Path -Parent $PSScriptRoot
$script:AllWeapons = @("glock", "mp5", "357", "shotgun")
$commonScript = Join-Path $PSScriptRoot "common.ps1"
if (Test-Path $commonScript) {
    . $commonScript
    if (Get-Command Import-HLServerEnv -ErrorAction SilentlyContinue) {
        Import-HLServerEnv
    }
}

function Write-Step {
    param([string]$Message)
    Write-Host "[stock-client-playtest] $Message"
}

function Write-Section {
    param([string]$Message)
    Write-Host ""
    Write-Host "=== $Message ==="
}

function Add-ContentLine {
    param(
        [string]$Path,
        [string]$Text
    )
    Add-Content -Path $Path -Value $Text
}

function Wait-ManualStep {
    param([string]$Message)

    Write-Host ""
    Write-Host $Message
    if (-not $NoPause -and -not $DryRun) {
        Read-Host "Press Enter when done" | Out-Null
    } else {
        Write-Step "Skipping pause because -NoPause or -DryRun is set."
    }
}

function Resolve-SelectedWeapons {
    if ($PSBoundParameters.ContainsKey("Weapons") -and $Weapons -and $Weapons.Count -gt 0) {
        return @($Weapons | Select-Object -Unique)
    }

    if ($Weapon -eq "all") {
        return @($script:AllWeapons)
    }

    return @($Weapon)
}

function Find-HalfLifeRoot {
    if (Get-Command Resolve-TestbedClientInstall -ErrorAction SilentlyContinue) {
        try {
            $clientInstall = Resolve-TestbedClientInstall -ExplicitHlExe $HlExe
            if ($clientInstall -and $clientInstall.Root) {
                return $clientInstall.Root
            }
        } catch {
            Write-Step "Shared Half-Life resolver failed; falling back to local path probes. $($_.Exception.Message)"
        }
    }

    $candidates = @()

    if ($env:HALFLIFE_ROOT) {
        $candidates += $env:HALFLIFE_ROOT
    }

    $candidates += @(
        (Join-Path $script:RepoRoot "Half-Life"),
        "C:\Program Files (x86)\Steam\steamapps\common\Half-Life",
        "C:\Program Files\Steam\steamapps\common\Half-Life",
        "D:\Steam\steamapps\common\Half-Life",
        "D:\SteamLibrary\steamapps\common\Half-Life",
        "D:\Games\Steam\steamapps\common\Half-Life"
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    return $null
}

function Get-LivePaths {
    $halfLifeRoot = Find-HalfLifeRoot

    if (-not $halfLifeRoot) {
        $fallback = Join-Path $script:RepoRoot "testbed"
        $halfLifeRoot = $fallback
    }

    $liveMod = Join-Path $halfLifeRoot "hlserver_testbed"
    $editor = Join-Path $liveMod "HlConfigEditorCpp.exe"

    if (-not $HlExe) {
        $script:ResolvedHlExe = Join-Path $halfLifeRoot "hl.exe"
    } else {
        $script:ResolvedHlExe = $HlExe
    }

    [pscustomobject]@{
        HalfLifeRoot = $halfLifeRoot
        LiveModRoot = $liveMod
        EditorExe = $editor
        HlExe = $script:ResolvedHlExe
    }
}

function Get-ReportRoot {
    param([object]$LivePaths)

    if ($OutputRoot) {
        return $OutputRoot
    }

    return Join-Path $script:RepoRoot "testbed\logs\reports\stock-client-playtests"
}

function ConvertTo-GoldSrcPacket {
    param([string]$Command)
    $payload = [System.Text.Encoding]::ASCII.GetBytes("rcon $Command")
    $bytes = New-Object byte[] ($payload.Length + 5)
    for ($i = 0; $i -lt 4; $i++) {
        $bytes[$i] = 255
    }
    [Array]::Copy($payload, 0, $bytes, 4, $payload.Length)
    $bytes[$bytes.Length - 1] = 0
    return $bytes
}

function Read-UdpResponse {
    param(
        [System.Net.Sockets.UdpClient]$Udp,
        [int]$TimeoutMs = 2500
    )

    $Udp.Client.ReceiveTimeout = $TimeoutMs
    $endpoint = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
    $bytes = $Udp.Receive([ref]$endpoint)
    if ($bytes.Length -le 4) {
        return ""
    }

    return [System.Text.Encoding]::ASCII.GetString($bytes, 4, $bytes.Length - 4).Trim([char]0)
}

function Invoke-GoldSrcRcon {
    param(
        [string]$ServerHost,
        [int]$ServerPort,
        [string]$Password,
        [string]$Command
    )

    if (-not $Password) {
        throw "RCON password is empty."
    }

    $udp = New-Object System.Net.Sockets.UdpClient
    try {
        $udp.Connect($ServerHost, $ServerPort)

        $challengePacket = ConvertTo-GoldSrcPacket "challenge rcon"
        [void]$udp.Send($challengePacket, $challengePacket.Length)
        $challengeResponse = Read-UdpResponse -Udp $udp

        if ($challengeResponse -notmatch "challenge rcon\s+([\-0-9]+)") {
            throw "Could not parse RCON challenge response: $challengeResponse"
        }

        $challenge = $Matches[1]
        $commandPacket = ConvertTo-GoldSrcPacket "$challenge `"$Password`" $Command"
        [void]$udp.Send($commandPacket, $commandPacket.Length)
        return Read-UdpResponse -Udp $udp
    } finally {
        $udp.Close()
    }
}

function Invoke-RconCommandChecked {
    param(
        [string]$Command,
        [string]$OutputPath = ""
    )

    if ($DryRun) {
        $message = "[dry-run] would send RCON: $Command"
        if ($OutputPath) {
            Add-ContentLine -Path $OutputPath -Text $message
        }
        return [pscustomobject]@{
            Command = $Command
            Success = $false
            Status = "unknown"
            Output = $message
            Error = "dry-run"
        }
    }

    try {
        $output = Invoke-GoldSrcRcon -ServerHost $HostName -ServerPort $Port -Password $RconPassword -Command $Command
        if ($OutputPath) {
            Add-ContentLine -Path $OutputPath -Text ">>> $Command"
            Add-ContentLine -Path $OutputPath -Text $output
            Add-ContentLine -Path $OutputPath -Text ""
        }

        return [pscustomobject]@{
            Command = $Command
            Success = $true
            Status = "yes"
            Output = $output
            Error = ""
        }
    } catch {
        $message = $_.Exception.Message
        if ($OutputPath) {
            Add-ContentLine -Path $OutputPath -Text ">>> $Command"
            Add-ContentLine -Path $OutputPath -Text "ERROR: $message"
            Add-ContentLine -Path $OutputPath -Text ""
        }

        return [pscustomobject]@{
            Command = $Command
            Success = $false
            Status = "no"
            Output = ""
            Error = $message
        }
    }
}

function Test-ClientConnectionFromStatus {
    param([string]$StatusText)

    if (-not $StatusText) {
        return $null
    }

    $lines = $StatusText -split "`r?`n"
    foreach ($line in $lines) {
        if ($line -match "^\s*#\s+\d+\s+" -or $line -match "^\s*#\s+\d+\s+`"") {
            return $true
        }
    }

    if ($StatusText -match "players\s*:\s*0\s+active") {
        return $false
    }

    if ($StatusText -match "0\s+users") {
        return $false
    }

    return $null
}

function Get-QConsoleCandidates {
    param([object]$LivePaths)

    $candidates = @(
        (Join-Path $LivePaths.HalfLifeRoot "qconsole.log"),
        (Join-Path $LivePaths.HalfLifeRoot "valve\qconsole.log"),
        (Join-Path $LivePaths.LiveModRoot "qconsole.log"),
        (Join-Path $script:RepoRoot "testbed\qconsole.log")
    )

    return @($candidates | Where-Object { $_ } | Select-Object -Unique)
}

function Get-ClientConnectionStatus {
    param(
        [object]$LivePaths,
        [string]$StatusText,
        [datetime]$Since
    )

    $statusResult = Test-ClientConnectionFromStatus -StatusText $StatusText
    if ($statusResult -eq $true) {
        return [pscustomobject]@{ Status = "yes"; Source = "rcon status"; Details = "status output includes at least one player row" }
    }

    if ($statusResult -eq $false) {
        return [pscustomobject]@{ Status = "no"; Source = "rcon status"; Details = "status output reports no active players" }
    }

    foreach ($path in Get-QConsoleCandidates -LivePaths $LivePaths) {
        if (-not (Test-Path $path)) {
            continue
        }

        $item = Get-Item $path
        if ($item.LastWriteTime -lt $Since) {
            continue
        }

        $tail = Get-Content -Path $path -Tail 120 -ErrorAction SilentlyContinue
        $evidence = $tail | Where-Object {
            $_ -match "connected" -or
            $_ -match "entered the game" -or
            $_ -match "userid" -or
            $_ -match "STEAM_"
        } | Select-Object -Last 1

        if ($evidence) {
            return [pscustomobject]@{ Status = "yes"; Source = $path; Details = "recent qconsole evidence: $evidence" }
        }
    }

    return [pscustomobject]@{ Status = "unknown"; Source = "none"; Details = "RCON status was unavailable or inconclusive and no fresh qconsole evidence was found" }
}

function Get-WeaponLogSearchRoots {
    param([object]$LivePaths)

    $roots = @(
        (Join-Path $LivePaths.LiveModRoot "logs"),
        (Join-Path $LivePaths.HalfLifeRoot "logs"),
        (Join-Path $script:RepoRoot "testbed\logs")
    )

    return @($roots | Where-Object { $_ } | Select-Object -Unique)
}

function Get-LatestWeaponLog {
    param([object]$LivePaths)

    $logs = @()
    foreach ($root in Get-WeaponLogSearchRoots -LivePaths $LivePaths) {
        if (Test-Path $root) {
            $logs += Get-ChildItem -Path $root -Filter "weapon-debug-*.log" -File -ErrorAction SilentlyContinue
        }
    }

    if (-not $logs -or $logs.Count -eq 0) {
        return $null
    }

    return $logs | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
}

function Get-LogSnapshot {
    param([object]$LivePaths)

    $log = Get-LatestWeaponLog -LivePaths $LivePaths
    if (-not $log) {
        return [pscustomobject]@{ Path = ""; Length = 0; LastWriteTimeUtc = [datetime]::MinValue }
    }

    return [pscustomobject]@{
        Path = $log.FullName
        Length = $log.Length
        LastWriteTimeUtc = $log.LastWriteTimeUtc
    }
}

function Get-WeaponEventCountSince {
    param(
        [string]$LogPath,
        [string]$WeaponName,
        [datetime]$Since
    )

    if (-not $LogPath -or -not (Test-Path $LogPath)) {
        return 0
    }

    $count = 0
    $sinceUtc = $Since.ToUniversalTime()

    foreach ($line in Get-Content -Path $LogPath -ErrorAction SilentlyContinue) {
        if ($line -notmatch "weapon=$([regex]::Escape($WeaponName))") {
            continue
        }

        if ($line -notmatch "type=(accepted|hit|kill)") {
            continue
        }

        $lineTime = $null
        if ($line -match "ts=([0-9]{4}-[0-9]{2}-[0-9]{2}T[^\s]+)") {
            try {
                $lineTime = ([datetimeoffset]::Parse($Matches[1])).UtcDateTime
            } catch {
                $lineTime = $null
            }
        }

        if ($lineTime) {
            if ($lineTime -ge $sinceUtc) {
                $count++
            }
        }
    }

    return $count
}

function Test-FreshTelemetry {
    param(
        [object]$Before,
        [object]$After,
        [datetime]$StepStart,
        [string]$WeaponName
    )

    $eventCount = 0
    if ($After.Path) {
        $eventCount = Get-WeaponEventCountSince -LogPath $After.Path -WeaponName $WeaponName -Since $StepStart
    }

    $fresh = $false
    $reason = "no telemetry change detected"

    if ($eventCount -gt 0) {
        $fresh = $true
        $reason = "$eventCount accepted/hit/kill events after step start"
    } elseif ($After.Path -and -not $Before.Path) {
        if ($After.LastWriteTimeUtc -ge $StepStart.ToUniversalTime()) {
            $fresh = $true
            $reason = "new weapon log appeared after step start"
        } else {
            $reason = "weapon log exists, but it is older than this step"
        }
    } elseif ($After.Path -and $Before.Path -and ($After.Path -ne $Before.Path)) {
        $fresh = $true
        $reason = "latest weapon log path changed"
    } elseif ($After.Path -and $Before.Path) {
        if ($After.Length -gt $Before.Length -or $After.LastWriteTimeUtc -gt $Before.LastWriteTimeUtc) {
            $fresh = $true
            $reason = "latest weapon log changed during this step"
        }
    }

    [pscustomobject]@{
        Fresh = $fresh
        EventCount = $eventCount
        Reason = $reason
        LogPath = $After.Path
    }
}

function Invoke-Analyzer {
    param(
        [object]$LivePaths,
        [string]$WeaponName,
        [string]$Destination
    )

    $analyzer = Join-Path $script:RepoRoot "scripts\analyze-weapon-log.ps1"
    if (-not (Test-Path $analyzer)) {
        Set-Content -Path $Destination -Value "Analyzer script missing: $analyzer"
        return [pscustomobject]@{ Success = $false; Path = $Destination; Error = "Analyzer script missing." }
    }

    $latest = Get-LatestWeaponLog -LivePaths $LivePaths
    if (-not $latest) {
        Set-Content -Path $Destination -Value "No weapon-debug log found."
        return [pscustomobject]@{ Success = $false; Path = $Destination; Error = "No weapon-debug log found." }
    }

    try {
        $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $analyzer -Path $latest.FullName -Weapon $WeaponName 2>&1
        $output | Set-Content -Path $Destination
        return [pscustomobject]@{ Success = $true; Path = $Destination; Error = ""; LogPath = $latest.FullName }
    } catch {
        $message = $_.Exception.Message
        Set-Content -Path $Destination -Value "Analyzer failed: $message"
        return [pscustomobject]@{ Success = $false; Path = $Destination; Error = $message; LogPath = $latest.FullName }
    }
}

function Get-WeaponInstructions {
    param([string]$WeaponName)

    switch ($WeaponName) {
        "glock" {
            return @(
                "Glock:",
                "- stand still and fire careful single clicks",
                "- fire rapid clicks without relying on hard tap-fire",
                "- wait for cadence/pattern reset, then fire again",
                "- expected telemetry: cadence/pattern fields and reset evidence"
            )
        }
        "mp5" {
            return @(
                "MP5:",
                "- fire a short 3-5 shot burst",
                "- pause, then fire a longer held spray",
                "- try a movement spray if practical",
                "- expected telemetry: burst-growth, pattern index, and reset evidence"
            )
        }
        "357" {
            return @(
                "357:",
                "- fire one careful first shot",
                "- fire fast follow-up clicks",
                "- wait for cadence/pattern reset, then fire again",
                "- expected telemetry: cadence and deterministic pattern progression"
            )
        }
        "shotgun" {
            return @(
                "Shotgun:",
                "- stand close to the target and fire one shell",
                "- fire a second shell after a short delay",
                "- wait for pellet pattern reset and fire again",
                "- expected telemetry: pellet/pattern fields and consistent hit/kill damage"
            )
        }
    }
}

function Invoke-CommandSet {
    param(
        [string]$Name,
        [string[]]$Commands,
        [string]$OutputPath
    )

    $results = @()
    foreach ($command in $Commands) {
        Write-Step "$Name command: $command"
        if ($DryRun) {
            Add-ContentLine -Path $OutputPath -Text "[dry-run] $command"
            $results += [pscustomobject]@{ Command = $command; Success = $false; Status = "unknown"; Error = "dry-run" }
            continue
        }

        if (-not $script:RconAvailable) {
            Add-ContentLine -Path $OutputPath -Text "[manual] $command"
            $results += [pscustomobject]@{ Command = $command; Success = $false; Status = "unknown"; Error = "RCON unavailable" }
            continue
        }

        $result = Invoke-RconCommandChecked -Command $command -OutputPath $OutputPath
        $results += $result
    }

    $failed = @($results | Where-Object { $_.Status -eq "no" })
    $unknown = @($results | Where-Object { $_.Status -eq "unknown" })

    if ($failed.Count -gt 0) {
        return [pscustomobject]@{ Status = "no"; Results = $results; Details = ($failed | Select-Object -First 1).Error }
    }

    if ($unknown.Count -gt 0) {
        return [pscustomobject]@{ Status = "unknown"; Results = $results; Details = "manual or dry-run commands were not verified by RCON" }
    }

    return [pscustomobject]@{ Status = "yes"; Results = $results; Details = "all commands returned RCON responses" }
}

function Write-ManualFallback {
    param([string[]]$Commands)

    Write-Host ""
    Write-Host "Manual fallback commands for HLDS console:"
    foreach ($command in $Commands) {
        Write-Host "  $command"
    }
}

$selectedWeapons = Resolve-SelectedWeapons
$livePaths = Get-LivePaths
$reportRoot = Get-ReportRoot -LivePaths $livePaths
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$reportDir = Join-Path $reportRoot $timestamp
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null

$summaryPath = Join-Path $reportDir "summary.txt"
$perWeaponPath = Join-Path $reportDir "per-weapon-summary.txt"
$packageCheckPath = Join-Path $reportDir "package-check.txt"
$rconLogPath = Join-Path $reportDir "rcon-validation.txt"
$commandLogPath = Join-Path $reportDir "commands.txt"
$clientStatusPath = Join-Path $reportDir "client-status.txt"

$script:RconAvailable = $false
$failures = New-Object System.Collections.Generic.List[string]
$warnings = New-Object System.Collections.Generic.List[string]
$weaponSummaries = @()

Set-Content -Path $summaryPath -Value @(
    "Improved HLDM stock-client playtest summary",
    "timestamp=$timestamp",
    "branch=$(git -C $script:RepoRoot rev-parse --abbrev-ref HEAD 2>$null)",
    "commit=$(git -C $script:RepoRoot rev-parse HEAD 2>$null)",
    "match_pack=$MatchPack",
    "target_profile=$TargetProfile",
    "target_spot=$TargetSpot",
    "weapon_selection=$($selectedWeapons -join ',')",
    "dry_run=$DryRun",
    "strict=$Strict",
    "require_rcon=$RequireRcon",
    "host=$HostName",
    "port=$Port",
    ""
)

Set-Content -Path $commandLogPath -Value @(
    "Generated command sequence",
    "host=$HostName",
    "port=$Port",
    ""
)

Set-Content -Path $rconLogPath -Value @(
    "RCON validation",
    "host=$HostName",
    "port=$Port",
    "password_provided=$(if ($RconPassword) { 'yes' } else { 'no' })",
    ""
)

Write-Section "Stable package readiness"
Write-Step "Repo root: $script:RepoRoot"
Write-Step "Report directory: $reportDir"
Write-Step "Half-Life root: $($livePaths.HalfLifeRoot)"
Write-Step "Live mod root: $($livePaths.LiveModRoot)"

if (-not (Test-Path (Join-Path $script:RepoRoot "scripts\check-improved-hldm.ps1"))) {
    $failures.Add("Missing scripts\check-improved-hldm.ps1")
} else {
    Write-Step "Package check script is present."
}

if (-not (Test-Path (Join-Path $script:RepoRoot "scripts\analyze-weapon-log.ps1"))) {
    $failures.Add("Missing scripts\analyze-weapon-log.ps1")
} else {
    Write-Step "Analyzer script is present."
}

$checkScript = Join-Path $script:RepoRoot "scripts\check-improved-hldm.ps1"
if (Test-Path $checkScript) {
    $checkArguments = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $checkScript)
    if ($HlExe) {
        $checkArguments += @("-HlExe", $HlExe)
    }

    $checkOutput = & powershell.exe @checkArguments 2>&1
    $checkExit = $LASTEXITCODE
    $checkOutput | Set-Content -Path $packageCheckPath
    Add-ContentLine -Path $summaryPath -Text "package_check=$packageCheckPath exit=$checkExit"

    if ($checkExit -eq 0) {
        Write-Step "Package check: PASS"
    } else {
        Write-Step "Package check: FAIL (exit $checkExit)"
        $failures.Add("Package check failed with exit code $checkExit.")
    }
} else {
    "Missing package check script: $checkScript" | Set-Content -Path $packageCheckPath
    Add-ContentLine -Path $summaryPath -Text "package_check=$packageCheckPath exit=missing"
}

if ($StartServer) {
    $launcher = Join-Path $script:RepoRoot "scripts\play-improved-hldm.bat"
    if (Test-Path $launcher) {
        Write-Step "Starting improved HLDM launcher: $launcher"
        if (-not $DryRun) {
            Start-Process -FilePath $launcher -WorkingDirectory $script:RepoRoot | Out-Null
        }
    } else {
        $warnings.Add("StartServer requested, but launcher was not found: $launcher")
        Write-Step "StartServer requested, but launcher was not found: $launcher"
    }
}

if (-not $NoClient) {
    $clientCommand = "`"$($livePaths.HlExe)`" -game hlserver_testbed -console +connect $HostName`:$Port"
    Write-Step "Stock client launch command: $clientCommand"
    Add-ContentLine -Path $summaryPath -Text "client_launch_command=$clientCommand"
} else {
    Write-Step "Client launch skipped because -NoClient is set."
}

Write-Section "RCON verification"
$statusResponse = ""
$cfgStatusResponse = ""

if ($RconPassword) {
    if ($DryRun) {
        Write-Step "Dry run: RCON is not contacted."
        Add-ContentLine -Path $rconLogPath -Text "[dry-run] status"
        Add-ContentLine -Path $rconLogPath -Text "[dry-run] exp_cfg_status"
    } else {
        $statusResult = Invoke-RconCommandChecked -Command "status" -OutputPath $rconLogPath
        if ($statusResult.Success) {
            $statusResponse = $statusResult.Output
            Write-Step "RCON status: success"
        } else {
            Write-Step "RCON status: failed ($($statusResult.Error))"
        }

        $cfgResult = Invoke-RconCommandChecked -Command "exp_cfg_status" -OutputPath $rconLogPath
        if ($cfgResult.Success) {
            $cfgStatusResponse = $cfgResult.Output
            Write-Step "RCON exp_cfg_status: success"
        } else {
            Write-Step "RCON exp_cfg_status: failed ($($cfgResult.Error))"
        }

        if ($statusResult.Success -and $cfgResult.Success) {
            $script:RconAvailable = $true
        }
    }
} else {
    Write-Step "RCON password not provided; command execution will use manual fallback."
}

if (-not $script:RconAvailable) {
    if ($RequireRcon -or $Strict) {
        $failures.Add("RCON verification failed or was skipped while -RequireRcon/-Strict was requested.")
    } else {
        $warnings.Add("RCON unavailable; manual fallback commands are required.")
    }

    Write-Host ""
    Write-Host "RCON verification: no"
    Write-Host "If you use RCON, start HLDS with a known rcon_password and rerun with:"
    Write-Host "  .\scripts\run-stock-client-playtest.ps1 -RconPassword <password>"
    Write-Host "Manual HLDS console fallback does not need the RCON password."
} else {
    Write-Host "RCON verification: yes"
}

Write-Section "Client connection detection"
$clientStatus = Get-ClientConnectionStatus -LivePaths $livePaths -StatusText $statusResponse -Since $script:StartedAt
$clientLine = ""
if ($clientStatus.Status -eq "yes") {
    $clientLine = "Client connected: yes"
} elseif ($clientStatus.Status -eq "no") {
    $clientLine = "Client connected: no"
} else {
    $clientLine = "Client status: unknown"
}

Write-Host $clientLine
Write-Step "Client detection source: $($clientStatus.Source)"
Write-Step "Client detection details: $($clientStatus.Details)"
Set-Content -Path $clientStatusPath -Value @(
    $clientLine,
    "source=$($clientStatus.Source)",
    "details=$($clientStatus.Details)"
)

if ($clientStatus.Status -ne "yes") {
    $connectCommand = "connect $HostName`:$Port"
    $clientLaunchCommand = "`"$($livePaths.HlExe)`" -game hlserver_testbed -console +connect $HostName`:$Port"
    Write-Host ""
    Write-Host "Client was not confirmed. Use:"
    Write-Host "  $clientLaunchCommand"
    Write-Host "or in the stock Half-Life console:"
    Write-Host "  $connectCommand"
    Add-ContentLine -Path $summaryPath -Text "client_connect_command=$connectCommand"

    if ($Strict) {
        $failures.Add("Strict mode requires a confirmed stock-client connection.")
    }
}

Write-Section "Initial sandbox setup validation"
$initialCommands = @(
    "exp_matchcfg_apply $MatchPack",
    "exp_sandbox_start",
    "exp_sandbox_target $TargetProfile",
    "exp_sandbox_spot $TargetSpot",
    "exp_sandbox_status"
)

$initialSetup = Invoke-CommandSet -Name "initial setup" -Commands $initialCommands -OutputPath $commandLogPath
Write-Step "exp_matchcfg_apply $MatchPack status: $($initialSetup.Status)"
Write-Step "sandbox start/target/spot status: $($initialSetup.Status)"

if ($initialSetup.Status -ne "yes") {
    Write-ManualFallback -Commands $initialCommands
    if ($Strict -and -not $DryRun) {
        $failures.Add("Strict mode requires verified initial sandbox setup.")
    }
}

foreach ($command in $initialCommands) {
    Add-ContentLine -Path $summaryPath -Text "initial_command=$command"
}

Write-Section "Guided weapon checks"
foreach ($weaponName in $selectedWeapons) {
    Write-Host ""
    Write-Host "--- $weaponName ---"

    $weaponCommands = @(
        "exp_sandbox_weapon $weaponName",
        "exp_sandbox_target $TargetProfile",
        "exp_sandbox_spot $TargetSpot",
        "exp_sandbox_reset",
        "exp_sandbox_verify",
        "exp_sandbox_status"
    )

    $before = Get-LogSnapshot -LivePaths $livePaths
    $stepStart = Get-Date
    $setupResult = Invoke-CommandSet -Name "$weaponName setup" -Commands $weaponCommands -OutputPath $commandLogPath

    foreach ($instruction in Get-WeaponInstructions -WeaponName $weaponName) {
        Write-Host $instruction
    }

    Wait-ManualStep "Shoot the $weaponName test now, then continue."

    $after = Get-LogSnapshot -LivePaths $livePaths
    $fresh = Test-FreshTelemetry -Before $before -After $after -StepStart $stepStart -WeaponName $weaponName

    if ($fresh.Fresh) {
        Write-Step "Fresh telemetry detected: yes ($($fresh.Reason))"
    } else {
        Write-Step "Fresh telemetry detected: no ($($fresh.Reason))"
    }

    $analysisPath = Join-Path $reportDir "$weaponName-analysis.txt"
    $analysis = Invoke-Analyzer -LivePaths $livePaths -WeaponName $weaponName -Destination $analysisPath
    if ($analysis.Success) {
        Write-Step "Analyzer report: $analysisPath"
    } else {
        Write-Step "Analyzer warning: $($analysis.Error)"
    }

    $stepClientStatus = $clientStatus.Status
    if ($fresh.Fresh) {
        $stepClientStatus = "yes"
    }

    $stepWarnings = New-Object System.Collections.Generic.List[string]
    if ($stepClientStatus -ne "yes") {
        $stepWarnings.Add("client_not_confirmed")
    }
    if ($setupResult.Status -ne "yes") {
        $stepWarnings.Add("setup_$($setupResult.Status)")
    }
    if (-not $fresh.Fresh) {
        $stepWarnings.Add("fresh_telemetry_missing")
    }
    if (-not $analysis.Success) {
        $stepWarnings.Add("analysis_warning")
    }

    if ($Strict) {
        if ($stepClientStatus -ne "yes") {
            $failures.Add("Strict mode: client not confirmed during $weaponName step.")
        }
        if (-not $script:RconAvailable) {
            $failures.Add("Strict mode: RCON unavailable during $weaponName step.")
        }
        if ($setupResult.Status -eq "no") {
            $failures.Add("Strict mode: setup command failed for $weaponName.")
        }
        if (-not $fresh.Fresh) {
            $failures.Add("Strict mode: no fresh telemetry detected for $weaponName.")
        }
    }

    $weaponSummaries += [pscustomobject]@{
        Weapon = $weaponName
        ClientConnected = $stepClientStatus
        SetupCommands = $setupResult.Status
        FreshTelemetry = $(if ($fresh.Fresh) { "yes" } else { "no" })
        EventCount = $fresh.EventCount
        AnalyzerReport = $analysisPath
        LogPath = $fresh.LogPath
        Warnings = (($stepWarnings.ToArray()) -join ",")
    }
}

$weaponSummaries | Format-Table -AutoSize | Out-String -Width 220 | Set-Content -Path $perWeaponPath

Add-ContentLine -Path $summaryPath -Text ""
Add-ContentLine -Path $summaryPath -Text "Per-weapon summary:"
Get-Content -Path $perWeaponPath | Add-Content -Path $summaryPath

if ($OpenEditor) {
    $editor = Join-Path $script:RepoRoot "scripts\open-hldm-editor.bat"
    if (Test-Path $editor) {
        Write-Step "Opening editor helper: $editor"
        if (-not $DryRun) {
            Start-Process -FilePath $editor -WorkingDirectory $script:RepoRoot | Out-Null
        }
    } else {
        $warnings.Add("OpenEditor requested, but editor helper was not found: $editor")
    }
}

Write-Section "Final playtest summary"
Write-Host "Summary: $summaryPath"
Write-Host "Per-weapon summary: $perWeaponPath"
Write-Host "RCON validation log: $rconLogPath"
Write-Host "Client status: $clientStatusPath"
Write-Host ""
Get-Content -Path $perWeaponPath | ForEach-Object { Write-Host $_ }

if ($warnings.Count -gt 0) {
    Add-ContentLine -Path $summaryPath -Text ""
    Add-ContentLine -Path $summaryPath -Text "Warnings:"
    foreach ($warning in $warnings) {
        Add-ContentLine -Path $summaryPath -Text "- $warning"
    }
}

if ($failures.Count -gt 0) {
    Add-ContentLine -Path $summaryPath -Text ""
    Add-ContentLine -Path $summaryPath -Text "Failures:"
    foreach ($failure in ($failures.ToArray() | Select-Object -Unique)) {
        Add-ContentLine -Path $summaryPath -Text "- $failure"
    }

    Write-Host ""
    Write-Host "Validation result: FAIL"
    foreach ($failure in ($failures.ToArray() | Select-Object -Unique)) {
        Write-Host "  $failure"
    }
    exit 1
}

Write-Host "Validation result: PASS/INFORMATIONAL"
Write-Host "Human subjective feel testing still requires manual shooting in the stock client."
exit 0
