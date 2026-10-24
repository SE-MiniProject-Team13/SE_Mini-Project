# Traffic Management Simulation (TMS) - Sprint 1

C++11 console simulation of a four-approach intersection. The implementation includes:
- Six-state signal cycle: NS Green -> NS Yellow -> All-Red 1 -> EW Green -> EW Yellow -> All-Red 2 -> NS Green.
- Bounded queues for each approach, with emergency vehicles prioritized within each queue.
- Fixed or adaptive green duration (FR-21, FR-22).
- Non-conflicting signal safety invariant (FR-20, BR-01, NFR-05).
- Emergency pre-emption hooks and logging (`EMERGENCY_ARRIVAL`, `EMERGENCY_START`, `EMERGENCY_END`, `VEHICLE_DEPARTURE`).
- Vehicle-count invariant check (`generated == served + blocked + waiting`).
- Deterministic 100-seed randomized signal safety test suite.

## Build (MinGW g++ on Windows)

Run these commands from the repository root in PowerShell:

```powershell
# Build the main simulation application
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/vehicle_queue.cpp src/signal_controller.cpp src/simulation.cpp src/main.cpp -o tms.exe
```

Alternatively, use the automated build and test script:
```powershell
powershell -ExecutionPolicy Bypass -File .\build_and_test.ps1
```

## Run

```powershell
# 1-Minute Demonstration mode (shows initial state, arrivals, phase transitions, departures, summary)
.\tms.exe

# Run 200 ticks in adaptive mode
.\tms.exe 200 adaptive

# Run 200 ticks in fixed mode
.\tms.exe 200 fixed

# Run with verbose tick logging
.\tms.exe 50 fixed --verbose
```

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
```

## Invariants Verified

1. **Safety Invariant:** Opposing approaches (North-South vs East-West) never simultaneously show non-red (green/yellow) lights.
2. **Clearance Invariant:** Every green-to-red change undergoes configured yellow clearance and fixed 1-tick all-red clearance.
3. **Vehicle Accounting Invariant:** `generatedCount == servedCount + blockedCount + waitingCount` at all ticks.
4. **Memory Safety:** Every dynamic vehicle allocation is accounted for: enqueued vehicles are owned by `VehicleQueue`, rejected vehicles are immediately freed, and served vehicles are freed after metrics collection.
