# Quick Build Script for C64 Emulator
# Usage: .\build.ps1 [Release|Debug]

param(
    [string]$Configuration = "Release"
)

# Build environment paths - detect automatically
$MSBuildPath = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
$ProjectPath = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectPath = Split-Path -Parent $ProjectPath
$SolutionFile = "cermu.sln"

# Alternative MSBuild locations to try
$MSBuildPaths = @(
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
)

# Find first available MSBuild
$MSBuildPath = $MSBuildPaths | Where-Object { Test-Path $_ } | Select-Object -First 1

Write-Host "Building C64 Emulator ($Configuration)..." -ForegroundColor Green

# Change to project directory
Push-Location $ProjectPath

try {
    # Check if MSBuild exists
    if (-not $MSBuildPath -or -not (Test-Path $MSBuildPath)) {
        Write-Error "MSBuild not found. Tried:"
        $MSBuildPaths | ForEach-Object { Write-Host "  - $_" -ForegroundColor Yellow }
        Write-Host ""
        Write-Host "Please install one of:" -ForegroundColor Yellow
        Write-Host "  - Visual Studio Build Tools 2022" -ForegroundColor White
        Write-Host "  - Visual Studio 2022 (any edition)" -ForegroundColor White
        exit 1
    }
    
    # Check if project directory exists
    if (-not (Test-Path $ProjectPath)) {
        Write-Error "Project directory not found: $ProjectPath"
        Write-Host "Please run this script from the project root directory" -ForegroundColor Yellow
        exit 1
    }

    # Build the solution
    Write-Host "Using MSBuild: $MSBuildPath" -ForegroundColor Cyan
    & $MSBuildPath $SolutionFile /p:Configuration=$Configuration

    if ($LASTEXITCODE -eq 0) {
        Write-Host "Build completed successfully!" -ForegroundColor Green
        Write-Host "Executables available in: bin\$Configuration\" -ForegroundColor Cyan
        
        # List available executables
        $BinPath = "bin\$Configuration"
        if (Test-Path $BinPath) {
            Write-Host "Available executables:" -ForegroundColor Yellow
            Get-ChildItem "$BinPath\*.exe" | ForEach-Object { 
                Write-Host "  - $($_.Name)" -ForegroundColor White
            }
        }
    } else {
        Write-Error "Build failed with exit code: $LASTEXITCODE"
        exit $LASTEXITCODE
    }
} finally {
    Pop-Location
}
