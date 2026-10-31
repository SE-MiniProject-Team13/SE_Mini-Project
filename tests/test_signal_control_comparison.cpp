#include "console_menu.h"
#include "signal_comparison.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

static bool sameConsumedTrace(
    const std::vector<ConsumedArrival>& left,
    const std::vector<ConsumedArrival>& right
) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (left[i].arrival != right[i].arrival) {
            return false;
        }
    }
    return true;
}

static void assertTraceMatchesSimulationEvents(const SignalModeRun& run) {
    std::size_t traceIndex = 0;
    for (std::size_t i = 0; i < run.events.size(); ++i) {
        const SimulationEvent& event = run.events[i];
        if (event.type != "ARRIVAL" && event.type != "REJECTION") {
            continue;
        }
        assert(traceIndex < run.arrivalTrace.size());
        const ConsumedArrival& consumed = run.arrivalTrace[traceIndex];
        assert(event.tick == consumed.arrival.tick);
        assert(event.vehicleId == consumed.arrival.sequenceId);
        assert(event.direction == consumed.arrival.direction);
        assert(event.emergency == consumed.arrival.emergency);
        assert(consumed.accepted == (event.type == "ARRIVAL"));
        ++traceIndex;
    }
    assert(traceIndex == run.arrivalTrace.size());
}

static bool sameStats(const SimulationStats& left, const SimulationStats& right) {
    return left.generated == right.generated &&
           left.served == right.served &&
           left.blocked == right.blocked &&
           left.waiting == right.waiting &&
           std::fabs(left.averageWaitingTime - right.averageWaitingTime) < 0.000001 &&
           left.maximumWaitingTime == right.maximumWaitingTime &&
           left.maximumQueueLength == right.maximumQueueLength &&
           std::fabs(left.throughput - right.throughput) < 0.000001 &&
           left.emergencyServed == right.emergencyServed &&
           left.averageEmergencyWaitingTime == right.averageEmergencyWaitingTime &&
           left.maximumEmergencyWaitingTime == right.maximumEmergencyWaitingTime &&
           left.preemptionCount == right.preemptionCount &&
           left.vehicleCountInvariantPassed == right.vehicleCountInvariantPassed &&
           left.signalSafetyPassed == right.signalSafetyPassed &&
           left.elapsedTicks == right.elapsedTicks;
}

static void testIdenticalArrivalScheduleAndIndependentRuns() {
    const SimulationConfig config(20, 10, 2, 1, 200, false, 5, 30, 1);
    const SignalControlComparison comparison = compareSignalModes(config, 8675309);

    assert(comparison.schedule.size() > 0);
    assert(comparison.fixed.mode == SignalMode::Fixed);
    assert(comparison.adaptive.mode == SignalMode::Adaptive);
    assert(comparison.fixed.arrivalTrace.size() == comparison.schedule.size());
    assert(comparison.adaptive.arrivalTrace.size() == comparison.schedule.size());
    assert(sameConsumedTrace(comparison.fixed.arrivalTrace,
                             comparison.adaptive.arrivalTrace));
    assertTraceMatchesSimulationEvents(comparison.fixed);
    assertTraceMatchesSimulationEvents(comparison.adaptive);
    assert(comparison.fixed.stats.generated == comparison.schedule.size());
    assert(comparison.adaptive.stats.generated == comparison.schedule.size());
    assert(comparison.fixed.stats.vehicleCountInvariantPassed);
    assert(comparison.adaptive.stats.vehicleCountInvariantPassed);
    assert(comparison.fixed.stats.signalSafetyPassed);
    assert(comparison.adaptive.stats.signalSafetyPassed);

    assert(&comparison.fixed.arrivalTrace != &comparison.adaptive.arrivalTrace);
    assert(&comparison.fixed.events != &comparison.adaptive.events);
    if (!comparison.fixed.arrivalTrace.empty()) {
        const bool adaptiveAccepted = comparison.adaptive.arrivalTrace[0].accepted;
        const std::size_t adaptiveEventCount = comparison.adaptive.events.size();
        SignalControlComparison independentCopy = comparison;
        independentCopy.fixed.arrivalTrace[0].accepted =
            !independentCopy.fixed.arrivalTrace[0].accepted;
        independentCopy.fixed.events.clear();
        assert(independentCopy.adaptive.arrivalTrace[0].accepted == adaptiveAccepted);
        assert(independentCopy.adaptive.events.size() == adaptiveEventCount);
    }
}

static void testKnownMetricsAndNoArrivalCase() {
    const SimulationConfig config(2, 2, 1, 1, 2, false, 1, 8, 1);
    std::vector<ScheduledArrival> schedule;
    const int ticks[] = {0, 0, 0, 1};
    const Direction directions[] = {
        Direction::North, Direction::North,
        Direction::North, Direction::North
    };
    for (int i = 0; i < 4; ++i) {
        ScheduledArrival arrival;
        arrival.tick = ticks[i];
        arrival.sequenceId = i + 1;
        arrival.direction = directions[i];
        arrival.emergency = false;
        schedule.push_back(arrival);
    }

    const SignalModeRun run =
        runScheduledSimulation(config, SignalMode::Fixed, schedule);
    assert(run.mode == SignalMode::Fixed);
    assert(run.stats.generated == 4);
    assert(run.stats.served == 2);
    assert(run.stats.blocked == 1);
    assert(run.stats.waiting == 1);
    assert(std::fabs(run.stats.averageWaitingTime - 0.5) < 0.001);
    assert(run.stats.maximumWaitingTime == 1);
    assert(run.stats.maximumQueueLength == 2);
    assert(std::fabs(run.stats.throughput - 1.0) < 0.001);
    assert(run.stats.vehicleCountInvariantPassed);

    const SimulationConfig emptyConfig(2, 2, 1, 1, 0, false, 1, 8, 1);
    const SignalControlComparison empty =
        compareSignalModes(emptyConfig, 19);
    assert(empty.schedule.empty());
    assert(empty.fixed.stats.generated == 0);
    assert(empty.fixed.stats.served == 0);
    assert(empty.fixed.stats.waiting == 0);
    assert(empty.fixed.stats.elapsedTicks == 0);
    std::ostringstream printed;
    printComparison(empty, printed);
    assert(printed.str().find("N/A") != std::string::npos);

    const SimulationConfig waitingConfig(2, 2, 1, 1, 1, false, 1, 8, 1);
    ScheduledArrival waitingArrival;
    waitingArrival.tick = 0;
    waitingArrival.sequenceId = 1;
    waitingArrival.direction = Direction::East;
    waitingArrival.emergency = false;
    const std::vector<ScheduledArrival> waitingSchedule(1, waitingArrival);
    const SignalModeRun waiting =
        runScheduledSimulation(waitingConfig, SignalMode::Fixed, waitingSchedule);
    assert(waiting.stats.served == 0);
    assert(waiting.stats.waiting == 1);
    assert(waiting.stats.maximumQueueLength == 1);
    assert(waiting.stats.vehicleCountInvariantPassed);

    ScheduledArrival invalidArrival = waitingArrival;
    invalidArrival.tick = 1;
    const std::vector<ScheduledArrival> invalidSchedule(1, invalidArrival);
    bool invalidScheduleRejected = false;
    try {
        runScheduledSimulation(waitingConfig, SignalMode::Fixed, invalidSchedule);
    } catch (const std::invalid_argument&) {
        invalidScheduleRejected = true;
    }
    assert(invalidScheduleRejected);
}

static void testRepeatabilityAndSeedVariation() {
    const SimulationConfig config(20, 10, 2, 1, 150, false, 5, 30, 1);
    const SignalControlComparison first = compareSignalModes(config, 31415);
    const SignalControlComparison repeated = compareSignalModes(config, 31415);
    const SignalControlComparison changedSeed = compareSignalModes(config, 31416);

    assert(first.schedule.size() == repeated.schedule.size());
    for (std::size_t i = 0; i < first.schedule.size(); ++i) {
        assert(first.schedule[i] == repeated.schedule[i]);
    }
    assert(sameConsumedTrace(first.fixed.arrivalTrace, repeated.fixed.arrivalTrace));
    assert(sameConsumedTrace(first.adaptive.arrivalTrace, repeated.adaptive.arrivalTrace));
    assert(sameStats(first.fixed.stats, repeated.fixed.stats));
    assert(sameStats(first.adaptive.stats, repeated.adaptive.stats));
    assert(first.schedule != changedSeed.schedule);
}

static std::size_t occurrences(
    const std::string& value,
    const std::string& needle
) {
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = value.find(needle, position)) != std::string::npos) {
        ++count;
        position += needle.size();
    }
    return count;
}

static void testMenuComparisonAndExistingOptions() {
    std::istringstream input(
        "invalid\n9\n1\n5\n20\n42\n5\n20\n42\n0\n"
    );
    std::ostringstream output;
    assert(runConsoleMenu(input, output) == 0);
    const std::string text = output.str();
    assert(text.find("Invalid menu input") != std::string::npos);
    assert(text.find("Unknown option") != std::string::npos);
    assert(text.find("Fixed signal-control run") != std::string::npos);
    assert(occurrences(text, "Fixed vs Adaptive Signal-Control Comparison") == 2);
    assert(occurrences(text, "Arrival attempts (vehicles)") == 2);
    assert(occurrences(text, "Adaptive") >= 3);
    assert(occurrences(text, "Select an option:") >= 5);

    std::istringstream adaptiveInput("2\n4\n0\n");
    std::ostringstream adaptiveOutput;
    assert(runConsoleMenu(adaptiveInput, adaptiveOutput) == 0);
    assert(adaptiveOutput.str().find("Adaptive signal-control run") !=
           std::string::npos);
    assert(adaptiveOutput.str().find("300 ticks") != std::string::npos);
}

int main() {
    testIdenticalArrivalScheduleAndIndependentRuns();
    testKnownMetricsAndNoArrivalCase();
    testRepeatabilityAndSeedVariation();
    testMenuComparisonAndExistingOptions();
    std::cout << "Fixed vs adaptive comparison tests passed.\n";
    return 0;
}
