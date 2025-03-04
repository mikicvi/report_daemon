/* ipc_monitor.c – A simple POSIX message queue monitor */

#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <fcntl.h>    // For O_* constants
#include <sys/stat.h> // For mode constants
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include "utils.h"

#define MQ_MSG_SIZE sizeof(struct task_msg)

struct task_msg
{
    pid_t pid;         // PID of reporting process
    char task[64];     // Task identifier (e.g., "backup", "move_reports")
    int result;        // 1 for success, 0 for failure
    char message[128]; // Additional details
    time_t timestamp;
};

int main(void)
{
    mqd_t mq;
    struct mq_attr attr;
    char buffer[MQ_MSG_SIZE];
    ssize_t bytes_read;
    struct task_msg msg;
    time_t t;
    char time_str[64];

    /* Open (or create) the message queue for reading in non-blocking mode */
    mq = mq_open(MQ_NAME, O_RDONLY | O_NONBLOCK | O_CREAT, 0666, NULL);
    if (mq == (mqd_t)-1)
    {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }

    if (mq_getattr(mq, &attr) == -1)
    {
        perror("mq_getattr");
        exit(EXIT_FAILURE);
    }
    printf("Monitoring POSIX message queue '%s'...\n", MQ_NAME);
    printf("Max messages: %ld, Message size: %ld bytes\n\n", attr.mq_maxmsg, attr.mq_msgsize);

    while (1)
    {
        bytes_read = mq_receive(mq, buffer, MQ_MSG_SIZE, NULL);
        if (bytes_read >= 0)
        {
            memcpy(&msg, buffer, sizeof(msg));
            t = msg.timestamp;
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&t));
            printf("PID: %d | Task: %s | Result: %s | Time: %s\nMessage: %s\n\n",
                   msg.pid, msg.task, msg.result ? "SUCCESS" : "FAILURE", time_str, msg.message);
        }
        else
        {
            if (errno != EAGAIN)
            {
                perror("mq_receive");
            }
            usleep(500000); // 500ms
        }
    }

    mq_close(mq);
    return 0;
}