#include "simulation.h"

#include <algorithm>
#include <sstream>

TrafficSimulation::TrafficSimulation(const SimulationConfig& simulationConfig)
    : config(simulationConfig),
      signal(SignalTimings(
          simulationConfig.adaptiveTiming ? SignalMode::Adaptive : SignalMode::Fixed,
          simulationConfig.greenTicks,
          simulationConfig.yellowTicks,
          simulationConfig.minGreenTicks,
          simulationConfig.maxGreenTicks,
          simulationConfig.dischargeRate
      )),
      northQueue(simulationConfig.queueCapacity),
      southQueue(simulationConfig.queueCapacity),
      eastQueue(simulationConfig.queueCapacity),
      westQueue(simulationConfig.queueCapacity),
      currentTick(0),
      nextVehicleId(1),
      generatedCount(0),
      servedCount(0),
      blockedCount(0),
      totalWaitingTime(0),
      totalEmergencyWaitingTime(0),
      emergencyServedCount(0),
      previousPreemptionActive(false),
      previousPreemptionPhase(-1) {
    if (config.maxTicks < 0) {
        config.maxTicks = 0;
    }
}

TrafficSimulation::~TrafficSimulation() {
    // VehicleQueue owns and deletes any vehicles still waiting in queues.
}

VehicleQueue& TrafficSimulation::queueFor(Direction direction) {
    switch (direction) {
        case Direction::North: return northQueue;
        case Direction::South: return southQueue;
        case Direction::East:  return eastQueue;
        case Direction::West:  return westQueue;
    }
    return northQueue;
}

const VehicleQueue& TrafficSimulation::queueFor(Direction direction) const {
    switch (direction) {
        case Direction::North: return northQueue;
        case Direction::South: return southQueue;
        case Direction::East:  return eastQueue;
        case Direction::West:  return westQueue;
    }
    return northQueue;
}

int TrafficSimulation::phaseFor(Direction direction) const {
    return (direction == Direction::North || direction == Direction::South)
               ? 0 : 1;
}

bool TrafficSimulation::addVehicle(Direction direction, bool emergency) {
    Vehicle* vehicle = new Vehicle(nextVehicleId++, direction, emergency,
                                   currentTick);
    ++generatedCount;

    if (!queueFor(direction).enqueue(vehicle)) {
        ++blockedCount;
        delete vehicle;
        return false;
    }

    if (emergency) {
        std::ostringstream message;
        message << "tick=" << currentTick
                << " EMERGENCY_ARRIVAL id=" << vehicle->id
                << " approach=";
        switch (direction) {
            case Direction::North: message << "North"; break;
            case Direction::South: message << "South"; break;
            case Direction::East:  message << "East"; break;
            case Direction::West:  message << "West"; break;
        }
        eventLog.push_back(message.str());
    }

    return true;
}

bool TrafficSimulation::emergencyWaitingForPhase(int phase) const {
    const Direction first = phase == 0 ? Direction::North : Direction::East;
    const Direction second = phase == 0 ? Direction::South : Direction::West;

    const Vehicle* firstVehicle = queueFor(first).front();
    const Vehicle* secondVehicle = queueFor(second).front();

    return (firstVehicle != nullptr && firstVehicle->isEmergency) ||
           (secondVehicle != nullptr && secondVehicle->isEmergency);
}

void TrafficSimulation::detectAndRequestEmergency() {
    const Vehicle* candidates[4] = {
        northQueue.front(), southQueue.front(),
        eastQueue.front(), westQueue.front()
    };

    const Vehicle* selected = nullptr;

    for (int i = 0; i < 4; ++i) {
        const Vehicle* candidate = candidates[i];

        if (candidate == nullptr || !candidate->isEmergency) {
            continue;
        }

        if (selected == nullptr ||
            candidate->arrivalTick < selected->arrivalTick ||
            (candidate->arrivalTick == selected->arrivalTick &&
             candidate->id < selected->id)) {
            selected = candidate;
        }
    }

    if (selected != nullptr) {
        Phase phase = (selected->direction == Direction::North || selected->direction == Direction::South)
                          ? Phase::NS : Phase::EW;
        signal.acceptPreemption(phase);
    } else if (signal.isPreemptionActive()) {
        signal.releasePreemption();
    }
}

void TrafficSimulation::dischargeGreenApproaches() {
    const Direction approaches[4] = {
        Direction::North, Direction::South,
        Direction::East,  Direction::West
    };

    for (int i = 0; i < 4; ++i) {
        Direction dir = approaches[i];
        if (signal.canDischarge(dir)) {
            std::vector<Vehicle*> departed;
            signal.dischargeVehicles(dir, queueFor(dir), currentTick, departed);

            for (std::size_t j = 0; j < departed.size(); ++j) {
                Vehicle* vehicle = departed[j];
                totalWaitingTime += vehicle->waitingTime;
                ++servedCount;

                if (vehicle->isEmergency) {
                    totalEmergencyWaitingTime += vehicle->waitingTime;
                    ++emergencyServedCount;
                }

                std::ostringstream message;
                message << "tick=" << currentTick
                        << " VEHICLE_DEPARTURE id=" << vehicle->id
                        << " waiting=" << vehicle->waitingTime;
                eventLog.push_back(message.str());

                delete vehicle;
            }
        }
    }
}

void TrafficSimulation::recordPreemptionEvents() {
    const bool active = signal.isPreemptionActive();
    int phase = -1;

    if (signal.getState() == SignalState::NS_Green) {
        phase = 0;
    } else if (signal.getState() == SignalState::EW_Green) {
        phase = 1;
    } else if (signal.isPreemptionActive()) {
        phase = (signal.getPreemptedPhase() == Phase::NS) ? 0 : 1;
    }

    if (active && !previousPreemptionActive) {
        std::ostringstream message;
        message << "tick=" << currentTick
                << " EMERGENCY_START approach="
                << (phase == 0 ? "North-South" : "East-West");

        eventLog.push_back(message.str());
    } else if (!active && previousPreemptionActive) {
        std::ostringstream message;
        message << "tick=" << currentTick
                << " EMERGENCY_END approach="
                << (previousPreemptionPhase == 0
                        ? "North-South" : "East-West");

        eventLog.push_back(message.str());
    }

    previousPreemptionActive = active;

    if (active && phase != -1) {
        previousPreemptionPhase = phase;
    }
}

void TrafficSimulation::recordSafetyFailure() {
    std::ostringstream message;
    message << "tick=" << currentTick
            << " INTERNAL_ERROR signal safety invariant violated";

    eventLog.push_back(message.str());
}

void TrafficSimulation::step() {
    if (currentTick >= config.maxTicks) {
        return;
    }

    detectAndRequestEmergency();

    // Check signal safety before allowing discharge
    if (!signal.checkSafety()) {
        recordSafetyFailure();
        QueueLengths qLengths(northQueue.size(), southQueue.size(),
                             eastQueue.size(), westQueue.size());
        signal.advanceState(currentTick, qLengths);
        ++currentTick;
        recordPreemptionEvents();
        return;
    }

    dischargeGreenApproaches();

    // Re-check emergency status after discharge
    detectAndRequestEmergency();

    // Advance controller state with current queue lengths
    QueueLengths qLengths(northQueue.size(), southQueue.size(),
                         eastQueue.size(), westQueue.size());
    signal.advanceState(currentTick, qLengths);

    // Enforce safety invariant
    signal.checkSafety();

    ++currentTick;
    recordPreemptionEvents();
}

void TrafficSimulation::run(int ticks) {
    if (ticks < 0) {
        ticks = 0;
    }

    for (int i = 0; i < ticks && currentTick < config.maxTicks; ++i) {
        step();
    }
}

void TrafficSimulation::reset() {
    northQueue.clear();
    southQueue.clear();
    eastQueue.clear();
    westQueue.clear();

    signal = SignalController(SignalTimings(
        config.adaptiveTiming ? SignalMode::Adaptive : SignalMode::Fixed,
        config.greenTicks,
        config.yellowTicks,
        config.minGreenTicks,
        config.maxGreenTicks,
        config.dischargeRate
    ));

    currentTick = 0;
    nextVehicleId = 1;
    generatedCount = 0;
    servedCount = 0;
    blockedCount = 0;
    totalWaitingTime = 0;
    totalEmergencyWaitingTime = 0;
    emergencyServedCount = 0;
    eventLog.clear();
    previousPreemptionActive = false;
    previousPreemptionPhase = -1;
}

int TrafficSimulation::getCurrentTick() const {
    return currentTick;
}

const SignalController& TrafficSimulation::getSignalController() const {
    return signal;
}

SignalController& TrafficSimulation::getSignalController() {
    return signal;
}

SimulationStats TrafficSimulation::getStats() const {
    SimulationStats stats;

    stats.generated = generatedCount;
    stats.served = servedCount;
    stats.blocked = blockedCount;
    stats.waiting = northQueue.size() + southQueue.size() +
                    eastQueue.size() + westQueue.size();

    stats.averageWaitingTime =
        servedCount == 0
            ? 0.0
            : static_cast<double>(totalWaitingTime) /
                  static_cast<double>(servedCount);

    stats.emergencyServed = emergencyServedCount;

    stats.averageEmergencyWaitingTime =
        emergencyServedCount == 0
            ? 0.0
            : static_cast<double>(totalEmergencyWaitingTime) /
                  static_cast<double>(emergencyServedCount);

    stats.elapsedTicks = currentTick;

    return stats;
}

std::size_t TrafficSimulation::getQueueSize(Direction direction) const {
    return queueFor(direction).size();
}

bool TrafficSimulation::checkVehicleCountInvariant() const {
    const std::size_t waiting =
        northQueue.size() + southQueue.size() +
        eastQueue.size() + westQueue.size();

    return generatedCount == servedCount + blockedCount + waiting;
}

const std::vector<std::string>& TrafficSimulation::getEventLog() const {
    return eventLog;
}
