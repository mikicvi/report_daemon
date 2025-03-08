#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "utils.h"

void lock_directories()
{
    log_message("INFO", "Locking directories...");

    // Save current umask and set strict permissions
    mode_t old_umask = umask(0077);

    if (chmod(UPLOAD_DIR, 0700) == -1)
    {
        log_message("ERROR", "Failed to lock upload directory");
    }
    if (chmod(REPORT_DIR, 0700) == -1)
    {
        log_message("ERROR", "Failed to lock report directory");
    }

    // Verify the permissions were set
    struct stat st;
    if (stat(UPLOAD_DIR, &st) == 0)
    {
        if ((st.st_mode & 0777) != 0700)
        {
            log_message("ERROR", "Upload directory permissions verification failed");
        }
    }

    if (stat(REPORT_DIR, &st) == 0)
    {
        if ((st.st_mode & 0777) != 0700)
        {
            log_message("ERROR", "Report directory permissions verification failed");
        }
    }

    umask(old_umask);
    log_message("INFO", "Directories locked");
}

void unlock_directories()
{
    log_message("INFO", "Unlocking directories...");

    // Save current umask and set permissive permissions
    mode_t old_umask = umask(0000);

    if (chmod(UPLOAD_DIR, 0777) == -1)
    {
        log_message("ERROR", "Failed to unlock upload directory");
    }
    if (chmod(REPORT_DIR, 0777) == -1)
    {
        log_message("ERROR", "Failed to unlock report directory");
    }

    // Verify the permissions were set
    struct stat st;
    if (stat(UPLOAD_DIR, &st) == 0)
    {
        if ((st.st_mode & 0777) != 0777)
        {
            log_message("ERROR", "Upload directory permissions verification failed");
        }
    }

    if (stat(REPORT_DIR, &st) == 0)
    {
        if ((st.st_mode & 0777) != 0777)
        {
            log_message("ERROR", "Report directory permissions verification failed");
        }
    }

    umask(old_umask);
    log_message("INFO", "Directories unlocked");
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
            log_message("ERROR", error_msg);
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

// Function to copy a file from src to dst
int copy_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    if (!in)
    {
        char err[256];
        snprintf(err, sizeof(err), "Failed to open source file %s: %s", src, strerror(errno));
        log_message("ERROR", err);
        return -1;
    }

    FILE *out = fopen(dst, "wb");
    if (!out)
    {
        char err[256];
        snprintf(err, sizeof(err), "Failed to open destination file %s: %s", dst, strerror(errno));
        log_message("ERROR", err);
        fclose(in);
        return -1;
    }

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), in)) > 0)
    {
        if (fwrite(buffer, 1, bytes, out) != bytes)
        {
            log_message("ERROR", "Error writing data during file copy");
            fclose(in);
            fclose(out);
            return -1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}