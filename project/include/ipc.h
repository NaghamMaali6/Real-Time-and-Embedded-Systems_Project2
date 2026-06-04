/*=============================================================
 * ipc.h *
 * Handles:
 *   - Shared Memory
 *   - Semaphores
 *   - Message Queues
 *============================================================*/
/*=============================================================
 * ipc.h
 *============================================================*/
#ifndef IPC_H
#define IPC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <unistd.h>

#include "traffic_light.h"

/*=============================================================
 * IPC Key Offsets
 *============================================================*/
#define SHM_KEY_OFFSET         1
#define SEM_KEY_OFFSET         2
#define MSG_KEY_OFFSET_CMD       3
#define MSG_KEY_OFFSET_DETECT    4
#define MSG_KEY_OFFSET_PED       5
#define MSG_KEY_OFFSET_EMERGENCY 6
#define MSG_KEY_OFFSET_LOG       7

/*=============================================================
 * Message Types
 *============================================================*/
#define MSG_VEHICLE_DETECTED   1
#define MSG_PED_REQUEST        2
#define MSG_EMERGENCY_DETECTED 3
#define MSG_EMERGENCY_CLEARED  4
#define MSG_CMD_LIGHT_CHANGE   5
#define MSG_LOG_EVENT          6

#define MSG_PED_ACTIVATED      7   /* controller → pedestrian: safe to cross, data = crossing_time */

/*=============================================================
 * System Status
 *============================================================*/
typedef enum
{
    SYSTEM_RUNNING,
    SYSTEM_STOPPED
} SystemStatus;

typedef enum
{
    MOVE_STRAIGHT,
    MOVE_LEFT,
    MOVE_RIGHT
} VehicleMove;

typedef enum {
    LANE_LEFT ,
    LANE_RIGHT_STRAIGHT
    
} VehicleLane;

typedef enum
{
    PED_DONT_WALK = 0,
    PED_WALK = 1
} PedLightState;

/* Visual vehicles — written by vehicle.c, read by graphics.c */
typedef struct {
    int   active;       /* 1 = visible on screen        */
    int   direction;    /* DIR_NORTH etc.               */
    VehicleMove move;   
    int   emergency;    /* 1 = emergency vehicle        */
    float progress;     /* 0.0 = entering, 1.0 = exited */
    float speed;
    float target_speed;
    VehicleLane lane;
} VisualVehicle;

#define MAX_VISUAL_VEHICLES 100

#define MAX_VISUAL_PEDESTRIANS 20

typedef struct
{
    int   active;

    Direction direction;

    /*
     * 0 = waiting
     * 1 = crossing
     */
    int   crossing;

    /*
     * Progress:
     * 0.0 -> starting sidewalk
     * 1.0 -> opposite sidewalk
     */
    float progress;

} VisualPedestrian;

/*=============================================================
 * Shared Memory Structure
 *============================================================*/
typedef struct
{
    LightColor   lights[8];           /* one per light ID 0-7      */
    int          active_phase;        /* current phase enum        */
    int          pedestrian_request[4]; /* indexed by Direction    */
    int          emergency_active;
    int          emergency_direction; /* DIR_NORTH etc., -1 = none */
    int          emergency_move;
    time_t       emergency_detect_time;
    int          emergency_deadline_reported;
    int          north_waiting;
    int          south_waiting;
    int          east_waiting;
    int          west_waiting;
    time_t       pedestrian_request_time[4]; /* indexed by Direction */
    int          ped_deadline_reported[4];   /* one-shot violation log */
    SystemStatus system_status;
    time_t       phase_start_time;
    VisualVehicle vehicles[MAX_VISUAL_VEHICLES];
    /* Pedestrian crossing animation */
    VisualPedestrian pedestrians[MAX_VISUAL_PEDESTRIANS];
    int pedestrian_light[DIR_COUNT];   
} SharedData;

/*=============================================================
 * Semaphore Definitions
 *============================================================*/
#define NUM_SEMAPHORES 1
#define SEM_MUTEX      0

/*=============================================================
 * Semaphore Union
 *============================================================*/
union semun
{
    int            val;
    struct semid_ds *buf;
    unsigned short *array;
};

/*=============================================================
 * Message Structure
 *============================================================*/
typedef struct
{
    long   msg_type;
    int    direction;
    int    data;
    time_t timestamp;
    VehicleMove move;
} IpcMessage;

/*=============================================================
 * Global IDs
 *============================================================*/
extern int shm_id;
extern int sem_id;

extern int cmd_queue_id;
extern int detect_queue_id;
extern int ped_queue_id;
extern int emergency_queue_id;
extern int log_queue_id;

extern SharedData *shared_data;

/*=============================================================
 * Function Prototypes
 *============================================================*/
void       ipc_report_error(const char *msg);

key_t      ipc_generate_key(int base, int offset);

int        ipc_shm_create(key_t key, size_t size);
SharedData *ipc_shm_attach(int shmid);
void       ipc_shm_detach(SharedData *shm_ptr);
void       ipc_shm_destroy(int shmid);

int        ipc_sem_create(key_t key, int num_sems);
void       ipc_sem_init(int semid, int sem_num, int value);
void       ipc_sem_wait(int semid, int sem_num);
void       ipc_sem_signal(int semid, int sem_num);
void       ipc_sem_destroy(int semid);

int        ipc_msg_create(key_t key);
void       ipc_msg_send(int msqid, const IpcMessage *msg);
void       ipc_msg_receive(int msqid, long msg_type, IpcMessage *msg_buf);
void       ipc_msg_destroy(int msqid);

void       ipc_init(int ipc_key_base);
void       cleanup_ipc(void);

#endif
