param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$solutionPath = Join-Path $repoRoot "KXPacketInspector.sln"
$msbuildPath = "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"

if (-not (Test-Path $solutionPath)) {
    throw "Solution not found: $solutionPath"
}

if (-not (Test-Path $msbuildPath)) {
    throw "MSBuild not found: $msbuildPath"
}

# Some shells expose both PATH and Path. MSBuild/CL can choke on that duplicate.
$canonicalPath = $null
if (Test-Path Env:PATH) {
    $canonicalPath = $env:PATH
} elseif (Test-Path Env:Path) {
    $canonicalPath = (Get-Item Env:Path).Value
}

Remove-Item Env:Path -ErrorAction SilentlyContinue
if ($null -ne $canonicalPath) {
    $env:PATH = $canonicalPath
}

Write-Host "Building $Configuration|$Platform via sanitized environment..."
& $msbuildPath $solutionPath /t:Build /p:Configuration=$Configuration /p:Platform=$Platform
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
