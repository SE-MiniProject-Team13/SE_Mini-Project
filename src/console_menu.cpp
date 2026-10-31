#include "console_menu.h"

#include "signal_comparison.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool parseChoice(const std::string& line, int& choice) {
    std::istringstream input(line);
    char extra = '\0';
    if (!(input >> choice) || (input >> extra)) {
        return false;
    }
    return true;
}

bool readBoundedInteger(
    std::istream& input,
    std::ostream& output,
    const char* prompt,
    int minimum,
    int maximum,
    int& value
) {
    output << prompt;
    std::string line;
    if (!std::getline(input, line)) {
        return false;
    }
    std::istringstream parser(line);
    char extra = '\0';
    if (!(parser >> value) || (parser >> extra) ||
        value < minimum || value > maximum) {
        output << "Invalid input. Enter a whole number from "
               << minimum << " to " << maximum << ".\n";
        return false;
    }
    return true;
}

SimulationConfig menuConfig(int ticks) {
    return SimulationConfig(20, 10, 2, 1, ticks, false, 5, 30, 1);
}

void runSingleMode(
    int ticks,
    SignalMode mode,
    int seed,
    std::ostream& output
) {
    const SimulationConfig config = menuConfig(ticks);
    const std::vector<ScheduledArrival> schedule =
        generateArrivalSchedule(ticks, seed);
    const SignalModeRun result =
        runScheduledSimulation(config, mode, schedule);
    printSingleRun(result, output);
}

} // namespace

int runConsoleMenu(std::istream& input, std::ostream& output) {
    const int defaultSeed = 42;
    bool running = true;

    while (running) {
        output << "\nTraffic Management Simulation\n"
               << "1. Run Fixed signal control (100 ticks)\n"
               << "2. Run Adaptive signal control (100 ticks)\n"
               << "3. Run Fixed signal control (300 ticks)\n"
               << "4. Run Adaptive signal control (300 ticks)\n"
               << "5. Compare Fixed vs Adaptive Signal Control\n"
               << "0. Exit\n"
               << "Select an option: ";

        std::string line;
        if (!std::getline(input, line)) {
            break;
        }

        int choice = -1;
        if (!parseChoice(line, choice)) {
            output << "Invalid menu input. Enter an option number.\n";
            continue;
        }

        switch (choice) {
            case 1:
                runSingleMode(100, SignalMode::Fixed, defaultSeed, output);
                break;
            case 2:
                runSingleMode(100, SignalMode::Adaptive, defaultSeed, output);
                break;
            case 3:
                runSingleMode(300, SignalMode::Fixed, defaultSeed, output);
                break;
            case 4:
                runSingleMode(300, SignalMode::Adaptive, defaultSeed, output);
                break;
            case 5: {
                int ticks = 0;
                int seed = 0;
                if (!readBoundedInteger(
                        input, output, "Simulation duration (0-1000 ticks): ",
                        0, 1000, ticks)) {
                    if (!input) {
                        running = false;
                    }
                    break;
                }
                if (!readBoundedInteger(
                        input, output, "Random seed (0-2147483647): ",
                        0, 2147483647, seed)) {
                    if (!input) {
                        running = false;
                    }
                    break;
                }
                const SignalControlComparison comparison =
                    compareSignalModes(menuConfig(ticks), seed);
                printComparison(comparison, output);
                break;
            }
            case 0:
                running = false;
                break;
            default:
                output << "Unknown option. Choose 0 through 5.\n";
                break;
        }
    }
    return 0;
}
