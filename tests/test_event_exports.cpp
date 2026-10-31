#ifdef NDEBUG
#undef NDEBUG
#endif

#include "config.h"
#include "event_logger.h"
#include "simulation_engine.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
std::string readFile(const std::string& path) {
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    assert(file.is_open());
    std::ostringstream contents;
    contents << file.rdbuf();
    std::string text = contents.str();
    for (std::string::size_type position = 0;
         (position = text.find("\r\n", position)) != std::string::npos;) {
        text.replace(position, 2, "\n");
        ++position;
    }
    return text;
}

void testEventCsvContentAndEscaping() {
    const std::string path = "test_event_exports_events.csv";
    EventLogger logger;
    std::string error;
    logger.record(3, SimulationEventType::VehicleArrival, 12, VehicleType::Car,
                  Direction::North, "Vehicle arrived");
    logger.record(4, SimulationEventType::SignalPhaseChange, -1, VehicleType::Car,
                  Direction::North, "NS Green -> NS Yellow");
    logger.record(5, SimulationEventType::VehicleBlocked, 13, VehicleType::Bus,
                  Direction::West, "Queue full, said \"stop\"\ntry again");

    assert(logger.getEvents().size() == 3);
    assert(logger.writeCsv(path, error));
    assert(error.empty());

    const std::string expected =
        "tick,event,vehicle_id,vehicle_type,approach,details\n"
        "3,vehicle_arrival,12,Car,North,\"Vehicle arrived\"\n"
        "4,signal_phase_change,,,,\"NS Green -> NS Yellow\"\n"
        "5,vehicle_blocked,13,Bus,West,\"Queue full, said \"\"stop\"\"\ntry again\"\n";
    assert(readFile(path) == expected);
    std::remove(path.c_str());
}

void testEmptyCsvAndWriteFailureAreReported() {
    const std::string path = "test_event_exports_empty.csv";
    EventLogger logger;
    std::string error;

    assert(logger.writeCsv(path, error));
    assert(readFile(path) == "tick,event,vehicle_id,vehicle_type,approach,details\n");
    std::remove(path.c_str());

    assert(!logger.writeCsv("missing-event-export-directory-for-test/events.csv", error));
    assert(!error.empty());
}

void testReportContainsSummaryAndReportsWriteFailure() {
    Config config;
    config.durationTicks = 4;
    config.arrivalProbability = 0.0;
    config.emergencyProbability = 0.0;
    SimulationEngine engine(config);
    std::ostringstream runOutput;
    engine.runConfiguredDuration(runOutput, true);

    const std::string path = "test_event_exports_report.txt";
    std::string error;
    assert(engine.exportReport(path, error));
    assert(error.empty());
    const std::string report = readFile(path);
    assert(report.find("=== Simulation Summary ===") != std::string::npos);
    assert(report.find("Ticks completed: 4") != std::string::npos);
    assert(report.find("Vehicles generated: 0") != std::string::npos);
    assert(report.find("Vehicle accounting:") != std::string::npos);
    std::remove(path.c_str());

    assert(!engine.exportReport("missing-event-export-directory-for-test/report.txt", error));
    assert(!error.empty());
}
}

int main() {
    testEventCsvContentAndEscaping();
    testEmptyCsvAndWriteFailureAreReported();
    testReportContainsSummaryAndReportsWriteFailure();
    std::cout << "All event logging and report export tests passed.\n";
    return 0;
}
