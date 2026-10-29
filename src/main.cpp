#include "simulation.h"
#include "console_menu.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

void printUsage(const char* program) {
    std::cout
        << "Traffic Management Simulation (TMS) - Sprint 1\n"
        << "Usage: " << program << " [ticks] [adaptive|fixed] [--verbose]\n\n"
        << "       " << program << " --menu\n\n"
        << "Examples:\n"
        << "  " << program << "\n"
        << "  " << program << " 100 adaptive\n"
        << "  " << program << " 50 fixed --verbose\n\n"
        << "  " << program << " --menu\n\n"
        << "Default run executes a deterministic demonstration showing signal cycling,\n"
        << "queue management, emergency vehicle handling, and safety invariants.\n";
}

void printStateSnapshot(const TrafficSimulation& simulation) {
    const SignalController& sc = simulation.getSignalController();
    std::cout << "  Signal: " << SignalController::stateToString(sc.getState())
              << " (" << sc.getTicksRemaining() << " ticks left)"
              << " | N:" << SignalController::colorToString(sc.getLightColor(Direction::North))
              << " S:" << SignalController::colorToString(sc.getLightColor(Direction::South))
              << " E:" << SignalController::colorToString(sc.getLightColor(Direction::East))
              << " W:" << SignalController::colorToString(sc.getLightColor(Direction::West))
              << " | Queues: [N:" << simulation.getQueueSize(Direction::North)
              << " S:" << simulation.getQueueSize(Direction::South)
              << " E:" << simulation.getQueueSize(Direction::East)
              << " W:" << simulation.getQueueSize(Direction::West) << "]\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--menu") {
        return runConsoleMenu(std::cin, std::cout);
    }

    int ticks = 30; // 30 ticks covers full 6-state cycle for demo
    bool adaptive = true;
    bool verbose = false;
    bool isDefaultDemo = (argc == 1);

    if (argc > 1) {
        std::string arg1(argv[1]);
        if (arg1 == "-h" || arg1 == "--help") {
            printUsage(argv[0]);
            return 0;
        }

        char* end = nullptr;
        const long parsed = std::strtol(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0' || parsed < 0 || parsed > 1000000) {
            std::cerr << "Error: ticks must be an integer from 0 to 1000000.\n";
            printUsage(argv[0]);
            return 2;
        }
        ticks = static_cast<int>(parsed);
    }

    if (argc > 2) {
        const std::string mode(argv[2]);
        if (mode == "adaptive") {
            adaptive = true;
        } else if (mode == "fixed") {
            adaptive = false;
        } else {
            printUsage(argv[0]);
            return 2;
        }
    }

    if (argc > 3) {
        const std::string opt(argv[3]);
        if (opt == "--verbose" || opt == "-v") {
            verbose = true;
        }
    }

    if (isDefaultDemo) {
        verbose = true;
    }

    std::cout << "===============================================================\n"
              << "       TRAFFIC MANAGEMENT SIMULATION (TMS) - SPRINT 1          \n"
              << "===============================================================\n"
              << "Configuration:\n"
              << "  Mode:              " << (adaptive ? "Adaptive" : "Fixed") << "\n"
              << "  Cycle Duration:    " << ticks << " ticks\n"
              << "  Queue Capacity:    20 vehicles / approach\n"
              << "  Base Green:        10 ticks (Gmin=5, Gmax=30)\n"
              << "  Yellow Clearance:  2 ticks\n"
              << "  All-Red Interval:  1 tick\n"
              << "  Discharge Rate:    1 vehicle / tick (Green only)\n"
              << "===============================================================\n\n";

    SimulationConfig config(20, 10, 2, 1, ticks, adaptive, 5, 30, 1);
    TrafficSimulation simulation(config);

    // Initial state display
    std::cout << "[TICK 00] Initial Intersection State:\n";
    printStateSnapshot(simulation);
    std::cout << "\n--- Injecting Workload ---\n";

    // Inject representative vehicles across approaches
    simulation.addVehicle(Direction::North); // ID 1
    std::cout << "  [Arrival] Vehicle #1 (North)\n";
    simulation.addVehicle(Direction::North); // ID 2
    std::cout << "  [Arrival] Vehicle #2 (North)\n";
    simulation.addVehicle(Direction::South); // ID 3
    std::cout << "  [Arrival] Vehicle #3 (South)\n";
    simulation.addVehicle(Direction::East);  // ID 4
    std::cout << "  [Arrival] Vehicle #4 (East)\n";
    simulation.addVehicle(Direction::East);  // ID 5
    std::cout << "  [Arrival] Vehicle #5 (East)\n";
    simulation.addVehicle(Direction::West);  // ID 6
    std::cout << "  [Arrival] Vehicle #6 (West)\n";
    simulation.addVehicle(Direction::East, true); // ID 7 (Emergency)
    std::cout << "  [Arrival] EMERGENCY Vehicle #7 (East) - Priority Tagged\n";
    simulation.addVehicle(Direction::South, true); // ID 8 (Emergency)
    std::cout << "  [Arrival] EMERGENCY Vehicle #8 (South) - Priority Tagged\n\n";

    SignalState previousState = simulation.getSignalController().getState();

    std::cout << "--- Executing Simulation Progression ---\n";

    for (int i = 0; i < ticks; ++i) {
        int currentTick = simulation.getCurrentTick();

        // Dynamically inject additional arrival at tick 5 to demonstrate queue update
        if (currentTick == 5) {
            simulation.addVehicle(Direction::West);
            if (verbose) {
                std::cout << "[TICK 05] Mid-run arrival: Vehicle #9 (West)\n";
            }
        }

        simulation.step();

        // Safety invariant verification at every tick
        if (!simulation.checkVehicleCountInvariant() ||
            simulation.getSignalController().isSafetyViolationDetected()) {
            std::cerr << "FATAL: Simulation invariant check failed at tick " << currentTick << "!\n";
            return 1;
        }

        SignalState currentState = simulation.getSignalController().getState();

        if (verbose) {
            bool stateChanged = (currentState != previousState);
            if (stateChanged || currentTick == 0 || currentTick % 5 == 0) {
                std::cout << "[TICK " << std::setw(2) << std::setfill('0') << (currentTick + 1) << "] ";
                if (stateChanged) {
                    std::cout << "SIGNAL TRANSITION -> "
                              << SignalController::stateToString(currentState) << "\n";
                }
                printStateSnapshot(simulation);
            }
        }

        previousState = currentState;
    }

    const SimulationStats stats = simulation.getStats();

    std::cout << "\n===============================================================\n"
              << "                    SIMULATION SUMMARY REPORT                  \n"
              << "===============================================================\n"
              << "Final Signal State:        "
              << SignalController::stateToString(simulation.getSignalController().getState())
              << " (" << simulation.getSignalController().getTicksRemaining() << " ticks left in phase)\n"
              << "Elapsed Simulation Ticks:  " << stats.elapsedTicks << "\n"
              << "Generated Vehicles:        " << stats.generated << "\n"
              << "Served Vehicles:           " << stats.served << "\n"
              << "Blocked (Rejected) Count:  " << stats.blocked << "\n"
              << "Vehicles Still Waiting:    " << stats.waiting << "\n"
              << "Average Waiting Time:      " << std::fixed << std::setprecision(2)
              << stats.averageWaitingTime << " ticks (served vehicles)\n"
              << "Emergency Vehicles Served: " << stats.emergencyServed << "\n"
              << "Avg Emergency Wait Time:   " << stats.averageEmergencyWaitingTime << " ticks\n"
              << "---------------------------------------------------------------\n"
              << "INVARIANT CHECKS:\n"
              << "  Vehicle Count Invariant (generated = served + blocked + waiting): "
              << (simulation.checkVehicleCountInvariant() ? "[PASS]" : "[FAIL]") << "\n"
              << "  Signal Safety Invariant (no concurrent non-red cross signals):    [PASS]\n"
              << "===============================================================\n\n";

    if (verbose) {
        std::cout << "=== Detailed Event Log (" << simulation.getEventLog().size() << " events) ===\n";
        for (const std::string& event : simulation.getEventLog()) {
            std::cout << "  " << event << '\n';
        }
        std::cout << "===============================================================\n";
    }

    return simulation.checkVehicleCountInvariant() ? 0 : 1;
}
