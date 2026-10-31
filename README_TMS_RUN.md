# Running the Traffic Management Simulation

The current application is a C++11 console program. Use g++ 9 or later from the repository root; CMake is optional and is not needed for these commands.

## Windows with MinGW g++

```powershell
# Build the main simulation application
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/main.cpp src/config.cpp src/event_logger.cpp src/signal_controller.cpp src/simulation_engine.cpp src/vehicle_generator.cpp src/vehicle_queue.cpp src/simulation.cpp src/signal_comparison.cpp src/console_menu.cpp -o traffic_sim.exe
.\traffic_sim.exe
```

## Ubuntu

```sh
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/main.cpp src/config.cpp src/event_logger.cpp src/signal_controller.cpp src/simulation_engine.cpp src/vehicle_generator.cpp src/vehicle_queue.cpp src/simulation.cpp src/signal_comparison.cpp src/console_menu.cpp -o traffic_sim
./traffic_sim
```

With no arguments, the configured `SimulationEngine` menu opens. Select **3** to run quietly to the configured duration and display the summary. This is the fastest first-run path and requires one menu selection. Select **4** to change settings; each setting displays its valid range, and invalid values are rejected with a correction prompt.

Select **5** to compare Fixed and Adaptive signal control on one shared arrival schedule. After a run, select **7** to write `report.txt` and `events.csv` into the current working directory.

## Run

```powershell
# Configured simulation menu (default)
.\traffic_sim.exe

# Sprint 1 demonstration (initial state, arrivals, phase transitions, summary)
.\traffic_sim.exe --demo

# Run 200 ticks in adaptive mode
.\traffic_sim.exe 200 adaptive

# Run 200 ticks in fixed mode
.\traffic_sim.exe 200 fixed

# Run with verbose tick logging
.\traffic_sim.exe 50 fixed --verbose

# Standalone Fixed vs Adaptive comparison menu (option 5)
.\traffic_sim.exe --menu
```

The comparison asks for a duration and seed (or uses the configured seed from the main menu), generates one deterministic arrival schedule, and runs separate Fixed and Adaptive simulations against that same schedule.

## Windows build and tests

Run this from PowerShell at the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\build_and_test.ps1
```

It builds the application, checks that menu option 3 displays the summary, and builds/runs all test suites. Generated `.exe` files and exported reports are ignored by Git.

## Build and Run Tests

Each test executable can be built and run independently:

```powershell
# 1. Vehicle Queue Unit Tests
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/vehicle_queue.cpp tests/test_vehicle_queue.cpp -o test_vehicle_queue.exe
.\test_vehicle_queue.exe

# 2. Signal Controller Unit Tests
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/signal_controller.cpp src/vehicle_queue.cpp tests/test_signal_controller.cpp -o test_signal_controller.exe
.\test_signal_controller.exe

# 3. Traffic Simulation Unit Tests
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/vehicle_queue.cpp src/signal_controller.cpp src/simulation.cpp tests/test_simulation.cpp -o test_simulation.exe
.\test_simulation.exe

# 4. Emergency Pre-emption Tests
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/vehicle_queue.cpp src/signal_controller.cpp src/simulation.cpp tests/test_emergency_preemption.cpp -o test_emergency_preemption.exe
.\test_emergency_preemption.exe

# 5. Final Requirements Regression Tests
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/vehicle_queue.cpp src/signal_controller.cpp src/simulation.cpp tests/test_final_requirements.cpp -o test_final_requirements.exe
.\test_final_requirements.exe

# 6. Signal Transition Safety Tests
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/vehicle_queue.cpp src/signal_controller.cpp src/simulation.cpp tests/test_signal_transition_safety.cpp -o test_signal_transition_safety.exe
.\test_signal_transition_safety.exe

# 7. Randomized Signal Safety Tests (100 Seeds, 500 Ticks Each)
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/vehicle_queue.cpp src/signal_controller.cpp src/simulation.cpp tests/test_randomized_safety.cpp -o test_randomized_safety.exe
.\test_randomized_safety.exe

# 8. Configuration Tests
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/config.cpp tests/test_config.cpp -o test_config.exe
.\test_config.exe

# 9. Emergency Handler Tests
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/emergency_handler.cpp src/signal_controller.cpp src/vehicle_queue.cpp tests/test_emergency_handler.cpp -o test_emergency_handler.exe
.\test_emergency_handler.exe

# 10. Simulation Engine Tests
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/config.cpp src/event_logger.cpp src/signal_controller.cpp src/simulation_engine.cpp src/vehicle_generator.cpp src/vehicle_queue.cpp tests/test_simulation_engine.cpp -o test_simulation_engine.exe
.\test_simulation_engine.exe

# 11. Simulation Analytics Tests
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/vehicle_queue.cpp src/signal_controller.cpp src/simulation.cpp tests/test_simulation_analytics.cpp -o test_simulation_analytics.exe
.\test_simulation_analytics.exe

# 12. Signal Control Comparison Tests
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/vehicle_queue.cpp src/signal_controller.cpp src/simulation.cpp src/signal_comparison.cpp src/console_menu.cpp tests/test_signal_control_comparison.cpp -o test_signal_control_comparison.exe
.\test_signal_control_comparison.exe
```

## Invariants Verified

1. **Safety Invariant:** Opposing approaches (North-South vs East-West) never simultaneously show non-red (green/yellow) lights.
2. **Clearance Invariant:** Every green-to-red change undergoes configured yellow clearance and fixed 1-tick all-red clearance.
3. **Vehicle Accounting Invariant:** `generatedCount == servedCount + blockedCount + waitingCount` at all ticks.
4. **Memory Safety:** Every dynamic vehicle allocation is accounted for: enqueued vehicles are owned by `VehicleQueue`, rejected vehicles are immediately freed, and served vehicles are freed after metrics collection.
