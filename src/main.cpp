#include "config.h"
#include "simulation_engine.h"

#include <exception>
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
        if ((input >> value) && !(input >> extra) && value >= minimum && value <= maximum) {
            return value;
        }
        std::cout << "Enter a whole number from " << minimum << " to " << maximum << ".\n";
    }
}

void showMenu(const Config& config, const SimulationEngine& engine) {
    std::cout << "\n=== Traffic Management Simulation ===\n"
              << "Tick " << engine.getCurrentTick() << " / " << config.durationTicks
              << " | mode: " << (config.signalMode == SignalMode::Fixed ? "Fixed" : "Adaptive") << "\n"
              << "1. Run continuously to configured duration\n"
              << "2. Step a chosen number of ticks (1-100)\n"
              << "3. Quiet run to configured duration\n"
              << "4. Configure a new simulation\n"
              << "5. Reset current simulation\n"
              << "6. Export report.txt and events.csv\n"
              << "0. Exit\n";
}
}

int main() {
    Config config;
    std::unique_ptr<SimulationEngine> engine(new SimulationEngine(config));

    std::cout << "Traffic Management Simulation\n"
              << "Press 4 from the menu to change the default configuration.\n";

    while (true) {
        showMenu(config, *engine);
        const int choice = readInteger("Select an option: ", 0, 6);
        if (!std::cin && std::cin.eof()) {
            break;
        }

        try {
            switch (choice) {
                case 1:
                    engine->runConfiguredDuration(std::cout, false);
                    break;
                case 2: {
                    const int ticks = readInteger("How many ticks (1-100)? ", 1, 100);
                    if (std::cin.eof()) break;
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
                case 5:
                    engine->reset();
                    std::cout << "Simulation reset. The configured seed will replay the same arrivals.\n";
                    break;
                case 6: {
                    std::string reportError;
                    std::string csvError;
                    const bool reportWritten = engine->exportReport("report.txt", reportError);
                    const bool csvWritten = engine->exportEventsCsv("events.csv", csvError);
                    if (reportWritten) std::cout << "Saved report.txt\n";
                    else std::cout << "Report export failed: " << reportError << '\n';
                    if (csvWritten) std::cout << "Saved events.csv\n";
                    else std::cout << "Event export failed: " << csvError << '\n';
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
