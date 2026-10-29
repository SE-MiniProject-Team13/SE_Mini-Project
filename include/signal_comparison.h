#ifndef SIGNAL_COMPARISON_H
#define SIGNAL_COMPARISON_H

#include "simulation.h"

#include <iosfwd>
#include <vector>

struct ScheduledArrival {
    int tick;
    int sequenceId;
    Direction direction;
    bool emergency;

    bool operator==(const ScheduledArrival& other) const {
        return tick == other.tick &&
               sequenceId == other.sequenceId &&
               direction == other.direction &&
               emergency == other.emergency;
    }

    bool operator!=(const ScheduledArrival& other) const {
        return !(*this == other);
    }
};

struct ConsumedArrival {
    ScheduledArrival arrival;
    bool accepted;
};

struct SignalModeRun {
    SignalMode mode;
    SimulationStats stats;
    std::vector<ConsumedArrival> arrivalTrace;
    std::vector<SimulationEvent> events;
};

struct SignalControlComparison {
    int seed;
    std::vector<ScheduledArrival> schedule;
    SignalModeRun fixed;
    SignalModeRun adaptive;
};

std::vector<ScheduledArrival> generateArrivalSchedule(
    int ticks,
    int seed
);

SignalModeRun runScheduledSimulation(
    const SimulationConfig& config,
    SignalMode mode,
    const std::vector<ScheduledArrival>& schedule
);

SignalControlComparison compareSignalModes(
    const SimulationConfig& config,
    int seed
);

void printComparison(
    const SignalControlComparison& comparison,
    std::ostream& output
);

void printSingleRun(const SignalModeRun& run, std::ostream& output);

#endif
