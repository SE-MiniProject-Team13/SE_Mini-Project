
#ifndef VEHICLE_H
#define VEHICLE_H

#include <string>

enum class Direction {
    North,
    South,
    East,
    West
};

enum class VehicleType {
    Car,
    Bike,
    Bus,
    Truck,
    Ambulance,
    FireTruck,
    Police
};

struct Vehicle {
    int id;
    Direction direction;
    bool isEmergency;
    int arrivalTick;
    int departureTick;
    int waitingTime;
    VehicleType type;

    Vehicle(
        int vehicleId = 0,
        Direction vehicleDirection = Direction::North,
        bool emergency = false,
        int arrival = 0,
        VehicleType vehicleType = VehicleType::Car
    )
        : id(vehicleId),
          direction(vehicleDirection),
          isEmergency(emergency),
          arrivalTick(arrival),
          departureTick(-1),
          waitingTime(0),
          type(vehicleType) {}
};

inline const char* directionToString(Direction direction) {
    switch (direction) {
        case Direction::North: return "North";
        case Direction::South: return "South";
        case Direction::East: return "East";
        case Direction::West: return "West";
    }
    return "Unknown";
}

inline const char* vehicleTypeToString(VehicleType type) {
    switch (type) {
        case VehicleType::Car: return "Car";
        case VehicleType::Bike: return "Bike";
        case VehicleType::Bus: return "Bus";
        case VehicleType::Truck: return "Truck";
        case VehicleType::Ambulance: return "Ambulance";
        case VehicleType::FireTruck: return "Fire truck";
        case VehicleType::Police: return "Police";
    }
    return "Unknown";
}

#endif
