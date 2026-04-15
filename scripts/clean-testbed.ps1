[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"

$repoRoot = Get-RepoRoot
$testbedRoot = Join-RepoPath "testbed"

foreach ($path in @(
    (Join-RepoPath "build"),
    (Join-RepoPath "artifacts"),
    (Join-RepoPath "testbed\runtime"),
    (Join-RepoPath "testbed\logs"),
    (Join-RepoPath "testbed\cache")
)) {
    if (Test-Path -LiteralPath $path) {
        Assert-PathWithinRoot -Path $path -Root $repoRoot -Description "Generated path"
        Remove-Item -LiteralPath $path -Recurse -Force
    }
}

Ensure-Directory -Path (Join-Path $testbedRoot "runtime")
Ensure-Directory -Path (Join-Path $testbedRoot "logs")
Ensure-Directory -Path (Join-Path $testbedRoot "cache")

Write-Step "Cleaned build outputs and disposable testbed directories"
