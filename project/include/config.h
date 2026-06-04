#ifndef CONFIG_H
#define CONFIG_H

typedef struct
{
    int green_time;
    int yellow_time;
    int all_red_time;

    int pedestrian_time;
    int emergency_time;

    int vehicle_min_interval;
    int vehicle_max_interval;

    int emergency_min_interval;
    int emergency_max_interval;

    int max_vehicles_per_direction;
    int max_pedestrian_wait;
    int max_emergency_response;

    char log_file[256];

    /* New variables */
    int max_green_time;
    int ipc_key_base;

} Config;


/* ============================= */
/* Function Prototypes           */
/* ============================= */

int load_config(const char *filename, Config *config);

int validate_config(const Config *config);

void print_config(const Config *config);

#endif
