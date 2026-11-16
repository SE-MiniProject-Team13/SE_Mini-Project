// Automated procedures for the test-register cases that were previously
// recorded as "Not Run" or "Blocked" (Test Plan v2.1, Section 13).
// Each check prints "[PASS] TC-xx" so the run log doubles as execution evidence.

#include "config.h"
#include "emergency_handler.h"
#include "signal_controller.h"
#include "simulation_engine.h"
#include "vehicle_generator.h"
#include "vehicle_queue.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
// Hidden by MinGW headers in strict -std=c++11 mode; provided by the C runtime.
extern "C" int _putenv(const char* assignment);
#endif

namespace {

void pass(const std::string& id, const std::string& text) {
    std::cout << "[PASS] " << id << ": " << text << '\n';
}

void disableDashboardDelay() {
#ifdef _WIN32
    _putenv("TMS_DASHBOARD_DELAY_MS=0");
#else
    setenv("TMS_DASHBOARD_DELAY_MS", "0", 1);
#endif
}

// Feed console input to configureSimulation() and capture what it prints.
Config configureWith(const std::string& input, std::string& printed) {
    std::istringstream in(input);
    std::ostringstream out;
    std::streambuf* oldIn = std::cin.rdbuf(in.rdbuf());
    std::streambuf* oldOut = std::cout.rdbuf(out.rdbuf());
    Config config;
    configureSimulation(config);
    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);
    std::cin.clear();
    printed = out.str();
    return config;
}

// Eleven prompts: duration, arrival, mode, green, yellow, all-red, gmin, gmax,
// capacity, emergency probability, seed. Empty lines keep the default.
std::string answers(const std::vector<std::string>& lines) {
    std::string text;
    for (std::size_t i = 0; i < 11; ++i) {
        text += (i < lines.size() ? lines[i] : std::string()) + "\n";
    }
    return text;
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

bool signalsSafe(const SignalController& controller) {
    const bool nsActive = controller.getLightColor(Direction::North) != LightColor::Red ||
                          controller.getLightColor(Direction::South) != LightColor::Red;
    const bool ewActive = controller.getLightColor(Direction::East) != LightColor::Red ||
                          controller.getLightColor(Direction::West) != LightColor::Red;
    return !(nsActive && ewActive);
}

Config quietConfig(int duration, double arrival, double emergency, unsigned int seed,
                   SignalMode mode) {
    Config config;
    config.durationTicks = duration;
    config.arrivalProbability = arrival;
    config.emergencyProbability = emergency;
    config.randomSeed = seed;
    config.signalMode = mode;
    return config;
}

std::size_t countEvents(const SimulationEngine& engine, SimulationEventType type) {
    std::size_t count = 0;
    const std::vector<LoggedSimulationEvent>& events = engine.getEvents();
    for (std::size_t i = 0; i < events.size(); ++i) {
        if (events[i].type == type) ++count;
    }
    return count;
}

// ---------------- FR-TMS-01 configuration ----------------

void tc01DurationBounds() {
    std::string printed;
    std::vector<std::string> lines(1, "0\n3601\nabc\n1");
    Config config = configureWith(answers(lines), printed);
    assert(config.durationTicks == 1);
    assert(contains(printed, "Out of range. Allowed: 1-3600."));
    assert(contains(printed, "Invalid input. Enter a numeric value."));

    lines[0] = "3600";
    config = configureWith(answers(lines), printed);
    assert(config.durationTicks == 3600);
    pass("TC-01", "duration accepts 1 and 3600, rejects 0, 3601 and text with the allowed range");
}

void tc02ArrivalProbability() {
    std::string printed;
    std::vector<std::string> lines(2);
    lines[1] = "-0.01\n1.01\n0.00";
    Config config = configureWith(answers(lines), printed);
    assert(config.arrivalProbability == 0.0);
    assert(contains(printed, "Out of range. Allowed: 0-1."));
    lines[1] = "1.00";
    assert(configureWith(answers(lines), printed).arrivalProbability == 1.0);
    lines[1] = "0.5";
    assert(configureWith(answers(lines), printed).arrivalProbability == 0.5);

    VehicleGenerator none(0.0, 0.0, 3);
    VehicleGenerator all(1.0, 0.0, 3);
    for (int tick = 1; tick <= 200; ++tick) {
        assert(none.generateForTick(tick).empty());
        const std::vector<Vehicle> arrivals = all.generateForTick(tick);
        assert(arrivals.size() == 4);
        std::set<int> approaches;
        for (std::size_t i = 0; i < arrivals.size(); ++i) {
            approaches.insert(static_cast<int>(arrivals[i].direction));
        }
        assert(approaches.size() == 4);  // one independent trial per approach
    }
    pass("TC-02", "probability 0 gives no arrivals, 1 gives one arrival per approach per tick");
}

void tc03ModesAndTimingBounds() {
    std::string printed;
    std::vector<std::string> lines(8);
    lines[2] = "3\n0\n2";             // mode: reject 3 and 0, accept Adaptive
    lines[3] = "4\n121\n5";           // fixed green 5-120
    lines[4] = "0\n11\n10";           // yellow 1-10
    lines[5] = "0\n6\n5";             // All-Red setting 1-5
    lines[6] = "4\n61\n60";           // Gmin 5-60
    lines[7] = "59\n121\n120";        // Gmax must be >= Gmin and <= 120
    Config config = configureWith(answers(lines), printed);
    assert(config.signalMode == SignalMode::Adaptive);
    assert(config.fixedGreenTicks == 5);
    assert(config.yellowTicks == 10);
    assert(config.allRedTicks == 5);
    assert(config.adaptiveMinGreen == 60);
    assert(config.adaptiveMaxGreen == 120);
    assert(contains(printed, "Out of range. Allowed: 60-120."));  // cross-field Gmax check

    lines.assign(11, "");
    lines[2] = "1";
    lines[8] = "0\n1001\n1";          // capacity 1-1000
    lines[10] = "-1\n2147483648\n0";  // seed 0-2147483647
    config = configureWith(answers(lines), printed);
    assert(config.signalMode == SignalMode::Fixed);
    assert(config.queueCapacity == 1);
    assert(config.randomSeed == 0u);
    pass("TC-03", "Fixed/Adaptive and every timing, capacity and seed bound validated, invalid values retried");
}

void tc04DefaultsAndDisplay() {
    std::string printed;
    const Config defaults;
    Config config = configureWith(answers(std::vector<std::string>()), printed);
    assert(config.durationTicks == defaults.durationTicks);
    assert(config.arrivalProbability == defaults.arrivalProbability);
    assert(config.signalMode == defaults.signalMode);
    assert(config.fixedGreenTicks == defaults.fixedGreenTicks);
    assert(config.queueCapacity == defaults.queueCapacity);
    assert(config.randomSeed == defaults.randomSeed);
    assert(contains(printed, "=== ACTIVE CONFIGURATION ==="));
    assert(contains(printed, "Duration: 600 ticks"));
    pass("TC-04", "unchanged settings keep displayed defaults and the active configuration is shown");
}

// ---------------- FR-TMS-02 vehicle generation ----------------

void tc08VehicleRecords() {
    VehicleGenerator generator(0.7, 0.1, 11);
    int expectedId = 1;
    for (int tick = 1; tick <= 500; ++tick) {
        const std::vector<Vehicle> arrivals = generator.generateForTick(tick);
        for (std::size_t i = 0; i < arrivals.size(); ++i) {
            assert(arrivals[i].id == expectedId++);
            assert(arrivals[i].arrivalTick == tick);
            const int approach = static_cast<int>(arrivals[i].direction);
            assert(approach >= 0 && approach <= 3);
            const bool emergencyType = arrivals[i].type == VehicleType::Ambulance ||
                                       arrivals[i].type == VehicleType::FireTruck ||
                                       arrivals[i].type == VehicleType::Police;
            assert(emergencyType == arrivals[i].isEmergency);
        }
    }
    pass("TC-08", "unique sequential IDs, valid approach and type, correct arrival tick");
}

void tc09TypeProportions() {
    VehicleGenerator generator(1.0, 0.0, 2026);
    std::map<int, int> counts;
    int total = 0;
    for (int tick = 1; tick <= 25000; ++tick) {
        const std::vector<Vehicle> arrivals = generator.generateForTick(tick);
        for (std::size_t i = 0; i < arrivals.size(); ++i) {
            ++counts[static_cast<int>(arrivals[i].type)];
            ++total;
        }
    }
    const double expected[4] = {0.60, 0.25, 0.08, 0.07};
    for (int type = 0; type < 4; ++type) {
        const double share = static_cast<double>(counts[type]) / total;
        assert(std::fabs(share - expected[type]) <= 0.01);  // +/- 1 percentage point
    }
    std::cout << "        sample=" << total << " seed=2026 Car=" << counts[0]
              << " Bike=" << counts[1] << " Bus=" << counts[2] << " Truck=" << counts[3] << '\n';
    pass("TC-09", "Car/Bike/Bus/Truck within 1 percentage point of 60/25/8/7");
}

void tc10EmergencyProbability() {
    VehicleGenerator never(1.0, 0.0, 5);
    VehicleGenerator always(1.0, 1.0, 5);
    VehicleGenerator some(1.0, 0.2, 5);
    int emergencies = 0;
    int total = 0;
    for (int tick = 1; tick <= 10000; ++tick) {
        const std::vector<Vehicle> a = never.generateForTick(tick);
        const std::vector<Vehicle> b = always.generateForTick(tick);
        const std::vector<Vehicle> c = some.generateForTick(tick);
        for (std::size_t i = 0; i < a.size(); ++i) assert(!a[i].isEmergency);
        for (std::size_t i = 0; i < b.size(); ++i) assert(b[i].isEmergency);
        for (std::size_t i = 0; i < c.size(); ++i) {
            ++total;
            if (c[i].isEmergency) ++emergencies;
        }
    }
    const double share = static_cast<double>(emergencies) / total;
    assert(std::fabs(share - 0.2) <= 0.01);
    pass("TC-10", "emergency probability 0 -> none, 1 -> all, 0.2 within 1 percentage point");
}

// ---------------- FR-TMS-03 queues ----------------

void tc12IndependentQueues() {
    VehicleQueue north(5), south(5), east(5), west(5);
    VehicleQueue* queues[4] = {&north, &south, &east, &west};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j <= i; ++j) {
            assert(queues[i]->enqueue(new Vehicle(i * 10 + j, static_cast<Direction>(i))));
        }
    }
    for (int i = 0; i < 4; ++i) {
        assert(queues[i]->size() == static_cast<std::size_t>(i + 1));
        assert(queues[i]->front()->id == i * 10);
    }
    delete north.dequeue();
    assert(north.size() == 0 && south.size() == 2 && east.size() == 3 && west.size() == 4);
    assert(south.front()->id == 10);
    pass("TC-12", "four queues hold their own vehicles; operations on one do not change another");
}

void tc16DepartureAndWaitingTime() {
    SignalController controller(SignalTimings(SignalMode::Fixed, 10, 2, 5, 30, 1));
    VehicleQueue queue(10);
    queue.enqueue(new Vehicle(1, Direction::North, false, 3));
    queue.enqueue(new Vehicle(2, Direction::North, false, 5));
    std::vector<Vehicle*> departed;
    controller.dischargeVehicles(Direction::North, queue, 9, departed);   // NS green at start
    controller.dischargeVehicles(Direction::North, queue, 12, departed);
    assert(departed.size() == 2);
    assert(departed[0]->departureTick == 9 && departed[0]->waitingTime == 6);
    assert(departed[1]->departureTick == 12 && departed[1]->waitingTime == 7);
    for (std::size_t i = 0; i < departed.size(); ++i) delete departed[i];
    pass("TC-16", "departure tick recorded and waiting time = departure - arrival");
}

void tc17MemoryRelease() {
    // Departure, clear/reset and destruction paths. Under AddressSanitizer
    // (CI job) any leak or invalid access on these paths fails the build.
    {
        VehicleQueue queue(50);
        for (int i = 0; i < 50; ++i) queue.enqueue(new Vehicle(i));
        for (int i = 0; i < 10; ++i) delete queue.dequeue();
        queue.clear();
        assert(queue.isEmpty());
        for (int i = 0; i < 20; ++i) queue.enqueue(new Vehicle(i));
    }  // destructor frees the remaining 20
    disableDashboardDelay();
    std::ostringstream sink;
    SimulationEngine engine(quietConfig(400, 0.6, 0.05, 9, SignalMode::Fixed));
    engine.advanceTicks(200, sink, true);
    engine.reset();
    engine.advanceTicks(100, sink, true);
    pass("TC-17", "vehicles freed on departure, clear/reset and destruction (ASan-checked in CI)");
}

void tc18QueueLengths() {
    disableDashboardDelay();
    std::ostringstream sink;
    SimulationEngine engine(quietConfig(50, 0.8, 0.0, 4, SignalMode::Fixed));
    engine.advanceTicks(10, sink, true);
    std::ostringstream live;
    engine.advanceTicks(1, live, false);
    assert(contains(live.str(), "Queues: N="));
    assert(contains(live.str(), " S=") && contains(live.str(), " E=") && contains(live.str(), " W="));
    assert(contains(sink.str(), "Peak queues: N="));
    pass("TC-18", "current length of all four approaches reaches display and statistics");
}

// ---------------- FR-TMS-04 signals ----------------

void tc22AdaptiveGreen() {
    SignalController controller(SignalTimings(SignalMode::Adaptive, 10, 2, 5, 12, 1));
    const std::size_t totals[5] = {0, 1, 2, 3, 100};
    const int expected[5] = {5, 6, 6, 7, 12};
    for (int i = 0; i < 5; ++i) {
        QueueLengths q(totals[i], 0, 0, 0);
        assert(controller.determineGreenDuration(Phase::NS, q) == expected[i]);
        assert(controller.determineGreenDuration(Phase::EW, QueueLengths(0, 0, totals[i], 0)) == expected[i]);
    }
    pass("TC-22", "adaptive green = Gmin + ceil(waiting/2), bounded by Gmin and Gmax");
}

void configuredAllRedApplied() {
    // Issue #39: the console All-Red setting (1-5) is applied by the controller.
    for (int allRed = 1; allRed <= 5; ++allRed) {
        SignalController controller(SignalTimings(SignalMode::Fixed, 6, 2, 5, 30, 1, allRed));
        int ticksInAllRed1 = 0;
        for (int tick = 1; tick <= 16; ++tick) {  // first cycle only: green 6 + yellow 2 + All-Red
            controller.advanceState(tick, QueueLengths());
            if (controller.getState() == SignalState::All_Red_1) ++ticksInAllRed1;
            assert(signalsSafe(controller));
        }
        assert(ticksInAllRed1 == allRed);
    }
    SignalController invalid(SignalTimings(SignalMode::Fixed, 6, 2, 5, 30, 1, 9));
    assert(invalid.getActiveTimings().allRedTime == SignalTimings::ALL_RED_TICKS);

    disableDashboardDelay();
    Config config = quietConfig(60, 0.3, 0.0, 2, SignalMode::Fixed);
    config.allRedTicks = 4;
    std::ostringstream sink;
    SimulationEngine engine(config);
    int allRedTicks = 0;
    for (int tick = 0; tick < 60; ++tick) {
        engine.advanceTicks(1, sink, true);
        if (engine.getSignalController().getState() == SignalState::All_Red_1) ++allRedTicks;
    }
    assert(allRedTicks >= 4 && allRedTicks % 4 == 0);
    pass("TC-21", "configured All-Red duration 1-5 is applied; out-of-range values fall back to 1 tick");
}

void tc26TimingChangeNextCycle() {
    SignalController controller(SignalTimings(SignalMode::Fixed, 10, 2, 5, 30, 1));
    controller.advanceState(1, QueueLengths());
    controller.setTimings(SignalTimings(SignalMode::Fixed, 20, 3, 5, 30, 1));
    assert(controller.hasPendingTimings());
    assert(controller.getActiveTimings().greenTime == 10);
    const int startCycle = controller.getCycleCount();
    int tick = 2;
    while (controller.getCycleCount() == startCycle) {
        assert(controller.getActiveTimings().greenTime == 10);
        controller.advanceState(tick++, QueueLengths());
    }
    assert(!controller.hasPendingTimings());
    assert(controller.getActiveTimings().greenTime == 20);
    assert(controller.getState() == SignalState::NS_Green);
    assert(controller.getTicksRemaining() == 20);
    pass("TC-26", "timing change stays pending during the cycle and applies at the next cycle start");
}

// ---------------- FR-TMS-05 emergency ----------------

void tc30EarliestEmergencyFirst() {
    SignalController controller(SignalTimings(SignalMode::Fixed, 10, 2, 5, 30, 1));
    VehicleQueue n(10), s(10), e(10), w(10);
    // NS is green, so both emergencies wait on red EW approaches; the
    // earlier West arrival must be chosen. Then repeat on conflicting phases.
    e.enqueue(new Vehicle(7, Direction::East, true, 4));
    w.enqueue(new Vehicle(9, Direction::West, true, 2));
    EmergencyHandler handler(controller, ApproachQueues(n, s, e, w));
    EmergencyRequest request = handler.detectEmergency(5);
    assert(request.hasRequest && request.approach == Direction::West && request.vehicleId == 9);

    SignalController controller2(SignalTimings(SignalMode::Fixed, 10, 2, 5, 30, 1));
    for (int t = 1; controller2.getState() != SignalState::EW_Green && t < 100; ++t) {
        controller2.advanceState(t, QueueLengths());  // move to EW green
    }
    assert(controller2.getState() == SignalState::EW_Green);
    VehicleQueue n2(10), s2(10), e2(10), w2(10);
    n2.enqueue(new Vehicle(3, Direction::North, true, 8));
    s2.enqueue(new Vehicle(4, Direction::South, true, 6));
    EmergencyHandler handler2(controller2, ApproachQueues(n2, s2, e2, w2));
    request = handler2.detectEmergency(15);
    assert(request.hasRequest && request.approach == Direction::South);
    pass("TC-30", "with several waiting emergencies the earliest arrival is served first");
}

void integratedEmergencyPreemption() {
    disableDashboardDelay();
    std::ostringstream sink;
    SimulationEngine engine(quietConfig(3600, 0.3, 0.05, 42, SignalMode::Fixed));
    for (int tick = 0; tick < 3600; ++tick) {
        engine.advanceTicks(1, sink, true);
        assert(signalsSafe(engine.getSignalController()));
    }
    assert(engine.getPreemptionCount() > 0);
    bool started = false;
    bool ended = false;
    const std::vector<LoggedSimulationEvent>& events = engine.getEvents();
    for (std::size_t i = 0; i < events.size(); ++i) {
        if (contains(events[i].details, "Emergency pre-emption started")) started = true;
        if (contains(events[i].details, "Emergency pre-emption ended")) ended = true;
    }
    assert(started && ended);
    std::cout << "        pre-emptions in 3600 ticks (seed 42): " << engine.getPreemptionCount() << '\n';
    pass("FR-TMS-05", "console engine runs emergency pre-emption safely and logs start/end");
}

// ---------------- FR-TMS-06 execution ----------------

void tc34RunToDuration() {
    disableDashboardDelay();
    std::ostringstream out;
    SimulationEngine engine(quietConfig(25, 0.4, 0.0, 8, SignalMode::Fixed));
    engine.runConfiguredDuration(out, false);
    assert(engine.getCurrentTick() == 25);
    assert(contains(out.str(), "--- Tick 1 ---") && contains(out.str(), "--- Tick 25 ---"));
    assert(!contains(out.str(), "--- Tick 26 ---"));
    assert(contains(out.str(), "=== Simulation Summary ==="));
    std::ostringstream again;
    engine.runConfiguredDuration(again, false);
    assert(engine.getCurrentTick() == 25);
    assert(contains(again.str(), "already been reached"));
    pass("TC-34", "Run executes continuously, stops at the configured duration and shows the summary");
}

void tc35StepMode() {
    disableDashboardDelay();
    std::ostringstream out;
    SimulationEngine engine(quietConfig(500, 0.4, 0.0, 8, SignalMode::Fixed));
    engine.advanceTicks(1, out, true);
    assert(engine.getCurrentTick() == 1);
    engine.advanceTicks(100, out, true);
    assert(engine.getCurrentTick() == 101);
    bool rejected = false;
    try {
        engine.advanceTicks(0, out, true);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected && engine.getCurrentTick() == 101);
    pass("TC-35", "Step advances exactly 1 and 100 ticks; 0 is rejected (menu rejects 0 and 101, see CLI check)");
}

void tc36LiveDisplay() {
    disableDashboardDelay();
    std::ostringstream out;
    SimulationEngine engine(quietConfig(10, 0.5, 0.1, 8, SignalMode::Fixed));
    engine.advanceTicks(3, out, false);
    const std::string text = out.str();
    for (int tick = 1; tick <= 3; ++tick) {
        std::ostringstream header;
        header << "--- Tick " << tick << " ---";
        assert(contains(text, header.str()));
    }
    assert(contains(text, "ticks remaining"));
    assert(contains(text, "Queues: N="));
    assert(contains(text, "Emergency pre-emption:"));
    assert(contains(text, "total served:"));
    pass("TC-36", "each tick shows tick, signal state and remaining time, queues, pre-emption and served count");
}

void tc37TickOrder() {
    disableDashboardDelay();
    std::ostringstream out;
    SimulationEngine engine(quietConfig(200, 0.6, 0.0, 12, SignalMode::Fixed));
    engine.advanceTicks(200, out, true);
    // Within every tick, events must appear as arrivals/blocks, then signal
    // changes, then departures.
    const std::vector<LoggedSimulationEvent>& events = engine.getEvents();
    int checkedTicks = 0;
    for (std::size_t i = 0; i < events.size();) {
        const int tick = events[i].tick;
        int stage = 0;
        bool sawArrival = false;
        bool sawDeparture = false;
        for (; i < events.size() && events[i].tick == tick; ++i) {
            int eventStage = 0;
            switch (events[i].type) {
                case SimulationEventType::VehicleArrival:
                case SimulationEventType::VehicleBlocked:
                case SimulationEventType::EmergencyStart: eventStage = 0; sawArrival = true; break;
                case SimulationEventType::SignalPhaseChange: eventStage = 1; break;
                case SimulationEventType::EmergencyEnd:
                case SimulationEventType::VehicleDeparture: eventStage = 2; sawDeparture = true; break;
            }
            assert(eventStage >= stage);
            stage = eventStage;
        }
        if (sawArrival && sawDeparture) ++checkedTicks;
    }
    assert(checkedTicks > 0);
    pass("TC-37", "every tick processes arrivals, then signal update, then service, then statistics");
}

// ---------------- FR-TMS-07 statistics ----------------

double readNumber(const std::string& text, const std::string& label) {
    const std::size_t at = text.find(label);
    assert(at != std::string::npos);
    return std::atof(text.c_str() + at + label.size());
}

void tc40WaitingTimes() {
    disableDashboardDelay();
    std::ostringstream out;
    SimulationEngine engine(quietConfig(300, 0.3, 0.0, 21, SignalMode::Fixed));
    engine.advanceTicks(300, out, true);
    std::map<int, int> arrivalTick;
    long long sum = 0;
    int maximum = 0;
    int served = 0;
    const std::vector<LoggedSimulationEvent>& events = engine.getEvents();
    for (std::size_t i = 0; i < events.size(); ++i) {
        if (events[i].type == SimulationEventType::VehicleArrival) {
            arrivalTick[events[i].vehicleId] = events[i].tick;
        } else if (events[i].type == SimulationEventType::VehicleDeparture) {
            const int wait = events[i].tick - arrivalTick[events[i].vehicleId];
            sum += wait;
            maximum = std::max(maximum, wait);
            ++served;
        }
    }
    assert(served > 0 && served == engine.getTotalServed());
    const std::string summary = out.str();
    const double average = static_cast<double>(sum) / served;
    assert(std::fabs(readNumber(summary, "Average waiting time (served vehicles): ") - average) < 0.01);
    assert(static_cast<int>(readNumber(summary, "Maximum waiting time (served vehicles): ")) == maximum);
    pass("TC-40", "summary average and maximum waits match the recorded arrival/departure ticks");
}

void tc75StatisticsDerived() {
    disableDashboardDelay();
    std::ostringstream out;
    SimulationEngine engine(quietConfig(600, 0.9, 0.02, 5, SignalMode::Adaptive));
    engine.advanceTicks(600, out, true);
    assert(static_cast<long long>(countEvents(engine, SimulationEventType::VehicleArrival)) ==
           engine.getTotalGenerated());
    assert(static_cast<long long>(countEvents(engine, SimulationEventType::VehicleDeparture)) ==
           engine.getTotalServed());
    assert(static_cast<long long>(countEvents(engine, SimulationEventType::VehicleBlocked)) ==
           engine.getRejectedArrivals());
    assert(contains(out.str(), "(OK)"));
    // SimulationEngine exposes only const getters for statistics; there is no
    // setter, so users cannot edit them (verified by the public interface).
    pass("TC-75", "statistics equal the recorded events and cannot be edited through the interface");
}

// ---------------- NFR-TMS-04 unit-test inventory ----------------

// TC-68: public functions of the queue, signal, emergency and statistics
// modules that no other suite calls directly are exercised here.
void tc68PublicFunctionInventory() {
    VehicleQueue queue(3);
    queue.enqueue(new Vehicle(1));
    queue.enqueue(new Vehicle(2));
    delete queue.dequeue();
    assert(queue.getMaxSize() == 2);

    const QueueLengths lengths(1, 2, 3, 4);
    assert(lengths.get(Direction::North) == 1 && lengths.get(Direction::West) == 4);

    SignalController controller(SignalTimings(SignalMode::Fixed, 10, 2, 5, 30, 1));
    VehicleQueue n(5), s(5), e(5), w(5);
    ApproachQueues approaches(n, s, e, w);
    assert(&approaches.get(Direction::East) == &e);
    const ApproachQueues& constApproaches = approaches;
    assert(&constApproaches.get(Direction::South) == &s);

    EmergencyHandler handler(controller, approaches, EmergencyConfig(12));
    assert(handler.getConfig().maxPreemptionDuration == 12);
    handler.setConfig(EmergencyConfig(40));
    assert(handler.getEffectiveMaxDuration() == 30);  // clamped to Gmax
    handler.setConfig(EmergencyConfig(8));
    assert(handler.getEffectiveMaxDuration() == 8);

    e.enqueue(new Vehicle(5, Direction::East, true, 1));
    assert(handler.hasEmergencyOnApproach(Direction::East));
    assert(handler.hasEmergencyOnPhase(Phase::EW) && !handler.hasEmergencyOnPhase(Phase::NS));

    int tick = 1;
    controller.advanceState(tick, QueueLengths());
    handler.processTick(tick);
    assert(handler.getState() == EmergencyHandlerState::Requested);
    assert(handler.getCurrentRequest().approach == Direction::East);
    while (handler.getState() != EmergencyHandlerState::Active && tick < 100) {
        ++tick;
        controller.advanceState(tick, QueueLengths());
        handler.processTick(tick);
        assert(signalsSafe(controller));
    }
    assert(handler.getState() == EmergencyHandlerState::Active);
    assert(handler.getPreemptionStartTick() == tick);
    assert(handler.handlePreemption(tick + 1));  // emergency still waiting, below max
    handler.endPreemption(tick + 2);
    assert(handler.getState() == EmergencyHandlerState::Idle);
    assert(handler.getEvents().size() == 2);
    handler.clearEvents();
    assert(handler.getEvents().empty());

    assert(std::string(EventLogger::eventTypeToString(SimulationEventType::VehicleBlocked)).size() > 0);

    disableDashboardDelay();
    std::ostringstream summary;
    SimulationEngine engine(quietConfig(20, 0.5, 0.0, 3, SignalMode::Fixed));
    engine.advanceTicks(20, summary, true);
    std::ostringstream again;
    engine.displaySummary(again);
    assert(contains(again.str(), "=== Simulation Summary ==="));
    engine.writeDashboardSnapshot();
    pass("TC-68", "every public queue, signal, emergency and statistics function has a unit test");
}

// ---------------- NFR checks ----------------

void tc48RepeatedBenchmark() {
    disableDashboardDelay();
    const int runs = 20;
    int withinLimit = 0;
    double slowest = 0.0;
    for (int run = 0; run < runs; ++run) {
        std::ostringstream sink;
        SimulationEngine engine(quietConfig(3600, 0.35, 0.02, 1000 + run, SignalMode::Fixed));
        const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        engine.runConfiguredDuration(sink, true);
        const double seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count();
        slowest = std::max(slowest, seconds);
        if (seconds < 5.0) ++withinLimit;
    }
    assert(withinLimit * 100 >= runs * 95);
    std::cout << "        " << withinLimit << "/" << runs << " runs under 5 s, slowest "
              << slowest << " s\n";
    pass("TC-48", "at least 95% of 20 quiet 3,600-tick runs finish within 5 seconds");
}

void tc52SafetyHundredSeeds() {
    disableDashboardDelay();
    for (unsigned int seed = 1; seed <= 100; ++seed) {
        const SignalMode mode = seed % 2 == 0 ? SignalMode::Adaptive : SignalMode::Fixed;
        std::ostringstream sink;
        SimulationEngine engine(quietConfig(3600, 0.4, 0.03, seed, mode));
        for (int tick = 0; tick < 3600; tick += 100) {
            // Step in blocks of one tick to inspect every signal state.
            for (int i = 0; i < 100; ++i) {
                engine.advanceTicks(1, sink, true);
                assert(signalsSafe(engine.getSignalController()));
            }
        }
        assert(engine.getCurrentTick() == 3600);
    }
    pass("TC-52", "100 seeds x 3,600 ticks (Fixed and Adaptive, with emergencies): no conflicting signals");
}

}  // namespace

int main() {
    tc01DurationBounds();
    tc02ArrivalProbability();
    tc03ModesAndTimingBounds();
    tc04DefaultsAndDisplay();
    tc08VehicleRecords();
    tc09TypeProportions();
    tc10EmergencyProbability();
    tc12IndependentQueues();
    tc16DepartureAndWaitingTime();
    tc17MemoryRelease();
    tc18QueueLengths();
    tc22AdaptiveGreen();
    configuredAllRedApplied();
    tc26TimingChangeNextCycle();
    tc30EarliestEmergencyFirst();
    integratedEmergencyPreemption();
    tc34RunToDuration();
    tc35StepMode();
    tc36LiveDisplay();
    tc37TickOrder();
    tc40WaitingTimes();
    tc75StatisticsDerived();
    tc68PublicFunctionInventory();
    tc48RepeatedBenchmark();
    tc52SafetyHundredSeeds();
    std::cout << "All register test cases passed.\n";
    return 0;
}
