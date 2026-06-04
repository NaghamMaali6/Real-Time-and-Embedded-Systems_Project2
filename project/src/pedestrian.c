/*=============================================================
 * pedestrian.c
 *============================================================*/
#include "pedestrian.h"

#include <stdio.h>
#include <unistd.h>
#include <time.h>

/*=============================================================
 * Send pedestrian crossing request
 *============================================================*/
void pedestrian_send_request(Direction direction)
{
    IpcMessage msg;
    int        i;

    /*
     * Protect shared memory while modifying pedestrian data
     */
    ipc_sem_wait(sem_id, SEM_MUTEX);

    /*
     * Mark request as pending
     */
    shared_data->pedestrian_request[direction] = 1;
    shared_data->pedestrian_request_time[direction] = time(NULL);
    shared_data->ped_deadline_reported[direction] = 0;

    /*
     * Create visual pedestrian object
     */
    for (i = 0; i < MAX_VISUAL_PEDESTRIANS; i++)
    {
        if (!shared_data->pedestrians[i].active)
        {
            shared_data->pedestrians[i].active    = 1;

            shared_data->pedestrians[i].direction = direction;

            /*
             * Initially waiting for controller permission
             */
            shared_data->pedestrians[i].crossing  = 0;

            /*
             * Start from beginning of zebra crossing
             */
            shared_data->pedestrians[i].progress  = 0.0f;

            break;
        }
    }

    ipc_sem_signal(sem_id, SEM_MUTEX);

    /*
     * Build IPC request message
     */
    msg.msg_type  = MSG_PED_REQUEST;
    msg.direction = direction;
    msg.data      = 0;
    msg.timestamp = time(NULL);

    /*
     * Send request to controller
     */
    ipc_msg_send(ped_queue_id, &msg);

    printf("[PEDESTRIAN] Request sent for %s crossing\n",
           direction_to_string(direction));
}

/*=============================================================
 * Pedestrian process main loop
 *============================================================*/
void pedestrian_process_run(void)
{
    int         ch;
    int         i;
    int         ped_index;

    Direction   direction;

    IpcMessage  reply;

    printf("[PEDESTRIAN] Pedestrian process started\n");

    printf("[PEDESTRIAN] Press key + Enter:\n");
    printf("  n = North\n");
    printf("  s = South\n");
    printf("  e = East\n");
    printf("  w = West\n");
    printf("  q = Quit\n\n");

    while (1)
    {
        ch = getchar();

        /*
         * Ignore ENTER key
         */
        if (ch == '\n' || ch == '\r')
            continue;

        /*
         * Check global shutdown state
         */
        ipc_sem_wait(sem_id, SEM_MUTEX);

        if (shared_data->system_status == SYSTEM_STOPPED)
        {
            ipc_sem_signal(sem_id, SEM_MUTEX);
            break;
        }

        ipc_sem_signal(sem_id, SEM_MUTEX);

        /*
         * Convert keyboard input into direction enum
         */
        switch (ch)
        {
            case 'n':
            case 'N':
                direction = DIR_NORTH;
                break;

            case 's':
            case 'S':
                direction = DIR_SOUTH;
                break;

            case 'e':
            case 'E':
                direction = DIR_EAST;
                break;

            case 'w':
            case 'W':
                direction = DIR_WEST;
                break;

            case 'q':
            case 'Q':
                printf("[PEDESTRIAN] Quit requested\n");
                return;

            default:
                printf("[PEDESTRIAN] Unknown key '%c'\n", ch);
                continue;
        }

        /*
         * Send request to controller
         */
        pedestrian_send_request(direction);

        printf("[PEDESTRIAN] Waiting for crossing permission...\n");

        /*
         * Wait until controller allows crossing
         *
         * Each direction uses unique message type:
         * MSG_PED_ACTIVATED + direction
         */
        ipc_msg_receive(cmd_queue_id,
                        MSG_PED_ACTIVATED + direction,
                        &reply);

        /*
         * Find waiting pedestrian object
         * and activate crossing
         */
        ped_index = -1;

        ipc_sem_wait(sem_id, SEM_MUTEX);

        for (i = 0; i < MAX_VISUAL_PEDESTRIANS; i++)
        {
            if (shared_data->pedestrians[i].active &&
                shared_data->pedestrians[i].direction == direction &&
                shared_data->pedestrians[i].crossing == 0)
            {
                shared_data->pedestrians[i].crossing = 1;

                ped_index = i;

                break;
            }
        }

        ipc_sem_signal(sem_id, SEM_MUTEX);

        printf("[PEDESTRIAN] Crossing %s for %d seconds\n",
               direction_to_string(direction),
               reply.data);

        /*
         * Wait until graphics/simulation
         * finishes pedestrian crossing
         */
        while (1)
        {
            int done;

            ipc_sem_wait(sem_id, SEM_MUTEX);

            /*
             * Crossing complete when pedestrian object
             * becomes inactive
             */
            done =
                (ped_index >= 0) &&
                (!shared_data->pedestrians[ped_index].active);

            ipc_sem_signal(sem_id, SEM_MUTEX);

            if (done)
                break;

            /*
             * Prevent CPU busy waiting
             */
            usleep(100000);
        }

        /*
         * Clear request flag after crossing completed
         */
        ipc_sem_wait(sem_id, SEM_MUTEX);

        shared_data->pedestrian_request[direction] = 0;
        shared_data->pedestrian_request_time[direction] = 0;
        shared_data->ped_deadline_reported[direction] = 0;

        ipc_sem_signal(sem_id, SEM_MUTEX);

        printf("[PEDESTRIAN] Crossing COMPLETED for %s\n",
               direction_to_string(direction));
    }

    printf("[PEDESTRIAN] Pedestrian process exiting\n");
}
