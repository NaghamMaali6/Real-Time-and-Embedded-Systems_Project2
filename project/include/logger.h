#ifndef LOGGER_H
#define LOGGER_H

#include "ipc.h"
#include "config.h"

void logger_run(const char* log_file);
void logger_log(const char* format, ...);

#endif
