#ifndef PEDESTRIAN_H
#define PEDESTRIAN_H

#include "traffic_light.h"
#include "ipc.h"
#include "config.h"

void pedestrian_send_request(Direction direction);
void pedestrian_process_run(void);

#endif
