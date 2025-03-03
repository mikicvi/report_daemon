#include "utils.h"
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sys/shm.h>
#include <time.h>
#include <dirent.h>

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
            log_message(msg);
            missing_count++;
        }
    }

    return missing_count;
}

void move_reports()
{
    log_message("Starting report transfer process...");

    DIR *dir;
    struct dirent *entry;

    // Open the uploads directory
    dir = opendir(UPLOAD_DIR);
    if (dir == NULL)
    {
        log_error("Error: Could not open upload directory.");
        return;
    }

    // Read each entry in the directory
    while ((entry = readdir(dir)) != NULL)
    {
        struct stat path_stat;
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", UPLOAD_DIR, entry->d_name);
        stat(filepath, &path_stat);
        if (S_ISREG(path_stat.st_mode))
        { // Check if it's a regular file
            char old_path[512], new_path[512];

            // Construct the full paths
            snprintf(old_path, sizeof(old_path), "%s/%s", UPLOAD_DIR, entry->d_name);
            snprintf(new_path, sizeof(new_path), "%s/%s", REPORT_DIR, entry->d_name);

            // Move the file
            if (rename(old_path, new_path) == 0)
            {
                char log_msg[512];
                snprintf(log_msg, sizeof(log_msg), "Moved report: %s to reporting directory.", entry->d_name);
                log_message(log_msg);
            }
            else
            {
                char error_msg[512];
                snprintf(error_msg, sizeof(error_msg), "Error moving report: %s - %s", entry->d_name, strerror(errno));
                log_error(error_msg);
            }
        }
    }

    // Close the directory
    closedir(dir);

    log_message("Report transfer process completed.");
}

void perform_backup()
{
    log_message("Starting backup process...");

    // Setup shared memory for IPC
    int shmid = shmget(SHM_KEY, sizeof(struct backup_status),
                       IPC_CREAT | 0666);
    if (shmid == -1)
    {
        log_error("Error: Failed to create shared memory");
        return;
    }

    struct backup_status *status = shmat(shmid, NULL, 0);
    if (status == (void *)-1)
    {
        log_error("Error: Failed to attach shared memory");
        return;
    }

    // Lock directories before backup
    lock_directories();

    // Move reports from upload to reporting directory
    move_reports();

    // Check for missing files
    int missing = check_required_files();
    if (missing > 0)
    {
        snprintf(status->message, sizeof(status->message),
                 "Warning: %d files missing", missing);
        status->success = 0;
        status->timestamp = time(NULL);
        log_message(status->message);
    }

    // Ensure backup directory exists
    if (ensure_directory(BACKUP_DIR) == -1)
    {
        status->success = 0;
        strcpy(status->message, "Backup directory creation failed");
        status->timestamp = time(NULL);
        unlock_directories();
        shmdt(status);
        return;
    }

    // Perform backup
    char backup_command[512];
    snprintf(backup_command, sizeof(backup_command), "cp -r %s/* %s/", REPORT_DIR, BACKUP_DIR);

    int result = system(backup_command);
    if (result == -1)
    {
        log_error("Error: Backup failed");
        status->success = 0;
        strcpy(status->message, "Backup operation failed");
        status->timestamp = time(NULL);
        unlock_directories();
        shmdt(status);
        return;
    }

    // Success case
    status->success = 1;
    strcpy(status->message, "Backup completed successfully");
    status->timestamp = time(NULL);

    unlock_directories();
    log_message("Backup completed successfully.");

    shmdt(status);
}