#include "controller.h"
#include "logger.h"
#include "ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>


//Forward declarations of all static helpers
 
static void  state_init(ControllerState* s, const Config* cfg);
static void  lights_set_phase(ControllerState* s, TrafficPhase phase);

static void  shm_push_lights(const ControllerState* s);

static int   check_emergency(ControllerState* s);
static int   check_pedestrian(ControllerState* s);
static void  check_vehicles(ControllerState* s, const Config* cfg);

static void  do_transition(ControllerState* s,
                           TrafficPhase     next,
                           const Config*    cfg);

static void  run_phase(ControllerState* s, const Config* cfg);

static void  handle_vehicle_msgs(ControllerState* s);
static void  handle_ped_msgs(ControllerState* s);
static void  handle_emergency_msgs(ControllerState* s);

static void  send_ped_activation(Direction dir, int crossing_time);
static void  safety_check(const ControllerState* s);

static int   phase_duration(const ControllerState* s,
                            const Config* cfg);

static int   pick_next_normal_phase(const ControllerState* s);

static int   congestion_priority(void);
static void  check_deadlines(const Config* cfg);



// Light color table per phase

static const LightColor PHASE_COLORS[11][8] =
{
    /* PHASE_NS_STRAIGHT_GREEN */
    {
        LIGHT_RED,   LIGHT_GREEN,
        LIGHT_RED,   LIGHT_RED,
        LIGHT_RED,   LIGHT_GREEN,
        LIGHT_RED,   LIGHT_RED
    },

    /* PHASE_NS_STRAIGHT_YELLOW */
    {
        LIGHT_RED,   LIGHT_YELLOW,
        LIGHT_RED,   LIGHT_RED,
        LIGHT_RED,   LIGHT_YELLOW,
        LIGHT_RED,   LIGHT_RED
    },

    /* PHASE_NS_LEFT_GREEN */
    {
        LIGHT_GREEN, LIGHT_RED,
        LIGHT_RED,   LIGHT_RED,
        LIGHT_GREEN, LIGHT_RED,
        LIGHT_RED,   LIGHT_RED
    },

    /* PHASE_NS_LEFT_YELLOW */
    {
        LIGHT_YELLOW, LIGHT_RED,
        LIGHT_RED,    LIGHT_RED,
        LIGHT_YELLOW, LIGHT_RED,
        LIGHT_RED,    LIGHT_RED
    },

    /* PHASE_EW_STRAIGHT_GREEN */
    {
        LIGHT_RED,   LIGHT_RED,
        LIGHT_RED,   LIGHT_GREEN,
        LIGHT_RED,   LIGHT_RED,
        LIGHT_RED,   LIGHT_GREEN
    },

    /* PHASE_EW_STRAIGHT_YELLOW */
    {
        LIGHT_RED,   LIGHT_RED,
        LIGHT_RED,   LIGHT_YELLOW,
        LIGHT_RED,   LIGHT_RED,
        LIGHT_RED,   LIGHT_YELLOW
    },

    /* PHASE_EW_LEFT_GREEN */
    {
        LIGHT_RED,   LIGHT_RED,
        LIGHT_GREEN, LIGHT_RED,
        LIGHT_RED,   LIGHT_RED,
        LIGHT_GREEN, LIGHT_RED
    },

    /* PHASE_EW_LEFT_YELLOW */
    {
        LIGHT_RED,    LIGHT_RED,
        LIGHT_YELLOW, LIGHT_RED,
        LIGHT_RED,    LIGHT_RED,
        LIGHT_YELLOW, LIGHT_RED
    },

    /* PHASE_ALL_RED */
    {
        LIGHT_RED, LIGHT_RED,
        LIGHT_RED, LIGHT_RED,
        LIGHT_RED, LIGHT_RED,
        LIGHT_RED, LIGHT_RED
    },

    /* PHASE_PEDESTRIAN */
    {
        LIGHT_RED, LIGHT_RED,
        LIGHT_RED, LIGHT_RED,
        LIGHT_RED, LIGHT_RED,
        LIGHT_RED, LIGHT_RED
    },

    /* PHASE_EMERGENCY */
    {
        LIGHT_RED, LIGHT_RED,
        LIGHT_RED, LIGHT_RED,
        LIGHT_RED, LIGHT_RED,
        LIGHT_RED, LIGHT_RED
    }
};


 //Initialize controller state

static void state_init(ControllerState* s, const Config* cfg)
{
    int i;

    for (i = 0; i < 8; i++)
    {
        traffic_light_init(&s->lights[i],
                           (Direction)(i / 2),
                           LIGHT_RED);
    }

    s->current_phase = PHASE_ALL_RED;
    s->next_phase    = PHASE_NS_STRAIGHT_GREEN;

    s->phase_start   = time(NULL);

    s->emergency_dir = -1;
    s->ped_direction = -1;

    memset(s->ped_pending, 0, sizeof(s->ped_pending));

    (void)cfg;
}


 // Apply colors for current phase

static void lights_set_phase(ControllerState* s,
                             TrafficPhase phase)
{
    int i;
 
   

    if (phase == PHASE_EMERGENCY &&
        s->emergency_dir >= 0)
    {
        /* All red first */
        for (i = 0; i < 8; i++)
        {
            //traffic_light_set_color(&s->lights[i],
                                    //LIGHT_RED);
            s->lights[i].color = LIGHT_RED;
        }

        /* ONLY one lane green during emergency */
        int base = s->emergency_dir * 2;

        /* default = straight */
        int lane = base + 1;

        switch (shared_data->emergency_move)
        {
            case MOVE_LEFT:
                lane = base;       // left lane
                break;

            case MOVE_RIGHT:
                lane = base + 1;   // right lane
                break;

            case MOVE_STRAIGHT:
            default:
                lane = base + 1;
                break;
        }

        s->lights[lane].color = LIGHT_GREEN;
        //traffic_light_set_color(&s->lights[lane], LIGHT_GREEN);
    }
    else
    {
        for (i = 0; i < 8; i++)
        {
            traffic_light_set_color(
                &s->lights[i],
                PHASE_COLORS[phase][i]
            );
        }
    }
    int d;

    /* default: all DON'T WALK */
    for (d = 0; d < DIR_COUNT; d++)
    {
        shared_data->pedestrian_light[d] = PED_DONT_WALK;
    }

    /* only in pedestrian phase -> WALK */
    if (phase == PHASE_PEDESTRIAN)
    {
        for (d = 0; d < DIR_COUNT; d++)
        {
            shared_data->pedestrian_light[d] = PED_WALK;
        }
    }
}

 // Push current light states into shared memory
 
static void shm_push_lights(const ControllerState* s)
{
    int i;

    ipc_sem_wait(sem_id, SEM_MUTEX);

    for (i = 0; i < 8; i++)
    {
        shared_data->lights[i] = s->lights[i].color;
    }

    shared_data->active_phase     = (int)s->current_phase;
    shared_data->phase_start_time = s->phase_start;

    ipc_sem_signal(sem_id, SEM_MUTEX);
}

//Handle vehicle messages
 
static void handle_vehicle_msgs(ControllerState* s)
{
    IpcMessage msg;

    (void)s;

    while (msgrcv(detect_queue_id,
                  &msg,
                  sizeof(IpcMessage) - sizeof(long),
                  MSG_VEHICLE_DETECTED,
                  IPC_NOWAIT) != -1)
    {
        logger_log("VEHICLE_DETECTED direction=%s move=%d",
                   direction_to_string(msg.direction),
                   msg.data);
    }
}


 // Handle pedestrian messages

static void handle_ped_msgs(ControllerState* s)
{
    IpcMessage msg;

    while (msgrcv(ped_queue_id,
                  &msg,
                  sizeof(IpcMessage) - sizeof(long),
                  MSG_PED_REQUEST,
                  IPC_NOWAIT) != -1)
    {
        if (msg.direction >= 0 &&
            msg.direction < DIR_COUNT)
        {
            s->ped_pending[msg.direction] = 1;

            logger_log("PED_REQUEST direction=%s",
                       direction_to_string(msg.direction));
        }
    }
}


 //Handle emergency messages

static void handle_emergency_msgs(ControllerState* s)
{
    IpcMessage msg;

    while (msgrcv(emergency_queue_id,
                  &msg,
                  sizeof(IpcMessage) - sizeof(long),
                  MSG_EMERGENCY_DETECTED,
                  IPC_NOWAIT) != -1)
    {
        s->emergency_dir = msg.direction;
        

        logger_log("EMERGENCY_DETECTED direction=%s priority=%d",
                   direction_to_string(msg.direction),
                   msg.data);
    }

    while (msgrcv(emergency_queue_id,
                  &msg,
                  sizeof(IpcMessage) - sizeof(long),
                  MSG_EMERGENCY_CLEARED,
                  IPC_NOWAIT) != -1)
    {
        logger_log("EMERGENCY_CLEARED direction=%s",
                   direction_to_string(msg.direction));

        //s->emergency_dir = -1;

        ipc_sem_wait(sem_id, SEM_MUTEX);

        shared_data->emergency_active    = 0;
        shared_data->emergency_direction = -1;

        ipc_sem_signal(sem_id, SEM_MUTEX);
    }
}

 // Check if emergency exists

static int check_emergency(ControllerState* s)
{
    return (s->emergency_dir >= 0);
}


 //Check pending pedestrian requests

static int check_pedestrian(ControllerState* s)
{
    int i;

    for (i = 0; i < DIR_COUNT; i++)
    {
        if (s->ped_pending[i])
        {
            return 1;
        }
    }

    return 0;
}


 // Find most congested direction

static int congestion_priority(void)
{
    int counts[4];
    int best_dir = DIR_NORTH;
    int best_val = 0;
    int i;

    ipc_sem_wait(sem_id, SEM_MUTEX);

    counts[DIR_NORTH] = shared_data->north_waiting;
    counts[DIR_SOUTH] = shared_data->south_waiting;
    counts[DIR_EAST]  = shared_data->east_waiting;
    counts[DIR_WEST]  = shared_data->west_waiting;

    ipc_sem_signal(sem_id, SEM_MUTEX);

    for (i = 0; i < DIR_COUNT; i++)
    {
        if (counts[i] > best_val)
        {
            best_val = counts[i];
            best_dir = i;
        }
    }

    return best_dir;
}

static void check_deadlines(const Config* cfg)
{
    int i;
    time_t now = time(NULL);

    ipc_sem_wait(sem_id, SEM_MUTEX);

    for (i = 0; i < DIR_COUNT; i++)
    {
        long waited = 0;

        if (shared_data->pedestrian_request[i] &&
            shared_data->pedestrian_request_time[i] > 0 &&
            !shared_data->ped_deadline_reported[i] &&
            (int)(now - shared_data->pedestrian_request_time[i]) >
                cfg->max_pedestrian_wait)
        {
            waited = (long)(now - shared_data->pedestrian_request_time[i]);
            shared_data->ped_deadline_reported[i] = 1;
            ipc_sem_signal(sem_id, SEM_MUTEX);
            logger_log("TIMING_VIOLATION pedestrian_wait direction=%s waited=%ld limit=%d",
                       direction_to_string((Direction)i),
                       waited,
                       cfg->max_pedestrian_wait);
            ipc_sem_wait(sem_id, SEM_MUTEX);
        }
    }

    if (shared_data->emergency_active &&
        shared_data->emergency_detect_time > 0 &&
        !shared_data->emergency_deadline_reported &&
        (int)(now - shared_data->emergency_detect_time) >
            cfg->max_emergency_response)
    {
        long waited = (long)(now - shared_data->emergency_detect_time);
        shared_data->emergency_deadline_reported = 1;
        ipc_sem_signal(sem_id, SEM_MUTEX);
        logger_log("TIMING_VIOLATION emergency_response waited=%ld limit=%d",
                   waited,
                   cfg->max_emergency_response);
        ipc_sem_wait(sem_id, SEM_MUTEX);
    }

    ipc_sem_signal(sem_id, SEM_MUTEX);
}


 // Pick next normal phase using congestion

static int pick_next_normal_phase(const ControllerState* s)
{
    int dir = congestion_priority();

    (void)s;

    switch (dir)
    {
        case DIR_NORTH:
        case DIR_SOUTH:
        {
            static int ns_toggle = 0;

            ns_toggle = !ns_toggle;

            if (ns_toggle)
            {
                return PHASE_NS_STRAIGHT_GREEN;
            }

            return PHASE_NS_LEFT_GREEN;
        }

        case DIR_EAST:
        case DIR_WEST:
        {
            static int ew_toggle = 0;

            ew_toggle = !ew_toggle;

            if (ew_toggle)
            {
                return PHASE_EW_STRAIGHT_GREEN;
            }

            return PHASE_EW_LEFT_GREEN;
        }

        default:
            return PHASE_NS_STRAIGHT_GREEN;
    }
}


 //Phase duration

static int phase_duration(const ControllerState* s,
                          const Config* cfg)
{
    switch (s->current_phase)
    {
        case PHASE_NS_STRAIGHT_GREEN:
        case PHASE_NS_LEFT_GREEN:
        case PHASE_EW_STRAIGHT_GREEN:
        case PHASE_EW_LEFT_GREEN:

            if (cfg->green_time > cfg->max_green_time)
            {
                return cfg->max_green_time;
            }

            return cfg->green_time;

        case PHASE_NS_STRAIGHT_YELLOW:
        case PHASE_NS_LEFT_YELLOW:
        case PHASE_EW_STRAIGHT_YELLOW:
        case PHASE_EW_LEFT_YELLOW:

            return cfg->yellow_time;

        case PHASE_ALL_RED:
            return cfg->all_red_time;

        case PHASE_PEDESTRIAN:
            return cfg->pedestrian_time;

        case PHASE_EMERGENCY:
            return cfg->emergency_time;

        default:
            return cfg->green_time;
    }
}


 //Send pedestrian activation

static void send_ped_activation(Direction dir, int crossing_time)
{
    IpcMessage msg;

    msg.msg_type  = MSG_PED_ACTIVATED + dir;
    msg.direction = dir;
    msg.data      = crossing_time;
    msg.timestamp = time(NULL);

    ipc_msg_send(cmd_queue_id, &msg);

    logger_log("PED_ACTIVATED direction=%s crossing_time=%d",
               direction_to_string(dir),
               crossing_time);
}

 //Safety checks

static void safety_check(const ControllerState* s)
{

    if (s->current_phase == PHASE_EMERGENCY)
    {
        return;
    }
    
    int ns_green =
    (
        s->lights[LIGHT_NORTH_LEFT].color  == LIGHT_GREEN ||
        s->lights[LIGHT_NORTH_RIGHT].color == LIGHT_GREEN ||
        s->lights[LIGHT_SOUTH_LEFT].color  == LIGHT_GREEN ||
        s->lights[LIGHT_SOUTH_RIGHT].color == LIGHT_GREEN
    );

    int ew_green =
    (
        s->lights[LIGHT_EAST_LEFT].color  == LIGHT_GREEN ||
        s->lights[LIGHT_EAST_RIGHT].color == LIGHT_GREEN ||
        s->lights[LIGHT_WEST_LEFT].color  == LIGHT_GREEN ||
        s->lights[LIGHT_WEST_RIGHT].color == LIGHT_GREEN
    );

    if (ns_green && ew_green)
    {
        logger_log("SAFETY_VIOLATION conflicting greens");

        fprintf(stderr,
                "[CONTROLLER] SAFETY VIOLATION\n");
    }

    if (s->current_phase == PHASE_PEDESTRIAN &&
        (ns_green || ew_green))
    {
        logger_log("SAFETY_VIOLATION pedestrian conflict");

        fprintf(stderr,
                "[CONTROLLER] PEDESTRIAN SAFETY VIOLATION\n");
    }
}


 //Perform safe transition

static void do_transition(ControllerState* s,
                          TrafficPhase next,
                          const Config* cfg)
{
    /* NS straight green -> yellow */
    if (s->current_phase == PHASE_NS_STRAIGHT_GREEN)
    {
        s->current_phase = PHASE_NS_STRAIGHT_YELLOW;
        s->phase_start   = time(NULL);

        lights_set_phase(s,
                         PHASE_NS_STRAIGHT_YELLOW);

        shm_push_lights(s);

        logger_log(
            "PHASE_CHANGE NS_STRAIGHT_GREEN -> NS_STRAIGHT_YELLOW"
        );

        sleep(cfg->yellow_time);
    }

    /* NS left green -> yellow */
    else if (s->current_phase == PHASE_NS_LEFT_GREEN)
    {
        s->current_phase = PHASE_NS_LEFT_YELLOW;
        s->phase_start   = time(NULL);

        lights_set_phase(s,
                         PHASE_NS_LEFT_YELLOW);

        shm_push_lights(s);

        logger_log(
            "PHASE_CHANGE NS_LEFT_GREEN -> NS_LEFT_YELLOW"
        );

        sleep(cfg->yellow_time);
    }

    /* EW straight green -> yellow */
    else if (s->current_phase == PHASE_EW_STRAIGHT_GREEN)
    {
        s->current_phase = PHASE_EW_STRAIGHT_YELLOW;
        s->phase_start   = time(NULL);

        lights_set_phase(s,
                         PHASE_EW_STRAIGHT_YELLOW);

        shm_push_lights(s);

        logger_log(
            "PHASE_CHANGE EW_STRAIGHT_GREEN -> EW_STRAIGHT_YELLOW"
        );

        sleep(cfg->yellow_time);
    }

    /* EW left green -> yellow */
    else if (s->current_phase == PHASE_EW_LEFT_GREEN)
    {
        s->current_phase = PHASE_EW_LEFT_YELLOW;
        s->phase_start   = time(NULL);

        lights_set_phase(s,
                         PHASE_EW_LEFT_YELLOW);

        shm_push_lights(s);

        logger_log(
            "PHASE_CHANGE EW_LEFT_GREEN -> EW_LEFT_YELLOW"
        );

        sleep(cfg->yellow_time);
    }

    /* ALL RED before entering active phase */
    if (next == PHASE_NS_STRAIGHT_GREEN ||
        next == PHASE_NS_LEFT_GREEN     ||
        next == PHASE_EW_STRAIGHT_GREEN ||
        next == PHASE_EW_LEFT_GREEN     ||
        next == PHASE_PEDESTRIAN        ||
        next == PHASE_EMERGENCY)
    {
        s->current_phase = PHASE_ALL_RED;
        s->phase_start   = time(NULL);

        lights_set_phase(s,
                         PHASE_ALL_RED);

        shm_push_lights(s);

        logger_log("PHASE_CHANGE -> ALL_RED");

        sleep(cfg->all_red_time);
    }

    /* Enter target phase */
    s->current_phase = next;
    s->phase_start   = time(NULL);

    lights_set_phase(s, next);

    shm_push_lights(s);

    safety_check(s);

    logger_log("PHASE_CHANGE -> %d",
               (int)next);
}


 //Decrement waiting counters

static void check_vehicles(ControllerState* s,
                           const Config* cfg)
{
    (void)cfg;

    ipc_sem_wait(sem_id, SEM_MUTEX);

    if (s->current_phase == PHASE_NS_STRAIGHT_GREEN ||
        s->current_phase == PHASE_NS_LEFT_GREEN)
    {
        if (shared_data->north_waiting > 0)
        {
            shared_data->north_waiting--;
        }

        if (shared_data->south_waiting > 0)
        {
            shared_data->south_waiting--;
        }
    }

    else if (s->current_phase == PHASE_EW_STRAIGHT_GREEN ||
             s->current_phase == PHASE_EW_LEFT_GREEN)
    {
        if (shared_data->east_waiting > 0)
        {
            shared_data->east_waiting--;
        }

        if (shared_data->west_waiting > 0)
        {
            shared_data->west_waiting--;
        }
    }

    else if (s->current_phase == PHASE_EMERGENCY &&
             s->emergency_dir >= 0)
    {
        int* waiting[4] =
        {
            &shared_data->north_waiting,
            &shared_data->south_waiting,
            &shared_data->east_waiting,
            &shared_data->west_waiting
        };

        if (*waiting[s->emergency_dir] > 0)
        {
            (*waiting[s->emergency_dir])--;
        }
    }

    ipc_sem_signal(sem_id, SEM_MUTEX);
}


 //Run current phase

static void run_phase(ControllerState* s,
                      const Config* cfg)
{
    int elapsed  = 0;
    int duration = phase_duration(s, cfg);

    while (elapsed < duration)
    {
        sleep(1);

        elapsed++;

        handle_vehicle_msgs(s);
        handle_ped_msgs(s);
        handle_emergency_msgs(s);
        check_deadlines(cfg);

        /* Emergency preempts immediately */
        if (check_emergency(s) &&
            s->current_phase != PHASE_EMERGENCY)
        {
            printf("[CONTROLLER] EMERGENCY PREEMPTION\n");

            return;
        }

        /* Shutdown check */
        ipc_sem_wait(sem_id, SEM_MUTEX);

        if (shared_data->system_status ==
            SYSTEM_STOPPED)
        {
            ipc_sem_signal(sem_id, SEM_MUTEX);

            return;
        }

        ipc_sem_signal(sem_id, SEM_MUTEX);
    }

    check_vehicles(s, cfg);
}


 // Main controller loop
 // Priority:
 //EMERGENCY > PEDESTRIAN > CONGESTION

void controller_run(const Config* config)
{
    ControllerState state;

    int i;

    state_init(&state, config);

    do_transition(&state,
                  PHASE_NS_STRAIGHT_GREEN,
                  config);

    printf("[CONTROLLER] Controller started\n");

    while (1)
    {
        ipc_sem_wait(sem_id, SEM_MUTEX);

        if (shared_data->system_status ==
            SYSTEM_STOPPED)
        {
            ipc_sem_signal(sem_id, SEM_MUTEX);

            break;
        }

        ipc_sem_signal(sem_id, SEM_MUTEX);

        handle_vehicle_msgs(&state);
        handle_ped_msgs(&state);
        handle_emergency_msgs(&state);
        check_deadlines(config);

        
         // PRIORITY 1 : EMERGENCY
        
       
        if (check_emergency(&state))
        {
            /* 1. Transition safely to emergency phase
             *    (do_transition handles yellow→allred first) */
            do_transition(&state, PHASE_EMERGENCY, config);
        
            /* 2. Run emergency phase — emergency direction has green */
            run_phase(&state, config);
        
            /* 3. Safety interval after emergency */
            do_transition(&state, PHASE_ALL_RED, config);
        
            /* 4. Clear emergency state */
            state.emergency_dir = -1;
        
            ipc_sem_wait(sem_id, SEM_MUTEX);
            shared_data->emergency_active    = 0;
            shared_data->emergency_direction = -1;
            ipc_sem_signal(sem_id, SEM_MUTEX);
        
            /* 5. Return to normal congestion-based scheduling */
            do_transition(&state,
                          (TrafficPhase)pick_next_normal_phase(&state),
                          config);
        
            run_phase(&state, config);
        
            continue;
        }

        
         //PRIORITY 2 : PEDESTRIAN
         
        if (check_pedestrian(&state))
        {
            for (i = 0; i < DIR_COUNT; i++)
            {
                if (state.ped_pending[i])
                {
                    state.ped_direction = i;
                    break;
                }
            }

            do_transition(&state,
                          PHASE_PEDESTRIAN,
                          config);

            send_ped_activation(
                (Direction)state.ped_direction,
                config->pedestrian_time
            );

            run_phase(&state, config);

            {
                int served_dir = state.ped_direction;

                state.ped_pending[served_dir] = 0;
                state.ped_direction = -1;

                ipc_sem_wait(sem_id, SEM_MUTEX);

                shared_data->pedestrian_request[served_dir] = 0;
                shared_data->pedestrian_request_time[served_dir] = 0;
                shared_data->ped_deadline_reported[served_dir] = 0;

                ipc_sem_signal(sem_id, SEM_MUTEX);
            }

            do_transition(
                &state,
                (TrafficPhase)
                pick_next_normal_phase(&state),
                config
            );

            run_phase(&state, config);

            continue;
        }

        
         // PRIORITY 3 : CONGESTION
         
        do_transition(
            &state,
            (TrafficPhase)
            pick_next_normal_phase(&state),
            config
        );

        run_phase(&state, config);
    }

    /* Safe shutdown */
    do_transition(&state,
                  PHASE_ALL_RED,
                  config);

    logger_log("SYSTEM_SHUTDOWN");

    printf("[CONTROLLER] Controller exiting\n");
}
