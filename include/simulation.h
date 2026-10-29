#ifndef SIMULATION_H
#define SIMULATION_H

#include "signal_controller.h"
#include "vehicle_queue.h"

#include <cstddef>
#include <string>
#include <vector>

struct SimulationConfig {
    std::size_t queueCapacity;
    int greenTicks;
    int yellowTicks;
    int allRedTicks;
    int minGreenTicks;
    int maxGreenTicks;
    int dischargeRate;
    bool adaptiveTiming;
    int maxTicks;

    SimulationConfig(std::size_t capacity = 20,
                     int green = 10,
                     int yellow = 2,
                     int allRed = 1,
                     int duration = 100,
                     bool adaptive = true,
                     int minGreen = 5,
                     int maxGreen = 30,
                     int discharge = 1)
        : queueCapacity(capacity),
          greenTicks(green),
          yellowTicks(yellow),
          allRedTicks(allRed),
          minGreenTicks(minGreen),
          maxGreenTicks(maxGreen),
          dischargeRate(discharge),
          adaptiveTiming(adaptive),
          maxTicks(duration) {}
};

struct SimulationStats {
    std::size_t generated;
    std::size_t served;
    std::size_t blocked;
    std::size_t waiting;
    double averageWaitingTime;
    std::size_t maximumWaitingTime;
    std::size_t maximumQueueLength;
    double throughput;
    std::size_t emergencyServed;
    double averageEmergencyWaitingTime;
    std::size_t maximumEmergencyWaitingTime;
    std::size_t preemptionCount;
    bool vehicleCountInvariantPassed;
    bool signalSafetyPassed;
    int elapsedTicks;

    SimulationStats()
        : generated(0), served(0), blocked(0), waiting(0),
          averageWaitingTime(0.0), maximumWaitingTime(0),
          maximumQueueLength(0), throughput(0.0), emergencyServed(0),
          averageEmergencyWaitingTime(0.0), maximumEmergencyWaitingTime(0),
          preemptionCount(0), vehicleCountInvariantPassed(true),
          signalSafetyPassed(true), elapsedTicks(0) {}
};

struct SimulationEvent {
    int tick;
    int vehicleId;
    std::string type;
    Direction direction;
    bool emergency;
    int waitingTime;
};

struct SimulationSnapshot {
    int tick;
    SignalState signalState;
    QueueLengths queues;
    bool preemptionActive;
};

class TrafficSimulation {
public:
    explicit TrafficSimulation(const SimulationConfig& config = SimulationConfig());
    ~TrafficSimulation();

    TrafficSimulation(const TrafficSimulation&) = delete;
    TrafficSimulation& operator=(const TrafficSimulation&) = delete;

    // Ownership transfers to the simulation. A rejected vehicle is deleted.
    bool addVehicle(Direction direction, bool emergency = false);
    void step();
    void run(int ticks);
    void reset();

    int getCurrentTick() const;
    const SignalController& getSignalController() const;
    SignalController& getSignalController();
    SimulationStats getStats() const;
    std::size_t getQueueSize(Direction direction) const;
    bool checkVehicleCountInvariant() const;
    const std::vector<std::string>& getEventLog() const;
    const std::vector<SimulationEvent>& getAnalyticsEvents() const;
    const std::vector<SimulationSnapshot>& getAnalyticsHistory() const;

private:
    SimulationConfig config;
    SignalController signal;
    VehicleQueue northQueue;
    VehicleQueue southQueue;
    VehicleQueue eastQueue;
    VehicleQueue westQueue;

    int currentTick;
    int nextVehicleId;
    std::size_t generatedCount;
    std::size_t servedCount;
    std::size_t blockedCount;
    long long totalWaitingTime;
    long long totalEmergencyWaitingTime;
    std::size_t emergencyServedCount;
    std::size_t maximumWaitingTime;
    std::size_t maximumEmergencyWaitingTime;
    std::size_t maximumQueueLength;
    std::size_t preemptionCount;
    std::vector<std::string> eventLog;
    std::vector<SimulationEvent> analyticsEvents;
    std::vector<SimulationSnapshot> analyticsHistory;
    bool previousPreemptionActive;
    int previousPreemptionPhase;

    VehicleQueue& queueFor(Direction direction);
    const VehicleQueue& queueFor(Direction direction) const;
    int phaseFor(Direction direction) const;
    bool emergencyWaitingForPhase(int phase) const;
    void detectAndRequestEmergency();
    void recordSnapshot();
    void recordAnalyticsEvent(const char* type, const Vehicle& vehicle, int waitingTime);
    void dischargeGreenApproaches();
    void recordPreemptionEvents();
    void recordSafetyFailure();
};

#endif // SIMULATION_H
