/* file_monitor.c – Monitor the upload directory and report events via IPC */

#include "utils.h"
#include <sys/inotify.h>
#include <limits.h>
#include <linux/limits.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <pwd.h>
#include <errno.h>
#include <mqueue.h> // Needed for POSIX message queues

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

/* Helper function to get file owner using stat */
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

/* Helper function that logs the event and reports it via IPC */
static void log_file_event(const char *event_type, const char *filename)
{
    struct file_event event;
    char filepath[PATH_MAX];
    event.timestamp = time(NULL);

    /* Construct full filepath from the upload directory */
    snprintf(filepath, sizeof(filepath), "%s/%s", UPLOAD_DIR, filename);

    /* Get file owner */
    get_file_owner(filepath, event.username, sizeof(event.username));
    strncpy(event.filename, filename, sizeof(event.filename) - 1);
    event.filename[sizeof(event.filename) - 1] = '\0';

    char log_entry[1024]; // Buffer for log message
    char timestamp_str[32];
    strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%d %H:%M:%S", localtime(&event.timestamp));

    snprintf(log_entry, sizeof(log_entry),
             "%s - File: %s, Owner: %s, Time: %s",
             event_type, event.filename, event.username, timestamp_str);

    /* Log the event locally */
    log_message("LOG", log_entry);

    /* Report the event via IPC */
    mqd_t mq = init_msg_queue();
    if (mq != (mqd_t)-1)
    {
        /* Send IPC message with event details; result 1 indicates success in processing the event */
        send_task_msg(mq, event_type, 1, log_entry);
        /* Clean up the message queue (close and unlink) */
        close_msg_queue(mq);
    }
}

void check_missing_reports()
{
    char filename[PATH_MAX];
    int missing_count = 0;
    char missing_files[DEPT_COUNT * 32] = ""; // Buffer to accumulate missing filenames

    /* Check each department file in the upload directory */
    for (int i = 1; i <= DEPT_COUNT; i++)
    {
        snprintf(filename, sizeof(filename), "%s/%s%d.xml",
                 UPLOAD_DIR, FILE_PREFIX, i);

        if (access(filename, F_OK) == -1)
        {
            if (strlen(missing_files) < sizeof(missing_files) - 32)
            {
                char temp[32];
                snprintf(temp, sizeof(temp), "dept%d.xml ", i);
                strcat(missing_files, temp);
            }
            missing_count++;
        }
    }

    if (missing_count > 0)
    {
        char log_entry[DEPT_COUNT * 32 + 64];
        snprintf(log_entry, sizeof(log_entry),
                 "Missing %d reports: %s",
                 missing_count, missing_files);
        log_message("ERROR", log_entry);

        /* Report missing file event via IPC */
        mqd_t mq = init_msg_queue();
        if (mq != (mqd_t)-1)
        {
            send_task_msg(mq, "missing_reports", 0, log_entry);
            close_msg_queue(mq);
        }
    }
    else
    {
        log_message("INFO", "All department reports present");
        mqd_t mq = init_msg_queue();
        if (mq != (mqd_t)-1)
        {
            send_task_msg(mq, "missing_reports", 1, "All reports present");
            close_msg_queue(mq);
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
        log_message("ERROR", "Error initializing inotify");
        return;
    }

    wd = inotify_add_watch(fd, UPLOAD_DIR,
                           IN_CREATE | IN_MODIFY | IN_DELETE |
                               IN_MOVED_TO | IN_CLOSE_WRITE |
                               IN_ATTRIB | IN_MOVE_SELF | IN_DELETE_SELF |
                               IN_MOVE | IN_DELETE_SELF | IN_DELETE);
    if (wd < 0)
    {
        log_message("ERROR", "Error adding watch on upload directory");
        close(fd);
        return;
    }

    log_message("INFO", "Started monitoring upload directory");

    while (1)
    {
        ssize_t length = read(fd, buffer, BUFFER_LEN);
        if (length == -1 && errno != EINTR)
        {
            log_message("ERROR", "Error reading inotify events");
            break;
        }

        if (length > 0)
        {
            char dbg_msg[256];
            snprintf(dbg_msg, sizeof(dbg_msg), "Received event data: %zd bytes", length);
            log_message("DEBUG", dbg_msg);

            for (char *ptr = buffer; ptr < buffer + length;)
            {
                struct inotify_event *event = (struct inotify_event *)ptr;

                if (event->len && !(event->mask & IN_ISDIR))
                {
                    if (event->mask & IN_CLOSE_WRITE || event->mask & IN_MOVED_TO)
                    {
                        /* Report file creation/completion */
                        log_file_event("CREATE", event->name);
                    }
                    else if (event->mask & IN_DELETE)
                    {
                        log_file_event("DELETE", event->name);
                    }
                    else if (event->mask & IN_MODIFY)
                    {
                        log_file_event("MODIFY", event->name);
                    }
                }
                ptr += sizeof(struct inotify_event) + event->len;
            }
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
}