#include "simulation_engine.h"

#include <cassert>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
void testQuietRunPrintsReadableSummary() {
    Config config;
    config.durationTicks = 2;
    config.arrivalProbability = 0.0;
    config.emergencyProbability = 0.0;

    SimulationEngine engine(config);
    std::ostringstream output;
    engine.runConfiguredDuration(output, true);

    const std::string summary = output.str();
    assert(engine.getCurrentTick() == 2);
    assert(summary.find("=== Simulation Summary ===") != std::string::npos);
    assert(summary.find("Ticks completed: 2") != std::string::npos);
    assert(summary.find("Vehicles generated: 0") != std::string::npos);
    assert(summary.find("Average waiting time (served vehicles): 0.00 ticks") != std::string::npos);
    assert(summary.find("Throughput: 0.000 vehicles/tick") != std::string::npos);
    assert(summary.find("Vehicle accounting: 0 generated = 0 served + 0 blocked + 0 waiting (OK)") != std::string::npos);
}

void testSummaryStatisticsKeepVehicleAccounting() {
    Config config;
    config.durationTicks = 12;
    config.arrivalProbability = 1.0;
    config.emergencyProbability = 0.0;
    config.queueCapacity = 2;

    SimulationEngine engine(config);
    std::ostringstream output;
    engine.runConfiguredDuration(output, true);

    const std::string summary = output.str();
    assert(engine.getCurrentTick() == 12);
    assert(engine.getTotalGenerated() > 0);
    assert(engine.getRejectedArrivals() > 0);
    assert(summary.find("Vehicles generated:") != std::string::npos);
    assert(summary.find("Average queue lengths: N=") != std::string::npos);
    assert(summary.find("Vehicle accounting:") != std::string::npos);
    assert(summary.find("(OK)") != std::string::npos);
    assert(summary.find("(CHECK)") == std::string::npos);
}

void testInvalidStepCountExplainsTheProblem() {
    Config config;
    SimulationEngine engine(config);
    std::ostringstream output;

    bool rejected = false;
    try {
        engine.advanceTicks(0, output);
    } catch (const std::invalid_argument& error) {
        rejected = std::string(error.what()).find("at least 1") != std::string::npos;
    }
    assert(rejected);
    assert(engine.getCurrentTick() == 0);
}
}

int main() {
    testQuietRunPrintsReadableSummary();
    testSummaryStatisticsKeepVehicleAccounting();
    testInvalidStepCountExplainsTheProblem();
    std::cout << "All simulation engine usability/statistics tests passed.\n";
    return 0;
}
