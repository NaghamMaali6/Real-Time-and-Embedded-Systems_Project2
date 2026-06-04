#include "emergency.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

/* =====================================================
 * Randomly generate an emergency event.
 * Returns 1 if generated, 0 if not.
 * ===================================================== */
static int emergency_generate(EmergencyEvent* event)
{
    if ((rand() % EMERGENCY_PROBABILITY_DENOM) >= EMERGENCY_PROBABILITY_NUMER)
        return 0;

    event->direction  = (Direction)(rand() % DIR_COUNT);
    event->move       = (VehicleMove)(rand() % 3);
    event->priority   = EMERGENCY_PRIORITY;
    event->detected_at = time(NULL);
    event->active     = 1;

    printf("[EMERGENCY] Vehicle detected from %s\n",
           direction_to_string(event->direction));

    return 1;
}

/* =====================================================
 * Notify controller: emergency detected
 * ===================================================== */
static void emergency_send_detected(const EmergencyEvent* event)
{
    IpcMessage msg;

    ipc_sem_wait(sem_id, SEM_MUTEX);
    shared_data->emergency_active    = 1;
    shared_data->emergency_direction = event->direction;
    shared_data->emergency_detect_time = time(NULL);
    shared_data->emergency_deadline_reported = 0;

    int i;
    for (i = 0; i < MAX_VISUAL_VEHICLES; i++)
    {
        if (!shared_data->vehicles[i].active)
        {
            shared_data->vehicles[i].active       = 1;
            shared_data->vehicles[i].direction    = event->direction;
            //shared_data->emergency_move = (rand() % 3); // 0 = straight, 1 = left, 2 = right
            //shared_data->vehicles[i].move         = shared_data->emergency_move;
            shared_data->vehicles[i].move = event->move;
            shared_data->vehicles[i].emergency    = 1;
            shared_data->vehicles[i].progress     = 0.0f;

            /* IMPORTANT: initialize motion state */
            shared_data->vehicles[i].speed        = 0.0f;
            shared_data->vehicles[i].target_speed = 0.0f;

            break;
        }
    }
    ipc_sem_signal(sem_id, SEM_MUTEX);

    msg.msg_type  = MSG_EMERGENCY_DETECTED;
    msg.direction = event->direction;
    msg.data      = event->priority;
    msg.timestamp = time(NULL);

    ipc_msg_send(emergency_queue_id, &msg);

    printf("[EMERGENCY] Detected from %s\n",
           direction_to_string(event->direction));
}

/* =====================================================
 * Notify controller: emergency cleared
 * ===================================================== */
static void emergency_send_cleared(const EmergencyEvent* event)
{
    IpcMessage msg;

    ipc_sem_wait(sem_id, SEM_MUTEX);
    shared_data->emergency_active    = 0;
    shared_data->emergency_direction = -1;
    shared_data->emergency_detect_time = 0;
    shared_data->emergency_deadline_reported = 0;
    ipc_sem_signal(sem_id, SEM_MUTEX);

    msg.msg_type  = MSG_EMERGENCY_CLEARED;
    msg.direction = event->direction;
    msg.data      = 0;
    msg.timestamp = time(NULL);

    ipc_msg_send(emergency_queue_id, &msg);

    printf("[EMERGENCY] Cleared from %s\n",
           direction_to_string(event->direction));
}

/* =====================================================
 * Emergency process main loop.
 * Sleeps a random interval, then randomly generates
 * an emergency, notifies controller, waits the
 * emergency duration, then clears it.
 * ===================================================== */
void emergency_process_run(const Config* config)
{
    EmergencyEvent event;
    int            sleep_time;

    srand((unsigned int)(time(NULL) ^ (unsigned int)getpid()));

    printf("[EMERGENCY] Emergency process started\n");

    while (1)
    {
        sleep_time = config->emergency_min_interval +
                     rand() % (config->emergency_max_interval -
                                config->emergency_min_interval + 1);

        sleep(sleep_time);

        ipc_sem_wait(sem_id, SEM_MUTEX);
        if (shared_data->system_status == SYSTEM_STOPPED)
        {
            ipc_sem_signal(sem_id, SEM_MUTEX);
            break;
        }
        ipc_sem_signal(sem_id, SEM_MUTEX);

        memset(&event, 0, sizeof(event));

        if (!emergency_generate(&event))
            continue;

        emergency_send_detected(&event);

        sleep(config->emergency_time);

        event.active = 0;
        emergency_send_cleared(&event);
    }

    printf("[EMERGENCY] Emergency process exiting\n");
}
