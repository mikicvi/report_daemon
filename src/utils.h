#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define UPLOAD_DIR "/var/reports/uploads"
#define REPORT_DIR "/var/reports/reporting"
#define BACKUP_DIR "/var/reports/backup"
#define LOG_FILE "/var/log/report_daemon.log"

// definitions for backup status
#define BACKUP_SUCCESS 1
#define BACKUP_FAILURE 0

// IPC functions
int init_shared_memory();
void cleanup_shared_memory();
int get_backup_status();

// File checking functions
int check_required_files();
void check_missing_reports();

// Logging function
void log_message(const char *type, const char *message);

// Directory and File functions
int ensure_directory(const char *dir_path);
int file_exists(const char *file_path);
int has_file_changed(const char *file_path);

// Lock directories during backup
void lock_directories();

// Unlock directories after backup
void unlock_directories();

// Move XML reports at 1AM
void move_reports();

// Perform backup at 1AM
void perform_backup();

// Function to check file modifications
void monitor_directory();

#endif