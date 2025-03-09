#include "utils.h"
#include <signal.h>
#include <time.h>
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mqueue.h>
#include <sys/wait.h>

#define PID_FILE "/tmp/report_daemon.pid"

volatile sig_atomic_t backup_requested = 0;

/* Signal handler for SIGUSR1 - sets a flag to trigger manual backup */
void handle_signal(int sig)
{
    if (sig == SIGUSR1)
    {
        log_message("INFO", "SIGUSR1 received: scheduling manual backup");
        backup_requested = 1;
    }
}

/* Signal handler for SIGCHLD - reap zombie processes */
void handle_child(int sig)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

/* Write the daemon's PID to a file so that the client mode can find it */
void write_pid_file()
{
    FILE *fp = fopen(PID_FILE, "w");
    if (fp)
    {
        fprintf(fp, "%d", getpid());
        fclose(fp);
        log_message("INFO", "PID file written successfully to " PID_FILE);
    }
    else
    {
        log_message("ERROR", "Failed to write PID file to " PID_FILE);
    }
}

/* Client mode: read the PID file and send SIGUSR1 to trigger manual backup */
int send_signal_to_daemon(const char *pid_file)
{
    FILE *fp = fopen(pid_file, "r");
    if (!fp)
    {
        fprintf(stderr, "Unable to open PID file '%s'\n", pid_file);
        return -1;
    }
    int pid;
    if (fscanf(fp, "%d", &pid) != 1)
    {
        fprintf(stderr, "Failed to read PID from file '%s'\n", pid_file);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    if (kill(pid, SIGUSR1) == -1)
    {
        fprintf(stderr, "Failed to send SIGUSR1 to PID %d: %s\n", pid, strerror(errno));
        return -1;
    }
    printf("Sent SIGUSR1 to daemon (PID %d) to trigger manual backup.\n", pid);
    return 0;
}

/* Daemonize the process */
void make_daemon()
{
    pid_t pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
    {
        exit(EXIT_SUCCESS);
    }

    // Child continues with daemon initialization
    umask(0);
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    // Second fork to ensure we can't reacquire a terminal
    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);

    if (chdir("/") < 0)
        exit(EXIT_FAILURE);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Set up signal handlers after daemonizing
    signal(SIGCHLD, handle_child);
    signal(SIGUSR1, handle_signal);
}

static void cleanup(void)
{
    unlink(PID_FILE);
    log_message("INFO", "Daemon shutting down, cleaned up PID file");
}

int main(int argc, char *argv[])
{
    /* If a command-line argument "manual-backup" is provided, run in client mode */
    if (argc > 1 && strcmp(argv[1], "manual-backup") == 0)
    {
        if (send_signal_to_daemon(PID_FILE) == 0)
            return 0;
        else
            return EXIT_FAILURE;
    }

    /* Daemonize first */
    make_daemon();

    /* Initialize the POSIX message queue for IPC */
    mqd_t mq = init_msg_queue();
    if (mq == (mqd_t)-1)
    {
        log_message("ERROR", "Failed to initialize message queue");
    }
    else
    {
        close_msg_queue(mq);
    }

    log_message("INFO", "Daemon started");

    /* Write PID file after daemonization */
    write_pid_file();

    time_t last_missing_check = 0;
    time_t last_backup_check = 0;

    /* Fork monitor process after daemon is fully initialized */
    pid_t monitor_pid = fork();
    if (monitor_pid == 0)
    {
        monitor_directory();
        exit(EXIT_SUCCESS);
    }
    else if (monitor_pid < 0)
    {
        log_message("ERROR", "Failed to fork monitor process");
        return EXIT_FAILURE;
    }

    /* Set up cleanup handler */
    atexit(cleanup);

    /* Main daemon loop */
    while (1)
    {
        time_t now = time(NULL);

        /* Check for missing reports at 23:30 (upload deadline) */
        if (is_time(23, 30) && (now - last_missing_check) >= 60)
        {
            log_message("INFO", "Checking for missing reports at deadline");
            lock_directories();
            check_missing_reports();
            last_missing_check = now;
        }

        /* Scheduled backup at 1:00 */
        if (is_time(1, 0) && (now - last_backup_check) >= 60)
        {
            log_message("INFO", "Starting scheduled backup");
            perform_backup();
            unlock_directories();
            last_backup_check = now;
        }

        /* Manual backup triggered by SIGUSR1 */
        if (backup_requested)
        {
            log_message("INFO", "Performing manual backup as requested");
            lock_directories();
            perform_backup();
            unlock_directories();
            backup_requested = 0;
        }

        sleep(5);
    }

    return 0;
}