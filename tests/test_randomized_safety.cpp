
#include "simulation.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <random>

int main() {
    const unsigned int NUM_SEEDS = 100;
    const int TICKS_PER_RUN = 500;

    for (unsigned int seed = 0; seed < NUM_SEEDS; ++seed) {
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> directionDist(0, 3);
        std::uniform_int_distribution<int> arrivalDist(0, 99);
        std::uniform_int_distribution<int> emergencyDist(0, 19);

        SimulationConfig config(
            30,   // Queue capacity
            10,   // Green ticks
            2,    // Yellow ticks
            1,    // All-red ticks
            TICKS_PER_RUN,
            true, // Adaptive timing
            5,    // Minimum green
            30    // Maximum green
        );

        TrafficSimulation simulation(config);

        for (int tick = 0; tick < TICKS_PER_RUN; ++tick) {
            // Randomly generate vehicles on each tick.
            for (int attempt = 0; attempt < 4; ++attempt) {
                if (arrivalDist(rng) < 35) {
                    Direction direction;

                    switch (directionDist(rng)) {
                        case 0: direction = Direction::North; break;
                        case 1: direction = Direction::South; break;
                        case 2: direction = Direction::East;  break;
                        default: direction = Direction::West; break;
                    }

                    const bool emergency = (emergencyDist(rng) == 0);
                    simulation.addVehicle(direction, emergency);
                }
            }

            simulation.step();

            SignalController& signal =
                simulation.getSignalController();

            // Opposing traffic directions must never discharge together.
            assert(signal.checkSafety());

            // Every generated vehicle must be accounted for.
            assert(simulation.checkVehicleCountInvariant());
        }

        // Check the invariants again after the run.
        assert(simulation.checkVehicleCountInvariant());
        assert(simulation.getSignalController().checkSafety());

        std::cout << "Seed " << seed << " passed\n";
    }

    std::cout << "\nAll 100 randomized simulation runs passed.\n";
    return 0;
}
