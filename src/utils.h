#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <mqueue.h>

#define UPLOAD_DIR "/var/reports/uploads"
#define REPORT_DIR "/var/reports/reporting"
#define BACKUP_DIR "/var/reports/backup"
#define LOG_FILE "/var/log/report_daemon.log"

// Definitions for backup status
#define BACKUP_SUCCESS 1
#define BACKUP_FAILURE 0

// Message queue name
#define MQ_NAME "/report_daemon_mq"

#ifndef DEPT_COUNT
#define DEPT_COUNT 4
#endif

#ifndef FILE_PREFIX
#define FILE_PREFIX "dept"
#endif

#define MAX_PATH_BUFFER 4096

/* IPC functions using POSIX message queues */
mqd_t init_msg_queue();
int send_task_msg(mqd_t mq, const char *task, int result, const char *msg_text);
void cleanup_msg_queue(mqd_t mq);
void close_msg_queue(mqd_t mq);

// File checking functions
void check_missing_reports();

// Logging function
void log_message(const char *type, const char *message);

// Directory and File functions
int ensure_directory(const char *dir_path);

// Lock directories during backup
void lock_directories();

// Unlock directories after backup
void unlock_directories();

// Perform backup functionality
void perform_backup();

// Function monitoring reports dir
void monitor_directory();

// Helper to check if it's a specific time
int is_time(int hour, int minute);

// Helper to copy a file
int copy_file(const char *src, const char *dst);

#endif