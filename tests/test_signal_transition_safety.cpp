#include "simulation.h"

#include <cassert>
#include <iostream>
#include <string>

static bool isGreen(SignalState state) {
    return state == SignalState::NS_Green ||
           state == SignalState::EW_Green;
}

static bool isYellow(SignalState state) {
    return state == SignalState::NS_Yellow ||
           state == SignalState::EW_Yellow;
}

static bool isAllRed(SignalState state) {
    return state == SignalState::All_Red_1 ||
           state == SignalState::All_Red_2;
}

static bool isValidTransition(SignalState from, SignalState to) {
    // A signal may remain in its current state for multiple ticks.
    if (from == to) {
        return true;
    }

    switch (from) {
        case SignalState::NS_Green:
            return to == SignalState::NS_Yellow;

        case SignalState::NS_Yellow:
            return to == SignalState::All_Red_1;

        case SignalState::All_Red_1:
            // Emergency pre-emption may select either phase.
            return to == SignalState::NS_Green ||
                   to == SignalState::EW_Green;

        case SignalState::EW_Green:
            return to == SignalState::EW_Yellow;

        case SignalState::EW_Yellow:
            return to == SignalState::All_Red_2;

        case SignalState::All_Red_2:
            // Emergency pre-emption may select either phase.
            return to == SignalState::NS_Green ||
                   to == SignalState::EW_Green;
    }

    return false;
}

static void checkTransitionSequence(
    TrafficSimulation& simulation,
    int ticks,
    int requiredYellowTicks,
    int requiredAllRedTicks,
    const std::string& scenario) {

    SignalState previous =
        simulation.getSignalController().getState();

    int stateDuration = 1;

    for (int tick = 0; tick < ticks; ++tick) {
        simulation.step();

        SignalController& controller =
            simulation.getSignalController();

        const SignalState current = controller.getState();

        assert(controller.checkSafety());
        assert(simulation.checkVehicleCountInvariant());

        if (current == previous) {
            ++stateDuration;
            continue;
        }

        // Every state change must follow the controller's safe sequence.
        if (!isValidTransition(previous, current)) {
            std::cerr << "Invalid transition in " << scenario
                      << " at tick " << simulation.getCurrentTick()
                      << ": " << static_cast<int>(previous)
                      << " -> " << static_cast<int>(current)
                      << '\n';
            assert(false);
        }

        // A yellow state must last for its configured duration.
        if (isYellow(previous)) {
            assert(stateDuration >= requiredYellowTicks);
        }

        // An all-red state must last for its configured duration.
        if (isAllRed(previous)) {
            assert(stateDuration >= requiredAllRedTicks);
        }

        // A green phase must not transition directly to the other green.
        if (isGreen(previous)) {
            assert(isYellow(current));
        }

        // Yellow must be followed by the matching all-red state.
        if (previous == SignalState::NS_Yellow) {
            assert(current == SignalState::All_Red_1);
        }

        if (previous == SignalState::EW_Yellow) {
            assert(current == SignalState::All_Red_2);
        }

        previous = current;
        stateDuration = 1;
    }

    std::cout << scenario << ": passed\n";
}

int main() {
    const int yellowTicks = 2;
    const int allRedTicks = 1;

    SimulationConfig config(
        50,  // Queue capacity
        8,   // Green ticks
        yellowTicks,
        allRedTicks,
        1000,
        false,  // Fixed timing makes the normal sequence predictable
        5,
        30
    );

    // Test 1: Normal signal cycling.
    TrafficSimulation normal(config);

    checkTransitionSequence(
        normal,
        300,
        yellowTicks,
        allRedTicks,
        "Normal signal transitions"
    );

    // Test 2: Emergency pre-emption with emergencies from both phases.
    TrafficSimulation emergency(config);

    assert(emergency.addVehicle(Direction::East, true));
    assert(emergency.addVehicle(Direction::North, true));

    checkTransitionSequence(
        emergency,
        300,
        yellowTicks,
        allRedTicks,
        "Emergency pre-emption transitions"
    );

    std::cout << "\nAll signal transition safety tests passed.\n";
    return 0;
}
