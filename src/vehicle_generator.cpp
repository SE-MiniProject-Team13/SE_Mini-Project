#include "vehicle_generator.h"

#include <array>
#include <algorithm>

namespace {
const std::array<Direction, 4> approaches = {{
    Direction::North, Direction::South, Direction::East, Direction::West
}};
}

VehicleGenerator::VehicleGenerator(
    double arrivalChance,
    double emergencyChance,
    unsigned int randomSeed
)
    : arrivalProbability(std::max(0.0, std::min(1.0, arrivalChance))),
      emergencyProbability(std::max(0.0, std::min(1.0, emergencyChance))),
      seed(randomSeed),
      nextVehicleId(1),
      randomEngine(randomSeed),
      arrivalDistribution(arrivalProbability),
      emergencyDistribution(emergencyProbability) {}

std::vector<Vehicle> VehicleGenerator::generateForTick(int tick) {
    std::vector<Vehicle> arrivals;
    for (std::array<Direction, 4>::const_iterator approach = approaches.begin();
         approach != approaches.end(); ++approach) {
        if (!arrivalDistribution(randomEngine)) {
            continue;
        }

        const bool emergency = emergencyDistribution(randomEngine);
        const VehicleType type = emergency ? generateEmergencyType() : generateNormalType();
        arrivals.push_back(Vehicle(nextVehicleId++, *approach, emergency, tick, type));
    }
    return arrivals;
}

VehicleType VehicleGenerator::generateNormalType() {
    std::uniform_int_distribution<int> typeRoll(1, 100);
    const int roll = typeRoll(randomEngine);
    if (roll <= 60) return VehicleType::Car;
    if (roll <= 85) return VehicleType::Bike;
    if (roll <= 93) return VehicleType::Bus;
    return VehicleType::Truck;
}

VehicleType VehicleGenerator::generateEmergencyType() {
    std::uniform_int_distribution<int> typeRoll(0, 2);
    switch (typeRoll(randomEngine)) {
        case 0: return VehicleType::Ambulance;
        case 1: return VehicleType::FireTruck;
        default: return VehicleType::Police;
    }
}

int VehicleGenerator::getNextVehicleId() const {
    return nextVehicleId;
}

void VehicleGenerator::reset() {
    nextVehicleId = 1;
    randomEngine.seed(seed);
}
