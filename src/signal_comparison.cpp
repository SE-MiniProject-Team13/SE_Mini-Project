#include "signal_comparison.h"

#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

std::string modeName(SignalMode mode) {
    return mode == SignalMode::Fixed ? "Fixed" : "Adaptive";
}

std::string metricValue(std::size_t value, bool defined) {
    if (!defined) {
        return "N/A";
    }
    return std::to_string(value);
}

std::string averageValue(double value, bool defined) {
    if (!defined) {
        return "N/A";
    }
    std::ostringstream formatted;
    formatted << std::fixed << std::setprecision(2) << value;
    return formatted.str();
}

void printTable(
    const SignalModeRun& fixed,
    const SignalModeRun& adaptive,
    std::ostream& output
) {
    const SimulationStats& fixedStats = fixed.stats;
    const SimulationStats& adaptiveStats = adaptive.stats;
    output << std::left << std::setw(36) << "Metric"
           << std::right << std::setw(16) << "Fixed"
           << std::setw(16) << "Adaptive" << '\n'
           << std::string(68, '-') << '\n';

    const struct Row {
        const char* label;
        std::string fixedValue;
        std::string adaptiveValue;
    } rows[] = {
        {"Arrival attempts (vehicles)", metricValue(fixedStats.generated, true),
         metricValue(adaptiveStats.generated, true)},
        {"Vehicles served", metricValue(fixedStats.served, true),
         metricValue(adaptiveStats.served, true)},
        {"Vehicles rejected", metricValue(fixedStats.blocked, true),
         metricValue(adaptiveStats.blocked, true)},
        {"Vehicles still waiting", metricValue(fixedStats.waiting, true),
         metricValue(adaptiveStats.waiting, true)},
        {"Average wait (ticks/served vehicle)", averageValue(fixedStats.averageWaitingTime, fixedStats.served > 0),
         averageValue(adaptiveStats.averageWaitingTime, adaptiveStats.served > 0)},
        {"Maximum wait (ticks/served vehicle)", metricValue(fixedStats.maximumWaitingTime, fixedStats.served > 0),
         metricValue(adaptiveStats.maximumWaitingTime, adaptiveStats.served > 0)},
        {"Maximum queue/approach (vehicles)", metricValue(fixedStats.maximumQueueLength, true),
         metricValue(adaptiveStats.maximumQueueLength, true)},
        {"Throughput (vehicles/tick)", averageValue(fixedStats.throughput, fixedStats.elapsedTicks > 0),
         averageValue(adaptiveStats.throughput, adaptiveStats.elapsedTicks > 0)},
        {"Emergency served (vehicles)", metricValue(fixedStats.emergencyServed, true),
         metricValue(adaptiveStats.emergencyServed, true)},
        {"Actual pre-emptions (events)", metricValue(fixedStats.preemptionCount, true),
         metricValue(adaptiveStats.preemptionCount, true)}
    };

    for (std::size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i) {
        output << std::left << std::setw(36) << rows[i].label
               << std::right << std::setw(16) << rows[i].fixedValue
               << std::setw(16) << rows[i].adaptiveValue << '\n';
    }
    output << std::left << std::setw(36) << "Vehicle accounting"
           << std::right << std::setw(16)
           << (fixedStats.vehicleCountInvariantPassed ? "PASS" : "FAIL")
           << std::setw(16)
           << (adaptiveStats.vehicleCountInvariantPassed ? "PASS" : "FAIL")
           << '\n';
    output << std::left << std::setw(36) << "Signal safety"
           << std::right << std::setw(16)
           << (fixedStats.signalSafetyPassed ? "PASS" : "FAIL")
           << std::setw(16)
           << (adaptiveStats.signalSafetyPassed ? "PASS" : "FAIL")
           << '\n';
}

} // namespace

std::vector<ScheduledArrival> generateArrivalSchedule(int ticks, int seed) {
    std::vector<ScheduledArrival> schedule;
    if (ticks < 0) {
        ticks = 0;
    }

    std::mt19937 random(static_cast<unsigned int>(seed));
    std::uniform_int_distribution<int> arrivalRoll(0, 99);
    std::uniform_int_distribution<int> directionRoll(0, 3);
    std::uniform_int_distribution<int> emergencyRoll(0, 99);
    const Direction directions[4] = {
        Direction::North, Direction::South,
        Direction::East, Direction::West
    };
    int sequenceId = 1;

    for (int tick = 0; tick < ticks; ++tick) {
        const int roll = arrivalRoll(random);
        const int arrivals = roll < 45 ? 0 : (roll < 90 ? 1 : 2);
        for (int i = 0; i < arrivals; ++i) {
            ScheduledArrival arrival;
            arrival.tick = tick;
            arrival.sequenceId = sequenceId++;
            arrival.direction = directions[directionRoll(random)];
            arrival.emergency = emergencyRoll(random) < 10;
            schedule.push_back(arrival);
        }
    }
    return schedule;
}

SignalModeRun runScheduledSimulation(
    const SimulationConfig& config,
    SignalMode mode,
    const std::vector<ScheduledArrival>& schedule
) {
    const int ticks = config.maxTicks < 0 ? 0 : config.maxTicks;
    for (std::size_t i = 0; i < schedule.size(); ++i) {
        if (schedule[i].tick < 0 ||
            schedule[i].tick >= ticks ||
            schedule[i].sequenceId != static_cast<int>(i + 1) ||
            (i > 0 && schedule[i].tick < schedule[i - 1].tick)) {
            throw std::invalid_argument(
                "Arrival schedule must be ordered, in range, and sequentially identified"
            );
        }
    }

    SimulationConfig runConfig = config;
    runConfig.adaptiveTiming = mode == SignalMode::Adaptive;
    TrafficSimulation simulation(runConfig);

    SignalModeRun result;
    result.mode = simulation.getSignalController().getActiveTimings().mode;
    result.arrivalTrace.reserve(schedule.size());

    std::size_t nextArrival = 0;
    for (int tick = 0; tick < ticks; ++tick) {
        while (nextArrival < schedule.size() &&
               schedule[nextArrival].tick == tick) {
            const ScheduledArrival& arrival = schedule[nextArrival];
            const bool accepted =
                simulation.addVehicle(arrival.direction, arrival.emergency);
            ConsumedArrival consumed;
            consumed.arrival = arrival;
            consumed.accepted = accepted;
            result.arrivalTrace.push_back(consumed);
            ++nextArrival;
        }
        simulation.step();
    }

    result.stats = simulation.getStats();
    result.events = simulation.getAnalyticsEvents();
    return result;
}

SignalControlComparison compareSignalModes(
    const SimulationConfig& config,
    int seed
) {
    SignalControlComparison comparison;
    comparison.seed = seed;
    comparison.schedule = generateArrivalSchedule(config.maxTicks, seed);
    comparison.fixed =
        runScheduledSimulation(config, SignalMode::Fixed, comparison.schedule);
    comparison.adaptive =
        runScheduledSimulation(config, SignalMode::Adaptive, comparison.schedule);
    return comparison;
}

void printComparison(
    const SignalControlComparison& comparison,
    std::ostream& output
) {
    output << "\nFixed vs Adaptive Signal-Control Comparison\n"
           << "Seed: " << comparison.seed
           << " | Scheduled arrival attempts: " << comparison.schedule.size()
           << " | Duration: " << comparison.fixed.stats.elapsedTicks
           << " ticks\n\n";
    printTable(comparison.fixed, comparison.adaptive, output);
}

void printSingleRun(const SignalModeRun& run, std::ostream& output) {
    const SimulationStats& stats = run.stats;
    output << "\n" << modeName(run.mode) << " signal-control run\n"
           << "Elapsed ticks: " << stats.elapsedTicks << '\n'
           << "Arrival attempts: " << stats.generated << '\n'
           << "Vehicles served: " << stats.served << '\n'
           << "Vehicles rejected: " << stats.blocked << '\n'
           << "Vehicles still waiting: " << stats.waiting << '\n'
           << "Average waiting time: "
           << averageValue(stats.averageWaitingTime, stats.served > 0)
           << " ticks/served vehicle\n"
           << "Maximum waiting time: "
           << metricValue(stats.maximumWaitingTime, stats.served > 0)
           << " ticks/served vehicle\n"
           << "Maximum queue length (per approach): " << stats.maximumQueueLength
           << " vehicles\n"
           << "Throughput: "
           << averageValue(stats.throughput, stats.elapsedTicks > 0)
           << " vehicles/tick\n"
           << "Emergency vehicles served: " << stats.emergencyServed << '\n'
           << "Actual pre-emption events: " << stats.preemptionCount << '\n'
           << "Vehicle accounting: "
           << (stats.vehicleCountInvariantPassed ? "PASS" : "FAIL") << '\n'
           << "Signal safety: "
           << (stats.signalSafetyPassed ? "PASS" : "FAIL") << '\n';
}
