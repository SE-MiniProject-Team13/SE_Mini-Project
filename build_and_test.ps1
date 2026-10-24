# build_and_test.ps1
# Minimal reproducible build and test script for Windows MinGW g++

$ErrorActionPreference = "Stop"

Write-Host "=================================================" -ForegroundColor Cyan
Write-Host "   TMS SPRINT 1 - BUILD AND TEST SUITE (MinGW)    " -ForegroundColor Cyan
Write-Host "=================================================" -ForegroundColor Cyan

$CXX = "g++"
$CXXFLAGS = @("-std=c++11", "-Wall", "-Wextra", "-pedantic", "-Iinclude")

function Build-And-Run([string]$name, [string[]]$sources, [bool]$run = $true) {
    $exe = "$name.exe"
    Write-Host "`n[BUILDING] $exe..." -ForegroundColor Yellow
    $cmd = @($CXX) + $CXXFLAGS + $sources + @("-o", $exe)
    & $cmd[0] $cmd[1..($cmd.Length - 1)]
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[FAIL] Compilation failed for $exe" -ForegroundColor Red
        exit 1
    }
    Write-Host "[SUCCESS] Built $exe" -ForegroundColor Green

    if ($run) {
        Write-Host "[RUNNING] $exe..." -ForegroundColor Cyan
        & ".\$exe"
        if ($LASTEXITCODE -ne 0) {
            Write-Host "[FAIL] Execution failed for $exe (exit code $LASTEXITCODE)" -ForegroundColor Red
            exit 1
        }
        Write-Host "[PASS] $exe executed successfully!" -ForegroundColor Green
    }
}

# 1. Build Main Application
Build-And-Run "tms" @("src/vehicle_queue.cpp", "src/signal_controller.cpp", "src/simulation.cpp", "src/main.cpp") $false

# 2. Build and Run All Test Suites
Build-And-Run "test_vehicle_queue" @("src/vehicle_queue.cpp", "tests/test_vehicle_queue.cpp") $true
Build-And-Run "test_signal_controller" @("src/signal_controller.cpp", "src/vehicle_queue.cpp", "tests/test_signal_controller.cpp") $true
Build-And-Run "test_simulation" @("src/vehicle_queue.cpp", "src/signal_controller.cpp", "src/simulation.cpp", "tests/test_simulation.cpp") $true
Build-And-Run "test_emergency_preemption" @("src/vehicle_queue.cpp", "src/signal_controller.cpp", "src/simulation.cpp", "tests/test_emergency_preemption.cpp") $true
Build-And-Run "test_final_requirements" @("src/vehicle_queue.cpp", "src/signal_controller.cpp", "src/simulation.cpp", "tests/test_final_requirements.cpp") $true
Build-And-Run "test_signal_transition_safety" @("src/vehicle_queue.cpp", "src/signal_controller.cpp", "src/simulation.cpp", "tests/test_signal_transition_safety.cpp") $true
Build-And-Run "test_randomized_safety" @("src/vehicle_queue.cpp", "src/signal_controller.cpp", "src/simulation.cpp", "tests/test_randomized_safety.cpp") $true

Write-Host "`n=================================================" -ForegroundColor Green
Write-Host "   ALL BUILDS AND TESTS PASSED SUCCESSFULLY!     " -ForegroundColor Green
Write-Host "=================================================" -ForegroundColor Green
