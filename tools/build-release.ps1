$ErrorActionPreference = "Stop"
& (Join-Path $PSScriptRoot "build.ps1") -Configuration Release -Platform x64
exit $LASTEXITCODE
