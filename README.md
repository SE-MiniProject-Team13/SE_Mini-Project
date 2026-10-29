# Traffic Management Simulation 🚦

## Overview

## Features
- **Vehicle Generation:** Simulates random vehicle arrivals from four directions.
- **Queue Management:** Manages waiting vehicles using FIFO queues and priority handling.
- **Traffic Signal Control:** Supports Fixed and Adaptive signal timing.
- **Emergency Vehicle Priority:** Provides emergency vehicles with priority through safe signal pre-emption.
- **Simulation Modes:** Supports continuous and step-by-step execution.
- **Statistics and Reports:** Tracks waiting time, queue lengths, throughput, and other traffic statistics.
- **Performance Comparison:** Compares Fixed and Adaptive signal control strategies.
- **Data Export:** Generates `report.txt` and `events.csv` for results and event logs.
- **Analytics Dashboard:** Runs the C++ simulation through a local API and visualizes
  recorded queues, signals, vehicle outcomes, waiting times, and emergency events.
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

---

*Traffic Management Simulation — exploring efficient traffic flow through software simulation.*

## Sprint 2 Analytics Dashboard

The web dashboard runs the existing C++ simulation engine; it does not simulate traffic
in the browser. The analytics executable records per-tick queue and signal snapshots and
vehicle events, and the dashboard derives its charts from those records.

### Requirements

- Windows with MinGW `g++` available on `PATH`
- Node.js 20 or newer and npm

### Install and start

From the repository root, install the frontend dependencies and launch the API and web
development server:

```powershell
npm install
npm run dev
```

Open [http://127.0.0.1:5173](http://127.0.0.1:5173). The backend compiles the separate
analytics executable into the operating system's temporary directory on the first run;
the Sprint 1 console executable and build workflow remain unchanged.

The dashboard offers fixed or adaptive timing, a reproducible random seed, and runs of
0–1000 ticks. In each tick, the seeded workload generates no arrival 45% of the time,
one arrival 45% of the time, and two arrivals 10% of the time. Each vehicle's approach
is selected uniformly and each has a 10% emergency chance.

Waiting time is departure tick minus arrival tick. Average and maximum waiting-time
metrics cover served vehicles only; emergency waiting metrics cover served emergency
vehicles only. The queue maximum is the highest observed size of any single approach
queue. Throughput is served vehicles divided by elapsed ticks. The time series contains
the initial state at tick 0 and a recorded state after each processed tick; events are
tagged with the simulation tick at which they occur. A pre-emption is counted only when
an opposing emergency arrival actually shortens the active green phase. Undefined
browser metrics (no served vehicles or no elapsed ticks) display as N/A.

### Build and tests

```powershell
# Existing C++ Sprint 1 regression suite plus focused analytics instrumentation test
powershell -ExecutionPolicy Bypass -File .\build_and_test.ps1

# Web production build and backend API tests
npm test
npm run build
```

The C++ tests are built into ignored `.exe` files at the repository root by the existing
Sprint 1 script. The Node API test suite exercises valid, invalid, and failed simulation
requests; the browser dashboard is served by Vite during development.
