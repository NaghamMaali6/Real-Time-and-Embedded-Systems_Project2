#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../include/config.h"
#include "../include/ipc.h"
#include "../include/controller.h"
#include "../include/vehicle.h"
#include "../include/pedestrian.h"
#include "../include/emergency.h"
#include "../include/logger.h"
#include "../include/graphics.h"


 //PIDs of all child processes — needed for cleanup

static pid_t pid_vehicle    = -1;
static pid_t pid_pedestrian = -1;
static pid_t pid_emergency  = -1;
static pid_t pid_logger     = -1;
static pid_t pid_graphics   = -1;
static pid_t pid_controller = -1;


 // Signal handler — graceful shutdown on Ctrl+C
 //Sets shared memory flag; all child loops check it
 
static void handle_sigint(int sig)
{
    (void)sig;

    if (shared_data != NULL)
    {
        ipc_sem_wait(sem_id, SEM_MUTEX);
        shared_data->system_status = SYSTEM_STOPPED;
        ipc_sem_signal(sem_id, SEM_MUTEX);
    }

    printf("\n[MAIN] Shutdown signal received. Stopping system...\n");
}


// Kill all child processes and clean up IPC

static void shutdown_system(void)
{
    cleanup_ipc();

    if (pid_vehicle    > 0) kill(pid_vehicle, SIGKILL);
    if (pid_pedestrian > 0) kill(pid_pedestrian, SIGKILL);
    if (pid_emergency  > 0) kill(pid_emergency, SIGKILL);
    if (pid_logger     > 0) kill(pid_logger, SIGKILL);
    if (pid_graphics   > 0) kill(pid_graphics, SIGKILL);
    if (pid_controller > 0) kill(pid_controller, SIGKILL);

    waitpid(-1, NULL, WNOHANG);

    printf("[MAIN] System shutdown complete.\n");
}


 //Fork helper — exits on failure
 
static pid_t safe_fork(const char* name)
{
    pid_t pid = fork();

    if (pid < 0)
    {
        perror(name);
        shutdown_system();
        exit(EXIT_FAILURE);
    }

    return pid;
}


 //MAIN

int main(int argc, char* argv[])
{
    Config config;

    /* ── 1. Arguments ──────────────────────────────── */
    if (argc != 2)
    {
        printf("Usage: %s <config_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* ── 2. Load + validate config ─────────────────── */
    if (load_config(argv[1], &config) == -1)
    {
        printf("[MAIN] Failed to load configuration.\n");
        exit(EXIT_FAILURE);
    }

    if (validate_config(&config) == -1)
    {
        printf("[MAIN] Configuration validation failed.\n");
        exit(EXIT_FAILURE);
    }

    print_config(&config);

    /* ── 3. IPC: shared memory + semaphores + queues ─ */
    ipc_init(config.ipc_key_base);
    printf("[MAIN] IPC initialized.\n");

    /* ── 4. Signal handler ──────────────────────────── */
    signal(SIGINT,  handle_sigint);
    signal(SIGTERM, handle_sigint);

    /* ── 5. Fork: Logger ────────────────────────────── */
    pid_logger = safe_fork("fork:logger");
    if (pid_logger == 0)
    {
        logger_run(config.log_file);
        exit(0);
    }

    /* ── 6. Fork: Vehicle ───────────────────────────── */
    pid_vehicle = safe_fork("fork:vehicle");
    if (pid_vehicle == 0)
    {
        vehicle_process_run(&config);
        exit(0);
    }

    /* ── 7. Fork: Emergency ─────────────────────────── */
    pid_emergency = safe_fork("fork:emergency");
    if (pid_emergency == 0)
    {
        emergency_process_run(&config);
        exit(0);
    }

    /* ── 8. Fork: Pedestrian ────────────────────────── */
    pid_pedestrian = safe_fork("fork:pedestrian");
    if (pid_pedestrian == 0)
    {
        pedestrian_process_run();
        exit(0);
    }

    /* ── 9. Fork: Graphics (OpenGL) ─────────────────── */
    /*
     * Graphics reads shared memory only — no writes.
     * Synchronized via shared memory: it polls
     * shared_data->lights[], active_phase, etc.
     * every frame so it always mirrors the simulation.
     */
    pid_graphics = safe_fork("fork:graphics");
    if (pid_graphics == 0)
    {
        graphics_run(argc, argv, &config);
        exit(0);
    }

    /* ── 10. Fork: Controller ───────────────────────── */
    /*
     * Controller is forked last so all producers
     * (vehicle, emergency, pedestrian) are already
     * running before the controller starts reading
     * their queues.
     */
    pid_controller = safe_fork("fork:controller");
    if (pid_controller == 0)
    {
        controller_run(&config);
        exit(0);
    }

    printf("[MAIN] All processes started. PID summary:\n");
    printf("  controller  = %d\n", pid_controller);
    printf("  vehicle     = %d\n", pid_vehicle);
    printf("  pedestrian  = %d\n", pid_pedestrian);
    printf("  emergency   = %d\n", pid_emergency);
    printf("  logger      = %d\n", pid_logger);
    printf("  graphics    = %d\n", pid_graphics);
    printf("[MAIN] Press Ctrl+C to stop.\n");

    /* ── 11. Wait for controller to finish ──────────── */
    /*
     * Controller is the heartbeat of the system.
     * When it exits (SYSTEM_STOPPED), we shut down
     * everything else.
     */
    waitpid(pid_controller, NULL, 0);
    pid_controller = -1;

    /* ── 12. Signal all others to stop ──────────────── */
    ipc_sem_wait(sem_id, SEM_MUTEX);
    shared_data->system_status = SYSTEM_STOPPED;
    ipc_sem_signal(sem_id, SEM_MUTEX);

    shutdown_system();

    exit(0) ;

    return 0;
}
