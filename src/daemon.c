#include "utils.h"
#include <signal.h>
#include <time.h>
#include <syslog.h>

// Signal handler to manually trigger backup
void handle_signal(int sig)
{
    if (sig == SIGUSR1)
    {
        log_message("Manual backup triggered.");
        perform_backup();
    }
}

// Converts the program into a daemon
void make_daemon()
{
    pid_t pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS); // Parent exits

    umask(0);
    setsid();
    if (chdir("/") < 0)
        exit(EXIT_FAILURE);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main()
{
    make_daemon();
    openlog("report_daemon", LOG_PID | LOG_CONS, LOG_DAEMON);
    signal(SIGUSR1, handle_signal); // Allow manual backup trigger

    log_message("Daemon started.");

    while (1)
    {
        time_t now;
        struct tm *current_time;
        time(&now);
        current_time = localtime(&now);

        // Check if the time is 1 AM
        if (current_time->tm_hour == 1 && current_time->tm_min == 0)
        {
            log_message("Starting scheduled tasks (move reports and backup).");
            perform_backup();
        }

        monitor_directory(); // Detect file modifications
        sleep(60);           // Check every 60 seconds
    }

    closelog();
    return 0;
}