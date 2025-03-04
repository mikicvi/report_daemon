#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <mqueue.h> // For POSIX message queues

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

/* File checking functions */
int check_required_files();
void check_missing_reports();

/* Logging function */
void log_message(const char *type, const char *message);

/* Directory and File functions */
int ensure_directory(const char *dir_path);
int file_exists(const char *file_path);

/* Lock/Unlock directory functions */
void lock_directories();
void unlock_directories();

/* Functions for moving and backing up XML reports */
void move_reports();
void perform_backup();

/* Function for monitoring the upload directory */
void monitor_directory();

/* Helper to check if it's a specific time */
int is_time(int hour, int minute);

/* Copy file using standard C file I/O */
int copy_file(const char *src, const char *dst);

/* Helper function to get the current date as "YYYY-MM-DD" */
void get_date_string(char *buffer, size_t size);

#endif