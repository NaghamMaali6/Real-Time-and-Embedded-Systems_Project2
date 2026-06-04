#include "traffic_light.h"

#include <stdio.h>
#include <time.h>

// Convert direction to string

const char* direction_to_string(Direction direction)
{
    switch(direction)
    {
        case DIR_NORTH:
            return "NORTH";

        case DIR_SOUTH:
            return "SOUTH";

        case DIR_EAST:
            return "EAST";

        case DIR_WEST:
            return "WEST";

        default:
            return "UNKNOWN";
    }
}


// Convert color to string

const char* light_color_to_string(LightColor color)
{
    switch(color)
    {
        case LIGHT_RED:
            return "RED";

        case LIGHT_YELLOW:
            return "YELLOW";

        case LIGHT_GREEN:
            return "GREEN";

        default:
            return "UNKNOWN";
    }
}


// Initialize traffic light

void traffic_light_init(TrafficLight* light,
                        Direction direction,
                        LightColor initial_color)
{
    if(light == NULL)
    {
        return;
    }

    light->direction = direction;
    light->color = initial_color;

    light->last_change_time = time(NULL);

    light->emergency_mode = 0;
    light->failure_status = 0;

    printf("[TRAFFIC LIGHT] %s initialized to %s\n",
           direction_to_string(direction),
           light_color_to_string(initial_color));
}

// Check safe transition
// Prevent GREEN -> RED directly

int traffic_light_is_safe_change(LightColor old_color,
                                 LightColor new_color)
{
    if(old_color == LIGHT_GREEN &&
       new_color == LIGHT_RED)
    {
        return 0;
    }

    return 1;
}


// Change light color safely

void traffic_light_set_color(TrafficLight* light,
                             LightColor new_color)
{
    if(light == NULL)
    {
        return;
    }

    if(!traffic_light_is_safe_change(light->color,
                                     new_color))
    {
        printf("[TRAFFIC LIGHT] UNSAFE TRANSITION: %s cannot go GREEN -> RED directly\n",
               direction_to_string(light->direction));

        return;
    }

    printf("[TRAFFIC LIGHT] %s : %s -> %s\n",
           direction_to_string(light->direction),
           light_color_to_string(light->color),
           light_color_to_string(new_color));

    light->color = new_color;

    light->last_change_time = time(NULL);
}
// Print current light status

void traffic_light_print_status(const TrafficLight* light)
{
    if(light == NULL)
    {
        return;
    }

    printf("[TRAFFIC LIGHT STATUS] Direction=%s Color=%s Emergency=%d Failure=%d LastChange=%ld\n",
           direction_to_string(light->direction),
           light_color_to_string(light->color),
           light->emergency_mode,
           light->failure_status,
           light->last_change_time);
}
