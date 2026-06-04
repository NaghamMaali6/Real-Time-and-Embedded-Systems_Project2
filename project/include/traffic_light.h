#ifndef TRAFFIC_LIGHT_H
#define TRAFFIC_LIGHT_H

#include <time.h>

// =====================================================
// Traffic light colors
// =====================================================
typedef enum
{
    LIGHT_RED,
    LIGHT_YELLOW,
    LIGHT_GREEN
} LightColor;

// =====================================================
// Road directions
// =====================================================
typedef enum
{
    DIR_NORTH,
    DIR_SOUTH,
    DIR_EAST,
    DIR_WEST,
    DIR_COUNT
} Direction;

// =====================================================
// Traffic light structure
// =====================================================
typedef struct
{
    Direction direction;
    LightColor color;

    time_t last_change_time;

    int emergency_mode;
    int failure_status;

} TrafficLight;

// =====================================================
// Function declarations
// =====================================================

const char* direction_to_string(Direction direction);
const char* light_color_to_string(LightColor color);

void traffic_light_init(TrafficLight* light,
                        Direction direction,
                        LightColor initial_color);

void traffic_light_set_color(TrafficLight* light,
                             LightColor new_color);

int traffic_light_is_safe_change(LightColor old_color,
                                 LightColor new_color);

void traffic_light_print_status(const TrafficLight* light);

#endif
