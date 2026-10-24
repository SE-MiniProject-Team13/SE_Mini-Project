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

## Technologies Used
- C / C++
- Data Structures: Queues, Priority Queues
- Concepts: Simulation, Randomized Vehicle Generation, Traffic Signal Control

## Build and Run

The project requires a C++11 compiler. With CMake installed:

```sh
cmake -S . -B build
cmake --build build
./build/traffic_sim
```

On Windows, run `build\\traffic_sim.exe` after building. The console menu supports
continuous execution, stepping by 1-100 ticks, quiet execution with a final summary,
configuration, and reset. The same configured random seed reproduces the same vehicle
arrival and type sequence.

The manual compiler command is:

```sh
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/main.cpp src/config.cpp src/event_logger.cpp src/signal_controller.cpp src/simulation_engine.cpp src/vehicle_generator.cpp src/vehicle_queue.cpp -o traffic_sim
```

Choose **Export report.txt and events.csv** from the menu after a run to save the
statistics summary and tick-by-tick event history in the current working directory.

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
