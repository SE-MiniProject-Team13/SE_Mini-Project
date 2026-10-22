#ifndef EMERGENCY_HANDLER_H
#define EMERGENCY_HANDLER_H

#include "signal_controller.h"
#include "vehicle.h"
#include "vehicle_queue.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

// Helper to map an approach direction to its corresponding signal phase
inline Phase directionToPhase(Direction dir) {
    if (dir == Direction::North || dir == Direction::South) {
        return Phase::NS;
    }
    return Phase::EW;
}

// Wrapper structure providing access to the four intersection approach queues
struct ApproachQueues {
    VehicleQueue& north;
    VehicleQueue& south;
    VehicleQueue& east;
    VehicleQueue& west;

    ApproachQueues(VehicleQueue& n, VehicleQueue& s, VehicleQueue& e, VehicleQueue& w)
        : north(n), south(s), east(e), west(w) {}

    VehicleQueue& get(Direction dir) {
        switch (dir) {
            case Direction::North: return north;
            case Direction::South: return south;
            case Direction::East:  return east;
            case Direction::West:  return west;
        }
        return north;
    }

    const VehicleQueue& get(Direction dir) const {
        switch (dir) {
            case Direction::North: return north;
            case Direction::South: return south;
            case Direction::East:  return east;
            case Direction::West:  return west;
        }
        return north;
    }
};

// Emergency Handler States per SAD Section 553 / 606:
// IDLE: no emergency preemption requested or active.
// REQUESTED: emergency detected on non-green approach; signal controller is transitioning safely.
// ACTIVE: emergency preemption green is active for the requested phase.
enum class EmergencyHandlerState {
    Idle,
    Requested,
    Active
};

// Emergency configuration parameters per SRS FR-31 & SAD Section 616
struct EmergencyConfig {
    static const int DEFAULT_MAX_PREEMPTION_DURATION = 15; // SRS FR-31 default: 15 ticks

    int maxPreemptionDuration;

    explicit EmergencyConfig(int maxDuration = DEFAULT_MAX_PREEMPTION_DURATION)
        : maxPreemptionDuration(maxDuration) {}
};

// Information describing an emergency vehicle requesting preemption
struct EmergencyRequest {
    bool hasRequest;
    Phase phase;
    Direction approach;
    int arrivalTick;
    int vehicleId;

    EmergencyRequest()
        : hasRequest(false),
          phase(Phase::NS),
          approach(Direction::North),
          arrivalTick(0),
          vehicleId(0) {}

    EmergencyRequest(Phase p, Direction a, int arrival, int id)
        : hasRequest(true),
          phase(p),
          approach(a),
          arrivalTick(arrival),
          vehicleId(id) {}
};

// Event types for emergency preemption per SRS FR-32 & Appendix B.5
enum class EmergencyEventType {
    EmergencyStart,
    EmergencyEnd
};

// Emergency event record matching SRS FR-32 and Appendix B.5 layout
struct EmergencyEvent {
    int tick;
    EmergencyEventType type;
    Direction approach;
    Phase phase;
    int vehicleId;

    EmergencyEvent(int t, EmergencyEventType tp, Direction app, Phase p, int vid = 0)
        : tick(t), type(tp), approach(app), phase(p), vehicleId(vid) {}

    const char* typeToString() const {
        switch (type) {
            case EmergencyEventType::EmergencyStart: return "EMERGENCY_START";
            case EmergencyEventType::EmergencyEnd:   return "EMERGENCY_END";
        }
        return "UNKNOWN";
    }

    char approachToChar() const {
        switch (approach) {
            case Direction::North: return 'N';
            case Direction::South: return 'S';
            case Direction::East:  return 'E';
            case Direction::West:  return 'W';
        }
        return ' ';
    }
};

// Separate statistics accumulator for emergency and normal vehicles per SRS FR-33 & SAD 555
struct EmergencyStats {
    std::size_t emergencyServedCount;
    unsigned long long emergencyWaitingTimeSum;
    int maxEmergencyWaitingTime;
    std::size_t preemptionCount;

    // Normal vehicle counters kept separate
    std::size_t normalServedCount;
    unsigned long long normalWaitingTimeSum;
    int maxNormalWaitingTime;

    EmergencyStats()
        : emergencyServedCount(0),
          emergencyWaitingTimeSum(0),
          maxEmergencyWaitingTime(0),
          preemptionCount(0),
          normalServedCount(0),
          normalWaitingTimeSum(0),
          maxNormalWaitingTime(0) {}

    double getAverageEmergencyWaitingTime() const {
        return emergencyServedCount == 0
            ? 0.0
            : static_cast<double>(emergencyWaitingTimeSum) / static_cast<double>(emergencyServedCount);
    }

    double getAverageNormalWaitingTime() const {
        return normalServedCount == 0
            ? 0.0
            : static_cast<double>(normalWaitingTimeSum) / static_cast<double>(normalServedCount);
    }

    void recordServed(const Vehicle& vehicle) {
        if (vehicle.isEmergency) {
            ++emergencyServedCount;
            emergencyWaitingTimeSum += static_cast<unsigned long long>(vehicle.waitingTime);
            if (vehicle.waitingTime > maxEmergencyWaitingTime) {
                maxEmergencyWaitingTime = vehicle.waitingTime;
            }
        } else {
            ++normalServedCount;
            normalWaitingTimeSum += static_cast<unsigned long long>(vehicle.waitingTime);
            if (vehicle.waitingTime > maxNormalWaitingTime) {
                maxNormalWaitingTime = vehicle.waitingTime;
            }
        }
    }

    void reset() {
        emergencyServedCount = 0;
        emergencyWaitingTimeSum = 0;
        maxEmergencyWaitingTime = 0;
        preemptionCount = 0;
        normalServedCount = 0;
        normalWaitingTimeSum = 0;
        maxNormalWaitingTime = 0;
    }
};

class EmergencyHandler {
private:
    SignalController& controller;
    ApproachQueues queues;
    EmergencyConfig config;

    EmergencyHandlerState state;
    EmergencyRequest currentRequest;
    int preemptionGreenTicks;
    int preemptionStartTick;

    EmergencyStats stats;
    std::vector<EmergencyEvent> events;
    std::function<void(const EmergencyEvent&)> eventListener;

    int computeEffectiveMaxDuration() const;

public:
    EmergencyHandler(
        SignalController& signalController,
        ApproachQueues approachQueues,
        const EmergencyConfig& cfg = EmergencyConfig()
    );

    // Per-tick processing for Step 2 of simulation loop (SAD Section 554)
    bool processTick(int currentTick);

    // Per-tick emergency detection (FR-28, FR-30, SAD Section 4.3 / 553)
    EmergencyRequest detectEmergency(int currentTick);

    // Preemption lifecycle methods (FR-28, FR-29, FR-31, SAD Section 4.3 / 553)
    bool requestPreemption(
        Phase phase,
        Direction approach,
        int arrivalTick,
        int vehicleId,
        int currentTick
    );
    bool handlePreemption(int currentTick);
    void endPreemption(int currentTick);

    // State inspection
    EmergencyHandlerState getState() const;
    bool isPreemptionActive() const;
    const EmergencyRequest& getCurrentRequest() const;
    int getPreemptionGreenTicks() const;
    int getPreemptionStartTick() const;
    int getEffectiveMaxDuration() const;

    // Queue inspection helpers
    bool hasEmergencyOnPhase(Phase phase) const;
    bool hasEmergencyOnApproach(Direction approach) const;

    // Configuration
    void setConfig(const EmergencyConfig& cfg);
    const EmergencyConfig& getConfig() const;

    // Statistics and Events (FR-32, FR-33, SAD Section 553 / 555)
    const EmergencyStats& getStats() const;
    EmergencyStats& getStats();
    void recordServed(const Vehicle& vehicle);
    const std::vector<EmergencyEvent>& getEvents() const;
    void clearEvents();
    void setEventListener(std::function<void(const EmergencyEvent&)> listener);

    // Reset handler state
    void reset();

    // String conversion helper
    static const char* stateToString(EmergencyHandlerState st);
};

#endif // EMERGENCY_HANDLER_H
