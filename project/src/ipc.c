/*=============================================================
 * ipc.c
 *============================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>

#include <unistd.h>

 #include "ipc.h"

/*=============================================================
 * Global Variables
 *============================================================*/
int shm_id = -1;
int sem_id = -1;

int cmd_queue_id       = -1;
int detect_queue_id    = -1;
int ped_queue_id       = -1;
int emergency_queue_id = -1;
int log_queue_id       = -1;

SharedData *shared_data = NULL;

/*=============================================================
 * Error Reporting
 *============================================================*/
void ipc_report_error(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

/*=============================================================
 * Generate IPC Key
 *============================================================*/
key_t ipc_generate_key(int base, int offset)
{
    return (key_t)(base + offset);
}

/*=============================================================
 * Shared Memory
 *============================================================*/
int ipc_shm_create(key_t key, size_t size)
{
    int shmid = shmget(key, size, IPC_CREAT | 0666);

    if (shmid == -1)
        ipc_report_error("shmget failed");

    return shmid;
}

SharedData *ipc_shm_attach(int shmid)
{
    SharedData *ptr = (SharedData *)shmat(shmid, NULL, 0);

    if (ptr == (SharedData *)-1)
        ipc_report_error("shmat failed");

    return ptr;
}

void ipc_shm_detach(SharedData *shm_ptr)
{
    if (shmdt(shm_ptr) == -1)
        ipc_report_error("shmdt failed");
}

void ipc_shm_destroy(int shmid)
{
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
        ipc_report_error("shmctl IPC_RMID failed");
}

/*=============================================================
 * Semaphores
 *============================================================*/
int ipc_sem_create(key_t key, int num_sems)
{
    int semid = semget(key, num_sems, IPC_CREAT | 0666);

    if (semid == -1)
        ipc_report_error("semget failed");

    return semid;
}

void ipc_sem_init(int semid, int sem_num, int value)
{
    union semun arg;
    arg.val = value;

    if (semctl(semid, sem_num, SETVAL, arg) == -1)
        ipc_report_error("semctl SETVAL failed");
}

void ipc_sem_wait(int semid, int sem_num)
{
    struct sembuf op;
    op.sem_num = sem_num;
    op.sem_op  = -1;
    op.sem_flg = SEM_UNDO;

    if (semop(semid, &op, 1) == -1)
        ipc_report_error("semop wait failed");
}

void ipc_sem_signal(int semid, int sem_num)
{
    struct sembuf op;
    op.sem_num = sem_num;
    op.sem_op  = 1;
    op.sem_flg = SEM_UNDO;

    if (semop(semid, &op, 1) == -1)
        ipc_report_error("semop signal failed");
}

void ipc_sem_destroy(int semid)
{
    if (semctl(semid, 0, IPC_RMID) == -1)
        ipc_report_error("semctl IPC_RMID failed");
}

/*=============================================================
 * Message Queues
 *============================================================*/
int ipc_msg_create(key_t key)
{
    int msqid = msgget(key, IPC_CREAT | 0666);

    if (msqid == -1)
        ipc_report_error("msgget failed");

    return msqid;
}

void ipc_msg_send(int msqid, const IpcMessage *msg)
{
    if (msgsnd(msqid, msg, sizeof(IpcMessage) - sizeof(long), 0) == -1)
        ipc_report_error("msgsnd failed");
}

void ipc_msg_receive(int msqid, long msg_type, IpcMessage *msg_buf)
{
    if (msgrcv(msqid, msg_buf, sizeof(IpcMessage) - sizeof(long), msg_type, 0) == -1)
        ipc_report_error("msgrcv failed");
}

void ipc_msg_destroy(int msqid)
{
    if (msgctl(msqid, IPC_RMID, NULL) == -1)
        ipc_report_error("msgctl IPC_RMID failed");
}

/*=============================================================
 * Full IPC Initialization
 *============================================================*/
void ipc_init(int ipc_key_base)
{
    /*
     * Create shared memory and semaphore
     */
    shm_id = ipc_shm_create(ipc_generate_key(ipc_key_base, SHM_KEY_OFFSET),
                             sizeof(SharedData));

    sem_id = ipc_sem_create(ipc_generate_key(ipc_key_base, SEM_KEY_OFFSET),
                             NUM_SEMAPHORES);

    /*
     * Create all 5 message queues
     */
    cmd_queue_id       = ipc_msg_create(ipc_generate_key(ipc_key_base, MSG_KEY_OFFSET_CMD));
    detect_queue_id    = ipc_msg_create(ipc_generate_key(ipc_key_base, MSG_KEY_OFFSET_DETECT));
    ped_queue_id       = ipc_msg_create(ipc_generate_key(ipc_key_base, MSG_KEY_OFFSET_PED));
    emergency_queue_id = ipc_msg_create(ipc_generate_key(ipc_key_base, MSG_KEY_OFFSET_EMERGENCY));
    log_queue_id       = ipc_msg_create(ipc_generate_key(ipc_key_base, MSG_KEY_OFFSET_LOG));

    /*
     * Attach shared memory
     */
    shared_data = ipc_shm_attach(shm_id);

    /*
     * Initialize semaphore to 1 (unlocked)
     */
    ipc_sem_init(sem_id, SEM_MUTEX, 1);

    /*
     * Zero all shared memory then set specific fields
     */
    memset(shared_data, 0, sizeof(SharedData));
    shared_data->emergency_direction = -1;
    shared_data->emergency_detect_time = 0;
    shared_data->emergency_deadline_reported = 0;
    shared_data->system_status       = SYSTEM_RUNNING;
    shared_data->phase_start_time    = time(NULL);
}

/*=============================================================
 * Cleanup All IPC Resources
 *============================================================*/
void cleanup_ipc(void)
{
    if (shared_data)
    {
        ipc_shm_detach(shared_data);
        shared_data = NULL;
    }

    if (shm_id != -1)       { ipc_shm_destroy(shm_id);               shm_id = -1; }
    if (sem_id != -1)       { ipc_sem_destroy(sem_id);               sem_id = -1; }

    if (cmd_queue_id       != -1) { ipc_msg_destroy(cmd_queue_id);       cmd_queue_id = -1; }
    if (detect_queue_id    != -1) { ipc_msg_destroy(detect_queue_id);    detect_queue_id = -1; }
    if (ped_queue_id       != -1) { ipc_msg_destroy(ped_queue_id);       ped_queue_id = -1; }
    if (emergency_queue_id != -1) { ipc_msg_destroy(emergency_queue_id); emergency_queue_id = -1; }
    if (log_queue_id       != -1) { ipc_msg_destroy(log_queue_id);       log_queue_id = -1; }
}
