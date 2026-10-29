# Traffic Management Simulation 🚦

## Overview
Traffic Management Simulation (TMS) is a C/C++ console-based project that simulates vehicle movement through a four-way traffic intersection. It uses queues and priority handling to manage traffic flow, control signal phases, and give emergency vehicles priority.

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

## Technologies Used
- C / C++
- Data Structures: Queues, Priority Queues
- Concepts: Simulation, Randomized Vehicle Generation, Traffic Signal Control

## Project Scope
The project focuses on a single four-way intersection with simulated traffic. It does not include real-time traffic data, pedestrian crossings, or control of physical traffic signals. It is developed to deal with the issue of all vehicles intersecting at the junction.

## Learning Outcomes
This project demonstrates the practical application of data structures, modular software design, simulation logic, and basic performance analysis.

## Team
Developed as a Software Engineering mini-project.

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
