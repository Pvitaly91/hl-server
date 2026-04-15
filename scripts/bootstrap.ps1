[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"

Write-Step "Verifying Windows build prerequisites"
Assert-BuildPrerequisites

$vendorRoot = Join-RepoPath "third_party\valve-halflife-sdk"
$licensesRoot = Join-RepoPath "LICENSES"

foreach ($path in @(
    (Join-RepoPath "docs"),
    (Join-RepoPath "src"),
    (Join-RepoPath "cmake"),
    (Join-RepoPath "scripts"),
    (Join-RepoPath ".github\workflows"),
    $licensesRoot,
    (Join-RepoPath "testbed\runtime"),
    (Join-RepoPath "testbed\logs"),
    (Join-RepoPath "testbed\cache"),
    (Join-RepoPath "third_party")
)) {
    Ensure-Directory -Path $path
}

if (-not (Test-Path -LiteralPath $vendorRoot -PathType Container)) {
    Write-Step "Vendoring pinned Valve Half-Life source snapshot"

    $git = Get-GitPath
    $tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("hl-server-bootstrap-" + [Guid]::NewGuid().ToString("N"))

    try {
        & $git init $tempRoot | Out-Null
        & $git -C $tempRoot remote add origin $script:ValveSdkUpstreamUrl
        & $git -C $tempRoot fetch --depth 1 origin $script:ValveSdkUpstreamCommit
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to fetch pinned upstream commit $script:ValveSdkUpstreamCommit."
        }

        & $git -C $tempRoot checkout --force FETCH_HEAD
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to checkout pinned upstream commit $script:ValveSdkUpstreamCommit."
        }

        Copy-DirectoryContents -Source $tempRoot -Destination $vendorRoot

        $vendorGitDir = Join-Path $vendorRoot ".git"
        if (Test-Path -LiteralPath $vendorGitDir) {
            Remove-Item -LiteralPath $vendorGitDir -Recurse -Force
        }
    }
    finally {
        if (Test-Path -LiteralPath $tempRoot) {
            Remove-Item -LiteralPath $tempRoot -Recurse -Force
        }
    }
}
else {
    Write-Step "Vendored Valve source already present"
}

$upstreamLicense = Join-Path $vendorRoot "LICENSE"
$copiedLicense = Join-Path $licensesRoot "valve-halflife-sdk-LICENSE.txt"

if (Test-LeafPath -Path $upstreamLicense) {
    Copy-Item -LiteralPath $upstreamLicense -Destination $copiedLicense -Force
}
else {
    throw "Expected upstream license file not found at $upstreamLicense."
}

$envExample = Join-RepoPath ".env.example"
if (-not (Test-LeafPath -Path $envExample)) {
    @"
# Copy this file to .env and fill in any paths that are not auto-detected.
# Leave values blank to let the scripts probe common Windows locations.

HLDS_EXE=
HL_EXE=
STEAMCMD_EXE=
HL_RUNTIME_TEMPLATE=
"@ | Set-Content -LiteralPath $envExample -Encoding ASCII
}

Write-Step "Bootstrap verification complete"
Write-Host ""
Write-Host "Next steps:"
Write-Host "  .\scripts\configure.ps1"
Write-Host "  .\scripts\build.ps1 -Configuration Debug"
Write-Host "  .\scripts\install-testbed.ps1 -Configuration Debug -AllowSteamCmdDownload"
Write-Host "  .\scripts\run-server.ps1 -Configuration Debug"
