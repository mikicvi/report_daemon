#include "utils.h"
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <dirent.h>

#ifndef DT_REG
#define DT_REG 8
#endif

/* Helper function to get the current date as "YYYY-MM-DD" */
void get_date_string(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    snprintf(buffer, size, "%04d-%02d-%02d",
             tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday);
}

void move_reports()
{
    char date_dir[MAX_PATH_BUFFER];
    get_date_string(date_dir, sizeof(date_dir));

    /* Create a subdirectory under REPORT_DIR for today's reports */
    char full_report_dir[MAX_PATH_BUFFER];
    snprintf(full_report_dir, sizeof(full_report_dir), "%s/%s", REPORT_DIR, date_dir);
    if (ensure_directory(full_report_dir) == -1)
    {
        log_message("ERROR", "Failed to create today's reporting directory");
        return;
    }

    DIR *dir = opendir(UPLOAD_DIR);
    if (!dir)
    {
        log_message("ERROR", "Failed to open upload directory for moving reports");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_REG) // Only process regular files
        {
            char *ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".xml") == 0)
            {
                char src_path[MAX_PATH_BUFFER];
                char dst_path[MAX_PATH_BUFFER];
                snprintf(src_path, sizeof(src_path), "%s/%s", UPLOAD_DIR, entry->d_name);
                snprintf(dst_path, sizeof(dst_path), "%s/%s", full_report_dir, entry->d_name);

                if (rename(src_path, dst_path) == 0)
                {
                    char msg[1024];
                    snprintf(msg, sizeof(msg), "Moved file %s to reporting directory %s", entry->d_name, full_report_dir);
                    log_message("INFO", msg);

                    /* Report the move operation via POSIX IPC */
                    mqd_t mq = mq_open(MQ_NAME, O_RDWR);
                    if (mq != (mqd_t)-1)
                    {
                        send_task_msg(mq, "move_reports", 1, msg);
                        mq_close(mq);
                    }
                }
                else
                {
                    char err[1024];
                    snprintf(err, sizeof(err), "Failed to move file %s: %s", entry->d_name, strerror(errno));
                    log_message("ERROR", err);

                    mqd_t mq = mq_open(MQ_NAME, O_RDWR);
                    if (mq != (mqd_t)-1)
                    {
                        send_task_msg(mq, "move_reports", 0, err);
                        mq_close(mq);
                    }
                }
            }
        }
    }
    closedir(dir);
}

void perform_backup()
{
    log_message("LOG", "Starting backup process...");

    /* First, move new reports */
    move_reports();

    /* Check for missing files */
    check_missing_reports();

    char date_dir[MAX_PATH_BUFFER];
    get_date_string(date_dir, sizeof(date_dir));

    /* Create a subdirectory in the backup directory for today's backup */
    char backup_date_dir[MAX_PATH_BUFFER];
    snprintf(backup_date_dir, sizeof(backup_date_dir), "%s/%s", BACKUP_DIR, date_dir);
    if (ensure_directory(backup_date_dir) == -1)
    {
        log_message("ERROR", "Backup directory creation failed for today's date");
        mqd_t mq = mq_open(MQ_NAME, O_RDWR);
        if (mq != (mqd_t)-1)
        {
            send_task_msg(mq, "backup", 0, "Backup directory creation failed");
            mq_close(mq);
        }
        return;
    }

    /* Source directory: today's reporting folder */
    char current_report_dir[MAX_PATH_BUFFER];
    snprintf(current_report_dir, sizeof(current_report_dir), "%s/%s", REPORT_DIR, date_dir);

    DIR *dir = opendir(current_report_dir);
    if (!dir)
    {
        log_message("ERROR", "Failed to open today's reporting directory for backup");
        mqd_t mq = mq_open(MQ_NAME, O_RDWR);
        if (mq != (mqd_t)-1)
        {
            send_task_msg(mq, "backup", 0, "Unable to open reporting directory for backup");
            mq_close(mq);
        }
        return;
    }

    int copy_failures = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_REG)
        {
            char src_file[MAX_PATH_BUFFER];
            char dst_file[MAX_PATH_BUFFER];
            snprintf(src_file, sizeof(src_file), "%s/%s", current_report_dir, entry->d_name);
            snprintf(dst_file, sizeof(dst_file), "%s/%s", backup_date_dir, entry->d_name);

            if (copy_file(src_file, dst_file) == 0)
            {
                char msg[1024];
                snprintf(msg, sizeof(msg), "Backed up file %s successfully", entry->d_name);
                log_message("INFO", msg);
                mqd_t mq = mq_open(MQ_NAME, O_RDWR);
                if (mq != (mqd_t)-1)
                {
                    send_task_msg(mq, "copy_file", 1, msg);
                    mq_close(mq);
                }
            }
            else
            {
                char err[1024];
                snprintf(err, sizeof(err), "Failed to back up file %s", entry->d_name);
                log_message("ERROR", err);
                mqd_t mq = mq_open(MQ_NAME, O_RDWR);
                if (mq != (mqd_t)-1)
                {
                    send_task_msg(mq, "copy_file", 0, err);
                    mq_close(mq);
                }
                copy_failures++;
            }
        }
    }
    closedir(dir);

    if (copy_failures == 0)
    {
        log_message("LOG", "Backup process completed successfully");
        mqd_t mq = mq_open(MQ_NAME, O_RDWR);
        if (mq != (mqd_t)-1)
        {
            send_task_msg(mq, "backup", 1, "Backup completed successfully");
            mq_close(mq);
        }
    }
    else
    {
        log_message("ERROR", "Backup process encountered errors");
        mqd_t mq = mq_open(MQ_NAME, O_RDWR);
        if (mq != (mqd_t)-1)
        {
            send_task_msg(mq, "backup", 0, "Backup completed with errors");
            mq_close(mq);
        }
    }
}