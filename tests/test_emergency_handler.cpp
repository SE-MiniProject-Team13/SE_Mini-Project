#include "emergency_handler.h"
#include "signal_controller.h"
#include "vehicle.h"
#include "vehicle_queue.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            assert(cond); \
        } \
    } while (0)

#define ASSERT_EQUAL(a, b) \
    do { \
        if ((a) != (b)) { \
            std::cerr << "Assertion failed: " #a " == " #b " (" << (a) << " != " << (b) << ") at " \
                      << __FILE__ << ":" << __LINE__ << std::endl; \
            assert((a) == (b)); \
        } \
    } while (0)

inline std::ostream& operator<<(std::ostream& os, Direction dir) {
    switch (dir) {
        case Direction::North: return os << "North";
        case Direction::South: return os << "South";
        case Direction::East:  return os << "East";
        case Direction::West:  return os << "West";
    }
    return os << "Unknown";
}

inline std::ostream& operator<<(std::ostream& os, Phase phase) {
    switch (phase) {
        case Phase::NS: return os << "NS";
        case Phase::EW: return os << "EW";
    }
    return os << "Unknown";
}

inline std::ostream& operator<<(std::ostream& os, EmergencyEventType type) {
    switch (type) {
        case EmergencyEventType::EmergencyStart: return os << "EMERGENCY_START";
        case EmergencyEventType::EmergencyEnd:   return os << "EMERGENCY_END";
    }
    return os << "Unknown";
}

inline std::ostream& operator<<(std::ostream& os, EmergencyHandlerState state) {
    return os << EmergencyHandler::stateToString(state);
}

inline std::ostream& operator<<(std::ostream& os, SignalState state) {
    return os << SignalController::stateToString(state);
}

inline std::ostream& operator<<(std::ostream& os, LightColor color) {
    return os << SignalController::colorToString(color);
}

// -----------------------------------------------------------------------------
// TC-27 / FR-27: Queue priority for emergency vehicles and FIFO preservation
// -----------------------------------------------------------------------------
void test_TC27_EmergencyQueuePriority() {
    VehicleQueue queue(10);

    Vehicle* n1 = new Vehicle(1, Direction::North, false, 1);
    Vehicle* n2 = new Vehicle(2, Direction::North, false, 2);
    Vehicle* e1 = new Vehicle(3, Direction::North, true, 3);
    Vehicle* e2 = new Vehicle(4, Direction::North, true, 4);

    // Queue normal vehicles first
    ASSERT_TRUE(queue.enqueue(n1));
    ASSERT_TRUE(queue.enqueue(n2));

    // Then insert two emergency vehicles
    ASSERT_TRUE(queue.enqueue(e1));
    ASSERT_TRUE(queue.enqueue(e2));

    ASSERT_EQUAL(queue.size(), static_cast<std::size_t>(4));

    // FR-27: Emergency vehicles must precede all normal vehicles
    // and maintain arrival FIFO order among themselves
    Vehicle* d1 = queue.dequeue();
    ASSERT_TRUE(d1 != nullptr);
    ASSERT_EQUAL(d1->id, 3); // e1 (first emergency)
    ASSERT_TRUE(d1->isEmergency);
    delete d1;

    Vehicle* d2 = queue.dequeue();
    ASSERT_TRUE(d2 != nullptr);
    ASSERT_EQUAL(d2->id, 4); // e2 (second emergency)
    ASSERT_TRUE(d2->isEmergency);
    delete d2;

    Vehicle* d3 = queue.dequeue();
    ASSERT_TRUE(d3 != nullptr);
    ASSERT_EQUAL(d3->id, 1); // n1 (first normal)
    ASSERT_TRUE(!d3->isEmergency);
    delete d3;

    Vehicle* d4 = queue.dequeue();
    ASSERT_TRUE(d4 != nullptr);
    ASSERT_EQUAL(d4->id, 2); // n2 (second normal)
    ASSERT_TRUE(!d4->isEmergency);
    delete d4;

    ASSERT_TRUE(queue.isEmpty());
    std::cout << "[PASS] TC-27: Emergency queue priority and FIFO order preserved" << std::endl;
}

// -----------------------------------------------------------------------------
// TC-28 / FR-28: Per-tick detection and non-green approach requirement
// -----------------------------------------------------------------------------
void test_TC28_PerTickDetectionAndNonGreenOnly() {
    SignalTimings timings(SignalMode::Fixed, 10, 2, 5, 30, 1);
    SignalController sc(timings);

    VehicleQueue qNorth(10);
    VehicleQueue qSouth(10);
    VehicleQueue qEast(10);
    VehicleQueue qWest(10);
    ApproachQueues queues(qNorth, qSouth, qEast, qWest);

    EmergencyHandler eh(sc, queues);

    // Signal starts in NS_Green. North and South are GREEN; East and West are RED.
    ASSERT_EQUAL(sc.getState(), SignalState::NS_Green);
    ASSERT_EQUAL(sc.getLightColor(Direction::North), LightColor::Green);
    ASSERT_EQUAL(sc.getLightColor(Direction::East), LightColor::Red);

    // Case 1: Emergency on GREEN approach (North) -> must NOT trigger preemption request
    qNorth.enqueue(new Vehicle(101, Direction::North, true, 1));

    EmergencyRequest req1 = eh.detectEmergency(1);
    ASSERT_TRUE(!req1.hasRequest);

    // Case 2: Normal vehicle at front of RED approach (East), emergency behind it -> must NOT trigger
    qEast.enqueue(new Vehicle(201, Direction::East, false, 2));
    ASSERT_EQUAL(qEast.front()->id, 201);
    ASSERT_TRUE(!qEast.front()->isEmergency);

    EmergencyRequest req2 = eh.detectEmergency(2);
    ASSERT_TRUE(!req2.hasRequest);

    // Case 3: Emergency at front of RED approach (East) -> triggers preemption request
    qEast.enqueue(new Vehicle(202, Direction::East, true, 3)); // In VehicleQueue, emergency jumps ahead to front
    ASSERT_EQUAL(qEast.front()->id, 202);
    ASSERT_TRUE(qEast.front()->isEmergency);

    EmergencyRequest req3 = eh.detectEmergency(3);
    ASSERT_TRUE(req3.hasRequest);
    ASSERT_TRUE(req3.phase == Phase::EW);
    ASSERT_TRUE(req3.approach == Direction::East);
    ASSERT_EQUAL(req3.vehicleId, 202);
    ASSERT_EQUAL(req3.arrivalTick, 3);

    // After requestPreemption, controller records preemption
    eh.requestPreemption(req3.phase, req3.approach, req3.arrivalTick, req3.vehicleId, 3);
    ASSERT_TRUE(sc.isPreemptionActive());
    ASSERT_TRUE(sc.getPreemptedPhase() == Phase::EW);
    ASSERT_TRUE(eh.isPreemptionActive());

    std::cout << "[PASS] TC-28: Per-tick detection only triggers for front emergency on non-green approach" << std::endl;
}

// -----------------------------------------------------------------------------
// TC-29 / FR-29: Safe yellow and one-tick All-Red transitions before green
// -----------------------------------------------------------------------------
void test_TC29_SafeTransitionsGreenYellowAllRed() {
    // Sub-case 1: Preemption requested during GREEN (must respect Gmin, yellow, all-red)
    {
        SignalTimings timings(SignalMode::Fixed, 10, 2, 5, 30, 1);
        SignalController sc(timings);
        VehicleQueue qN(10), qS(10), qE(10), qW(10);
        ApproachQueues queues(qN, qS, qE, qW);
        EmergencyHandler eh(sc, queues);

        // Tick 1: Start simulation
        sc.advanceState(1, QueueLengths());
        ASSERT_EQUAL(sc.getState(), SignalState::NS_Green);
        ASSERT_EQUAL(sc.getTicksInCurrentState(), 1);

        // Request preemption for Phase EW during NS_Green at tick 1
        eh.requestPreemption(Phase::EW, Direction::East, 1, 301, 1);
        ASSERT_TRUE(sc.isPreemptionActive());

        // NS_Green must NOT skip to yellow immediately because Gmin = 5.
        // It must run until at least 5 ticks total have elapsed.
        // Tick 2:
        sc.advanceState(2, QueueLengths());
        ASSERT_EQUAL(sc.getState(), SignalState::NS_Green);
        // Tick 3:
        sc.advanceState(3, QueueLengths());
        ASSERT_EQUAL(sc.getState(), SignalState::NS_Green);
        // Tick 4:
        sc.advanceState(4, QueueLengths());
        ASSERT_EQUAL(sc.getState(), SignalState::NS_Green);
        // Tick 5: Gmin satisfied (5 ticks)
        sc.advanceState(5, QueueLengths());
        ASSERT_EQUAL(sc.getState(), SignalState::NS_Green);

        // Tick 6: Transitions to NS_Yellow (full yellow duration = 2 ticks)
        sc.advanceState(6, QueueLengths());
        ASSERT_EQUAL(sc.getState(), SignalState::NS_Yellow);
        ASSERT_EQUAL(sc.getTicksRemaining(), 2);

        // Tick 7: In NS_Yellow (1 tick remaining)
        sc.advanceState(7, QueueLengths());
        ASSERT_EQUAL(sc.getState(), SignalState::NS_Yellow);
        ASSERT_EQUAL(sc.getTicksRemaining(), 1);

        // Tick 8: Transitions to All_Red_1 (fixed 1 tick)
        sc.advanceState(8, QueueLengths());
        ASSERT_EQUAL(sc.getState(), SignalState::All_Red_1);
        ASSERT_EQUAL(sc.getTicksRemaining(), 1);

        // Tick 9: Transitions to EW_Green!
        sc.advanceState(9, QueueLengths());
        ASSERT_EQUAL(sc.getState(), SignalState::EW_Green);

        // Process tick 9 in handler: preemption becomes Active
        eh.processTick(9);
        ASSERT_EQUAL(eh.getState(), EmergencyHandlerState::Active);
    }

    // Sub-case 2: Preemption requested during YELLOW
    {
        SignalTimings timings(SignalMode::Fixed, 10, 2, 5, 30, 1);
        SignalController sc(timings);
        VehicleQueue qNorth(10), qSouth(10), qEast(10), qWest(10);
        ApproachQueues queues(qNorth, qSouth, qEast, qWest);
        EmergencyHandler eh(sc, queues);

        // Advance to NS_Yellow (tick 11)
        for (int t = 1; t <= 11; ++t) {
            sc.advanceState(t, QueueLengths());
        }
        ASSERT_EQUAL(sc.getState(), SignalState::NS_Yellow);

        // Request preemption for EW during NS_Yellow
        eh.requestPreemption(Phase::EW, Direction::East, 11, 302, 11);

        // Yellow must complete its remaining ticks
        sc.advanceState(12, QueueLengths());
        ASSERT_EQUAL(sc.getState(), SignalState::NS_Yellow);

        // All_Red_1 must run (1 tick)
        sc.advanceState(13, QueueLengths());
        ASSERT_EQUAL(sc.getState(), SignalState::All_Red_1);

        // Then EW_Green begins
        sc.advanceState(14, QueueLengths());
        ASSERT_EQUAL(sc.getState(), SignalState::EW_Green);
    }

    // Sub-case 3: Preemption requested during ALL-RED
    {
        SignalTimings timings(SignalMode::Fixed, 10, 2, 5, 30, 1);
        SignalController sc(timings);
        VehicleQueue qNorth(10), qSouth(10), qEast(10), qWest(10);
        ApproachQueues queues(qNorth, qSouth, qEast, qWest);
        EmergencyHandler eh(sc, queues);

        // Advance to All_Red_1 (tick 13)
        for (int t = 1; t <= 13; ++t) {
            sc.advanceState(t, QueueLengths());
        }
        ASSERT_EQUAL(sc.getState(), SignalState::All_Red_1);

        // Request preemption for EW during All_Red_1
        eh.requestPreemption(Phase::EW, Direction::East, 13, 303, 13);

        // All_Red_1 completes its 1 tick
        sc.advanceState(14, QueueLengths());
        ASSERT_EQUAL(sc.getState(), SignalState::EW_Green);
    }

    // Sub-case 4: Same-phase yellow transition (SAD 553 / NFR-06)
    // If phase has just shown yellow, yellow -> all-red -> GREEN of same phase is allowed
    {
        SignalTimings timings(SignalMode::Fixed, 10, 2, 5, 30, 1);
        SignalController sc(timings);
        VehicleQueue qNorth(10), qSouth(10), qEast(10), qWest(10);
        ApproachQueues queues(qNorth, qSouth, qEast, qWest);
        EmergencyHandler eh(sc, queues);

        // Advance to NS_Yellow (tick 11)
        for (int t = 1; t <= 11; ++t) {
            sc.advanceState(t, QueueLengths());
        }
        ASSERT_EQUAL(sc.getState(), SignalState::NS_Yellow);

        // Preemption requested for Phase NS while in NS_Yellow
        eh.requestPreemption(Phase::NS, Direction::North, 11, 304, 11);

        // Yellow completes
        sc.advanceState(12, QueueLengths());
        ASSERT_EQUAL(sc.getState(), SignalState::NS_Yellow);

        // All_Red_1 completes (1 tick)
        sc.advanceState(13, QueueLengths());
        ASSERT_EQUAL(sc.getState(), SignalState::All_Red_1);

        // Transitions safely back to NS_Green!
        sc.advanceState(14, QueueLengths());
        ASSERT_EQUAL(sc.getState(), SignalState::NS_Green);
    }

    std::cout << "[PASS] TC-29: Safe transitions through green, yellow, and all-red never skip safety states" << std::endl;
}

// -----------------------------------------------------------------------------
// TC-30 / FR-30: Conflict resolution by earliest arrival tick then lower vehicle ID
// -----------------------------------------------------------------------------
void test_TC30_ConflictResolutionEarliestArrivalAndLowerId() {
    SignalTimings timings(SignalMode::Fixed, 10, 2, 5, 30, 1);

    // Sub-case 1: Earliest arrival tick wins
    {
        SignalController sc(timings);
        VehicleQueue qNorth(10), qSouth(10), qEast(10), qWest(10);
        ApproachQueues queues(qNorth, qSouth, qEast, qWest);
        EmergencyHandler eh(sc, queues);

        // Advance to All_Red_1 so neither NS nor EW is green
        for (int t = 1; t <= 13; ++t) {
            sc.advanceState(t, QueueLengths());
        }
        ASSERT_EQUAL(sc.getState(), SignalState::All_Red_1);

        // Conflicting emergencies:
        // North (Phase NS): arrival tick 10, ID 500
        // East (Phase EW): arrival tick 7, ID 600
        qNorth.enqueue(new Vehicle(500, Direction::North, true, 10));
        qEast.enqueue(new Vehicle(600, Direction::East, true, 7));

        EmergencyRequest req = eh.detectEmergency(13);
        ASSERT_TRUE(req.hasRequest);
        ASSERT_TRUE(req.phase == Phase::EW);
        ASSERT_TRUE(req.approach == Direction::East);
        ASSERT_EQUAL(req.arrivalTick, 7);
        ASSERT_EQUAL(req.vehicleId, 600);
    }

    // Sub-case 2: Equal arrival tick -> lower vehicle ID wins as tie-breaker
    {
        SignalController sc(timings);
        VehicleQueue qNorth(10), qSouth(10), qEast(10), qWest(10);
        ApproachQueues queues(qNorth, qSouth, qEast, qWest);
        EmergencyHandler eh(sc, queues);

        // Advance to All_Red_1
        for (int t = 1; t <= 13; ++t) {
            sc.advanceState(t, QueueLengths());
        }
        ASSERT_EQUAL(sc.getState(), SignalState::All_Red_1);

        // Same arrival tick (8), but North has ID 401 and East has ID 405
        qNorth.enqueue(new Vehicle(401, Direction::North, true, 8));
        qEast.enqueue(new Vehicle(405, Direction::East, true, 8));

        EmergencyRequest req = eh.detectEmergency(13);
        ASSERT_TRUE(req.hasRequest);
        ASSERT_TRUE(req.phase == Phase::NS);
        ASSERT_TRUE(req.approach == Direction::North);
        ASSERT_EQUAL(req.vehicleId, 401);
    }

    // Sub-case 3: Equal arrival tick -> East has lower ID
    {
        SignalController sc(timings);
        VehicleQueue qNorth(10), qSouth(10), qEast(10), qWest(10);
        ApproachQueues queues(qNorth, qSouth, qEast, qWest);
        EmergencyHandler eh(sc, queues);

        for (int t = 1; t <= 13; ++t) {
            sc.advanceState(t, QueueLengths());
        }
        ASSERT_EQUAL(sc.getState(), SignalState::All_Red_1);

        // Same arrival tick (8), but East has ID 400 and North has ID 402
        qNorth.enqueue(new Vehicle(402, Direction::North, true, 8));
        qEast.enqueue(new Vehicle(400, Direction::East, true, 8));

        EmergencyRequest req = eh.detectEmergency(13);
        ASSERT_TRUE(req.hasRequest);
        ASSERT_TRUE(req.phase == Phase::EW);
        ASSERT_TRUE(req.approach == Direction::East);
        ASSERT_EQUAL(req.vehicleId, 400);
    }

    std::cout << "[PASS] TC-30: Conflict resolution prioritizes earliest arrival then lower vehicle ID" << std::endl;
}

// -----------------------------------------------------------------------------
// TC-31 / FR-31: Maximum pre-emption duration and safe resumption
// -----------------------------------------------------------------------------
void test_TC31_MaxPreemptionDurationAndSafeResumption() {
    SignalTimings timings(SignalMode::Fixed, 10, 2, 5, 30, 1);
    SignalController sc(timings);
    VehicleQueue qNorth(10), qSouth(10), qEast(20), qWest(10);
    ApproachQueues queues(qNorth, qSouth, qEast, qWest);

    EmergencyConfig cfg(15); // Default 15 ticks max duration
    EmergencyHandler eh(sc, queues, cfg);

    // Queue 20 emergency vehicles on East approach
    for (int i = 0; i < 20; ++i) {
        qEast.enqueue(new Vehicle(700 + i, Direction::East, true, 1));
    }

    // Start at tick 1 in NS_Green
    sc.advanceState(1, QueueLengths());
    eh.processTick(1); // Detects East emergency and requests preemption for Phase EW
    ASSERT_EQUAL(eh.getState(), EmergencyHandlerState::Requested);

    // Advance until EW_Green is granted
    int currentTick = 2;
    while (sc.getState() != SignalState::EW_Green) {
        eh.processTick(currentTick);
        sc.advanceState(currentTick, QueueLengths());
        ++currentTick;
    }

    // First tick of EW_Green
    eh.processTick(currentTick);
    ASSERT_EQUAL(eh.getState(), EmergencyHandlerState::Active);
    ASSERT_EQUAL(eh.getPreemptionGreenTicks(), 1);

    // Advance through preemption green while emergencies remain in the queue.
    // Preemption must NOT run indefinitely: it must terminate when preemptionGreenTicks reaches 15.
    while (eh.getState() == EmergencyHandlerState::Active) {
        ++currentTick;
        eh.processTick(currentTick);
        sc.advanceState(currentTick, QueueLengths());
    }

    // Preemption has ended!
    ASSERT_EQUAL(eh.getState(), EmergencyHandlerState::Idle);

    // Controller must now proceed through safe resumption:
    // EW_Yellow -> All_Red_2 -> NS_Green
    ASSERT_TRUE(!sc.isPreemptionActive());
    ASSERT_EQUAL(sc.getState(), SignalState::EW_Yellow);
    ASSERT_EQUAL(sc.getTicksRemaining(), 2);

    // Step through yellow and all-red
    ++currentTick;
    sc.advanceState(currentTick, QueueLengths());
    ASSERT_EQUAL(sc.getState(), SignalState::EW_Yellow);

    ++currentTick;
    sc.advanceState(currentTick, QueueLengths());
    ASSERT_EQUAL(sc.getState(), SignalState::All_Red_2);
    ASSERT_EQUAL(sc.getTicksRemaining(), 1);

    ++currentTick;
    sc.advanceState(currentTick, QueueLengths());
    ASSERT_EQUAL(sc.getState(), SignalState::NS_Green);

    // Normal cycling has safely resumed and other approaches were not blocked indefinitely!
    std::cout << "[PASS] TC-31: Pre-emption ends at max duration (15 ticks) and safely resumes normal cycle" << std::endl;
}

// -----------------------------------------------------------------------------
// TC-32 / FR-32: EMERGENCY_START and EMERGENCY_END event logging
// -----------------------------------------------------------------------------
void test_TC32_EmergencyStartAndEndEvents() {
    SignalTimings timings(SignalMode::Fixed, 10, 2, 5, 30, 1);
    SignalController sc(timings);
    VehicleQueue qNorth(10), qSouth(10), qEast(10), qWest(10);
    ApproachQueues queues(qNorth, qSouth, qEast, qWest);

    EmergencyConfig cfg(15);
    EmergencyHandler eh(sc, queues, cfg);

    std::vector<EmergencyEvent> loggedEvents;
    eh.setEventListener([&loggedEvents](const EmergencyEvent& evt) {
        loggedEvents.push_back(evt);
    });

    // Enqueue 1 emergency vehicle on East approach
    qEast.enqueue(new Vehicle(888, Direction::East, true, 5));

    // Advance simulation to trigger preemption
    int tick = 1;
    sc.advanceState(tick, QueueLengths());
    eh.processTick(tick);

    while (sc.getState() != SignalState::EW_Green) {
        ++tick;
        eh.processTick(tick);
        sc.advanceState(tick, QueueLengths());
    }

    // EW_Green starts
    int greenStartTick = tick;
    eh.processTick(tick);
    ASSERT_EQUAL(eh.getState(), EmergencyHandlerState::Active);

    // Verify EMERGENCY_START was emitted
    ASSERT_TRUE(loggedEvents.size() >= 1);
    const EmergencyEvent& startEvt = loggedEvents[0];
    ASSERT_EQUAL(startEvt.tick, greenStartTick);
    ASSERT_EQUAL(startEvt.type, EmergencyEventType::EmergencyStart);
    ASSERT_EQUAL(startEvt.approach, Direction::East);
    ASSERT_EQUAL(startEvt.vehicleId, 888);

    // Discharge the emergency vehicle at this green tick
    std::vector<Vehicle*> departed;
    sc.dischargeVehicles(Direction::East, qEast, tick, departed);
    ASSERT_EQUAL(departed.size(), static_cast<std::size_t>(1));
    eh.recordServed(*departed[0]);
    delete departed[0];

    // Emergency queue is now empty. Green must continue until Gmin (5 ticks).
    while (eh.getState() == EmergencyHandlerState::Active) {
        ++tick;
        eh.processTick(tick);
        sc.advanceState(tick, QueueLengths());
    }

    // Verify EMERGENCY_END was emitted
    ASSERT_EQUAL(loggedEvents.size(), static_cast<std::size_t>(2));
    const EmergencyEvent& endEvt = loggedEvents[1];
    ASSERT_EQUAL(endEvt.type, EmergencyEventType::EmergencyEnd);
    ASSERT_EQUAL(endEvt.approach, Direction::East);
    ASSERT_EQUAL(endEvt.vehicleId, 888);

    // Verify event records via getEvents() and formatting helpers matching Appendix B.5
    const std::vector<EmergencyEvent>& internalEvents = eh.getEvents();
    ASSERT_EQUAL(internalEvents.size(), static_cast<std::size_t>(2));

    ASSERT_EQUAL(internalEvents[0].tick, greenStartTick);
    ASSERT_EQUAL(std::string(internalEvents[0].typeToString()), "EMERGENCY_START");
    ASSERT_EQUAL(internalEvents[0].approachToChar(), 'E');
    ASSERT_EQUAL(internalEvents[0].vehicleId, 888);

    ASSERT_EQUAL(std::string(internalEvents[1].typeToString()), "EMERGENCY_END");
    ASSERT_EQUAL(internalEvents[1].approachToChar(), 'E');
    ASSERT_EQUAL(internalEvents[1].vehicleId, 888);

    std::cout << "[PASS] TC-32: EMERGENCY_START and EMERGENCY_END events recorded with tick and approach" << std::endl;
}

// -----------------------------------------------------------------------------
// TC-33 / FR-33: Separate emergency waiting-time tracking
// -----------------------------------------------------------------------------
void test_TC33_SeparateEmergencyWaitingTimeTracking() {
    EmergencyStats stats;

    // Normal vehicles:
    // v1: arrival 0, departure 10, wait 10
    Vehicle v1(1, Direction::North, false, 0);
    v1.departureTick = 10;
    v1.waitingTime = 10;
    stats.recordServed(v1);

    // v2: arrival 5, departure 15, wait 10
    Vehicle v2(2, Direction::North, false, 5);
    v2.departureTick = 15;
    v2.waitingTime = 10;
    stats.recordServed(v2);

    // Emergency vehicles:
    // e1: arrival 8, departure 10, wait 2
    Vehicle e1(3, Direction::East, true, 8);
    e1.departureTick = 10;
    e1.waitingTime = 2;
    stats.recordServed(e1);

    // e2: arrival 10, departure 14, wait 4
    Vehicle e2(4, Direction::East, true, 10);
    e2.departureTick = 14;
    e2.waitingTime = 4;
    stats.recordServed(e2);

    // Verify separate tracking
    ASSERT_EQUAL(stats.normalServedCount, static_cast<std::size_t>(2));
    ASSERT_EQUAL(stats.normalWaitingTimeSum, static_cast<unsigned long long>(20));
    ASSERT_EQUAL(stats.maxNormalWaitingTime, 10);
    ASSERT_TRUE(std::abs(stats.getAverageNormalWaitingTime() - 10.0) < 1e-6);

    ASSERT_EQUAL(stats.emergencyServedCount, static_cast<std::size_t>(2));
    ASSERT_EQUAL(stats.emergencyWaitingTimeSum, static_cast<unsigned long long>(6));
    ASSERT_EQUAL(stats.maxEmergencyWaitingTime, 4);
    ASSERT_TRUE(std::abs(stats.getAverageEmergencyWaitingTime() - 3.0) < 1e-6);

    std::cout << "[PASS] TC-33: Emergency waiting time recorded and reported separately from normal vehicles" << std::endl;
}

int main() {
    std::cout << "Running Emergency Handler Unit Tests (TC-27 to TC-33)..." << std::endl;

    test_TC27_EmergencyQueuePriority();
    test_TC28_PerTickDetectionAndNonGreenOnly();
    test_TC29_SafeTransitionsGreenYellowAllRed();
    test_TC30_ConflictResolutionEarliestArrivalAndLowerId();
    test_TC31_MaxPreemptionDurationAndSafeResumption();
    test_TC32_EmergencyStartAndEndEvents();
    test_TC33_SeparateEmergencyWaitingTimeTracking();

    std::cout << "All Emergency Handler tests (TC-27 to TC-33) passed successfully!" << std::endl;
    return 0;
}
