#include "event_logger.h"

#include <fstream>

namespace {
std::string escapeCsv(const std::string& value) {
    std::string escaped = "\"";
    for (std::string::const_iterator character = value.begin(); character != value.end(); ++character) {
        if (*character == '\"') escaped += "\"\"";
        else escaped += *character;
    }
    escaped += '\"';
    return escaped;
}
}

void EventLogger::record(
    int tick,
    SimulationEventType type,
    int vehicleId,
    VehicleType vehicleType,
    Direction approach,
    const std::string& details
) {
    events.push_back(LoggedSimulationEvent(tick, type, vehicleId, vehicleType, approach, details));
}

const std::vector<LoggedSimulationEvent>& EventLogger::getEvents() const {
    return events;
}

bool EventLogger::writeCsv(const std::string& path, std::string& error) const {
    std::ofstream file(path.c_str(), std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        error = "Could not open '" + path + "' for writing.";
        return false;
    }

    file << "tick,event,vehicle_id,vehicle_type,approach,details\n";
    for (std::vector<LoggedSimulationEvent>::const_iterator event = events.begin();
         event != events.end(); ++event) {
        file << event->tick << ','
             << eventTypeToString(event->type) << ',';
        if (event->vehicleId < 0) file << ',';
        else file << event->vehicleId << ',';
        if (event->vehicleId < 0) file << ',';
        else file << vehicleTypeToString(event->vehicleType) << ',';
        if (event->vehicleId < 0) file << ',';
        else file << directionToString(event->approach) << ',';
        file << escapeCsv(event->details) << '\n';
        if (!file) {
            error = "An error occurred while writing '" + path + "'.";
            return false;
        }
    }

    file.close();
    if (!file) {
        error = "An error occurred while closing '" + path + "'.";
        return false;
    }
    error.clear();
    return true;
}

void EventLogger::clear() {
    events.clear();
}

const char* EventLogger::eventTypeToString(SimulationEventType type) {
    switch (type) {
        case SimulationEventType::VehicleArrival: return "vehicle_arrival";
        case SimulationEventType::VehicleDeparture: return "vehicle_departure";
        case SimulationEventType::VehicleBlocked: return "vehicle_blocked";
        case SimulationEventType::SignalPhaseChange: return "signal_phase_change";
        case SimulationEventType::EmergencyStart: return "emergency_start";
        case SimulationEventType::EmergencyEnd: return "emergency_end";
    }
    return "unknown";
}
