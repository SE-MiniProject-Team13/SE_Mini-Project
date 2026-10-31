#include "simulation.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>

namespace {

bool parseInteger(const char* text, int minimum, int maximum, int& value) {
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < minimum || parsed > maximum) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

bool validRunId(const std::string& value) {
    if (value.size() != 36) {
        return false;
    }
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (value[i] != '-') {
                return false;
            }
        } else if (!((value[i] >= '0' && value[i] <= '9') ||
                     (value[i] >= 'a' && value[i] <= 'f'))) {
            return false;
        }
    }
    return true;
}

const char* directionName(Direction direction) {
    switch (direction) {
        case Direction::North: return "North";
        case Direction::South: return "South";
        case Direction::East: return "East";
        case Direction::West: return "West";
    }
    return "Unknown";
}

const char* colorFor(SignalState state, Direction direction) {
    const bool northSouth =
        direction == Direction::North || direction == Direction::South;
    switch (state) {
        case SignalState::NS_Green: return northSouth ? "GREEN" : "RED";
        case SignalState::NS_Yellow: return northSouth ? "YELLOW" : "RED";
        case SignalState::All_Red_1:
        case SignalState::All_Red_2: return "RED";
        case SignalState::EW_Green: return northSouth ? "RED" : "GREEN";
        case SignalState::EW_Yellow: return northSouth ? "RED" : "YELLOW";
    }
    return "RED";
}

void printOptionalMetric(std::size_t value, bool defined) {
    if (defined) {
        std::cout << value;
    } else {
        std::cout << "null";
    }
}

void printOptionalAverage(double value, bool defined) {
    if (defined) {
        std::cout << value;
    } else {
        std::cout << "null";
    }
}

void writeJson(
    const std::string& runId,
    const std::string& mode,
    int seed,
    const TrafficSimulation& simulation
) {
    const SimulationStats stats = simulation.getStats();
    const std::vector<SimulationSnapshot>& history =
        simulation.getAnalyticsHistory();
    const std::vector<SimulationEvent>& events =
        simulation.getAnalyticsEvents();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "{\"runId\":\"" << runId
              << "\",\"status\":\"completed\",\"mode\":\"" << mode
              << "\",\"seed\":" << seed
              << ",\"summary\":{\"generated\":" << stats.generated
              << ",\"served\":" << stats.served
              << ",\"blocked\":" << stats.blocked
              << ",\"waiting\":" << stats.waiting
              << ",\"averageWaitingTime\":";
    printOptionalAverage(stats.averageWaitingTime, stats.served > 0);
    std::cout << ",\"maximumWaitingTime\":";
    printOptionalMetric(stats.maximumWaitingTime, stats.served > 0);
    std::cout << ",\"maximumQueueLength\":" << stats.maximumQueueLength
              << ",\"throughput\":";
    printOptionalAverage(stats.throughput, stats.elapsedTicks > 0);
    std::cout << ",\"emergencyServed\":" << stats.emergencyServed
              << ",\"averageEmergencyWaitingTime\":";
    printOptionalAverage(
        stats.averageEmergencyWaitingTime,
        stats.emergencyServed > 0
    );
    std::cout << ",\"maximumEmergencyWaitingTime\":";
    printOptionalMetric(
        stats.maximumEmergencyWaitingTime,
        stats.emergencyServed > 0
    );
    std::cout << ",\"preemptionCount\":" << stats.preemptionCount
              << ",\"vehicleCountInvariantPassed\":"
              << (stats.vehicleCountInvariantPassed ? "true" : "false")
              << ",\"signalSafetyPassed\":"
              << (stats.signalSafetyPassed ? "true" : "false")
              << ",\"elapsedTicks\":" << stats.elapsedTicks << "}"
              << ",\"timeSeries\":[";

    for (std::size_t i = 0; i < history.size(); ++i) {
        const SimulationSnapshot& point = history[i];
        if (i != 0) {
            std::cout << ',';
        }
        std::cout << "{\"tick\":" << point.tick
                  << ",\"state\":\""
                  << SignalController::stateToString(point.signalState)
                  << "\",\"preemptionActive\":"
                  << (point.preemptionActive ? "true" : "false")
                  << ",\"queues\":{\"North\":" << point.queues.north
                  << ",\"South\":" << point.queues.south
                  << ",\"East\":" << point.queues.east
                  << ",\"West\":" << point.queues.west
                  << "},\"signals\":{\"North\":\""
                  << colorFor(point.signalState, Direction::North)
                  << "\",\"South\":\""
                  << colorFor(point.signalState, Direction::South)
                  << "\",\"East\":\""
                  << colorFor(point.signalState, Direction::East)
                  << "\",\"West\":\""
                  << colorFor(point.signalState, Direction::West)
                  << "\"}}";
    }

    std::cout << "],\"events\":[";
    for (std::size_t i = 0; i < events.size(); ++i) {
        const SimulationEvent& event = events[i];
        if (i != 0) {
            std::cout << ',';
        }
        std::cout << "{\"tick\":" << event.tick
                  << ",\"vehicleId\":" << event.vehicleId
                  << ",\"type\":\"" << event.type
                  << "\",\"direction\":\""
                  << directionName(event.direction)
                  << "\",\"emergency\":"
                  << (event.emergency ? "true" : "false")
                  << ",\"waitingTime\":";
        if (event.waitingTime < 0) {
            std::cout << "null";
        } else {
            std::cout << event.waitingTime;
        }
        std::cout << '}';
    }
    std::cout << "]}\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 6 || std::string(argv[1]) != "--analytics") {
        std::cerr << "Usage: analytics [--analytics <runId> <ticks> "
                  << "<fixed|adaptive> <seed>]\n";
        return 2;
    }

    const std::string runId(argv[2]);
    int ticks = 0;
    int seed = 0;
    const std::string mode(argv[4]);
    if (!validRunId(runId) ||
        !parseInteger(argv[3], 0, 1000, ticks) ||
        (mode != "fixed" && mode != "adaptive") ||
        !parseInteger(argv[5], 0, 2147483647, seed)) {
        std::cerr << "Invalid analytics run arguments.\n";
        return 2;
    }

    SimulationConfig config(
        20, 8, 2, 1, ticks, mode == "adaptive", 5, 30, 1
    );
    TrafficSimulation simulation(config);
    std::mt19937 random(static_cast<unsigned int>(seed));
    std::uniform_int_distribution<int> arrivalRoll(0, 99);
    std::uniform_int_distribution<int> directionRoll(0, 3);
    std::uniform_int_distribution<int> emergencyRoll(0, 99);
    const Direction directions[4] = {
        Direction::North, Direction::South,
        Direction::East, Direction::West
    };

    for (int tick = 0; tick < ticks; ++tick) {
        const int roll = arrivalRoll(random);
        const int arrivals = roll < 45 ? 0 : (roll < 90 ? 1 : 2);
        for (int i = 0; i < arrivals; ++i) {
            const Direction direction = directions[directionRoll(random)];
            const bool emergency = emergencyRoll(random) < 10;
            simulation.addVehicle(direction, emergency);
        }
        simulation.step();
    }

    writeJson(runId, mode, seed, simulation);
    return simulation.checkVehicleCountInvariant() ? 0 : 1;
}
