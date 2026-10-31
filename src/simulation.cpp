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
      maximumWaitingTime(0),
      maximumEmergencyWaitingTime(0),
      maximumQueueLength(0),
      preemptionCount(0),
      previousPreemptionActive(false),
      previousPreemptionPhase(-1) {
    if (config.maxTicks < 0) {
        config.maxTicks = 0;
    }
    recordSnapshot();
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
        recordAnalyticsEvent("REJECTION", *vehicle, -1);
        delete vehicle;
        return false;
    }

    recordAnalyticsEvent("ARRIVAL", *vehicle, -1);
    if (queueFor(direction).size() > maximumQueueLength) {
        maximumQueueLength = queueFor(direction).size();
    }
    if (!analyticsHistory.empty() &&
        analyticsHistory.back().tick == currentTick) {
        SimulationSnapshot& snapshot = analyticsHistory.back();
        switch (direction) {
            case Direction::North: snapshot.queues.north = northQueue.size(); break;
            case Direction::South: snapshot.queues.south = southQueue.size(); break;
            case Direction::East: snapshot.queues.east = eastQueue.size(); break;
            case Direction::West: snapshot.queues.west = westQueue.size(); break;
        }
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

void TrafficSimulation::recordAnalyticsEvent(
    const char* type,
    const Vehicle& vehicle,
    int waitingTime
) {
    SimulationEvent event;
    event.tick = currentTick;
    event.vehicleId = vehicle.id;
    event.type = type;
    event.direction = vehicle.direction;
    event.emergency = vehicle.isEmergency;
    event.waitingTime = waitingTime;
    analyticsEvents.push_back(event);
}

void TrafficSimulation::recordSnapshot() {
    SimulationSnapshot snapshot;
    snapshot.tick = currentTick;
    snapshot.signalState = signal.getState();
    snapshot.queues = QueueLengths(
        northQueue.size(),
        southQueue.size(),
        eastQueue.size(),
        westQueue.size()
    );
    snapshot.preemptionActive = signal.isPreemptionActive();
    analyticsHistory.push_back(snapshot);

    const std::size_t queueSizes[4] = {
        snapshot.queues.north, snapshot.queues.south,
        snapshot.queues.east, snapshot.queues.west
    };
    for (int i = 0; i < 4; ++i) {
        if (queueSizes[i] > maximumQueueLength) {
            maximumQueueLength = queueSizes[i];
        }
    }
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
        if (signal.acceptPreemption(phase)) {
            ++preemptionCount;
            std::ostringstream message;
            message << "tick=" << currentTick
                    << " EMERGENCY_PREEMPTION approach="
                    << (phase == Phase::NS ? "North-South" : "East-West");
            eventLog.push_back(message.str());

            SimulationEvent event;
            event.tick = currentTick;
            event.vehicleId = selected->id;
            event.type = "PREEMPTION";
            event.direction = selected->direction;
            event.emergency = true;
            event.waitingTime = -1;
            analyticsEvents.push_back(event);
        }
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
                if (static_cast<std::size_t>(vehicle->waitingTime) > maximumWaitingTime) {
                    maximumWaitingTime = static_cast<std::size_t>(vehicle->waitingTime);
                }

                if (vehicle->isEmergency) {
                    totalEmergencyWaitingTime += vehicle->waitingTime;
                    ++emergencyServedCount;
                    if (static_cast<std::size_t>(vehicle->waitingTime) >
                        maximumEmergencyWaitingTime) {
                        maximumEmergencyWaitingTime =
                            static_cast<std::size_t>(vehicle->waitingTime);
                    }
                }

                recordAnalyticsEvent("DEPARTURE", *vehicle, vehicle->waitingTime);
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
        recordSnapshot();
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
    recordSnapshot();
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
    maximumWaitingTime = 0;
    maximumEmergencyWaitingTime = 0;
    maximumQueueLength = 0;
    preemptionCount = 0;
    eventLog.clear();
    analyticsEvents.clear();
    analyticsHistory.clear();
    previousPreemptionActive = false;
    previousPreemptionPhase = -1;
    recordSnapshot();
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

    stats.maximumWaitingTime = maximumWaitingTime;
    stats.maximumQueueLength = maximumQueueLength;
    stats.throughput =
        currentTick == 0
            ? 0.0
            : static_cast<double>(servedCount) /
                  static_cast<double>(currentTick);
    stats.emergencyServed = emergencyServedCount;

    stats.averageEmergencyWaitingTime =
        emergencyServedCount == 0
            ? 0.0
            : static_cast<double>(totalEmergencyWaitingTime) /
                  static_cast<double>(emergencyServedCount);

    stats.maximumEmergencyWaitingTime = maximumEmergencyWaitingTime;
    stats.preemptionCount = preemptionCount;
    stats.vehicleCountInvariantPassed = checkVehicleCountInvariant();
    stats.signalSafetyPassed = !signal.isSafetyViolationDetected();
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

const std::vector<SimulationEvent>& TrafficSimulation::getAnalyticsEvents() const {
    return analyticsEvents;
}

const std::vector<SimulationSnapshot>& TrafficSimulation::getAnalyticsHistory() const {
    return analyticsHistory;
}
