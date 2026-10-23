
#include "config.h"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

static std::string runConfigurationTest(
    Config& config,
    const std::string& input
) {
    std::istringstream testInput(input);
    std::ostringstream testOutput;

    std::streambuf* originalInput = std::cin.rdbuf();
    std::streambuf* originalOutput = std::cout.rdbuf();

    std::cin.clear();
    std::cin.rdbuf(testInput.rdbuf());
    std::cout.rdbuf(testOutput.rdbuf());

    bool result = configureSimulation(config);

    std::cin.rdbuf(originalInput);
    std::cout.rdbuf(originalOutput);
    std::cin.clear();

    assert(result);
    return testOutput.str();
}

int main() {
    // Test 1: pressing Enter keeps the default configuration.
    {
        Config config;

        std::string input = "\n\n\n\n\n\n\n\n\n\n\n";
        std::string output = runConfigurationTest(config, input);

        assert(config.durationTicks == 600);
        assert(config.arrivalProbability == 0.35);
        assert(config.signalMode == SignalMode::Fixed);
        assert(config.queueCapacity == 100);
        assert(config.randomSeed == 42);
        assert(output.find("Configuration validated successfully")
               != std::string::npos);

        std::cout << "PASS: Default configuration\n";
    }

    // Test 2: invalid duration inputs are rejected.
    {
        Config config;

        std::string input =
            "abc\n"     // Not numeric
            "4000\n"    // Above the allowed maximum
            "3500\n"    // Valid duration
            "0.98\n"    // Arrival probability
            "1\n"       // Fixed mode
            "20\n"      // Fixed green
            "3\n"       // Yellow
            "1\n"       // All-red
            "5\n"       // Adaptive minimum
            "40\n"      // Adaptive maximum
            "150\n"     // Queue capacity
            "0.05\n"    // Emergency probability
            "42\n";     // Random seed

        std::string output = runConfigurationTest(config, input);

        assert(config.durationTicks == 3500);
        assert(config.arrivalProbability == 0.98);
        assert(config.queueCapacity == 150);
        assert(output.find("Invalid input") != std::string::npos);
        assert(output.find("Out of range") != std::string::npos);

        std::cout << "PASS: Invalid input and range validation\n";
    }

    // Test 3: adaptive maximum must be at least the minimum.
    {
        Config config;

        std::string input =
            "600\n"
            "0.35\n"
            "2\n"
            "20\n"
            "3\n"
            "1\n"
            "60\n"
            "40\n"      // Invalid: less than minimum green
            "80\n"      // Valid maximum green
            "100\n"
            "0.02\n"
            "42\n";

        std::string output = runConfigurationTest(config, input);

        assert(config.signalMode == SignalMode::Adaptive);
        assert(config.adaptiveMinGreen == 60);
        assert(config.adaptiveMaxGreen == 80);
        assert(output.find("Out of range") != std::string::npos);

        std::cout << "PASS: Adaptive green-time validation\n";
    }

    
    // Test 4: probability boundaries are accepted.
    {
        Config config;

        std::string input =
            "1\n"      // Duration
            "0\n"      // Arrival probability
            "1\n"      // Fixed signal mode
            "5\n"      // Fixed green
            "1\n"      // Yellow
            "1\n"      // All-red
            "5\n"      // Adaptive minimum green
            "5\n"      // Adaptive maximum green
            "1\n"      // Queue capacity
            "0\n"      // Emergency probability
            "0\n";     // Random seed

        runConfigurationTest(config, input);

        assert(config.durationTicks == 1);
        assert(config.arrivalProbability == 0.0);
        assert(config.emergencyProbability == 0.0);
        assert(config.queueCapacity == 1);
        assert(config.randomSeed == 0);

        std::cout << "PASS: Boundary values\n";
    }


    std::cout << "\nAll configuration tests passed.\n";
    return 0;
}
