#include "vehicle.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define VEHICLE_SPEED 0.03f
#define PED_SPEED     0.06f
#define TICK_MS       16
#define BASE_TICK_MS  500.0f
#define TICK_SCALE    ((float)TICK_MS / BASE_TICK_MS)

static int vehicle_is_allowed(const VisualVehicle* v, const SharedData* s)
{
    if (v->emergency)
        return 1;

    if (v->lane == LANE_LEFT)
    {
         // left turn signals 
        switch (v->direction)
        {
            case DIR_NORTH: return (s->lights[0] == LIGHT_GREEN);
            case DIR_EAST:  return (s->lights[2] == LIGHT_GREEN);
            case DIR_SOUTH: return (s->lights[4] == LIGHT_GREEN);
            case DIR_WEST:  return (s->lights[6] == LIGHT_GREEN);
            default:        return 0;
        }
    }
    else {
        // straight + right lane
        switch (v->direction)
        {
            case DIR_NORTH:
                return (s->lights[1] == LIGHT_GREEN);
            case DIR_EAST:
                return (s->lights[3] == LIGHT_GREEN);
            case DIR_SOUTH:
                return (s->lights[5] == LIGHT_GREEN);
            case DIR_WEST:
                return (s->lights[7] == LIGHT_GREEN);
            default:
                return 0;
        }
    }
}

static void vehicle_add_visual(Direction direction, VehicleMove move, int emergency)
{
    int i;
    VehicleLane lane;
    if (move == MOVE_LEFT)
        lane = LANE_LEFT;
    else
        lane = LANE_RIGHT_STRAIGHT;

    for (i = 0; i < MAX_VISUAL_VEHICLES; i++)
    {
        if (!shared_data->vehicles[i].active)
        {


            shared_data->vehicles[i].active       = 1;
            shared_data->vehicles[i].direction    = direction;
            shared_data->vehicles[i].move         = move;
            shared_data->vehicles[i].lane         = lane;
            shared_data->vehicles[i].emergency    = emergency;
            shared_data->vehicles[i].progress     = 0.0f;
            shared_data->vehicles[i].speed        = 0.0f;
            shared_data->vehicles[i].target_speed = 0.0f;
            break;
        }
    }
}

static void vehicle_detect(Direction direction)
{
    switch (direction)
    {
        case DIR_NORTH: shared_data->north_waiting++; break;
        case DIR_SOUTH: shared_data->south_waiting++; break;
        case DIR_EAST:  shared_data->east_waiting++;  break;
        case DIR_WEST:  shared_data->west_waiting++;  break;
        default: break;
    }

    printf("[VEHICLE] Vehicle detected from %s\n", direction_to_string(direction));
}

static void vehicle_send_event(Direction direction, VehicleMove move)
{
    IpcMessage msg;
    msg.msg_type  = MSG_VEHICLE_DETECTED;
    msg.direction = direction;
    msg.data      = (int)move;
    msg.timestamp = time(NULL);
    ipc_msg_send(detect_queue_id, &msg);
    printf("[VEHICLE] %s going %s\n", direction_to_string(direction), move_to_string(move));
}

const char* move_to_string(VehicleMove move)
{
    switch (move)
    {
        case MOVE_STRAIGHT: return "STRAIGHT";
        case MOVE_LEFT:     return "LEFT";
        case MOVE_RIGHT:    return "RIGHT";
        default:            return "UNKNOWN";
    }
}

static void update_motion(void)
{
    int i;
    float accel = 0.004f * TICK_SCALE;

    for (i = 0; i < MAX_VISUAL_VEHICLES; i++)
    {
        int is_green;
        float p;

        if (!shared_data->vehicles[i].active)
            continue;

        p = shared_data->vehicles[i].progress;
        is_green = vehicle_is_allowed(&shared_data->vehicles[i], shared_data);

        if (p >= 0.50f)
        {
            /* Once inside the intersection, keep clearing it safely. */
            shared_data->vehicles[i].target_speed = VEHICLE_SPEED * TICK_SCALE;
        }
        else if (is_green)
        {
            shared_data->vehicles[i].target_speed = VEHICLE_SPEED * TICK_SCALE;
        }
        else
        {
            /* Red/yellow lock before intersection: never cross stop line. */
            if (p >= 0.44f)
            {
                shared_data->vehicles[i].progress = 0.44f;
            }

            shared_data->vehicles[i].speed = 0.0f;
            shared_data->vehicles[i].target_speed = 0.0f;
            continue;
        }

        if (shared_data->vehicles[i].speed < shared_data->vehicles[i].target_speed)
            shared_data->vehicles[i].speed += accel;
        else if (shared_data->vehicles[i].speed > shared_data->vehicles[i].target_speed)
            shared_data->vehicles[i].speed -= accel;

        if (shared_data->vehicles[i].speed < 0.0f)
            shared_data->vehicles[i].speed = 0.0f;
        if (shared_data->vehicles[i].speed > VEHICLE_SPEED * TICK_SCALE)
            shared_data->vehicles[i].speed = VEHICLE_SPEED * TICK_SCALE;

        shared_data->vehicles[i].progress += shared_data->vehicles[i].speed;
        if (shared_data->vehicles[i].progress >= 1.0f)
            shared_data->vehicles[i].active = 0;
    }

    for (i = 0; i < MAX_VISUAL_PEDESTRIANS; i++)
    {
        if (!shared_data->pedestrians[i].active)
            continue;

        if (shared_data->pedestrians[i].crossing)
        {
            shared_data->pedestrians[i].progress += PED_SPEED * TICK_SCALE;
            if (shared_data->pedestrians[i].progress >= 1.15f)
                shared_data->pedestrians[i].active = 0;
        }
    }
}

void vehicle_process_run(const Config* config)
{
    Direction direction;
    int spawn_ms = 0;
    int next_spawn_ms;

    srand((unsigned int)(time(NULL) ^ (unsigned int)getpid()));
    next_spawn_ms = (config->vehicle_min_interval +
                    rand() % (config->vehicle_max_interval - config->vehicle_min_interval + 1)) * 1000;

    printf("[VEHICLE] Vehicle process started\n");

    while (1)
    {
        ipc_sem_wait(sem_id, SEM_MUTEX);
        if (shared_data->system_status == SYSTEM_STOPPED)
        {
            ipc_sem_signal(sem_id, SEM_MUTEX);
            break;
        }

        update_motion();
        ipc_sem_signal(sem_id, SEM_MUTEX);

        spawn_ms += TICK_MS;
        if (spawn_ms >= next_spawn_ms)
        {
            direction = (Direction)(rand() % DIR_COUNT);
            VehicleLane lane;

            int r = rand() % 2;

            if (r == 0)
                lane = LANE_LEFT;
            else
                lane = LANE_RIGHT_STRAIGHT;
            VehicleMove move;

            if (lane == LANE_LEFT)
            {
                move = MOVE_LEFT;
            }
            else
            {
                int r = rand() % 2;
                move = (r == 0) ? MOVE_STRAIGHT : MOVE_RIGHT;
            }

            ipc_sem_wait(sem_id, SEM_MUTEX);
            vehicle_detect(direction);
            vehicle_add_visual(direction, move, 0);
            ipc_sem_signal(sem_id, SEM_MUTEX);
            vehicle_send_event(direction, move);

            spawn_ms = 0;
            next_spawn_ms = (config->vehicle_min_interval +
                            rand() % (config->vehicle_max_interval - config->vehicle_min_interval + 1)) * 1000;
        }

        usleep(TICK_MS * 1000);
    }

    printf("[VEHICLE] Vehicle process exiting\n");
}
