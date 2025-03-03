#include "utils.h"
#include <time.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

void log_message(const char *type, const char *message)
{
    FILE *log = fopen(LOG_FILE, "a");
    if (log)
    {
        time_t now;
        struct tm *tm_info;
        char timestamp[26];

        time(&now);
        tm_info = localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

        if (fprintf(log, "[%s] %-7s %s\n", timestamp, type, message) < 0)
        {
            // If fprintf fails, try to log to stderr
            fprintf(stderr, "Error writing to log file\n");
        }

        fclose(log);
    }
    else
    {
        // If we can't open the log file, write to stderr
        fprintf(stderr, "Cannot open log file: %s\n", strerror(errno));
    }
}