[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\common.ps1"
Import-HLServerEnv

if (-not (Test-Path -LiteralPath (Join-RepoPath "third_party\valve-halflife-sdk") -PathType Container)) {
    & "$PSScriptRoot\bootstrap.ps1"
}

Write-Step "Configuring Visual Studio 2022 Win32 build"

$cmake = Get-CMakePath
& $cmake --preset vs2022-win32
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed."
}

Write-Step "Configure complete"
Write-Host "Build directory: $(Get-BuildDirectory)"
Write-Host "Solution: $(Get-SolutionPath)"
