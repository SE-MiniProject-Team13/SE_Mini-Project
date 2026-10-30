#ifndef VEHICLE_GENERATOR_H
#define VEHICLE_GENERATOR_H

#include "vehicle.h"

#include <random>
#include <vector>

class VehicleGenerator {
private:
    double arrivalProbability;
    double emergencyProbability;
    unsigned int seed;
    int nextVehicleId;
    std::mt19937 randomEngine;
    std::bernoulli_distribution arrivalDistribution;
    std::bernoulli_distribution emergencyDistribution;

    VehicleType generateNormalType();
    VehicleType generateEmergencyType();

public:
    VehicleGenerator(double arrivalChance, double emergencyChance, unsigned int randomSeed);

    // One independent arrival trial is made for each of the four approaches.
    std::vector<Vehicle> generateForTick(int tick);
    int getNextVehicleId() const;
    void reset();
};

#endif
