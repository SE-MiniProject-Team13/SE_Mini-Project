# Running the Traffic Management Simulation

The current application is a C++11 console program. Use g++ 9 or later from the repository root; CMake is optional and is not needed for these commands.

## Windows with MinGW g++

```powershell
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/main.cpp src/config.cpp src/event_logger.cpp src/signal_controller.cpp src/simulation_engine.cpp src/vehicle_generator.cpp src/vehicle_queue.cpp -o traffic_sim.exe
.\traffic_sim.exe
```

## Ubuntu

```sh
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/main.cpp src/config.cpp src/event_logger.cpp src/signal_controller.cpp src/simulation_engine.cpp src/vehicle_generator.cpp src/vehicle_queue.cpp -o traffic_sim
./traffic_sim
```

At the menu, select **3** to run quietly to the configured duration and display the summary. This is the fastest first-run path and requires one menu selection. Select **4** to change settings; each setting displays its valid range, and invalid values are rejected with a correction prompt.

After a run, select **6** to write `report.txt` and `events.csv` into the current working directory.

## Windows build and tests

Run this from PowerShell at the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\build_and_test.ps1
```

It builds the application, checks that menu option 3 displays the summary, and builds/runs all test suites. Generated `.exe` files and exported reports are ignored by Git.
