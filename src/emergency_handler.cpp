#include "emergency_handler.h"

EmergencyHandler::EmergencyHandler(
    SignalController& signalController,
    ApproachQueues approachQueues,
    const EmergencyConfig& cfg
)
    : controller(signalController),
      queues(approachQueues),
      config(cfg),
      state(EmergencyHandlerState::Idle),
      currentRequest(),
      preemptionGreenTicks(0),
      preemptionStartTick(-1),
      stats(),
      events(),
      eventListener(nullptr) {}

int EmergencyHandler::computeEffectiveMaxDuration() const {
    const SignalTimings& timings = controller.getActiveTimings();
    int duration = config.maxPreemptionDuration;
    if (duration < timings.gmin) {
        duration = timings.gmin;
    }
    if (duration > timings.gmax) {
        duration = timings.gmax;
    }
    return duration;
}

EmergencyRequest EmergencyHandler::detectEmergency(int currentTick) {
    (void)currentTick;

    // In step 2 of every tick, examine front of each of the 4 approach queues (SAD 553 / FR-28)
    const Direction approaches[4] = {
        Direction::North,
        Direction::South,
        Direction::East,
        Direction::West
    };

    bool candidateFound = false;
    Direction bestApproach = Direction::North;
    Phase bestPhase = Phase::NS;
    int bestArrival = 0;
    int bestId = 0;

    for (int i = 0; i < 4; ++i) {
        Direction app = approaches[i];
        const VehicleQueue& q = queues.get(app);
        Vehicle* front = q.front();

        // FR-28: Emergency vehicle must be at front of queue and signal must not be GREEN
        if (front == nullptr || !front->isEmergency) {
            continue;
        }
        if (controller.getLightColor(app) == LightColor::Green) {
            continue;
        }

        Phase phase = directionToPhase(app);

        if (!candidateFound) {
            candidateFound = true;
            bestApproach = app;
            bestPhase = phase;
            bestArrival = front->arrivalTick;
            bestId = front->id;
        } else {
            // Conflict resolution per FR-30 / SAD 553:
            // Earliest arrival tick wins; tie-breaker: lower vehicle ID.
            if (front->arrivalTick < bestArrival) {
                bestApproach = app;
                bestPhase = phase;
                bestArrival = front->arrivalTick;
                bestId = front->id;
            } else if (front->arrivalTick == bestArrival && front->id < bestId) {
                bestApproach = app;
                bestPhase = phase;
                bestArrival = front->arrivalTick;
                bestId = front->id;
            }
        }
    }

    if (!candidateFound) {
        return EmergencyRequest();
    }

    return EmergencyRequest(bestPhase, bestApproach, bestArrival, bestId);
}

bool EmergencyHandler::requestPreemption(
    Phase phase,
    Direction approach,
    int arrivalTick,
    int vehicleId,
    int currentTick
) {
    currentRequest = EmergencyRequest(phase, approach, arrivalTick, vehicleId);
    state = EmergencyHandlerState::Requested;

    controller.acceptPreemption(phase);

    // If requested phase is already showing green, activate preemption immediately
    bool isGreen = (phase == Phase::NS && controller.getState() == SignalState::NS_Green) ||
                   (phase == Phase::EW && controller.getState() == SignalState::EW_Green);
    if (isGreen) {
        state = EmergencyHandlerState::Active;
        preemptionGreenTicks = 1;
        preemptionStartTick = currentTick;
        ++stats.preemptionCount;
        EmergencyEvent evt(
            currentTick,
            EmergencyEventType::EmergencyStart,
            approach,
            phase,
            vehicleId
        );
        events.push_back(evt);
        if (eventListener) {
            eventListener(evt);
        }
    }

    return true;
}

bool EmergencyHandler::handlePreemption(int currentTick) {
    if (state != EmergencyHandlerState::Active) {
        return false;
    }

    ++preemptionGreenTicks;

    int gmin = controller.getActiveTimings().gmin;
    int maxDur = computeEffectiveMaxDuration();
    bool emergencyWaiting = hasEmergencyOnPhase(currentRequest.phase);

    // FR-31 & SAD 553:
    // Preemption ends when no emergency vehicle remains on phase and at least Gmin ticks elapsed,
    // OR when the configured maximum preemption duration is reached.
    if ((!emergencyWaiting && preemptionGreenTicks >= gmin) || (preemptionGreenTicks >= maxDur)) {
        endPreemption(currentTick);
        return false;
    }

    return true;
}

void EmergencyHandler::endPreemption(int currentTick) {
    if (state == EmergencyHandlerState::Idle) {
        return;
    }

    // FR-32: Log EMERGENCY_END event with tick and approach
    EmergencyEvent evt(
        currentTick,
        EmergencyEventType::EmergencyEnd,
        currentRequest.approach,
        currentRequest.phase,
        currentRequest.vehicleId
    );
    events.push_back(evt);
    if (eventListener) {
        eventListener(evt);
    }

    state = EmergencyHandlerState::Idle;
    currentRequest = EmergencyRequest();
    preemptionGreenTicks = 0;
    preemptionStartTick = -1;

    // Resumes normal sequence with yellow -> all-red -> other phase (FR-31)
    controller.releasePreemption();
}

bool EmergencyHandler::processTick(int currentTick) {
    if (state == EmergencyHandlerState::Idle) {
        EmergencyRequest req = detectEmergency(currentTick);
        if (req.hasRequest) {
            requestPreemption(req.phase, req.approach, req.arrivalTick, req.vehicleId, currentTick);
        }
    } else if (state == EmergencyHandlerState::Requested) {
        bool isGreen = (currentRequest.phase == Phase::NS &&
                        controller.getState() == SignalState::NS_Green) ||
                       (currentRequest.phase == Phase::EW &&
                        controller.getState() == SignalState::EW_Green);
        if (isGreen) {
            state = EmergencyHandlerState::Active;
            preemptionGreenTicks = 1;
            preemptionStartTick = currentTick;
            ++stats.preemptionCount;
            EmergencyEvent evt(
                currentTick,
                EmergencyEventType::EmergencyStart,
                currentRequest.approach,
                currentRequest.phase,
                currentRequest.vehicleId
            );
            events.push_back(evt);
            if (eventListener) {
                eventListener(evt);
            }
        }
    } else if (state == EmergencyHandlerState::Active) {
        handlePreemption(currentTick);
    }

    return (state == EmergencyHandlerState::Active);
}

EmergencyHandlerState EmergencyHandler::getState() const {
    return state;
}

bool EmergencyHandler::isPreemptionActive() const {
    return state == EmergencyHandlerState::Requested || state == EmergencyHandlerState::Active;
}

const EmergencyRequest& EmergencyHandler::getCurrentRequest() const {
    return currentRequest;
}

int EmergencyHandler::getPreemptionGreenTicks() const {
    return preemptionGreenTicks;
}

int EmergencyHandler::getPreemptionStartTick() const {
    return preemptionStartTick;
}

int EmergencyHandler::getEffectiveMaxDuration() const {
    return computeEffectiveMaxDuration();
}

bool EmergencyHandler::hasEmergencyOnPhase(Phase phase) const {
    if (phase == Phase::NS) {
        return hasEmergencyOnApproach(Direction::North) ||
               hasEmergencyOnApproach(Direction::South);
    }
    return hasEmergencyOnApproach(Direction::East) ||
           hasEmergencyOnApproach(Direction::West);
}

bool EmergencyHandler::hasEmergencyOnApproach(Direction approach) const {
    const VehicleQueue& q = queues.get(approach);
    Vehicle* front = q.front();
    return (front != nullptr && front->isEmergency);
}

void EmergencyHandler::setConfig(const EmergencyConfig& cfg) {
    config = cfg;
}

const EmergencyConfig& EmergencyHandler::getConfig() const {
    return config;
}

const EmergencyStats& EmergencyHandler::getStats() const {
    return stats;
}

EmergencyStats& EmergencyHandler::getStats() {
    return stats;
}

void EmergencyHandler::recordServed(const Vehicle& vehicle) {
    stats.recordServed(vehicle);
}

const std::vector<EmergencyEvent>& EmergencyHandler::getEvents() const {
    return events;
}

void EmergencyHandler::clearEvents() {
    events.clear();
}

void EmergencyHandler::setEventListener(std::function<void(const EmergencyEvent&)> listener) {
    eventListener = listener;
}

void EmergencyHandler::reset() {
    state = EmergencyHandlerState::Idle;
    currentRequest = EmergencyRequest();
    preemptionGreenTicks = 0;
    preemptionStartTick = -1;
    stats.reset();
    events.clear();
}

const char* EmergencyHandler::stateToString(EmergencyHandlerState st) {
    switch (st) {
        case EmergencyHandlerState::Idle:      return "IDLE";
        case EmergencyHandlerState::Requested: return "REQUESTED";
        case EmergencyHandlerState::Active:    return "ACTIVE";
    }
    return "UNKNOWN";
}
