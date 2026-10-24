#include "simulation.h"
#include <cassert>
#include <iostream>

int main() {
    SimulationConfig config(10, 4, 1, 1, 12, false, 2, 8);
    TrafficSimulation sim(config);
    assert(sim.addVehicle(Direction::North, true));
    sim.run(100);
    assert(sim.getCurrentTick() == 12);
    assert(sim.getStats().elapsedTicks == 12);
    assert(sim.checkVehicleCountInvariant());

    // The emergency is either served within the run or remains waiting; either
    // way, the statistics and vehicle accounting must stay internally valid.
    const SimulationStats stats = sim.getStats();
    assert(stats.emergencyServed <= stats.served);
    assert(stats.averageEmergencyWaitingTime >= 0.0);
    sim.step();
    assert(sim.getCurrentTick() == 12);

    sim.reset();
    assert(sim.getCurrentTick() == 0);
    assert(sim.getStats().emergencyServed == 0);
    assert(sim.checkVehicleCountInvariant());

    std::cout << "Final requirement tests passed.\n";
    return 0;
}
