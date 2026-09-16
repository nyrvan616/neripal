param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build/windows'

& cmake -S $projectRoot -B $buildDirectory -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE" }

& cmake --build $buildDirectory
if ($LASTEXITCODE -ne 0) { throw "CMake build failed with exit code $LASTEXITCODE" }

& ctest --test-dir $buildDirectory --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "CTest failed with exit code $LASTEXITCODE" }
