#ifndef VEHICLE_H
#define VEHICLE_H

#include "ipc.h"
#include "config.h"

const char* move_to_string(VehicleMove move);

void vehicle_process_run(const Config* config);

#endif
