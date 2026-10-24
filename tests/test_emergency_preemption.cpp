#include "simulation.h"

#include <cassert>
#include <iostream>
#include <string>

static bool logContains(const TrafficSimulation& simulation,
                        const std::string& needle) {
    const std::vector<std::string>& log = simulation.getEventLog();
    for (std::size_t i = 0; i < log.size(); ++i) {
        if (log[i].find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

int main() {
    // An eastbound emergency must eventually receive EW green and be logged.
    SimulationConfig config(20, 10, 2, 1, 100, false, 5, 30);
    TrafficSimulation simulation(config);
    assert(simulation.addVehicle(Direction::East, true));
    simulation.run(30);
    assert(logContains(simulation, "EMERGENCY_START"));
    assert(logContains(simulation, "EMERGENCY_END"));
    assert(simulation.checkVehicleCountInvariant());

    // Two emergency requests should not create an unsafe signal state.
    TrafficSimulation competing(config);
    assert(competing.addVehicle(Direction::East, true));
    assert(competing.addVehicle(Direction::North, true));
    for (int i = 0; i < 80; ++i) {
        competing.step();
        assert(competing.getSignalController().checkSafety());
        assert(competing.checkVehicleCountInvariant());
    }
    assert(logContains(competing, "EMERGENCY_START"));
    assert(logContains(competing, "EMERGENCY_END"));

    std::cout << "Emergency pre-emption tests passed.\n";
    return 0;
}
