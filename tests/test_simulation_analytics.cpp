#include "simulation.h"

#include <cassert>
#include <cmath>
#include <iostream>

static void testSummaryAndQueueHistory() {
    SimulationConfig config(2, 2, 1, 1, 2, false, 1, 8, 1);
    TrafficSimulation simulation(config);

    assert(simulation.addVehicle(Direction::North));
    assert(simulation.addVehicle(Direction::North));
    assert(!simulation.addVehicle(Direction::North));
    simulation.run(2);

    const SimulationStats stats = simulation.getStats();
    assert(stats.generated == 3);
    assert(stats.served == 2);
    assert(stats.blocked == 1);
    assert(stats.waiting == 0);
    assert(stats.vehicleCountInvariantPassed);
    assert(stats.maximumWaitingTime == 1);
    assert(stats.maximumQueueLength == 2);
    assert(std::fabs(stats.averageWaitingTime - 0.5) < 0.001);
    assert(std::fabs(stats.throughput - 1.0) < 0.001);
    assert(stats.elapsedTicks == 2);

    const std::vector<SimulationSnapshot>& history =
        simulation.getAnalyticsHistory();
    assert(history.size() == 3);
    assert(history[0].tick == 0 && history[0].queues.north == 2);
    assert(history[1].tick == 1 && history[1].queues.north == 1);
    assert(history[2].tick == 2 && history[2].queues.north == 0);
    assert(history[0].signalState == SignalState::NS_Green);

    const std::vector<SimulationEvent>& events =
        simulation.getAnalyticsEvents();
    assert(events.size() == 5);
    assert(events[0].type == "ARRIVAL");
    assert(events[2].type == "REJECTION");
    assert(events[3].type == "DEPARTURE");
    assert(events[3].waitingTime == 0);
    assert(events[4].type == "DEPARTURE");
    assert(events[4].waitingTime == 1);
}

static void testEmergencyAndActualPreemption() {
    SimulationConfig config(10, 8, 2, 1, 30, false, 1, 8, 1);
    TrafficSimulation samePhase(config);
    assert(samePhase.addVehicle(Direction::North, true));
    samePhase.step();
    assert(samePhase.getStats().preemptionCount == 0);

    TrafficSimulation simulation(config);
    assert(simulation.addVehicle(Direction::East, true));

    simulation.step();
    assert(simulation.getStats().preemptionCount == 1);

    simulation.run(29);
    const SimulationStats stats = simulation.getStats();
    assert(stats.emergencyServed == 1);
    assert(std::fabs(stats.averageEmergencyWaitingTime - 5.0) < 0.001);
    assert(stats.maximumEmergencyWaitingTime == 5);
    assert(stats.preemptionCount == 1);
    assert(stats.signalSafetyPassed);

    std::size_t preemptionEvents = 0;
    for (std::size_t i = 0; i < simulation.getAnalyticsEvents().size(); ++i) {
        if (simulation.getAnalyticsEvents()[i].type == "PREEMPTION") {
            ++preemptionEvents;
        }
    }
    assert(preemptionEvents == 1);
}

static void testMaximumQueueAcrossApproachesAndSignalHistory() {
    SimulationConfig queuedConfig(3, 1, 1, 1, 0, false, 1, 8, 1);
    TrafficSimulation queued(queuedConfig);
    assert(queued.addVehicle(Direction::North));
    assert(queued.addVehicle(Direction::North));
    assert(queued.addVehicle(Direction::West));
    assert(queued.addVehicle(Direction::West));
    assert(queued.addVehicle(Direction::West));
    assert(queued.getStats().maximumQueueLength == 3);

    SimulationConfig signalConfig(5, 1, 1, 1, 4, false, 1, 8, 1);
    TrafficSimulation signals(signalConfig);
    signals.run(4);
    const std::vector<SimulationSnapshot>& history =
        signals.getAnalyticsHistory();
    assert(history.size() == 5);
    assert(history[0].signalState == SignalState::NS_Green);
    assert(history[1].signalState == SignalState::NS_Green);
    assert(history[2].signalState == SignalState::NS_Yellow);
    assert(history[3].signalState == SignalState::All_Red_1);
    assert(history[4].signalState == SignalState::EW_Green);
}

static void testEmptyRunAndNoServedMetrics() {
    SimulationConfig config(2, 4, 1, 1, 0, false, 1, 8, 1);
    TrafficSimulation simulation(config);
    const SimulationStats stats = simulation.getStats();

    assert(stats.elapsedTicks == 0);
    assert(stats.generated == 0);
    assert(stats.served == 0);
    assert(stats.waiting == 0);
    assert(stats.averageWaitingTime == 0.0);
    assert(stats.maximumWaitingTime == 0);
    assert(stats.throughput == 0.0);
    assert(stats.vehicleCountInvariantPassed);
    assert(simulation.getAnalyticsHistory().size() == 1);
}

int main() {
    testSummaryAndQueueHistory();
    testEmergencyAndActualPreemption();
    testMaximumQueueAcrossApproachesAndSignalHistory();
    testEmptyRunAndNoServedMetrics();
    std::cout << "Simulation analytics tests passed.\n";
    return 0;
}
