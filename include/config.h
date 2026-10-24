
#ifndef CONFIG_H
#define CONFIG_H

#include "signal_mode.h"

#include <string>

struct Config {
    int durationTicks = 600;
    double arrivalProbability = 0.35;

    SignalMode signalMode = SignalMode::Fixed;

    int fixedGreenTicks = 20;
    int yellowTicks = 3;
    int allRedTicks = 1;

    int adaptiveMinGreen = 5;
    int adaptiveMaxGreen = 40;

    int queueCapacity = 100;
    double emergencyProbability = 0.02;

    unsigned int randomSeed = 42;
};

bool configureSimulation(Config& config);
void displayConfig(const Config& config);

#endif
