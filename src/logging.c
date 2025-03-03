#include "utils.h"
#include <time.h>
#include <string.h>
#include <errno.h>

// Logs messages to the system log file
void log_message(const char *message)
{
    FILE *log = fopen(LOG_FILE, "a");
    if (log)
    {
        time_t now = time(NULL);
        fprintf(log, "[%s] %s\n", ctime(&now), message);
        fclose(log);
    }
    else
    {
        fprintf(stderr, "Error opening log file: %s\n", strerror(errno));
    }
}