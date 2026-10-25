# Traffic Management Simulation 🚦

## Overview

Traffic Management Simulation (TMS) is a C++11 console application that simulates vehicle movement through a four-way intersection. It manages queues, controls traffic signals, prioritizes emergency vehicles, displays statistics, and exports reports.

## Quick start

You need **g++ 9 or later**. No CMake is required for the commands below. Run them from the repository root.

### Windows (MinGW g++)

```powershell
g++ --version
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/main.cpp src/config.cpp src/event_logger.cpp src/signal_controller.cpp src/simulation_engine.cpp src/vehicle_generator.cpp src/vehicle_queue.cpp -o traffic_sim.exe
.\traffic_sim.exe
```

### Ubuntu

```sh
g++ --version
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/main.cpp src/config.cpp src/event_logger.cpp src/signal_controller.cpp src/simulation_engine.cpp src/vehicle_generator.cpp src/vehicle_queue.cpp -o traffic_sim
./traffic_sim
```

When the menu appears, choose **3. Quiet run to configured duration** to run the default simulation and see its summary immediately. You can reach the summary with one menu selection. Choose **4** first only if you want to change the configuration. Invalid menu entries show the accepted range; configuration prompts show the valid range and keep asking until the input is corrected.

Choose **6. Export report.txt and events.csv** after a run to save the summary and event history in the current working directory. The same configured random seed reproduces the same vehicle arrival and type sequence.

## Build and run the tests

On Windows, from PowerShell at the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\build_and_test.ps1
```

The script builds the current application and runs the unit-test executables with MinGW g++. GitHub Actions also builds the application on Ubuntu and Windows with MinGW and runs the critical module tests on Ubuntu.

## Project structure

- `src/` contains the implementations for configuration, event logging, signals, simulation, vehicle generation, and queue management.
- `include/` contains the public interfaces for those modules.
- `tests/` contains unit and regression tests for queues, signals, emergency handling, configuration, simulation behavior, and statistics summaries.

The project targets C++11 and uses standard C++ library facilities for console input/output and file export, so it can be built with g++ on Ubuntu and Windows MinGW.

## Project scope

The project focuses on a simulated four-way intersection. It does not use real-time traffic data, include pedestrian crossings, or control physical traffic signals.

## Team

- Patangi Naveen
- Pranav SP
- Rachana K
- Vedanta Tripathi
