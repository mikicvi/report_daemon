#include "utils.h"
#include <signal.h>
#include <time.h>
#include <syslog.h>

// Signal handler to manually trigger backup
void handle_signal(int sig)
{
    if (sig == SIGUSR1)
    {
        log_message("INFO", "Manual backup triggered");
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

// Helper function to check if it's a specific time
int is_time(int hour, int minute)
{
    time_t now;
    struct tm *current_time;
    time(&now);
    current_time = localtime(&now);
    return current_time->tm_hour == hour && current_time->tm_min == minute;
}

int main()
{
    make_daemon();
    signal(SIGUSR1, handle_signal);

    log_message("INFO", "Daemon started");

    // Track last check times to prevent multiple executions in the same minute
    time_t last_missing_check = 0;
    time_t last_backup_check = 0;
    pid_t monitor_pid = fork();
    if (monitor_pid == 0)
    {
        // Child process
        monitor_directory();
        exit(EXIT_SUCCESS);
    }
    else if (monitor_pid < 0)
    {
        log_message("ERROR", "Failed to fork monitor process");
    }

    while (1)
    {
        time_t now = time(NULL);

        if (is_time(22, 30) && (now - last_missing_check) >= 60)
        {
            log_message("INFO", "Checking for missing reports at deadline");
            lock_directories();
            check_missing_reports();
            last_missing_check = now;
        }

        // Check for backup time at 1:00
        if (is_time(1, 0) && (now - last_backup_check) >= 60)
        {
            log_message("INFO", "Starting scheduled backup");
            perform_backup();
            last_backup_check = now;
        }
        sleep(10); // Check every 10 seconds
    }

    return 0;
}