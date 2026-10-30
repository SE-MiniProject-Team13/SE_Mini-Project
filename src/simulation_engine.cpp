#include "simulation_engine.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#define NOMINMAX
#include <windows.h>

namespace {
const Direction approaches[] = {
    Direction::North, Direction::South, Direction::East, Direction::West
};
}

SignalTimings SimulationEngine::makeSignalTimings(const Config& config) {
    return SignalTimings(
        config.signalMode,
        config.fixedGreenTicks,
        config.yellowTicks,
        config.adaptiveMinGreen,
        config.adaptiveMaxGreen,
        1
    );
}

std::size_t SimulationEngine::approachIndex(Direction direction) {
    switch (direction) {
        case Direction::North: return 0;
        case Direction::South: return 1;
        case Direction::East: return 2;
        case Direction::West: return 3;
    }
    return 0;
}

SimulationEngine::SimulationEngine(const Config& simulationConfig)
    : config(simulationConfig),
      queues(),
      generator(config.arrivalProbability, config.emergencyProbability, config.randomSeed),
      signalController(makeSignalTimings(config)),
      currentTick(0),
      totalGenerated(0),
      emergencyGenerated(0),
      totalServed(0),
      emergencyServed(0),
      rejectedArrivals(0),
      totalWaitingTime(0),
      emergencyWaitingTime(0),
      maximumWaitingTime(0),
      preemptionCount(0),
      queueLengthTotals(),
      maximumQueueLengths() {
    for (std::size_t index = 0; index < queues.size(); ++index) {
        queues[index].reset(new VehicleQueue(static_cast<std::size_t>(config.queueCapacity)));
        maximumQueueLengths[index] = 0;
    }
    writeDashboardSnapshot();
}

QueueLengths SimulationEngine::getQueueLengths() const {
    return QueueLengths(
        queues[0]->size(), queues[1]->size(), queues[2]->size(), queues[3]->size()
    );
}

int SimulationEngine::advanceOneTick(std::ostream& output, bool quiet) {
    const int nextTick = currentTick + 1;

    // Required FR-TMS-06 order: arrivals, signal update, service, statistics/display.
    std::vector<Vehicle> arrivals = generator.generateForTick(nextTick);
    for (std::vector<Vehicle>::const_iterator vehicle = arrivals.begin();
         vehicle != arrivals.end(); ++vehicle) {
        ++totalGenerated;
        if (vehicle->isEmergency) {
            ++emergencyGenerated;
        }
        eventLogger.record(nextTick, SimulationEventType::VehicleArrival,
                           vehicle->id, vehicle->type, vehicle->direction,
                           vehicle->isEmergency ? "Emergency vehicle arrived" : "Vehicle arrived");
        Vehicle* queuedVehicle = new Vehicle(*vehicle);
        VehicleQueue& queue = *queues[approachIndex(vehicle->direction)];
        if (!queue.enqueue(queuedVehicle)) {
            delete queuedVehicle;
            ++rejectedArrivals;
            eventLogger.record(nextTick, SimulationEventType::VehicleBlocked,
                               vehicle->id, vehicle->type, vehicle->direction,
                               "Queue is full; arrival rejected");
        } else if (vehicle->isEmergency) {
            eventLogger.record(nextTick, SimulationEventType::EmergencyStart,
                               vehicle->id, vehicle->type, vehicle->direction,
                               "Emergency vehicle entered the queue");
        }
    }

    const SignalState previousState = signalController.getState();
    signalController.advanceState(nextTick, getQueueLengths());
    const SignalState updatedState = signalController.getState();
    if (updatedState != previousState) {
        eventLogger.record(nextTick, SimulationEventType::SignalPhaseChange, -1,
                           VehicleType::Car, Direction::North,
                           std::string(SignalController::stateToString(previousState)) +
                               " -> " + SignalController::stateToString(updatedState));
    }
    int servedThisTick = 0;
    for (std::size_t index = 0; index < queues.size(); ++index) {
        maximumQueueLengths[index] = std::max(maximumQueueLengths[index], queues[index]->size());
    }
    std::vector<Vehicle*> departedVehicles;
    for (std::size_t index = 0; index < queues.size(); ++index) {
        signalController.dischargeVehicles(
            approaches[index], *queues[index], nextTick, departedVehicles
        );
    }

    for (std::vector<Vehicle*>::const_iterator vehicle = departedVehicles.begin();
         vehicle != departedVehicles.end(); ++vehicle) {
        ++servedThisTick;
        ++totalServed;
        totalWaitingTime += (*vehicle)->waitingTime;
        maximumWaitingTime = std::max(maximumWaitingTime, (*vehicle)->waitingTime);
        if ((*vehicle)->isEmergency) {
            ++emergencyServed;
            emergencyWaitingTime += (*vehicle)->waitingTime;
            eventLogger.record(nextTick, SimulationEventType::EmergencyEnd,
                               (*vehicle)->id, (*vehicle)->type, (*vehicle)->direction,
                               "Emergency vehicle departed the intersection");
        }
        eventLogger.record(nextTick, SimulationEventType::VehicleDeparture,
                           (*vehicle)->id, (*vehicle)->type, (*vehicle)->direction,
                           "Vehicle served by green signal");
        delete *vehicle;
    }

    currentTick = nextTick;
    for (std::size_t index = 0; index < queues.size(); ++index) {
        queueLengthTotals[index] += static_cast<long long>(queues[index]->size());
        maximumQueueLengths[index] = std::max(maximumQueueLengths[index], queues[index]->size());
    }

    writeDashboardSnapshot();
    if (!quiet) {
        displayTick(output, arrivals, servedThisTick);
    }
    return servedThisTick;
}

void SimulationEngine::displayTick(
    std::ostream& output,
    const std::vector<Vehicle>& arrivals,
    int servedThisTick
) const {
    output << "\n--- Tick " << currentTick << " ---\n";
    output << "Signal: " << SignalController::stateToString(signalController.getState())
           << " (" << signalController.getTicksRemaining() << " ticks remaining)\n";
    output << "Lights: N " << SignalController::colorToString(signalController.getLightColor(Direction::North))
           << " | S " << SignalController::colorToString(signalController.getLightColor(Direction::South))
           << " | E " << SignalController::colorToString(signalController.getLightColor(Direction::East))
           << " | W " << SignalController::colorToString(signalController.getLightColor(Direction::West)) << '\n';

    if (arrivals.empty()) {
        output << "Arrivals: none\n";
    } else {
        output << "Arrivals: ";
        for (std::size_t index = 0; index < arrivals.size(); ++index) {
            if (index != 0) output << ", ";
            output << '#' << arrivals[index].id << ' '
                   << vehicleTypeToString(arrivals[index].type)
                   << (arrivals[index].isEmergency ? " [EMERGENCY]" : "")
                   << " from " << directionToString(arrivals[index].direction);
        }
        output << '\n';
    }

    output << "Queues: N=" << queues[0]->size()
           << " S=" << queues[1]->size()
           << " E=" << queues[2]->size()
           << " W=" << queues[3]->size() << '\n';
    output << "Vehicles served this tick: " << servedThisTick
           << " | total served: " << totalServed << '\n';
}


void SimulationEngine::advanceTicks(int count, std::ostream& output, bool quiet) {
    if (count < 1) {
        throw std::invalid_argument("Tick count must be at least 1.");
    }

    // Delay between simulation ticks (in milliseconds).
    // 1000 = 1 second per tick.
    const int delayMs = 1000;

    for (int index = 0; index < count; ++index) {
        // Process one complete simulation tick.
        advanceOneTick(output, quiet);

        // Flush output so the terminal displays each tick immediately.
        if (!quiet) {
            output.flush();
        }

        // Pause before processing the next tick.
        if (delayMs > 0) {
            Sleep(static_cast<DWORD>(delayMs));
        }
    }

    displaySummary(output);
}

void SimulationEngine::runConfiguredDuration(std::ostream& output, bool quiet) {
    const int remaining = std::max(0, config.durationTicks - currentTick);
    if (remaining == 0) {
        output << "Configured duration has already been reached. Reset to run again.\n";
        displaySummary(output);
        return;
    }
    advanceTicks(remaining, output, quiet);
}

void SimulationEngine::displaySummary(std::ostream& output) const {
    const double averageWait = totalServed == 0
        ? 0.0
        : static_cast<double>(totalWaitingTime) / static_cast<double>(totalServed);
    const QueueLengths waiting = getQueueLengths();
    output << "\n=== Simulation Summary ===\n"
           << "Ticks completed: " << currentTick << '\n'
           << "Vehicles generated: " << totalGenerated << '\n'
           << "Vehicles served: " << totalServed << '\n'
           << "Vehicles blocked: " << rejectedArrivals << '\n'
           << "Vehicles still waiting: "
           << (waiting.north + waiting.south + waiting.east + waiting.west) << '\n'
           << "Emergency vehicles generated: " << emergencyGenerated << '\n'
           << "Emergency vehicles served: " << emergencyServed << '\n'
           << "Emergency average waiting time: "
           << std::fixed << std::setprecision(2)
           << (emergencyServed == 0 ? 0.0 : static_cast<double>(emergencyWaitingTime) / emergencyServed)
           << " ticks\n"
           << "Pre-emptions: " << preemptionCount << '\n'
           << "Average waiting time (served vehicles): " << averageWait << " ticks\n"
           << "Maximum waiting time (served vehicles): " << maximumWaitingTime << " ticks\n"
           << "Throughput: " << std::setprecision(3)
           << (currentTick == 0 ? 0.0 : static_cast<double>(totalServed) / currentTick)
           << " vehicles/tick\n"
           << "Peak queues: N=" << maximumQueueLengths[0]
           << " S=" << maximumQueueLengths[1]
           << " E=" << maximumQueueLengths[2]
           << " W=" << maximumQueueLengths[3] << '\n'
           << "Average queue lengths: N="
           << (currentTick == 0 ? 0.0 : static_cast<double>(queueLengthTotals[0]) / currentTick)
           << " S=" << (currentTick == 0 ? 0.0 : static_cast<double>(queueLengthTotals[1]) / currentTick)
           << " E=" << (currentTick == 0 ? 0.0 : static_cast<double>(queueLengthTotals[2]) / currentTick)
           << " W=" << (currentTick == 0 ? 0.0 : static_cast<double>(queueLengthTotals[3]) / currentTick)
           << '\n'
           << "Vehicle accounting: " << totalGenerated << " generated = "
           << totalServed << " served + " << rejectedArrivals << " blocked + "
           << (waiting.north + waiting.south + waiting.east + waiting.west)
           << " waiting (" << (totalGenerated == totalServed + rejectedArrivals +
                                  static_cast<long long>(waiting.north + waiting.south + waiting.east + waiting.west)
                              ? "OK" : "CHECK") << ")\n"
           << "===========================\n";
}

bool SimulationEngine::exportReport(const std::string& path, std::string& error) const {
    std::ofstream file(path.c_str(), std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        error = "Could not open '" + path + "' for writing.";
        return false;
    }
    displaySummary(file);
    file.close();
    if (!file) {
        error = "An error occurred while writing '" + path + "'.";
        return false;
    }
    error.clear();
    return true;
}

bool SimulationEngine::exportEventsCsv(const std::string& path, std::string& error) const {
    return eventLogger.writeCsv(path, error);
}

void SimulationEngine::reset() {
    for (std::size_t index = 0; index < queues.size(); ++index) {
        queues[index]->clear();
        maximumQueueLengths[index] = 0;
    }
    generator.reset();
    eventLogger.clear();
    signalController.reset();
    currentTick = 0;
    totalGenerated = 0;
    emergencyGenerated = 0;
    totalServed = 0;
    emergencyServed = 0;
    rejectedArrivals = 0;
    totalWaitingTime = 0;
    emergencyWaitingTime = 0;
    maximumWaitingTime = 0;
    preemptionCount = 0;
    queueLengthTotals.fill(0);
    writeDashboardSnapshot();
}

void SimulationEngine::writeDashboardSnapshot() const {
    std::ofstream file("dashboard_state.json", std::ios::out | std::ios::trunc);
    if (!file.is_open()) return;

    const QueueLengths waiting = getQueueLengths();
    const long long waitingVehicles = static_cast<long long>(
        waiting.north + waiting.south + waiting.east + waiting.west
    );
    const double averageWait = totalServed == 0
        ? 0.0
        : static_cast<double>(totalWaitingTime) / static_cast<double>(totalServed);
    const double throughput = currentTick == 0
        ? 0.0
        : static_cast<double>(totalServed) / static_cast<double>(currentTick);

    file << std::fixed << std::setprecision(3)
         << "{\n"
         << "  \"tick\": " << currentTick << ",\n"
         << "  \"duration\": " << config.durationTicks << ",\n"
         << "  \"generated\": " << totalGenerated << ",\n"
         << "  \"served\": " << totalServed << ",\n"
         << "  \"blocked\": " << rejectedArrivals << ",\n"
         << "  \"waiting\": " << waitingVehicles << ",\n"
         << "  \"average_wait\": " << averageWait << ",\n"
         << "  \"throughput\": " << throughput << ",\n"
         << "  \"mode\": \"" << SignalController::modeToString(config.signalMode) << "\",\n"
         << "  \"signal_state\": \"" << SignalController::stateToString(signalController.getState()) << "\",\n"
         << "  \"ticks_remaining\": " << signalController.getTicksRemaining() << ",\n"
         << "  \"preemption\": " << (signalController.isPreemptionActive() ? "true" : "false") << ",\n"
         << "  \"capacity\": " << config.queueCapacity << ",\n"
         << "  \"lights\": {"
         << "\"North\": \"" << SignalController::colorToString(signalController.getLightColor(Direction::North)) << "\", "
         << "\"South\": \"" << SignalController::colorToString(signalController.getLightColor(Direction::South)) << "\", "
         << "\"East\": \"" << SignalController::colorToString(signalController.getLightColor(Direction::East)) << "\", "
         << "\"West\": \"" << SignalController::colorToString(signalController.getLightColor(Direction::West)) << "\"},\n"
         << "  \"queues\": {"
         << "\"North\": " << waiting.north << ", "
         << "\"South\": " << waiting.south << ", "
         << "\"East\": " << waiting.east << ", "
         << "\"West\": " << waiting.west << "}\n"
         << "}\n";
}

int SimulationEngine::getCurrentTick() const { return currentTick; }
long long SimulationEngine::getTotalGenerated() const { return totalGenerated; }
long long SimulationEngine::getTotalServed() const { return totalServed; }
long long SimulationEngine::getRejectedArrivals() const { return rejectedArrivals; }
