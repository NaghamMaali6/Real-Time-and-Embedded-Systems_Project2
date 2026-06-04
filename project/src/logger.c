#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/msg.h>
#include <sys/stat.h>

/* =====================================================
 * Internal: file path stored at startup
 * ===================================================== */
static char log_path[256] = "logs/system.log";

/* =====================================================
 * Internal: write one line to file + stdout
 * ===================================================== */
static void write_entry(FILE* f, const char* text)
{
    time_t     now = time(NULL);
    struct tm* t   = localtime(&now);
    char       ts[32];

    strftime(ts, sizeof(ts), "%H:%M:%S", t);

    fprintf(f,      "[%s] %s\n", ts, text);
    fprintf(stdout, "[%s] %s\n", ts, text);
    fflush(f);
    fflush(stdout);
}

/* =====================================================
 * logger_log — called from any process to send a
 * formatted string to the logger via log_queue_id
 * ===================================================== */
void logger_log(const char* format, ...)
{
    IpcMessage msg;
    va_list    args;
    char       buf[240];   /* fits inside IpcMessage.data area via text */

    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    /*
     * We reuse IpcMessage but carry text differently:
     * pack the string into a dedicated log message.
     * msg.data is only 4 bytes so we need a text message.
     * We use log_queue_id with a LogMessage instead.
     */
    memset(&msg, 0, sizeof(msg));
    msg.msg_type  = MSG_LOG_EVENT;
    msg.direction = 0;
    msg.data      = 0;
    msg.timestamp = time(NULL);

    /* Send via log queue — text carried in a wrapper */
    /* Use a larger structure for log messages */
    {
        typedef struct {
            long msg_type;
            char text[240];
        } LogMsg;

        LogMsg lm;
        lm.msg_type = MSG_LOG_EVENT;
        strncpy(lm.text, buf, sizeof(lm.text) - 1);
        lm.text[sizeof(lm.text) - 1] = '\0';

        msgsnd(log_queue_id,
               &lm,
               sizeof(lm) - sizeof(long),
               0);
    }
}

/* =====================================================
 * logger_run — the logger process main loop.
 * Receives LogMsg from log_queue_id and writes them.
 * ===================================================== */
void logger_run(const char* log_file)
{
    typedef struct {
        long msg_type;
        char text[240];
    } LogMsg;

    FILE*  f;
    LogMsg lm;

    /* Store path for logger_log() calls in this process */
    strncpy(log_path, log_file, sizeof(log_path) - 1);

    /* Create logs/ directory if it doesn't exist */
    mkdir("logs", 0777);

    f = fopen(log_file, "a");
    if (f == NULL)
    {
        perror("[LOGGER] Cannot open log file");
        return;
    }

    write_entry(f, "SYSTEM_START");

    printf("[LOGGER] Logger started, writing to %s\n", log_file);

    while (1)
    {
        /* Blocking receive */
        if (msgrcv(log_queue_id,
                   &lm,
                   sizeof(lm) - sizeof(long),
                   MSG_LOG_EVENT,
                   0) == -1)
        {
            /* Queue destroyed = shutdown */
            break;
        }

        lm.text[239] = '\0';
        write_entry(f, lm.text);

        /* Check shutdown flag */
        ipc_sem_wait(sem_id, SEM_MUTEX);
        if (shared_data->system_status == SYSTEM_STOPPED)
        {
            ipc_sem_signal(sem_id, SEM_MUTEX);
            break;
        }
        ipc_sem_signal(sem_id, SEM_MUTEX);
    }

    write_entry(f, "SYSTEM_SHUTDOWN");
    fclose(f);

    printf("[LOGGER] Logger exiting.\n");
}
