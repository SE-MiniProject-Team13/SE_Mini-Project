# Build the current TMS application and run its C++11 test executables with MinGW g++.
$ErrorActionPreference = "Stop"

if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
    throw "g++ was not found. Install MinGW-w64 g++ 9 or later and add its bin folder to PATH."
}

$CXXFLAGS = @("-std=c++11", "-Wall", "-Wextra", "-pedantic", "-Iinclude")
$buildDirectory = Join-Path (Get-Location) ".codex-build"
New-Item -ItemType Directory -Path $buildDirectory -Force | Out-Null

function Build-And-Run([string]$name, [string[]]$sources, [bool]$run = $true) {
    $exe = Join-Path $buildDirectory "$name.exe"
    Write-Host "`n[BUILDING] $exe..." -ForegroundColor Yellow
    $arguments = $CXXFLAGS + $sources + @("-o", $exe)
    & g++ @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Compilation failed for $exe (exit code $LASTEXITCODE)."
    }
    Write-Host "[SUCCESS] Built $exe" -ForegroundColor Green

    if ($run) {
        Write-Host "[RUNNING] $exe..." -ForegroundColor Cyan
        & $exe
        if ($LASTEXITCODE -ne 0) {
            throw "Execution failed for $exe (exit code $LASTEXITCODE)."
        }
        Write-Host "[PASS] $exe" -ForegroundColor Green
    }
}

$applicationSources = @(
    "src/main.cpp", "src/config.cpp", "src/event_logger.cpp",
    "src/signal_controller.cpp", "src/simulation_engine.cpp",
    "src/vehicle_generator.cpp", "src/vehicle_queue.cpp"
)
Build-And-Run "traffic_sim" $applicationSources $false

# Verify the short path to a summary: one menu choice, then exit.
$smokeDirectory = Join-Path $env:TEMP ("tms-smoke-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $smokeDirectory | Out-Null
$inputFile = Join-Path $smokeDirectory "input.txt"
$outputFile = Join-Path $smokeDirectory "output.txt"
$errorFile = Join-Path $smokeDirectory "error.txt"
try {
    Set-Content -LiteralPath $inputFile -Value "3`n0`n" -NoNewline -Encoding ascii
    $applicationPath = Join-Path $buildDirectory "traffic_sim.exe"
    $process = Start-Process -FilePath $applicationPath -Wait -PassThru -NoNewWindow `
        -RedirectStandardInput $inputFile -RedirectStandardOutput $outputFile `
        -RedirectStandardError $errorFile
    if ($process.ExitCode -ne 0) {
        throw "The menu smoke run failed (exit code $($process.ExitCode))."
    }
    $smokeOutput = Get-Content -LiteralPath $outputFile -Raw
    if ($smokeOutput -notmatch "=== Simulation Summary ===") {
        throw "Choosing menu option 3 did not display a simulation summary."
    }
    Write-Host "[PASS] One menu choice reaches the simulation summary." -ForegroundColor Green
}
finally {
    Remove-Item -LiteralPath $inputFile, $outputFile, $errorFile -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $smokeDirectory -Force -ErrorAction SilentlyContinue
}

# Current application statistics and error behavior.
Build-And-Run "test_simulation_engine" @(
    "src/config.cpp", "src/event_logger.cpp", "src/signal_controller.cpp",
    "src/simulation_engine.cpp", "src/vehicle_generator.cpp",
    "src/vehicle_queue.cpp", "tests/test_simulation_engine.cpp"
)

# Critical module unit tests.
Build-And-Run "test_vehicle_queue" @("src/vehicle_queue.cpp", "tests/test_vehicle_queue.cpp")
Build-And-Run "test_signal_controller" @("src/signal_controller.cpp", "src/vehicle_queue.cpp", "tests/test_signal_controller.cpp")
Build-And-Run "test_config" @("src/config.cpp", "tests/test_config.cpp")
Build-And-Run "test_emergency_handler" @("src/emergency_handler.cpp", "src/signal_controller.cpp", "src/vehicle_queue.cpp", "tests/test_emergency_handler.cpp")

# Existing simulation regression and safety suites.
Build-And-Run "test_simulation" @("src/vehicle_queue.cpp", "src/signal_controller.cpp", "src/simulation.cpp", "tests/test_simulation.cpp")
Build-And-Run "test_emergency_preemption" @("src/vehicle_queue.cpp", "src/signal_controller.cpp", "src/simulation.cpp", "tests/test_emergency_preemption.cpp")
Build-And-Run "test_final_requirements" @("src/vehicle_queue.cpp", "src/signal_controller.cpp", "src/simulation.cpp", "tests/test_final_requirements.cpp")
Build-And-Run "test_signal_transition_safety" @("src/vehicle_queue.cpp", "src/signal_controller.cpp", "src/simulation.cpp", "tests/test_signal_transition_safety.cpp")
Build-And-Run "test_randomized_safety" @("src/vehicle_queue.cpp", "src/signal_controller.cpp", "src/simulation.cpp", "tests/test_randomized_safety.cpp")

# Performance and efficiency tests (NFR-TMS-01 / TC-48 to TC-51)
Build-And-Run "test_performance" @(
    "src/config.cpp", "src/event_logger.cpp", "src/signal_controller.cpp",
    "src/simulation_engine.cpp", "src/vehicle_generator.cpp",
    "src/vehicle_queue.cpp", "tests/test_performance.cpp"
)

Write-Host "`nAll application, usability, performance, and unit-test checks passed." -ForegroundColor Green
