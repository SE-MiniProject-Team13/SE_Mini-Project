#include "signal_controller.h"

SignalController::SignalController(const SignalTimings& timings)
    : initialTimings(timings),
      activeTimings(timings),
      pendingTimings(timings),
      hasPendingTimingChanges(false),
      currentState(SignalState::NS_Green),
      ticksRemaining(0),
      cycleCount(0),
      preemptionActive(false),
      preemptedPhase(Phase::NS),
      safetyViolationDetected(false),
      isFirstTick(true),
      ticksInCurrentState(0) {
    initialTimings.allRedTime = SignalTimings::ALL_RED_TICKS;
    activeTimings.allRedTime = SignalTimings::ALL_RED_TICKS;
    pendingTimings.allRedTime = SignalTimings::ALL_RED_TICKS;
    ticksRemaining = determineGreenDuration(SignalState::NS_Green, QueueLengths());
}

bool SignalController::advanceState(int tick, const QueueLengths& queueLengths) {
    (void)tick;

    if (isFirstTick) {
        isFirstTick = false;
        ticksInCurrentState = 1;
        checkSafety();
        return false;
    }

    ++ticksInCurrentState;

    // During active preemption on a green phase, maintain green until releasePreemption()
    if (preemptionActive &&
        ((currentState == SignalState::NS_Green && preemptedPhase == Phase::NS) ||
         (currentState == SignalState::EW_Green && preemptedPhase == Phase::EW))) {
        checkSafety();
        return false;
    }

    if (ticksRemaining <= 1) {
        transitionToNextState(queueLengths);
        checkSafety();
        return true;
    }

    --ticksRemaining;
    checkSafety();
    return false;
}

bool SignalController::advanceState(const QueueLengths& queueLengths) {
    return advanceState(0, queueLengths);
}

void SignalController::transitionToNextState(const QueueLengths& queueLengths) {
    ticksInCurrentState = 0;
    switch (currentState) {
        case SignalState::NS_Green:
            currentState = SignalState::NS_Yellow;
            ticksRemaining = activeTimings.yellowTime;
            break;

        case SignalState::NS_Yellow:
            currentState = SignalState::All_Red_1;
            ticksRemaining = SignalTimings::ALL_RED_TICKS;
            break;

        case SignalState::All_Red_1:
            if (preemptionActive && preemptedPhase == Phase::NS) {
                // Return to NS_Green for emergency preemption after safe All-Red (SAD Section 553 / FR-29)
                currentState = SignalState::NS_Green;
                ticksRemaining = activeTimings.gmax;
            } else {
                currentState = SignalState::EW_Green;
                ticksRemaining = determineGreenDuration(SignalState::EW_Green, queueLengths);
            }
            break;

        case SignalState::EW_Green:
            currentState = SignalState::EW_Yellow;
            ticksRemaining = activeTimings.yellowTime;
            break;

        case SignalState::EW_Yellow:
            currentState = SignalState::All_Red_2;
            ticksRemaining = SignalTimings::ALL_RED_TICKS;
            break;

        case SignalState::All_Red_2:
            if (preemptionActive && preemptedPhase == Phase::EW) {
                // Return to EW_Green for emergency preemption after safe All-Red (SAD Section 553 / FR-29)
                currentState = SignalState::EW_Green;
                ticksRemaining = activeTimings.gmax;
            } else {
                currentState = SignalState::NS_Green;
                ++cycleCount;
                if (hasPendingTimingChanges) {
                    activeTimings = pendingTimings;
                    hasPendingTimingChanges = false;
                }
                ticksRemaining = determineGreenDuration(SignalState::NS_Green, queueLengths);
            }
            break;
    }
}

int SignalController::determineGreenDuration(
    SignalState nextGreenState,
    const QueueLengths& queueLengths
) const {
    if (activeTimings.mode == SignalMode::Fixed) {
        return activeTimings.greenTime;
    }

    std::size_t waiting = 0;
    if (nextGreenState == SignalState::NS_Green) {
        waiting = queueLengths.north + queueLengths.south;
    } else if (nextGreenState == SignalState::EW_Green) {
        waiting = queueLengths.east + queueLengths.west;
    }

    // Formula per FR-22 & TC-22:
    // Green duration equals Gmin + ceil(waiting vehicles / 2), capped at Gmax.
    int added = static_cast<int>((waiting + 1) / 2);
    int duration = activeTimings.gmin + added;

    if (duration < activeTimings.gmin) {
        duration = activeTimings.gmin;
    }
    if (duration > activeTimings.gmax) {
        duration = activeTimings.gmax;
    }

    return duration;
}

int SignalController::determineGreenDuration(
    Phase phase,
    const QueueLengths& queueLengths
) const {
    if (phase == Phase::NS) {
        return determineGreenDuration(SignalState::NS_Green, queueLengths);
    } else {
        return determineGreenDuration(SignalState::EW_Green, queueLengths);
    }
}

LightColor SignalController::getLightColor(Direction approach) const {
    switch (currentState) {
        case SignalState::NS_Green:
            if (approach == Direction::North || approach == Direction::South) {
                return LightColor::Green;
            }
            return LightColor::Red;

        case SignalState::NS_Yellow:
            if (approach == Direction::North || approach == Direction::South) {
                return LightColor::Yellow;
            }
            return LightColor::Red;

        case SignalState::All_Red_1:
        case SignalState::All_Red_2:
            return LightColor::Red;

        case SignalState::EW_Green:
            if (approach == Direction::East || approach == Direction::West) {
                return LightColor::Green;
            }
            return LightColor::Red;

        case SignalState::EW_Yellow:
            if (approach == Direction::East || approach == Direction::West) {
                return LightColor::Yellow;
            }
            return LightColor::Red;
    }
    return LightColor::Red;
}

bool SignalController::checkSafety() {
    bool nsNonRed = (getLightColor(Direction::North) != LightColor::Red ||
                     getLightColor(Direction::South) != LightColor::Red);
    bool ewNonRed = (getLightColor(Direction::East) != LightColor::Red ||
                     getLightColor(Direction::West) != LightColor::Red);

    // BR-01 & FR-20: NS and EW must never be non-red (green/yellow) concurrently.
    if (nsNonRed && ewNonRed) {
        forceAllRed();
        safetyViolationDetected = true;
        return false;
    }
    return true;
}

void SignalController::forceAllRed() {
    currentState = SignalState::All_Red_1;
    ticksRemaining = SignalTimings::ALL_RED_TICKS;
}

bool SignalController::canDischarge(Direction approach) const {
    // BR-04 & FR-23: only GREEN allows discharge
    return getLightColor(approach) == LightColor::Green;
}

int SignalController::getDischargeRate() const {
    return activeTimings.dischargeRate;
}

int SignalController::dischargeVehicles(
    Direction approach,
    VehicleQueue& queue,
    int currentTick,
    std::vector<Vehicle*>& departedVehicles
) {
    if (!canDischarge(approach)) {
        return 0;
    }

    int discharged = 0;
    int limit = activeTimings.dischargeRate;

    while (discharged < limit && !queue.isEmpty()) {
        Vehicle* vehicle = queue.dequeue();
        if (vehicle != nullptr) {
            vehicle->departureTick = currentTick;
            vehicle->waitingTime = vehicle->departureTick - vehicle->arrivalTick;
            departedVehicles.push_back(vehicle);
            ++discharged;
        }
    }

    return discharged;
}

void SignalController::setTimings(const SignalTimings& newTimings) {
    SignalTimings sanitized = newTimings;
    // SRS FR-21: The All-Red interval is strictly fixed at 1 tick and cannot be altered.
    sanitized.allRedTime = SignalTimings::ALL_RED_TICKS;

    // FR-26 & SAD Section 4.3: Runtime timing changes are staged as pending
    // and applied strictly at the start of the next cycle (S6 -> S1 transition).
    pendingTimings = sanitized;
    hasPendingTimingChanges = true;
}

void SignalController::setMode(SignalMode newMode) {
    SignalTimings next = hasPendingTimingChanges ? pendingTimings : activeTimings;
    next.mode = newMode;
    setTimings(next);
}

SignalState SignalController::getState() const {
    return currentState;
}

Phase SignalController::getActivePhase() const {
    switch (currentState) {
        case SignalState::NS_Green:
        case SignalState::NS_Yellow:
        case SignalState::All_Red_2:
            return Phase::NS;
        case SignalState::EW_Green:
        case SignalState::EW_Yellow:
        case SignalState::All_Red_1:
            return Phase::EW;
    }
    return Phase::NS;
}

int SignalController::getTicksRemaining() const {
    return ticksRemaining;
}

int SignalController::getCycleCount() const {
    return cycleCount;
}

bool SignalController::isPreemptionActive() const {
    return preemptionActive;
}

bool SignalController::isSafetyViolationDetected() const {
    return safetyViolationDetected;
}

bool SignalController::hasPendingTimings() const {
    return hasPendingTimingChanges;
}

const SignalTimings& SignalController::getActiveTimings() const {
    return activeTimings;
}

const SignalTimings& SignalController::getPendingTimings() const {
    return hasPendingTimingChanges ? pendingTimings : activeTimings;
}

SignalStateRecord SignalController::getStateRecord() const {
    SignalStateRecord rec;
    rec.state = currentState;
    rec.stateName = stateToString(currentState);
    rec.ticksRemaining = ticksRemaining;
    rec.cycleCount = cycleCount;
    rec.preemptionActive = preemptionActive;
    rec.northLight = getLightColor(Direction::North);
    rec.southLight = getLightColor(Direction::South);
    rec.eastLight = getLightColor(Direction::East);
    rec.westLight = getLightColor(Direction::West);
    return rec;
}

bool SignalController::acceptPreemption(Phase phase) {
    const bool newRequest = !preemptionActive || preemptedPhase != phase;
    preemptedPhase = phase;
    preemptionActive = true;

    if (!newRequest) {
        return false;
    }

    const bool opposingGreen =
        (currentState == SignalState::NS_Green && phase == Phase::EW) ||
        (currentState == SignalState::EW_Green && phase == Phase::NS);

    if (opposingGreen && ticksRemaining > 1) {
        ticksRemaining = 1;
        return true;
    }

    return false;
    preemptedPhase = phase;
    preemptionActive = true;

    // Safe transition coordination (FR-29, BR-03, SAD Section 553):
    // If the opposite phase is currently GREEN:
    // That green must continue until Gmin ticks have passed before transitioning to yellow.
    if ((phase == Phase::EW && currentState == SignalState::NS_Green) ||
        (phase == Phase::NS && currentState == SignalState::EW_Green)) {
        if (ticksInCurrentState < activeTimings.gmin) {
            ticksRemaining = activeTimings.gmin - ticksInCurrentState + 1;
        } else {
            ticksRemaining = 1;
        }
    }
    // If opposite phase is already in YELLOW or All-Red, let those complete their normal durations.
    return true;
}

void SignalController::releasePreemption() {
    preemptionActive = false;
    // When preemption green ends, transition immediately to yellow on the next advanceState tick
    // (FR-31, SAD Section 553: continues normal sequence with yellow -> all-red -> other phase)
    if (currentState == SignalState::NS_Green || currentState == SignalState::EW_Green) {
        ticksRemaining = 1;
    }
}

Phase SignalController::getPreemptedPhase() const {
    return preemptedPhase;
}

int SignalController::getTicksInCurrentState() const {
    return ticksInCurrentState;
}

void SignalController::reset() {
    activeTimings = initialTimings;
    pendingTimings = initialTimings;
    hasPendingTimingChanges = false;
    currentState = SignalState::NS_Green;
    cycleCount = 0;
    preemptionActive = false;
    preemptedPhase = Phase::NS;
    safetyViolationDetected = false;
    isFirstTick = true;
    ticksInCurrentState = 0;
    ticksRemaining = determineGreenDuration(SignalState::NS_Green, QueueLengths());
}

const char* SignalController::stateToString(SignalState state, bool distinguishAllRed) {
    switch (state) {
        case SignalState::NS_Green:  return "NS_GREEN";
        case SignalState::NS_Yellow: return "NS_YELLOW";
        case SignalState::All_Red_1: return distinguishAllRed ? "ALL_RED_1" : "ALL_RED";
        case SignalState::EW_Green:  return "EW_GREEN";
        case SignalState::EW_Yellow: return "EW_YELLOW";
        case SignalState::All_Red_2: return distinguishAllRed ? "ALL_RED_2" : "ALL_RED";
    }
    return "UNKNOWN";
}

const char* SignalController::colorToString(LightColor color) {
    switch (color) {
        case LightColor::Green:  return "GREEN";
        case LightColor::Yellow: return "YELLOW";
        case LightColor::Red:    return "RED";
    }
    return "UNKNOWN";
}

const char* SignalController::modeToString(SignalMode mode) {
    switch (mode) {
        case SignalMode::Fixed:    return "Fixed";
        case SignalMode::Adaptive: return "Adaptive";
    }
    return "UNKNOWN";
}
