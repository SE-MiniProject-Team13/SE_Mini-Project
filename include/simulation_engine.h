#ifndef SIMULATION_ENGINE_H
#define SIMULATION_ENGINE_H

#include "config.h"
#include "event_logger.h"
#include "signal_controller.h"
#include "vehicle_generator.h"

#include <array>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

class SimulationEngine {
private:
    Config config;
    std::array<std::unique_ptr<VehicleQueue>, 4> queues;
    VehicleGenerator generator;
    SignalController signalController;
    EventLogger eventLogger;
    int currentTick;
    long long totalGenerated;
    long long emergencyGenerated;
    long long totalServed;
    long long emergencyServed;
    long long rejectedArrivals;
    long long totalWaitingTime;
    long long emergencyWaitingTime;
    int maximumWaitingTime;
    int preemptionCount;
    std::array<long long, 4> queueLengthTotals;
    std::array<std::size_t, 4> maximumQueueLengths;

    static SignalTimings makeSignalTimings(const Config& config);
    static std::size_t approachIndex(Direction direction);
    QueueLengths getQueueLengths() const;
    int advanceOneTick(std::ostream& output, bool quiet);
    void displayTick(std::ostream& output, const std::vector<Vehicle>& arrivals,
                     int servedThisTick) const;

public:
    explicit SimulationEngine(const Config& simulationConfig);

    // Run exactly the requested number of ticks. Step mode uses 1..100.
    void advanceTicks(int count, std::ostream& output, bool quiet = false);
    void runConfiguredDuration(std::ostream& output, bool quiet = false);
    void displaySummary(std::ostream& output) const;
    bool exportReport(const std::string& path, std::string& error) const;
    bool exportEventsCsv(const std::string& path, std::string& error) const;
    void reset();
    // Writes a read-only snapshot for the optional visual dashboard.
    void writeDashboardSnapshot() const;

    int getCurrentTick() const;
    long long getTotalGenerated() const;
    long long getTotalServed() const;
    long long getRejectedArrivals() const;
};

#endif
