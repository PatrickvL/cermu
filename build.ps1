# Quick Build Script for C64 Emulator
# Usage: .\build.ps1 [Release|Debug]

param(
    [string]$Configuration = "Release"
)

# Build environment paths
$MSBuildPath = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
$ProjectPath = "c:\Workspaces\Mine\aiemu\code\c"
$SolutionFile = "aiemuc.sln"

Write-Host "Building C64 Emulator ($Configuration)..." -ForegroundColor Green

# Change to project directory
Push-Location $ProjectPath

try {
    # Check if MSBuild exists
    if (-not (Test-Path $MSBuildPath)) {
        Write-Error "MSBuild not found at: $MSBuildPath"
        Write-Host "Please install Visual Studio Build Tools 2022" -ForegroundColor Yellow
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
