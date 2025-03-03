#include "utils.h"
#include <sys/shm.h>
#include <errno.h>
#include <string.h>
#include <time.h> // Include time.h

// Shared memory structure matching backup.c
struct backup_status
{
    int success;
    char message[256];
    time_t timestamp;
};

#define SHM_KEY 1234

int init_shared_memory()
{
    // Create shared memory segment
    int shmid = shmget(SHM_KEY, sizeof(struct backup_status), IPC_CREAT | 0666);
    if (shmid == -1)
    {
        log_message("Error: Failed to create shared memory");
        return -1;
    }

    // Attach to the segment
    struct backup_status *status = shmat(shmid, NULL, 0);
    if (status == (void *)-1)
    {
        log_message("Error: Failed to attach shared memory");
        return -1;
    }

    // Initialize the status
    status->success = BACKUP_FAILURE;
    strcpy(status->message, "Backup not yet performed");
    status->timestamp = time(NULL);

    // Detach from segment
    if (shmdt(status) == -1)
    {
        log_message("Error: Failed to detach shared memory");
        return -1;
    }

    return shmid;
}

void cleanup_shared_memory()
{
    int shmid = shmget(SHM_KEY, sizeof(struct backup_status), 0666);
    if (shmid != -1)
    {
        if (shmctl(shmid, IPC_RMID, NULL) == -1)
        {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg),
                     "Error removing shared memory: %s", strerror(errno));
            log_message(error_msg);
        }
    }
}

int get_backup_status()
{
    int shmid = shmget(SHM_KEY, sizeof(struct backup_status), 0666);
    if (shmid == -1)
    {
        log_message("Error: Cannot access shared memory");
        return BACKUP_FAILURE;
    }

    struct backup_status *status = shmat(shmid, NULL, 0);
    if (status == (void *)-1)
    {
        log_message("Error: Failed to attach to shared memory");
        return BACKUP_FAILURE;
    }

    int result = status->success;

    if (shmdt(status) == -1)
    {
        log_message("Error: Failed to detach shared memory");
        return BACKUP_FAILURE;
    }

    return result;
}