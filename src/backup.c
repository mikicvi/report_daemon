#include "utils.h"
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sys/shm.h>
#include <time.h>
#include <sys/wait.h> // Add for waitpid
#include <unistd.h>   // Add for fork/exec

#define SHM_KEY 1234
#define DEPT_COUNT 4
#define FILE_PREFIX "dept"

// Shared memory structure for IPC
struct backup_status
{
    int success;
    char message[256];
    time_t timestamp;
};

int check_required_files()
{
    char filename[256];
    int missing_count = 0;

    for (int i = 1; i <= DEPT_COUNT; i++)
    {
        snprintf(filename, sizeof(filename), "%s/%s%d.xml",
                 REPORT_DIR, FILE_PREFIX, i);

        if (access(filename, F_OK) == -1)
        {
            char msg[300];
            snprintf(msg, sizeof(msg), "Missing report: %s", filename);
            log_message("LOG", msg);
            missing_count++;
        }
    }

    return missing_count;
}

void move_reports()
{
    pid_t pid = fork();

    if (pid == -1)
    {
        log_message("ERROR", "Fork failed during move operation");
        return;
    }

    if (pid == 0)
    {
        // Child process
        char command[1024];
        snprintf(command, sizeof(command),
                 "mv %s/*.xml %s/ 2>/dev/null",
                 UPLOAD_DIR, REPORT_DIR);

        execl("/bin/sh", "sh", "-c", command, (char *)NULL);

        // If execl returns, there was an error
        log_message("ERROR", "Exec failed during move operation");
        _exit(1);
    }
    else
    {
        // Parent process
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            log_message("INFO", "Files moved successfully");
        }
        else
        {
            log_message("ERROR", "Move operation failed");
        }
    }
}

void perform_backup()
{
    log_message("LOG", "Starting backup process...");

    // Setup shared memory for IPC
    int shmid = shmget(SHM_KEY, sizeof(struct backup_status),
                       IPC_CREAT | 0666);
    if (shmid == -1)
    {
        log_message("LOG", "Error: Failed to create shared memory");
        return;
    }

    struct backup_status *status = shmat(shmid, NULL, 0);
    if (status == (void *)-1)
    {
        log_message("LOG", "Error: Failed to attach shared memory");
        return;
    }

    // Move reports before backup
    move_reports();

    // Lock directories before backup
    lock_directories();

    // Check for missing files
    int missing = check_required_files();
    if (missing > 0)
    {
        snprintf(status->message, sizeof(status->message),
                 "Warning: %d files missing", missing);
        status->success = 0;
        status->timestamp = time(NULL);
        log_message("LOG", status->message);
    }

    // Ensure backup directory exists
    if (mkdir(BACKUP_DIR, 0755) == -1 && errno != EEXIST)
    {
        log_message("LOG", "Error: Failed to create backup directory");
        status->success = 0;
        strcpy(status->message, "Backup directory creation failed");
        status->timestamp = time(NULL);
        unlock_directories();
        shmdt(status);
        return;
    }

    pid_t pid = fork();

    if (pid == -1)
    {
        log_message("ERROR", "Fork failed during backup operation");
        status->success = 0;
        strcpy(status->message, "Backup operation failed - fork error");
        status->timestamp = time(NULL);
        unlock_directories();
        shmdt(status);
        return;
    }

    if (pid == 0)
    {
        // Child process
        char src_path[512];
        char dst_path[512];
        snprintf(src_path, sizeof(src_path), "%s/*", REPORT_DIR);
        snprintf(dst_path, sizeof(dst_path), "%s/", BACKUP_DIR);

        execl("/bin/cp", "cp", "-r", src_path, dst_path, (char *)NULL);

        // If execl returns, there was an error
        log_message("ERROR", "Exec failed during backup operation");
        _exit(1);
    }
    else
    {
        // Parent process
        int child_status;
        waitpid(pid, &child_status, 0);

        if (WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0)
        {
            // Success case
            status->success = 1;
            strcpy(status->message, "Backup completed successfully");
            status->timestamp = time(NULL);
            log_message("LOG", "Backup completed successfully.");
        }
        else
        {
            // Failure case
            log_message("ERROR", "Backup operation failed");
            status->success = 0;
            strcpy(status->message, "Backup operation failed - child process error");
            status->timestamp = time(NULL);
        }

        unlock_directories();
        shmdt(status);
    }
}