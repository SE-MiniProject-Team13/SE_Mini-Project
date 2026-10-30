#include "config.h"
#include "console_menu.h"
#include "signal_comparison.h"
#include "simulation.h"
#include "simulation_engine.h"

#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace {

int readInteger(const std::string& prompt, int minimum, int maximum) {
    std::string line;
    while (true) {
        std::cout << prompt;
        if (!std::getline(std::cin, line)) {
            return 0;
        }
        std::istringstream input(line);
        int value = 0;
        char extra = '\0';
        if ((input >> value) && !(input >> extra) &&
            value >= minimum && value <= maximum) {
            return value;
        }
        std::cout << "Enter a whole number from " << minimum
                  << " to " << maximum << ".\n";
    }
}

void showMenu(const Config& config, const SimulationEngine& engine) {
    std::cout << "\n=== Traffic Management Simulation ===\n"
              << "Tick " << engine.getCurrentTick() << " / " << config.durationTicks
              << " | mode: "
              << (config.signalMode == SignalMode::Fixed ? "Fixed" : "Adaptive")
              << "\n"
              << "1. Run continuously to configured duration\n"
              << "2. Step a chosen number of ticks (1-100)\n"
              << "3. Quiet run to configured duration\n"
              << "4. Configure a new simulation\n"
              << "5. Compare Fixed vs Adaptive Signal Control\n"
              << "6. Reset current simulation\n"
              << "7. Export report.txt and events.csv\n"
              << "0. Exit\n";
}

int runConfiguredMenu() {
    Config config;
    std::unique_ptr<SimulationEngine> engine(new SimulationEngine(config));

    std::cout << "Traffic Management Simulation\n"
              << "Press 4 from the menu to change the default configuration.\n";

    while (true) {
        showMenu(config, *engine);
        const int choice = readInteger("Select an option: ", 0, 7);
        if (!std::cin && std::cin.eof()) {
            break;
        }

        try {
            switch (choice) {
                case 1:
                    engine->runConfiguredDuration(std::cout, false);
                    break;
                case 2: {
                    const int ticks = readInteger(
                        "How many ticks (1-100)? ", 1, 100
                    );
                    if (std::cin.eof()) {
                        break;
                    }
                    engine->advanceTicks(ticks, std::cout, false);
                    break;
                }
                case 3:
                    engine->runConfiguredDuration(std::cout, true);
                    break;
                case 4:
                    configureSimulation(config);
                    engine.reset(new SimulationEngine(config));
                    std::cout << "New configuration applied; simulation restarted at tick 0.\n";
                    break;
                case 5: {
                    const SimulationConfig comparisonConfig(
                        static_cast<std::size_t>(config.queueCapacity),
                        config.fixedGreenTicks,
                        config.yellowTicks,
                        config.allRedTicks,
                        config.durationTicks,
                        false,
                        config.adaptiveMinGreen,
                        config.adaptiveMaxGreen,
                        1
                    );
                    const SignalControlComparison comparison = compareSignalModes(
                        comparisonConfig,
                        static_cast<int>(config.randomSeed),
                        config.arrivalProbability,
                        config.emergencyProbability
                    );
                    printComparison(comparison, std::cout);
                    break;
                }
                case 6:
                    engine->reset();
                    std::cout << "Simulation reset. The configured seed will replay the same arrivals.\n";
                    break;
                case 7: {
                    std::string reportError;
                    std::string csvError;
                    const bool reportWritten =
                        engine->exportReport("report.txt", reportError);
                    const bool csvWritten =
                        engine->exportEventsCsv("events.csv", csvError);
                    if (reportWritten) {
                        std::cout << "Saved report.txt\n";
                    } else {
                        std::cout << "Report export failed: " << reportError << '\n';
                    }
                    if (csvWritten) {
                        std::cout << "Saved events.csv\n";
                    } else {
                        std::cout << "Event export failed: " << csvError << '\n';
                    }
                    break;
                }
                case 0:
                    std::cout << "Goodbye.\n";
                    return 0;
            }
        } catch (const std::exception& error) {
            std::cout << "Simulation error: " << error.what() << '\n';
        }
    }
    return 0;
}

void printUsage(const char* program) {
    std::cout
        << "Traffic Management Simulation\n"
        << "Usage:\n"
        << "  " << program << "                         Open the configured simulation menu\n"
        << "  " << program << " --menu                  Open the Fixed/Adaptive menu\n"
        << "  " << program << " --demo                  Run the Sprint 1 demonstration\n"
        << "  " << program << " [ticks] [adaptive|fixed] [--verbose]\n";
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

int runLegacyCli(int argc, char* argv[]) {
    int ticks = 30;
    bool adaptive = true;
    bool verbose = false;
    const bool isDefaultDemo = argc == 1;

    if (argc > 1) {
        const std::string arg1(argv[1]);
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
        const std::string option(argv[3]);
        if (option == "--verbose" || option == "-v") {
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

    std::cout << "[TICK 00] Initial Intersection State:\n";
    printStateSnapshot(simulation);
    std::cout << "\n--- Injecting Workload ---\n";
    simulation.addVehicle(Direction::North);
    std::cout << "  [Arrival] Vehicle #1 (North)\n";
    simulation.addVehicle(Direction::North);
    std::cout << "  [Arrival] Vehicle #2 (North)\n";
    simulation.addVehicle(Direction::South);
    std::cout << "  [Arrival] Vehicle #3 (South)\n";
    simulation.addVehicle(Direction::East);
    std::cout << "  [Arrival] Vehicle #4 (East)\n";
    simulation.addVehicle(Direction::East);
    std::cout << "  [Arrival] Vehicle #5 (East)\n";
    simulation.addVehicle(Direction::West);
    std::cout << "  [Arrival] Vehicle #6 (West)\n";
    simulation.addVehicle(Direction::East, true);
    std::cout << "  [Arrival] EMERGENCY Vehicle #7 (East) - Priority Tagged\n";
    simulation.addVehicle(Direction::South, true);
    std::cout << "  [Arrival] EMERGENCY Vehicle #8 (South) - Priority Tagged\n\n";

    SignalState previousState = simulation.getSignalController().getState();
    std::cout << "--- Executing Simulation Progression ---\n";
    for (int i = 0; i < ticks; ++i) {
        const int currentTick = simulation.getCurrentTick();
        if (currentTick == 5) {
            simulation.addVehicle(Direction::West);
            if (verbose) {
                std::cout << "[TICK 05] Mid-run arrival: Vehicle #9 (West)\n";
            }
        }

        simulation.step();
        if (!simulation.checkVehicleCountInvariant() ||
            simulation.getSignalController().isSafetyViolationDetected()) {
            std::cerr << "FATAL: Simulation invariant check failed at tick "
                      << currentTick << "!\n";
            return 1;
        }

        const SignalState currentState = simulation.getSignalController().getState();
        if (verbose) {
            const bool stateChanged = currentState != previousState;
            if (stateChanged || currentTick == 0 || currentTick % 5 == 0) {
                std::cout << "[TICK " << std::setw(2) << std::setfill('0')
                          << currentTick + 1 << "] ";
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
              << " (" << simulation.getSignalController().getTicksRemaining()
              << " ticks left in phase)\n"
              << "Elapsed Simulation Ticks:  " << stats.elapsedTicks << "\n"
              << "Generated Vehicles:        " << stats.generated << "\n"
              << "Served Vehicles:           " << stats.served << "\n"
              << "Blocked (Rejected) Count:  " << stats.blocked << "\n"
              << "Vehicles Still Waiting:    " << stats.waiting << "\n"
              << "Average Waiting Time:      " << std::fixed << std::setprecision(2)
              << stats.averageWaitingTime << " ticks (served vehicles)\n"
              << "Emergency Vehicles Served: " << stats.emergencyServed << "\n"
              << "Avg Emergency Wait Time:   "
              << stats.averageEmergencyWaitingTime << " ticks\n"
              << "---------------------------------------------------------------\n"
              << "INVARIANT CHECKS:\n"
              << "  Vehicle Count Invariant (generated = served + blocked + waiting): "
              << (simulation.checkVehicleCountInvariant() ? "[PASS]" : "[FAIL]") << "\n"
              << "  Signal Safety Invariant (no concurrent non-red cross signals):    [PASS]\n"
              << "===============================================================\n\n";

    if (verbose) {
        std::cout << "=== Detailed Event Log ("
                  << simulation.getEventLog().size() << " events) ===\n";
        const std::vector<std::string>& events = simulation.getEventLog();
        for (std::size_t i = 0; i < events.size(); ++i) {
            std::cout << "  " << events[i] << '\n';
        }
        std::cout << "===============================================================\n";
    }
    return simulation.checkVehicleCountInvariant() ? 0 : 1;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc == 1) {
        return runConfiguredMenu();
    }
    if (argc == 2 && std::string(argv[1]) == "--menu") {
        return runConsoleMenu(std::cin, std::cout);
    }
    if (argc == 2 && std::string(argv[1]) == "--demo") {
        char* demoArguments[] = {argv[0], nullptr};
        return runLegacyCli(1, demoArguments);
    }
    return runLegacyCli(argc, argv);
}
