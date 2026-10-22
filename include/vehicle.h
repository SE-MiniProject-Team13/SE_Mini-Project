
#ifndef VEHICLE_H
#define VEHICLE_H

#include <string>

enum class Direction {
    North,
    South,
    East,
    West
};

struct Vehicle {
    int id;
    Direction direction;
    bool isEmergency;
    int arrivalTick;
    int departureTick;
    int waitingTime;

    Vehicle(
        int vehicleId = 0,
        Direction vehicleDirection = Direction::North,
        bool emergency = false,
        int arrival = 0
    )
        : id(vehicleId),
          direction(vehicleDirection),
          isEmergency(emergency),
          arrivalTick(arrival),
          departureTick(-1),
          waitingTime(0) {}
};

#endif
