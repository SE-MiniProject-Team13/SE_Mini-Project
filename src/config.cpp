
#include "config.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>

template <typename T>
bool parseValue(const std::string& input, T& value) {
    std::istringstream stream(input);

    if (!(stream >> value)) {
        return false;
    }

    stream >> std::ws;

    if (!stream.eof()) {
        return false;
    }

    return std::isfinite(static_cast<double>(value));
}

template <typename T>
T readValue(
    const std::string& prompt,
    T defaultValue,
    T minimum,
    T maximum
) {
    // Keep the default valid for the current allowed range.
    if (defaultValue < minimum) defaultValue = minimum;
    if (defaultValue > maximum) defaultValue = maximum;

    std::string input;

    while (true) {
        std::cout << prompt << " [" << defaultValue
                  << "] (range " << minimum << "-" << maximum
                  << "): ";

        if (!std::getline(std::cin, input)) {
            std::cout << "\nInput ended. Using default.\n";
            return defaultValue;
        }

        if (input.empty()) {
            return defaultValue;
        }

        T value{};

        if (!parseValue(input, value)) {
            std::cout << "Invalid input. Enter a numeric value.\n";
            continue;
        }

        if (value < minimum || value > maximum) {
            std::cout << "Out of range. Allowed: "
                      << minimum << "-" << maximum << ".\n";
            continue;
        }

        return value;
    }
}

Config readConfiguration(const Config& current) {
    Config config = current;

    std::cout << "\n=== Simulation Configuration ===\n";
    std::cout << "Press Enter to keep each default.\n\n";

    config.durationTicks = readValue<int>(
        "Simulation duration in ticks",
        config.durationTicks, 1, 3600
    );

    config.arrivalProbability = readValue<double>(
        "Vehicle arrival probability",
        config.arrivalProbability, 0.0, 1.0
    );

    int mode = readValue<int>(
        "Signal mode (1 = Fixed, 2 = Adaptive)",
        config.signalMode == SignalMode::Fixed ? 1 : 2,
        1, 2
    );

    config.signalMode =
        mode == 1 ? SignalMode::Fixed : SignalMode::Adaptive;

    config.fixedGreenTicks = readValue<int>(
        "Fixed-mode green duration",
        config.fixedGreenTicks, 5, 120
    );

    config.yellowTicks = readValue<int>(
        "Yellow duration",
        config.yellowTicks, 1, 10
    );

    config.allRedTicks = readValue<int>(
        "All-red duration",
        config.allRedTicks, 1, 5
    );

    config.adaptiveMinGreen = readValue<int>(
        "Adaptive minimum green duration",
        config.adaptiveMinGreen, 5, 60
    );

    config.adaptiveMaxGreen = readValue<int>(
        "Adaptive maximum green duration",
        config.adaptiveMaxGreen, config.adaptiveMinGreen, 120
    );

    config.queueCapacity = readValue<int>(
        "Queue capacity per approach",
        config.queueCapacity, 1, 1000
    );

    config.emergencyProbability = readValue<double>(
        "Emergency vehicle probability",
        config.emergencyProbability, 0.0, 1.0
    );

    long long seed = readValue<long long>(
        "Random seed",
        static_cast<long long>(config.randomSeed),
        0LL, 2147483647LL
    );

    config.randomSeed = static_cast<unsigned int>(seed);

    return config;
}

bool configureSimulation(Config& config) {
    Config candidate = readConfiguration(config);

    // Commit settings only after all prompts finish.
    config = candidate;

    std::cout << "\nConfiguration validated successfully.\n";
    displayConfig(config);

    return true;
}

void displayConfig(const Config& config) {
    std::cout << "\n=== ACTIVE CONFIGURATION ===\n";
    std::cout << "Duration: " << config.durationTicks << " ticks\n";
    std::cout << "Arrival probability: "
              << config.arrivalProbability << '\n';
    std::cout << "Signal mode: "
              << (config.signalMode == SignalMode::Fixed
                      ? "Fixed" : "Adaptive") << '\n';
    std::cout << "Fixed green: " << config.fixedGreenTicks << " ticks\n";
    std::cout << "Yellow: " << config.yellowTicks << " ticks\n";
    std::cout << "All-red: " << config.allRedTicks << " ticks\n";
    std::cout << "Adaptive minimum green: "
              << config.adaptiveMinGreen << " ticks\n";
    std::cout << "Adaptive maximum green: "
              << config.adaptiveMaxGreen << " ticks\n";
    std::cout << "Queue capacity: " << config.queueCapacity << '\n';
    std::cout << "Emergency probability: "
              << config.emergencyProbability << '\n';
    std::cout << "Random seed: " << config.randomSeed << '\n';
    std::cout << "============================\n";
}
