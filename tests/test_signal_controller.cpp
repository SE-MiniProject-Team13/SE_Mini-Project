#include "signal_controller.h"
#include "vehicle_queue.h"

#include <cassert>
#include <iostream>
#include <vector>

// 1. TC-19: Complete six-state cycle
void testSixStateCycle() {
    SignalTimings timings(SignalMode::Fixed, 10, 2, 5, 30, 1);
    SignalController controller(timings);

    // Initial state: S1 (NS_GREEN)
    assert(controller.getState() == SignalState::NS_Green);
    assert(controller.getLightColor(Direction::North) == LightColor::Green);
    assert(controller.getLightColor(Direction::South) == LightColor::Green);
    assert(controller.getLightColor(Direction::East) == LightColor::Red);
    assert(controller.getLightColor(Direction::West) == LightColor::Red);

    // Advance through the cycle and capture the sequence of states
    std::vector<SignalState> sequence;
    sequence.push_back(controller.getState());

    for (int tick = 1; tick <= 55; ++tick) {
        bool changed = controller.advanceState(tick, QueueLengths());
        if (changed) {
            sequence.push_back(controller.getState());
        }
    }

    // Sequence must follow:
    // S1: NS_Green -> S2: NS_Yellow -> S3: All_Red_1 -> S4: EW_Green -> S5: EW_Yellow -> S6: All_Red_2 -> S1: NS_Green
    assert(sequence.size() >= 7);
    assert(sequence[0] == SignalState::NS_Green);
    assert(sequence[1] == SignalState::NS_Yellow);
    assert(sequence[2] == SignalState::All_Red_1);
    assert(sequence[3] == SignalState::EW_Green);
    assert(sequence[4] == SignalState::EW_Yellow);
    assert(sequence[5] == SignalState::All_Red_2);
    assert(sequence[6] == SignalState::NS_Green);

    std::cout << "[PASS] testSixStateCycle\n";
}

// 2. TC-21: Correct fixed-mode durations
void testFixedModeDurations() {
    // green = 10, yellow = 2, all-red = 1
    SignalTimings timings(SignalMode::Fixed, 10, 2, 5, 30, 1);
    SignalController controller(timings);

    int nsGreenCount = 0;
    int nsYellowCount = 0;
    int allRed1Count = 0;
    int ewGreenCount = 0;
    int ewYellowCount = 0;
    int allRed2Count = 0;

    // Run for exactly one 26-tick cycle (ticks 1 to 26)
    for (int tick = 1; tick <= 26; ++tick) {
        controller.advanceState(tick, QueueLengths());

        switch (controller.getState()) {
            case SignalState::NS_Green:  ++nsGreenCount; break;
            case SignalState::NS_Yellow: ++nsYellowCount; break;
            case SignalState::All_Red_1: ++allRed1Count; break;
            case SignalState::EW_Green:  ++ewGreenCount; break;
            case SignalState::EW_Yellow: ++ewYellowCount; break;
            case SignalState::All_Red_2: ++allRed2Count; break;
        }
    }

    assert(nsGreenCount == 10);
    assert(nsYellowCount == 2);
    assert(allRed1Count == 1);
    assert(ewGreenCount == 10);
    assert(ewYellowCount == 2);
    assert(allRed2Count == 1);

    // On tick 27, it must return to NS_Green and complete 1 full cycle
    controller.advanceState(27, QueueLengths());
    assert(controller.getState() == SignalState::NS_Green);
    assert(controller.getCycleCount() == 1);

    // Test with custom configured durations: green = 4, yellow = 3
    SignalTimings customTimings(SignalMode::Fixed, 4, 3, 2, 20, 1);
    SignalController customController(customTimings);

    int cNsGreen = 0, cNsYellow = 0, cAllRed1 = 0;
    int cEwGreen = 0, cEwYellow = 0, cAllRed2 = 0;

    // Cycle duration = 4 + 3 + 1 + 4 + 3 + 1 = 16 ticks
    for (int tick = 1; tick <= 16; ++tick) {
        customController.advanceState(tick, QueueLengths());
        switch (customController.getState()) {
            case SignalState::NS_Green:  ++cNsGreen; break;
            case SignalState::NS_Yellow: ++cNsYellow; break;
            case SignalState::All_Red_1: ++cAllRed1; break;
            case SignalState::EW_Green:  ++cEwGreen; break;
            case SignalState::EW_Yellow: ++cEwYellow; break;
            case SignalState::All_Red_2: ++cAllRed2; break;
        }
    }

    assert(cNsGreen == 4);
    assert(cNsYellow == 3);
    assert(cAllRed1 == 1);
    assert(cEwGreen == 4);
    assert(cEwYellow == 3);
    assert(cAllRed2 == 1);

    // Regression check: verify All-Red duration cannot be modified through setTimings (SRS FR-21)
    SignalTimings attemptInvalidAllRed = customTimings;
    attemptInvalidAllRed.allRedTime = 99; // Attempt to configure invalid all-red duration
    customController.setTimings(attemptInvalidAllRed);
    assert(customController.getPendingTimings().allRedTime == SignalTimings::ALL_RED_TICKS);
    assert(customController.getActiveTimings().allRedTime == SignalTimings::ALL_RED_TICKS);

    std::cout << "[PASS] testFixedModeDurations\n";
}

// 3. TC-22: Adaptive timing calculation based on queue lengths
void testAdaptiveTimingCalculations() {
    // Defaults: Gmin = 5, Gmax = 30
    SignalTimings timings(SignalMode::Adaptive, 10, 2, 5, 30, 1);
    SignalController controller(timings);

    // FR-22 & TC-22: green time = Gmin + ceil(waiting / 2), capped at Gmax
    // Waiting = 0 -> 5 + 0 = 5
    assert(controller.determineGreenDuration(SignalState::NS_Green, QueueLengths(0, 0, 0, 0)) == 5);
    // Waiting = 1 -> 5 + ceil(1/2) = 6
    assert(controller.determineGreenDuration(SignalState::NS_Green, QueueLengths(1, 0, 0, 0)) == 6);
    assert(controller.determineGreenDuration(SignalState::NS_Green, QueueLengths(0, 1, 0, 0)) == 6);
    // Waiting = 2 -> 5 + ceil(2/2) = 6
    assert(controller.determineGreenDuration(SignalState::NS_Green, QueueLengths(1, 1, 0, 0)) == 6);
    assert(controller.determineGreenDuration(SignalState::NS_Green, QueueLengths(2, 0, 0, 0)) == 6);
    // Waiting = 3 -> 5 + ceil(3/2) = 7
    assert(controller.determineGreenDuration(SignalState::NS_Green, QueueLengths(2, 1, 0, 0)) == 7);
    assert(controller.determineGreenDuration(SignalState::NS_Green, QueueLengths(3, 0, 0, 0)) == 7);
    // Waiting = 4 -> 5 + ceil(4/2) = 7
    assert(controller.determineGreenDuration(SignalState::NS_Green, QueueLengths(2, 2, 0, 0)) == 7);
    // Waiting = 10 -> 5 + ceil(10/2) = 10
    assert(controller.determineGreenDuration(SignalState::NS_Green, QueueLengths(5, 5, 0, 0)) == 10);

    // Test EW approach queue lengths:
    // Waiting on EW = 6 -> 5 + ceil(6/2) = 8
    assert(controller.determineGreenDuration(SignalState::EW_Green, QueueLengths(0, 0, 4, 2)) == 8);
    assert(controller.determineGreenDuration(Phase::EW, QueueLengths(0, 0, 4, 2)) == 8);

    std::cout << "[PASS] testAdaptiveTimingCalculations\n";
}

// 4. TC-22 & BR-03: Gmin and Gmax enforcement
void testGminAndGmaxBounds() {
    SignalTimings timings(SignalMode::Adaptive, 10, 2, 5, 30, 1);
    SignalController controller(timings);

    // Queue length exceeds Gmax: 100 waiting -> 5 + 50 = 55, capped at Gmax = 30
    assert(controller.determineGreenDuration(SignalState::NS_Green, QueueLengths(50, 50, 0, 0)) == 30);
    assert(controller.determineGreenDuration(SignalState::EW_Green, QueueLengths(0, 0, 60, 40)) == 30);

    // Zero queue length -> never below Gmin = 5
    assert(controller.determineGreenDuration(SignalState::NS_Green, QueueLengths(0, 0, 0, 0)) == 5);

    // Custom Gmin=10, Gmax=20
    SignalTimings customBounds(SignalMode::Adaptive, 10, 2, 10, 20, 1);
    SignalController customController(customBounds);

    assert(customController.determineGreenDuration(SignalState::NS_Green, QueueLengths(0, 0, 0, 0)) == 10);
    assert(customController.determineGreenDuration(SignalState::NS_Green, QueueLengths(1, 0, 0, 0)) == 11);
    assert(customController.determineGreenDuration(SignalState::NS_Green, QueueLengths(50, 50, 0, 0)) == 20);

    std::cout << "[PASS] testGminAndGmaxBounds\n";
}

// 5. TC-20, FR-20, BR-01, NFR-05: Prevention of conflicting signal states
void testConflictFreeSignals() {
    SignalTimings timings(SignalMode::Fixed, 10, 2, 5, 30, 1);
    SignalController controller(timings);

    // Verify conflict-free invariants across 10 full cycles (260 ticks) in Fixed mode
    for (int tick = 1; tick <= 260; ++tick) {
        controller.advanceState(tick, QueueLengths());

        LightColor n = controller.getLightColor(Direction::North);
        LightColor s = controller.getLightColor(Direction::South);
        LightColor e = controller.getLightColor(Direction::East);
        LightColor w = controller.getLightColor(Direction::West);

        // North and South always match; East and West always match
        assert(n == s);
        assert(e == w);

        // Core safety requirement (FR-20, BR-01): NS and EW must never be GREEN or YELLOW at the same time
        bool nsNonRed = (n == LightColor::Green || n == LightColor::Yellow);
        bool ewNonRed = (e == LightColor::Green || e == LightColor::Yellow);

        assert(!(nsNonRed && ewNonRed));
        assert(controller.checkSafety());
        assert(!controller.isSafetyViolationDetected());

        // In All-Red states, verify all four approaches are strictly Red
        if (controller.getState() == SignalState::All_Red_1 ||
            controller.getState() == SignalState::All_Red_2) {
            assert(n == LightColor::Red);
            assert(s == LightColor::Red);
            assert(e == LightColor::Red);
            assert(w == LightColor::Red);
        }
    }

    // Verify conflict-free invariants in Adaptive mode as well
    SignalTimings adaptiveTimings(SignalMode::Adaptive, 10, 2, 5, 30, 1);
    SignalController adaptiveController(adaptiveTimings);
    QueueLengths dynamicQueues(3, 2, 5, 4);

    for (int tick = 1; tick <= 200; ++tick) {
        adaptiveController.advanceState(tick, dynamicQueues);
        LightColor n = adaptiveController.getLightColor(Direction::North);
        LightColor e = adaptiveController.getLightColor(Direction::East);
        bool nsNonRed = (n == LightColor::Green || n == LightColor::Yellow);
        bool ewNonRed = (e == LightColor::Green || e == LightColor::Yellow);
        assert(!(nsNonRed && ewNonRed));
        assert(adaptiveController.checkSafety());
        assert(!adaptiveController.isSafetyViolationDetected());
    }

    // Verify public fail-safe override mechanism (forceAllRed):
    // Tests that forceAllRed() immediately sets state to All_Red_1 with all approaches RED and discharge disabled
    controller.forceAllRed();
    assert(controller.getState() == SignalState::All_Red_1);
    assert(controller.getTicksRemaining() == SignalTimings::ALL_RED_TICKS);
    assert(controller.getLightColor(Direction::North) == LightColor::Red);
    assert(controller.getLightColor(Direction::South) == LightColor::Red);
    assert(controller.getLightColor(Direction::East) == LightColor::Red);
    assert(controller.getLightColor(Direction::West) == LightColor::Red);
    assert(!controller.canDischarge(Direction::North));
    assert(!controller.canDischarge(Direction::East));
    assert(controller.checkSafety()); // All-Red is intrinsically conflict-free

    std::cout << "[PASS] testConflictFreeSignals\n";
}

// 6. TC-23, FR-23, BR-04: Vehicle discharge rules
void testVehicleDischargeRules() {
    // Discharge rate = 2
    SignalTimings timings(SignalMode::Fixed, 10, 2, 5, 30, 2);
    SignalController controller(timings);

    VehicleQueue northQueue(10);
    VehicleQueue eastQueue(10);

    for (int i = 1; i <= 5; ++i) {
        northQueue.enqueue(new Vehicle{i, Direction::North, false, 0});
        eastQueue.enqueue(new Vehicle{10 + i, Direction::East, false, 0});
    }

    std::vector<Vehicle*> departed;

    // Initial state: NS_Green
    controller.advanceState(1, QueueLengths());
    assert(controller.getState() == SignalState::NS_Green);
    assert(controller.canDischarge(Direction::North));
    assert(!controller.canDischarge(Direction::East));

    // Discharge on Green: up to dischargeRate (2) vehicles leave
    int dischargedN = controller.dischargeVehicles(Direction::North, northQueue, 1, departed);
    assert(dischargedN == 2);
    assert(northQueue.size() == 3);
    assert(departed.size() == 2);
    assert(departed[0]->id == 1 && departed[0]->waitingTime == 1);
    assert(departed[1]->id == 2 && departed[1]->waitingTime == 1);

    // Attempt discharge on Red approach (East): exactly 0 leave
    int dischargedE = controller.dischargeVehicles(Direction::East, eastQueue, 1, departed);
    assert(dischargedE == 0);
    assert(eastQueue.size() == 5);

    // Advance to NS_Yellow (tick 11 transitions from NS_Green to NS_Yellow)
    for (int tick = 2; tick <= 11; ++tick) {
        controller.advanceState(tick, QueueLengths());
    }
    assert(controller.getState() == SignalState::NS_Yellow);
    assert(!controller.canDischarge(Direction::North));
    assert(!controller.canDischarge(Direction::East));

    // On Yellow, 0 vehicles leave
    int dischargedYellow = controller.dischargeVehicles(Direction::North, northQueue, 11, departed);
    assert(dischargedYellow == 0);
    assert(northQueue.size() == 3);

    // Clean up allocated departed vehicles
    for (Vehicle* v : departed) {
        delete v;
    }
    northQueue.clear();
    eastQueue.clear();

    std::cout << "[PASS] testVehicleDischargeRules\n";
}

// 7. TC-26, FR-26: Timing changes applied at next cycle
void testTimingChangesTakeEffectAtNextCycle() {
    // Start with Fixed green = 10, yellow = 2
    SignalTimings initialTimings(SignalMode::Fixed, 10, 2, 5, 30, 1);
    SignalController controller(initialTimings);

    // Initial configuration must be active immediately upon construction
    assert(!controller.hasPendingTimings());
    assert(controller.getActiveTimings().greenTime == 10);
    assert(controller.getActiveTimings().yellowTime == 2);
    assert(controller.getActiveTimings().allRedTime == 1);
    assert(controller.getActiveTimings().dischargeRate == 1);

    // Run until tick 15 (during EW_Green of Cycle 1)
    for (int tick = 1; tick <= 15; ++tick) {
        controller.advanceState(tick, QueueLengths());
    }
    assert(controller.getState() == SignalState::EW_Green);
    assert(controller.getCycleCount() == 0);

    // Propose changed timings mid-cycle: green = 20, yellow = 3, dischargeRate = 2
    // Also attempt to tamper with allRedTime (set to 7) to verify FR-21 preservation
    SignalTimings newTimings(SignalMode::Fixed, 20, 3, 5, 30, 2);
    newTimings.allRedTime = 7;
    controller.setTimings(newTimings);

    // FR-26: Changes must be staged as pending and MUST NOT alter active configuration mid-cycle
    assert(controller.hasPendingTimings());
    assert(controller.getActiveTimings().greenTime == 10);
    assert(controller.getActiveTimings().yellowTime == 2);
    assert(controller.getActiveTimings().allRedTime == 1);
    assert(controller.getActiveTimings().dischargeRate == 1);
    assert(controller.getDischargeRate() == 1);

    // Pending timings must have sanitized allRedTime = 1 per FR-21
    assert(controller.getPendingTimings().greenTime == 20);
    assert(controller.getPendingTimings().yellowTime == 3);
    assert(controller.getPendingTimings().allRedTime == 1);
    assert(controller.getPendingTimings().dischargeRate == 2);

    // Advance tick-by-tick to the end of Cycle 1 (ticks 16 to 26)
    // Verify that on every tick of the current cycle, active timings remain completely untouched
    for (int tick = 16; tick <= 26; ++tick) {
        controller.advanceState(tick, QueueLengths());
        assert(controller.hasPendingTimings());
        assert(controller.getActiveTimings().greenTime == 10);
        assert(controller.getActiveTimings().yellowTime == 2);
        assert(controller.getActiveTimings().allRedTime == 1);
        assert(controller.getActiveTimings().dischargeRate == 1);
    }
    assert(controller.getState() == SignalState::All_Red_2);

    // Transition into Cycle 2 on tick 27 (S6 -> S1 boundary)
    controller.advanceState(27, QueueLengths());
    assert(controller.getState() == SignalState::NS_Green);
    assert(controller.getCycleCount() == 1);

    // New timings are now active!
    assert(!controller.hasPendingTimings());
    assert(controller.getActiveTimings().greenTime == 20);
    assert(controller.getActiveTimings().yellowTime == 3);
    assert(controller.getActiveTimings().allRedTime == 1);
    assert(controller.getActiveTimings().dischargeRate == 2);
    assert(controller.getDischargeRate() == 2);
    assert(controller.getTicksRemaining() == 20);

    // Now propose another timing change during Cycle 2 (at tick 35, during NS_Green)
    SignalTimings cycle3Timings(SignalMode::Fixed, 15, 2, 5, 30, 3);
    controller.setTimings(cycle3Timings);
    assert(controller.hasPendingTimings());
    // Active timings must stay at green=20 throughout Cycle 2
    assert(controller.getActiveTimings().greenTime == 20);
    assert(controller.getActiveTimings().yellowTime == 3);
    assert(controller.getActiveTimings().dischargeRate == 2);

    std::cout << "[PASS] testTimingChangesTakeEffectAtNextCycle\n";
}

// 8. TC-24 & TC-25: State record display and cycle counting
void testStateRecordAndDisplay() {
    SignalTimings timings(SignalMode::Fixed, 10, 2, 5, 30, 1);
    SignalController controller(timings);

    SignalStateRecord rec = controller.getStateRecord();
    assert(rec.state == SignalState::NS_Green);
    assert(rec.stateName == "NS_GREEN");
    assert(rec.ticksRemaining == 10);
    assert(rec.cycleCount == 0);
    assert(!rec.preemptionActive);
    assert(rec.northLight == LightColor::Green);
    assert(rec.southLight == LightColor::Green);
    assert(rec.eastLight == LightColor::Red);
    assert(rec.westLight == LightColor::Red);

    // Test preemption interface hooks required by SAD 4.3 for future Emergency Handler integration (FR-TMS-05)
    // Verifies hook contract, stored phase, and preemptionActive flag (SRS B.4) without claiming full preemption logic
    assert(!controller.isPreemptionActive());
    assert(controller.getPreemptedPhase() == Phase::NS);
    controller.acceptPreemption(Phase::EW);
    assert(controller.isPreemptionActive());
    assert(controller.getPreemptedPhase() == Phase::EW);
    controller.releasePreemption();
    assert(!controller.isPreemptionActive());

    std::cout << "[PASS] testStateRecordAndDisplay\n";
}

// 9. Adaptive multi-cycle execution
void testAdaptiveMultiCycle() {
    // Adaptive mode with Gmin = 5, Gmax = 30, yellow = 2, all-red = 1
    SignalTimings timings(SignalMode::Adaptive, 10, 2, 5, 30, 1);
    SignalController controller(timings);

    // Initial NS_Green with 0 waiting -> Gmin = 5 ticks (ticks 1..5)
    // S2: NS_Yellow -> 2 ticks (ticks 6..7)
    // S3: All_Red_1 -> 1 tick (tick 8)
    // At tick 9, transitions to EW_Green. Let EW queue have 6 waiting -> Gmin + ceil(6/2) = 5 + 3 = 8 ticks! (ticks 9..16)
    // S5: EW_Yellow -> 2 ticks (ticks 17..18)
    // S6: All_Red_2 -> 1 tick (tick 19)
    // At tick 20, transitions to NS_Green. Cycle 1 finished!

    int nsGreenTicks = 0;
    int ewGreenTicks = 0;

    QueueLengths ewQueues(0, 0, 4, 2); // 6 vehicles waiting on EW

    for (int tick = 1; tick <= 19; ++tick) {
        controller.advanceState(tick, ewQueues);
        if (controller.getState() == SignalState::NS_Green) {
            ++nsGreenTicks;
        } else if (controller.getState() == SignalState::EW_Green) {
            ++ewGreenTicks;
        }
    }

    assert(nsGreenTicks == 5); // Gmin = 5
    assert(ewGreenTicks == 8); // 5 + 3 = 8

    // Tick 20 transitions to Cycle 2 NS_Green
    controller.advanceState(20, QueueLengths());
    assert(controller.getState() == SignalState::NS_Green);
    assert(controller.getCycleCount() == 1);

    std::cout << "[PASS] testAdaptiveMultiCycle\n";
}

// 10. Mode switching taking effect at next cycle
void testModeSwitching() {
    SignalTimings timings(SignalMode::Fixed, 10, 2, 5, 30, 1);
    SignalController controller(timings);

    // Initial state: Fixed mode active
    assert(controller.getActiveTimings().mode == SignalMode::Fixed);
    assert(!controller.hasPendingTimings());

    // Run until tick 14 (during EW_Green of cycle 1)
    for (int tick = 1; tick <= 14; ++tick) {
        controller.advanceState(tick, QueueLengths());
    }
    assert(controller.getActiveTimings().mode == SignalMode::Fixed);

    // Switch to Adaptive mode midway through cycle 1
    controller.setMode(SignalMode::Adaptive);
    assert(controller.hasPendingTimings());
    // FR-26: Active mode MUST remain Fixed throughout the remainder of Cycle 1
    assert(controller.getActiveTimings().mode == SignalMode::Fixed);
    assert(controller.getPendingTimings().mode == SignalMode::Adaptive);

    // Complete cycle 1 (ticks 15 to 26) - verify active mode stays Fixed on every tick
    for (int tick = 15; tick <= 26; ++tick) {
        controller.advanceState(tick, QueueLengths());
        assert(controller.getActiveTimings().mode == SignalMode::Fixed);
    }
    assert(controller.getState() == SignalState::All_Red_2);

    // Tick 27 starts cycle 2 (S6 -> S1): pending Adaptive mode takes effect
    QueueLengths nsQueues(4, 4, 0, 0); // 8 waiting -> Gmin 5 + ceil(8/2) = 9 ticks
    controller.advanceState(27, nsQueues);

    assert(controller.getState() == SignalState::NS_Green);
    assert(controller.getCycleCount() == 1);
    assert(controller.getActiveTimings().mode == SignalMode::Adaptive);
    assert(!controller.hasPendingTimings());
    assert(controller.getTicksRemaining() == 9);

    // Mid-way through cycle 2 (at tick 30), request switch back to Fixed mode
    controller.advanceState(28, nsQueues);
    controller.advanceState(29, nsQueues);
    controller.advanceState(30, nsQueues);
    controller.setMode(SignalMode::Fixed);

    assert(controller.hasPendingTimings());
    // FR-26: Active mode MUST remain Adaptive throughout Cycle 2
    assert(controller.getActiveTimings().mode == SignalMode::Adaptive);
    assert(controller.getPendingTimings().mode == SignalMode::Fixed);

    // 1. Advance through the remainder of Cycle 2, verifying active mode remains Adaptive on every tick
    int tick = 31;
    while (controller.getCycleCount() == 1) {
        assert(controller.getActiveTimings().mode == SignalMode::Adaptive);
        assert(controller.hasPendingTimings());
        assert(controller.getPendingTimings().mode == SignalMode::Fixed);
        controller.advanceState(tick++, QueueLengths());
    }

    // 2. Advanced through S6 -> S1 boundary into Cycle 3
    assert(controller.getState() == SignalState::NS_Green);
    assert(controller.getCycleCount() == 2);

    // 3. Verify Fixed mode becomes active at the start of Cycle 3 and no pending timing changes remain
    assert(controller.getActiveTimings().mode == SignalMode::Fixed);
    assert(!controller.hasPendingTimings());
    assert(controller.getTicksRemaining() == 10); // Configured fixed green duration

    std::cout << "[PASS] testModeSwitching\n";
}

// 11. Reset functionality
void testReset() {
    SignalTimings timings(SignalMode::Fixed, 10, 2, 5, 30, 1);
    SignalController controller(timings);

    for (int tick = 1; tick <= 35; ++tick) {
        controller.advanceState(tick, QueueLengths());
    }
    assert(controller.getCycleCount() >= 1);

    controller.reset();
    assert(controller.getState() == SignalState::NS_Green);
    assert(controller.getCycleCount() == 0);
    assert(controller.getTicksRemaining() == 10);
    assert(!controller.isPreemptionActive());
    assert(!controller.isSafetyViolationDetected());

    std::cout << "[PASS] testReset\n";
}

// 12. Static string helpers
void testStaticHelpers() {
    assert(std::string(SignalController::stateToString(SignalState::NS_Green)) == "NS_GREEN");
    assert(std::string(SignalController::stateToString(SignalState::NS_Yellow)) == "NS_YELLOW");
    assert(std::string(SignalController::stateToString(SignalState::All_Red_1, true)) == "ALL_RED_1");
    assert(std::string(SignalController::stateToString(SignalState::All_Red_1, false)) == "ALL_RED");
    assert(std::string(SignalController::stateToString(SignalState::EW_Green)) == "EW_GREEN");
    assert(std::string(SignalController::stateToString(SignalState::EW_Yellow)) == "EW_YELLOW");
    assert(std::string(SignalController::stateToString(SignalState::All_Red_2, true)) == "ALL_RED_2");
    assert(std::string(SignalController::stateToString(SignalState::All_Red_2, false)) == "ALL_RED");

    assert(std::string(SignalController::colorToString(LightColor::Green)) == "GREEN");
    assert(std::string(SignalController::colorToString(LightColor::Yellow)) == "YELLOW");
    assert(std::string(SignalController::colorToString(LightColor::Red)) == "RED");

    assert(std::string(SignalController::modeToString(SignalMode::Fixed)) == "Fixed");
    assert(std::string(SignalController::modeToString(SignalMode::Adaptive)) == "Adaptive");

    std::cout << "[PASS] testStaticHelpers\n";
}

// 13. Active phase mapping across cycle states (All-Red transitions map to upcoming phase)
void testActivePhaseMapping() {
    SignalTimings timings(SignalMode::Fixed, 10, 2, 5, 30, 1);
    SignalController controller(timings);

    // Initial state before tick 1
    assert(controller.getState() == SignalState::NS_Green);
    assert(controller.getActivePhase() == Phase::NS);

    for (int tick = 1; tick <= 27; ++tick) {
        controller.advanceState(tick, QueueLengths());

        if (tick >= 1 && tick <= 10) {
            assert(controller.getState() == SignalState::NS_Green);
            assert(controller.getActivePhase() == Phase::NS);
        } else if (tick >= 11 && tick <= 12) {
            assert(controller.getState() == SignalState::NS_Yellow);
            assert(controller.getActivePhase() == Phase::NS);
        } else if (tick == 13) {
            assert(controller.getState() == SignalState::All_Red_1);
            assert(controller.getActivePhase() == Phase::EW); // Transitions to EW
        } else if (tick >= 14 && tick <= 23) {
            assert(controller.getState() == SignalState::EW_Green);
            assert(controller.getActivePhase() == Phase::EW);
        } else if (tick >= 24 && tick <= 25) {
            assert(controller.getState() == SignalState::EW_Yellow);
            assert(controller.getActivePhase() == Phase::EW);
        } else if (tick == 26) {
            assert(controller.getState() == SignalState::All_Red_2);
            assert(controller.getActivePhase() == Phase::NS); // Transitions to NS
        } else if (tick == 27) {
            assert(controller.getState() == SignalState::NS_Green);
            assert(controller.getActivePhase() == Phase::NS);
        }
    }

    std::cout << "[PASS] testActivePhaseMapping\n";
}

int main() {
    std::cout << "Running Signal Controller Unit Tests...\n";

    testSixStateCycle();
    testFixedModeDurations();
    testAdaptiveTimingCalculations();
    testGminAndGmaxBounds();
    testConflictFreeSignals();
    testVehicleDischargeRules();
    testTimingChangesTakeEffectAtNextCycle();
    testStateRecordAndDisplay();
    testAdaptiveMultiCycle();
    testModeSwitching();
    testReset();
    testStaticHelpers();
    testActivePhaseMapping();

    std::cout << "All Signal Controller tests passed successfully!\n";
    return 0;
}
