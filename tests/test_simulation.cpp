#include "simulation.h"

#include <cassert>
#include <iostream>

static void testAdaptiveGreenDuration() {
    SimulationConfig config(20, 10, 2, 1, 100, true, 5, 30);
    TrafficSimulation simulation(config);

    simulation.addVehicle(Direction::East);
    simulation.addVehicle(Direction::East);
    simulation.addVehicle(Direction::West);

    // Step until the cycle transitions into EW_Green
    while (simulation.getSignalController().getState() != SignalState::EW_Green) {
        simulation.step();
    }

    assert(simulation.getSignalController().getState() == SignalState::EW_Green);
    // Three waiting vehicles on EW -> Gmin 5 + ceil(3 / 2) = 7 ticks.
    assert(simulation.getSignalController().getTicksRemaining() == 7);
    assert(simulation.checkVehicleCountInvariant());
}

static void testQueueInvariantAndReset() {
    SimulationConfig config(2, 10, 2, 1, 100, true, 5, 30);
    TrafficSimulation simulation(config);

    assert(simulation.addVehicle(Direction::North));
    assert(simulation.addVehicle(Direction::North));
    assert(!simulation.addVehicle(Direction::North)); // Queue full.

    for (int i = 0; i < 30; ++i) {
        simulation.step();
        assert(simulation.checkVehicleCountInvariant());
        assert(simulation.getSignalController().checkSafety());
    }

    SimulationStats stats = simulation.getStats();
    assert(stats.generated == stats.served + stats.blocked + stats.waiting);

    simulation.reset();
    assert(simulation.getCurrentTick() == 0);
    assert(simulation.checkVehicleCountInvariant());
    assert(simulation.getStats().generated == 0);
    assert(simulation.getSignalController().getState() == SignalState::NS_Green);
}

int main() {
    testAdaptiveGreenDuration();
    testQueueInvariantAndReset();
    std::cout << "All simulation tests passed.\n";
    return 0;
}
