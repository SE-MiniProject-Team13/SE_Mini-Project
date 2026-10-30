#ifndef EVENT_LOGGER_H
#define EVENT_LOGGER_H

#include "vehicle.h"

#include <string>
#include <vector>

enum class SimulationEventType {
    VehicleArrival,
    VehicleDeparture,
    VehicleBlocked,
    SignalPhaseChange,
    EmergencyStart,
    EmergencyEnd
};

struct LoggedSimulationEvent {
    int tick;
    SimulationEventType type;
    int vehicleId;
    VehicleType vehicleType;
    Direction approach;
    std::string details;

    LoggedSimulationEvent(int eventTick, SimulationEventType eventType,
                    int id, VehicleType typeOfVehicle, Direction direction,
                    const std::string& description)
        : tick(eventTick), type(eventType), vehicleId(id),
          vehicleType(typeOfVehicle), approach(direction), details(description) {}
};

class EventLogger {
private:
    std::vector<LoggedSimulationEvent> events;

public:
    void record(int tick, SimulationEventType type, int vehicleId,
                VehicleType vehicleType, Direction approach,
                const std::string& details);
    const std::vector<LoggedSimulationEvent>& getEvents() const;
    bool writeCsv(const std::string& path, std::string& error) const;
    void clear();
    static const char* eventTypeToString(SimulationEventType type);
};

#endif
