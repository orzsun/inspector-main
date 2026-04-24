$ErrorActionPreference = "Stop"
& (Join-Path $PSScriptRoot "build.ps1") -Configuration Debug -Platform x64
exit $LASTEXITCODE
