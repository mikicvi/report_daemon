#include "utils.h"
#include <sys/inotify.h>
#include <limits.h>
#include <linux/limits.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <pwd.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUFFER_LEN (1024 * (EVENT_SIZE + 16))
#define DEPT_COUNT 4
#define FILE_PREFIX "dept"

struct file_event
{
    char username[256];
    char filename[256];
    time_t timestamp;
};

// Helper function to get file owner using stat
static void get_file_owner(const char *filepath, char *username, size_t size)
{
    struct stat file_stat;
    if (stat(filepath, &file_stat) == 0)
    {
        struct passwd *pw = getpwuid(file_stat.st_uid);
        if (pw)
        {
            strncpy(username, pw->pw_name, size - 1);
            username[size - 1] = '\0';
        }
        else
        {
            snprintf(username, size, "%d", file_stat.st_uid);
        }
    }
    else
    {
        strncpy(username, "unknown", size - 1);
    }
}

static void log_file_event(const char *event_type, const char *filename)
{
    struct file_event event;
    char filepath[PATH_MAX];
    event.timestamp = time(NULL);

    // Construct full filepath
    snprintf(filepath, sizeof(filepath), "%s/%s", UPLOAD_DIR, filename);

    // Get file owner
    get_file_owner(filepath, event.username, sizeof(event.username));
    strncpy(event.filename, filename, sizeof(event.filename) - 1);

    char log_entry[1024]; // Increased size
    char timestamp_str[32];
    strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%d %H:%M:%S",
             localtime(&event.timestamp));

    snprintf(log_entry, sizeof(log_entry),
             "%s - File: %s, Owner: %s, Time: %s",
             event_type, event.filename, event.username, timestamp_str);
    log_message(log_entry);
}

void check_missing_reports()
{
    time_t now;
    struct tm *current_time;
    time(&now);
    current_time = localtime(&now);

    // Check if the time is 11:30 PM
    if (current_time->tm_hour == 23 && current_time->tm_min >= 30)
    {
        char filename[256];
        int missing_count = 0;

        for (int i = 1; i <= DEPT_COUNT; i++)
        {
            snprintf(filename, sizeof(filename), "%s/%s%d.xml",
                     UPLOAD_DIR, FILE_PREFIX, i);

            if (!file_exists(filename))
            {
                char log_entry[512]; // Increased size
                snprintf(log_entry, sizeof(log_entry), "Missing report: %s", filename);
                log_error(log_entry);
                missing_count++;
            }
        }

        if (missing_count > 0)
        {
            char log_entry[256];
            snprintf(log_entry, sizeof(log_entry), "Total missing reports: %d", missing_count);
            log_error(log_entry);
        }
    }
}

void monitor_directory()
{
    int fd, wd;
    char buffer[BUFFER_LEN];

    fd = inotify_init();
    if (fd < 0)
    {
        log_error("Error initializing inotify.");
        return;
    }

    wd = inotify_add_watch(fd, UPLOAD_DIR, IN_MODIFY | IN_CREATE | IN_DELETE);
    if (wd < 0)
    {
        log_error("Error adding watch on upload directory.");
        return;
    }

    log_message("Monitoring upload directory for file changes.");

    while (1)
    {
        int length = read(fd, buffer, BUFFER_LEN);
        if (length < 0)
        {
            log_error("Error reading inotify event.");
            break;
        }

        int i = 0;
        while (i < length)
        {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            if (event->len)
            {
                if (event->mask & IN_CREATE)
                {
                    log_file_event("CREATE", event->name);
                }
                else if (event->mask & IN_MODIFY)
                {
                    log_file_event("MODIFY", event->name);
                }
                else if (event->mask & IN_DELETE)
                {
                    log_file_event("DELETE", event->name);
                }
            }
            i += EVENT_SIZE + event->len;
        }

        check_missing_reports();
    }

    inotify_rm_watch(fd, wd);
    close(fd);
}