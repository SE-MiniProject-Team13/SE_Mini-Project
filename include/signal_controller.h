#ifndef SIGNAL_CONTROLLER_H
#define SIGNAL_CONTROLLER_H

#include "vehicle.h"
#include "vehicle_queue.h"
#include "signal_mode.h"

#include <cstddef>
#include <string>
#include <vector>

enum class SignalState {
    NS_Green,
    NS_Yellow,
    All_Red_1,
    EW_Green,
    EW_Yellow,
    All_Red_2
};

enum class LightColor {
    Green,
    Yellow,
    Red
};

enum class Phase {
    NS,
    EW
};

struct SignalTimings {
    static const int ALL_RED_TICKS = 1; // SRS FR-21: fixed one-tick All-Red interval

    SignalMode mode;
    int greenTime;       // Base green time in ticks (default 10, range 1..60)
    int yellowTime;      // Yellow time in ticks (default 2, range 1..9)
    int allRedTime;      // Fixed 1 tick per FR-21 (not user-configurable)
    int gmin;            // Minimum green time (default 5)
    int gmax;            // Maximum green time (default 30)
    int dischargeRate;   // Vehicles per tick on green (default 1, range 1..5)

    explicit SignalTimings(
        SignalMode m = SignalMode::Fixed,
        int green = 10,
        int yellow = 2,
        int minG = 5,
        int maxG = 30,
        int discharge = 1
    )
        : mode(m),
          greenTime(green),
          yellowTime(yellow),
          allRedTime(ALL_RED_TICKS),
          gmin(minG),
          gmax(maxG),
          dischargeRate(discharge) {}
};

struct QueueLengths {
    std::size_t north;
    std::size_t south;
    std::size_t east;
    std::size_t west;

    QueueLengths(std::size_t n = 0, std::size_t s = 0, std::size_t e = 0, std::size_t w = 0)
        : north(n), south(s), east(e), west(w) {}

    std::size_t get(Direction dir) const {
        switch (dir) {
            case Direction::North: return north;
            case Direction::South: return south;
            case Direction::East:  return east;
            case Direction::West:  return west;
        }
        return 0;
    }
};

struct SignalStateRecord {
    SignalState state;
    std::string stateName;
    int ticksRemaining;
    int cycleCount;
    bool preemptionActive;
    LightColor northLight;
    LightColor southLight;
    LightColor eastLight;
    LightColor westLight;
};

class SignalController {
private:
    SignalTimings initialTimings;
    SignalTimings activeTimings;
    SignalTimings pendingTimings;
    bool hasPendingTimingChanges;

    SignalState currentState;
    int ticksRemaining;
    int cycleCount;
    bool preemptionActive;
    Phase preemptedPhase;
    bool safetyViolationDetected;
    bool isFirstTick;
    int ticksInCurrentState;

    void transitionToNextState(const QueueLengths& queueLengths);

public:
    explicit SignalController(const SignalTimings& timings = SignalTimings());

    // Core cycle progression (FR-19, FR-21, FR-22, FR-24, FR-25)
    bool advanceState(int tick, const QueueLengths& queueLengths);
    bool advanceState(const QueueLengths& queueLengths = QueueLengths());

    // Adaptive & Fixed green duration calculation (FR-21, FR-22, BR-03)
    int determineGreenDuration(SignalState nextGreenState, const QueueLengths& queueLengths) const;
    int determineGreenDuration(Phase phase, const QueueLengths& queueLengths) const;

    // Safety checks (FR-20, BR-01, NFR-05)
    bool checkSafety();
    void forceAllRed();
    bool isSafetyViolationDetected() const;

    // Vehicle discharge rules (FR-23, BR-04)
    bool canDischarge(Direction approach) const;
    int getDischargeRate() const;
    int dischargeVehicles(
        Direction approach,
        VehicleQueue& queue,
        int currentTick,
        std::vector<Vehicle*>& departedVehicles
    );

    // Timing changes applied at next cycle (FR-26, SAD Section 4.3)
    void setTimings(const SignalTimings& newTimings);
    void setMode(SignalMode newMode);
    bool hasPendingTimings() const;
    const SignalTimings& getActiveTimings() const;
    const SignalTimings& getPendingTimings() const;

    // State inspection (FR-24, Appendix B.4)
    SignalState getState() const;
    LightColor getLightColor(Direction approach) const;
    // Returns active phase (NS or EW). During All-Red transitions (All_Red_1, All_Red_2),
    // returns the upcoming phase preparing to receive green (All_Red_1 -> EW, All_Red_2 -> NS).
    Phase getActivePhase() const;
    int getTicksRemaining() const;
    int getTicksInCurrentState() const;
    int getCycleCount() const;
    bool isPreemptionActive() const;
    Phase getPreemptedPhase() const;
    SignalStateRecord getStateRecord() const;

    // Preemption interface hooks required by SAD Section 4.3 for future Emergency Handler integration.
    // Note: Full emergency vehicle priority and pre-emption sequencing (FR-27 to FR-33) are part of
    // FR-TMS-05 (Issue #38). SignalController provides these hooks to record preemption status.
    bool acceptPreemption(Phase phase);
    void releasePreemption();

    // Reset simulator state
    void reset();

    // Static string helpers
    static const char* stateToString(SignalState state, bool distinguishAllRed = true);
    static const char* colorToString(LightColor color);
    static const char* modeToString(SignalMode mode);
};

#endif // SIGNAL_CONTROLLER_H
