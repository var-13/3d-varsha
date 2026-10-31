# ThermoBench server launcher
# Run from project root: .\start.ps1

$mingw = "C:\msys64\mingw64\bin"
$env:PATH = "$mingw;" + $env:PATH

if (-not (Test-Path ".\build\bin\ThermoBenchServer.exe")) {
    Write-Host "Server not built yet. Run .\build.ps1 first." -ForegroundColor Red
    exit 1
}

Write-Host "Starting ThermoBench server at http://localhost:8080 ..." -ForegroundColor Cyan
Write-Host "Press Ctrl+C to stop." -ForegroundColor Gray
Write-Host ""
& ".\build\bin\ThermoBenchServer.exe" ui 8080
