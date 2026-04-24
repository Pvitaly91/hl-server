[CmdletBinding()]
param(
    [string[]]$Weapons = @("glock", "mp5", "357", "shotgun"),

    [string]$MatchPack = "hldm_skill_default",

    [ValidateSet("unarmored", "vest", "vest_headprotected")]
    [string]$TargetProfile = "vest_headprotected",

    [string]$TargetSpot = "default",

    [string]$HostName = "127.0.0.1",

    [int]$Port = 27015,

    [string]$RconPassword,

    [string]$HlExe,

    [string]$OutputRoot,

    [switch]$StartServer,
    [switch]$NoClient,
    [switch]$OpenEditor,
    [switch]$DryRun,
    [switch]$NoPause
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

function Add-ReportLine {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [string]$Text
    )

    $Lines.Add($Text) | Out-Null
}

function Write-AndRecord {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [string]$Text = ""
    )

    Write-Host $Text
    Add-ReportLine -Lines $Lines -Text $Text
}

function Write-Section {
    param(
        [System.Collections.Generic.List[string]]$Lines,
        [string]$Title
    )

    Write-AndRecord -Lines $Lines
    Write-AndRecord -Lines $Lines -Text $Title
    Write-AndRecord -Lines $Lines -Text ("".PadLeft($Title.Length, "-"))
}

function Wait-ForManualStep {
    param(
        [string]$Prompt,
        [switch]$Skip
    )

    if ($Skip) {
        Write-Host "$Prompt [skipped by -NoPause/-DryRun]"
        return
    }

    Read-Host "$Prompt Press Enter when done" | Out-Null
}

function Get-LivePaths {
    param([string]$ExplicitHlExe)

    $clientInstall = Resolve-TestbedClientInstall -ExplicitHlExe $ExplicitHlExe
    $liveModRoot = if ($clientInstall) {
        Get-TestbedLiveModRoot -ClientRoot $clientInstall.Root -GameDirName (Get-TestbedLiveModName)
    }
    else {
        $null
    }

    return [PSCustomObject]@{
        ClientInstall = $clientInstall
        ClientRoot = if ($clientInstall) { $clientInstall.Root } else { $null }
        HlExe = if ($clientInstall) { $clientInstall.HlExe } else { $null }
        HldsExe = if ($clientInstall -and $clientInstall.Probe) { $clientInstall.Probe.HldsExe } else { $null }
        LiveModRoot = $liveModRoot
        LiveLogs = if ($liveModRoot) { Join-Path $liveModRoot "logs" } else { $null }
        EditorExe = if ($liveModRoot) { Join-Path $liveModRoot "HlConfigEditorCpp.exe" } else { $null }
        MatchPacks = if ($liveModRoot) { Join-Path $liveModRoot "match_packs" } else { $null }
    }
}

function New-GoldSrcPacket {
    param([string]$Payload)

    $prefix = [byte[]](255, 255, 255, 255)
    $body = [System.Text.Encoding]::ASCII.GetBytes($Payload)
    $packet = New-Object byte[] ($prefix.Length + $body.Length)
    [Array]::Copy($prefix, 0, $packet, 0, $prefix.Length)
    [Array]::Copy($body, 0, $packet, $prefix.Length, $body.Length)
    return $packet
}

function ConvertFrom-GoldSrcPacket {
    param([byte[]]$Packet)

    if ($Packet.Length -ge 4 -and $Packet[0] -eq 255 -and $Packet[1] -eq 255 -and $Packet[2] -eq 255 -and $Packet[3] -eq 255) {
        $payload = New-Object byte[] ($Packet.Length - 4)
        [Array]::Copy($Packet, 4, $payload, 0, $payload.Length)
    }
    else {
        $payload = $Packet
    }

    $text = [System.Text.Encoding]::ASCII.GetString($payload)
    if ($text.StartsWith("l")) {
        $text = $text.Substring(1)
    }

    return $text.Trim([char]0, "`r", "`n")
}

function Invoke-GoldSrcRcon {
    param(
        [string]$HostName,
        [int]$Port,
        [string]$Password,
        [string[]]$Commands,
        [int]$TimeoutMilliseconds = 1500
    )

    if ([string]::IsNullOrWhiteSpace($Password)) {
        throw "RCON password is empty."
    }

    if ($Password.Contains('"') -or $Password.Contains("`r") -or $Password.Contains("`n")) {
        throw "RCON password cannot contain quote or newline characters."
    }

    $udp = New-Object System.Net.Sockets.UdpClient
    try {
        $udp.Client.ReceiveTimeout = $TimeoutMilliseconds
        $udp.Client.SendTimeout = $TimeoutMilliseconds
        $udp.Connect($HostName, $Port)

        $challengePacket = New-GoldSrcPacket -Payload "challenge rcon`n"
        $udp.Send($challengePacket, $challengePacket.Length) | Out-Null
        $remote = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
        $challengeResponse = $udp.Receive([ref]$remote)
        $challengeText = ConvertFrom-GoldSrcPacket -Packet $challengeResponse
        if ($challengeText -notmatch "challenge rcon\s+(\S+)") {
            throw "HLDS did not return an RCON challenge. Response: $challengeText"
        }

        $challenge = $Matches[1]
        $responses = New-Object System.Collections.Generic.List[string]
        foreach ($command in $Commands) {
            if ([string]::IsNullOrWhiteSpace($command)) {
                continue
            }

            $responses.Add("> $command") | Out-Null
            $packet = New-GoldSrcPacket -Payload ("rcon {0} ""{1}"" {2}`n" -f $challenge, $Password, $command)
            $udp.Send($packet, $packet.Length) | Out-Null
            try {
                $commandResponse = $udp.Receive([ref]$remote)
                $responses.Add((ConvertFrom-GoldSrcPacket -Packet $commandResponse)) | Out-Null
            }
            catch [System.Net.Sockets.SocketException] {
                $responses.Add("(no response before timeout; command may still have been accepted)") | Out-Null
            }
        }

        $responseText = $responses -join "`r`n"
        if ($responseText -match "Bad rcon_password|Bad Password|No password set") {
            throw "HLDS rejected the RCON password. Response: $responseText"
        }

        return $responseText
    }
    finally {
        $udp.Close()
    }
}

function Test-ClientConnectionFromStatus {
    param([string]$StatusText)

    if ([string]::IsNullOrWhiteSpace($StatusText)) {
        return $false
    }

    if ($StatusText -match "(?im)^\s*players\s*:\s*([1-9]\d*)\s+active") {
        return $true
    }

    if ($StatusText -match "(?im)^#\s*\d+\s+""[^""]+""") {
        return $true
    }

    return $false
}

function Get-LatestWeaponLog {
    param($LivePaths)

    $roots = New-Object System.Collections.Generic.List[string]
    $roots.Add((Get-TestbedLogsRoot))
    if ($LivePaths.LiveLogs) {
        $roots.Add($LivePaths.LiveLogs)
    }
    if ($LivePaths.ClientRoot) {
        $roots.Add((Join-Path $LivePaths.ClientRoot "logs"))
    }

    $logs = New-Object System.Collections.Generic.List[object]
    foreach ($root in ($roots | Select-Object -Unique)) {
        if (-not (Test-Path -LiteralPath $root -PathType Container)) {
            continue
        }

        foreach ($log in (Get-ChildItem -LiteralPath $root -Filter "weapon-debug-*.log" -File -ErrorAction SilentlyContinue)) {
            $logs.Add($log)
        }
    }

    return @($logs | Sort-Object LastWriteTime -Descending | Select-Object -First 1)
}

function Invoke-AnalyzerForWeapon {
    param(
        [string]$Weapon,
        [string]$Destination,
        [string]$LogPath
    )

    $analyzer = Join-Path $PSScriptRoot "analyze-weapon-log.ps1"
    if (-not (Test-Path -LiteralPath $analyzer -PathType Leaf)) {
        throw "Analyzer script not found at $analyzer"
    }

    if ([string]::IsNullOrWhiteSpace($LogPath)) {
        "No weapon-debug log was available for $Weapon." | Set-Content -LiteralPath $Destination -Encoding ASCII
        return 2
    }

    $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $analyzer -Path $LogPath -Weapon $Weapon 2>&1
    $exitCode = $LASTEXITCODE
    $output | Set-Content -LiteralPath $Destination -Encoding ASCII
    return $exitCode
}

function Get-WeaponInstructions {
    param([string]$Weapon)

    switch ($Weapon.ToLowerInvariant()) {
        "glock" {
            return @(
                "Stand still and fire one careful click.",
                "Wait for recovery, then fire another careful click.",
                "Fire several rapid clicks.",
                "Expected evidence: accepted shots plus cadence/pattern growth; no hard tap-fire gate required."
            )
        }
        "mp5" {
            return @(
                "Fire a controlled 3-5 shot burst.",
                "Pause for recovery.",
                "Fire a longer spray while standing, then try a short movement spray.",
                "Expected evidence: burst-growth/pattern progression and reset/recovery after the pause."
            )
        }
        "357" {
            return @(
                "Fire one careful first shot.",
                "Fire fast follow-up shots.",
                "Wait for cadence/pattern reset, then fire again.",
                "Expected evidence: cadence/pattern penalties on rushed follow-ups and reset after idle time."
            )
        }
        "shotgun" {
            return @(
                "Stand close to the dummy and fire one shell.",
                "Fire a second shell after a short delay.",
                "Wait for pattern reset and fire another shell.",
                "Expected evidence: pellet/pattern telemetry with consistent hit/kill damage summaries."
            )
        }
        default {
            return @("No instructions are defined for $Weapon.")
        }
    }
}

foreach ($weapon in $Weapons) {
    if (@("glock", "mp5", "357", "shotgun") -notcontains $weapon.ToLowerInvariant()) {
        throw "Unsupported weapon '$weapon'. Use glock, mp5, 357, or shotgun."
    }
}

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path (Join-Path (Get-TestbedLogsRoot) "reports") "stock-client-playtests"
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$reportRoot = Join-Path $OutputRoot $timestamp
Ensure-Directory -Path $reportRoot

$summary = New-Object System.Collections.Generic.List[string]
$livePaths = Get-LivePaths -ExplicitHlExe $HlExe
$skipPause = $NoPause -or $DryRun
$rconProvided = -not [string]::IsNullOrWhiteSpace($RconPassword)
$rconAvailable = $false

Write-Section -Lines $summary -Title "Stable Improved HLDM stock-client playtest"
Write-AndRecord -Lines $summary -Text "timestamp=$timestamp"
Write-AndRecord -Lines $summary -Text "branch=$(git rev-parse --abbrev-ref HEAD)"
Write-AndRecord -Lines $summary -Text "commit=$(git rev-parse HEAD)"
Write-AndRecord -Lines $summary -Text "output=$reportRoot"
Write-AndRecord -Lines $summary -Text "host=$HostName"
Write-AndRecord -Lines $summary -Text "port=$Port"
Write-AndRecord -Lines $summary -Text "match_pack=$MatchPack"
Write-AndRecord -Lines $summary -Text "target_profile=$TargetProfile"
Write-AndRecord -Lines $summary -Text "target_spot=$TargetSpot"
Write-AndRecord -Lines $summary -Text ("rcon_password={0}" -f $(if ($rconProvided) { "provided" } else { "not provided" }))
Write-AndRecord -Lines $summary -Text "client_root=$($livePaths.ClientRoot)"
Write-AndRecord -Lines $summary -Text "hl_exe=$($livePaths.HlExe)"
Write-AndRecord -Lines $summary -Text "live_mod_root=$($livePaths.LiveModRoot)"

Write-Section -Lines $summary -Title "Package readiness"
$packageCheckPath = Join-Path $reportRoot "package-check.txt"
$checkOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "check-improved-hldm.ps1") 2>&1
$checkExit = $LASTEXITCODE
$checkOutput | Set-Content -LiteralPath $packageCheckPath -Encoding ASCII
Write-AndRecord -Lines $summary -Text "package_check=$packageCheckPath exit=$checkExit"
if ($checkExit -ne 0) {
    Write-AndRecord -Lines $summary -Text "Package check failed. Fix this before manual playtesting."
}

if ($StartServer -and -not $DryRun) {
    Write-Section -Lines $summary -Title "Launching HLDS and stock client"
    Write-AndRecord -Lines $summary -Text "The launcher does not persist or write RCON passwords. Set rcon_password in HLDS or a private local cfg before relying on RCON."
    $launchArgs = @{
        Port = $Port
    }
    if ($NoClient) {
        $launchArgs["NoClient"] = $true
    }
    if (-not [string]::IsNullOrWhiteSpace($HlExe)) {
        $launchArgs["HlExe"] = $HlExe
    }
    & "$PSScriptRoot\play-improved-hldm.ps1" @launchArgs
}
else {
    Write-Section -Lines $summary -Title "Launch instructions"
    Write-AndRecord -Lines $summary -Text "Dry-run or no -StartServer was selected; no HLDS/client process was launched."
    Write-AndRecord -Lines $summary -Text "Start live session:"
    Write-AndRecord -Lines $summary -Text "  .\scripts\play-improved-hldm.bat"
    Write-AndRecord -Lines $summary -Text "Manual client connect command:"
    Write-AndRecord -Lines $summary -Text "  connect ${HostName}:$Port"
}

Write-Section -Lines $summary -Title "Client connection and RCON"
if ($rconProvided -and -not $DryRun) {
    try {
        $statusResponse = Invoke-GoldSrcRcon -HostName $HostName -Port $Port -Password $RconPassword -Commands @("status")
        $rconAvailable = $true
        $statusPath = Join-Path $reportRoot "rcon-status.txt"
        $statusResponse | Set-Content -LiteralPath $statusPath -Encoding ASCII
        $clientConnected = Test-ClientConnectionFromStatus -StatusText $statusResponse
        Write-AndRecord -Lines $summary -Text "rcon_status=$statusPath"
        Write-AndRecord -Lines $summary -Text ("rcon=PASS client_connected={0}" -f $(if ($clientConnected) { "PASS" } else { "FAIL_OR_NOT_YET" }))
        if (-not $clientConnected) {
            Write-AndRecord -Lines $summary -Text "Manual connect command: connect ${HostName}:$Port"
        }
    }
    catch {
        Write-AndRecord -Lines $summary -Text "rcon=FAIL $($_.Exception.Message)"
        Write-AndRecord -Lines $summary -Text "Fallback: paste the printed commands into the HLDS console manually."
        Write-AndRecord -Lines $summary -Text "Manual connect command: connect ${HostName}:$Port"
    }
}
else {
    Write-AndRecord -Lines $summary -Text "RCON was not tested. Provide -RconPassword and run without -DryRun to test it."
    Write-AndRecord -Lines $summary -Text "Manual connect command: connect ${HostName}:$Port"
}

$initialCommands = @(
    "exp_matchcfg_apply $MatchPack",
    "exp_sandbox_start"
)
Write-Section -Lines $summary -Title "Initial live commands"
foreach ($command in $initialCommands) {
    Write-AndRecord -Lines $summary -Text "  $command"
}
if ($rconAvailable) {
    try {
        $response = Invoke-GoldSrcRcon -HostName $HostName -Port $Port -Password $RconPassword -Commands $initialCommands
        $response | Set-Content -LiteralPath (Join-Path $reportRoot "rcon-initial-commands.txt") -Encoding ASCII
        Write-AndRecord -Lines $summary -Text "initial_rcon=PASS"
    }
    catch {
        Write-AndRecord -Lines $summary -Text "initial_rcon=FAIL $($_.Exception.Message)"
    }
}

foreach ($weapon in $Weapons) {
    $normalizedWeapon = $weapon.ToLowerInvariant()
    $weaponCommands = @(
        "exp_sandbox_weapon $normalizedWeapon",
        "exp_sandbox_pack $MatchPack",
        "exp_sandbox_target $TargetProfile",
        "exp_sandbox_spot $TargetSpot",
        "exp_sandbox_reset",
        "exp_sandbox_status",
        "exp_sandbox_verify"
    )

    Write-Section -Lines $summary -Title ("{0} manual test" -f $normalizedWeapon)
    Write-AndRecord -Lines $summary -Text "Commands:"
    foreach ($command in $weaponCommands) {
        Write-AndRecord -Lines $summary -Text "  $command"
    }

    if ($rconAvailable) {
        try {
            $weaponResponse = Invoke-GoldSrcRcon -HostName $HostName -Port $Port -Password $RconPassword -Commands $weaponCommands
            $weaponResponse | Set-Content -LiteralPath (Join-Path $reportRoot ("{0}-rcon.txt" -f $normalizedWeapon)) -Encoding ASCII
            Write-AndRecord -Lines $summary -Text "weapon_rcon_$normalizedWeapon=PASS"
        }
        catch {
            Write-AndRecord -Lines $summary -Text "weapon_rcon_$normalizedWeapon=FAIL $($_.Exception.Message)"
            Write-AndRecord -Lines $summary -Text "Paste the commands above into HLDS manually before shooting."
        }
    }
    else {
        Write-AndRecord -Lines $summary -Text "weapon_rcon_$normalizedWeapon=not_run"
    }

    Write-AndRecord -Lines $summary -Text "Manual shooting instructions:"
    foreach ($instruction in (Get-WeaponInstructions -Weapon $normalizedWeapon)) {
        Write-AndRecord -Lines $summary -Text "  - $instruction"
    }

    Wait-ForManualStep -Prompt ("Shoot {0} test now." -f $normalizedWeapon) -Skip:$skipPause

    $latestLog = Get-LatestWeaponLog -LivePaths $livePaths
    $analysisPath = Join-Path $reportRoot ("{0}-analysis.txt" -f $normalizedWeapon)
    $analysisExit = Invoke-AnalyzerForWeapon -Weapon $normalizedWeapon -Destination $analysisPath -LogPath $(if ($latestLog) { $latestLog.FullName } else { $null })
    Write-AndRecord -Lines $summary -Text ("analysis_{0}={1} exit={2}" -f $normalizedWeapon, $analysisPath, $analysisExit)

    if ($latestLog) {
        Copy-Item -LiteralPath $latestLog.FullName -Destination (Join-Path $reportRoot ("latest-weapon-debug-after-{0}.log" -f $normalizedWeapon)) -Force
        Write-AndRecord -Lines $summary -Text ("latest_log_after_{0}={1}" -f $normalizedWeapon, $latestLog.FullName)
    }
}

Write-Section -Lines $summary -Title "Editor integration"
Write-AndRecord -Lines $summary -Text "The editor can be used alongside this script:"
Write-AndRecord -Lines $summary -Text "  - Live Server tab: send the same RCON commands."
Write-AndRecord -Lines $summary -Text "  - Telemetry tab: analyze the latest weapon log."
Write-AndRecord -Lines $summary -Text "  - Guided Tests tab: run guided editor-side setup."
Write-AndRecord -Lines $summary -Text "  - Reports tab: compare saved guided reports."

if ($OpenEditor -and -not $DryRun) {
    & "$PSScriptRoot\open-hldm-editor.ps1"
}

$summaryPath = Join-Path $reportRoot "summary.txt"
$summary | Set-Content -LiteralPath $summaryPath -Encoding ASCII

Write-Host ""
Write-Host "Stock-client playtest report ready"
Write-Host "  output : $reportRoot"
Write-Host "  summary: $summaryPath"
Write-Host ""
Write-Host "Human validation still required: shoot in the stock client and judge subjective feel manually."
