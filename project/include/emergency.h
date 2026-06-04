#ifndef EMERGENCY_H
#define EMERGENCY_H

#include <time.h>

#include "traffic_light.h"
#include "ipc.h"
#include "config.h"
#include "vehicle.h"

/* Probability: 1 in DENOM chance, fires when roll < NUMER */
#define EMERGENCY_PROBABILITY_DENOM 100
#define EMERGENCY_PROBABILITY_NUMER  40
#define EMERGENCY_PRIORITY            1


typedef struct
{
    Direction direction;
    int       priority;
    int       active;
    time_t    detected_at;
    VehicleMove move;

} EmergencyEvent;


void emergency_process_run(const Config* config);

#endif
