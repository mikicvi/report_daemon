#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#define UPLOAD_DIR "/var/reports/uploads"
#define REPORT_DIR "/var/reports/reporting"

void lock_directories()
{
    syslog(LOG_INFO, "Locking directories...");

    if (chmod(UPLOAD_DIR, 0000) == -1)
    {
        syslog(LOG_ERR, "Failed to lock upload directory");
    }
    if (chmod(REPORT_DIR, 0000) == -1)
    {
        syslog(LOG_ERR, "Failed to lock report directory");
    }

    syslog(LOG_INFO, "Directories locked");
}

void unlock_directories()
{
    syslog(LOG_INFO, "Unlocking directories...");

    if (chmod(UPLOAD_DIR, 0777) == -1)
    {
        syslog(LOG_ERR, "Failed to unlock upload directory");
    }
    if (chmod(REPORT_DIR, 0777) == -1)
    {
        syslog(LOG_ERR, "Failed to unlock report directory");
    }

    syslog(LOG_INFO, "Directories unlocked");
}

// New logging function for errors
void log_error(const char *message)
{
    syslog(LOG_ERR, "%s", message);
}

// Function to ensure a directory exists
int ensure_directory(const char *dir_path)
{
    struct stat st = {0};

    if (stat(dir_path, &st) == -1)
    {
        if (mkdir(dir_path, 0777) == -1)
        {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "Error creating directory %s: %s", dir_path, strerror(errno));
            log_error(error_msg);
            return -1;
        }
    }
    return 0;
}

// Function to check if a file exists
int file_exists(const char *file_path)
{
    return access(file_path, F_OK) != -1;
}

// Function to check if a file has been modified
int has_file_changed(const char *file_path)
{
    struct stat current_stat;
    static struct stat previous_stat;
    static int first_run = 1;

    if (stat(file_path, &current_stat) == -1)
    {
        log_error("Error getting file status.");
        return 0; // Assume no change if file not accessible
    }

    if (first_run)
    {
        previous_stat = current_stat;
        first_run = 0;
        return 1; // Consider it changed on the first run
    }

    if (current_stat.st_mtime > previous_stat.st_mtime)
    {
        previous_stat = current_stat; // Update previous stat
        return 1;                     // File has changed
    }
    else
    {
        return 0; // File has not changed
    }
}