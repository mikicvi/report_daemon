#include "utils.h"
#include <mqueue.h>
#include <fcntl.h>    // For O_* constants
#include <sys/stat.h> // For mode constants
#include <errno.h>
#include <time.h>
#include <string.h>

#define MQ_MAX_MSG 10
#define MQ_MSG_SIZE sizeof(struct task_msg)

struct task_msg
{
    pid_t pid;         // PID of reporting process
    char task[64];     // Task identifier (e.g., "backup", "move_reports")
    int result;        // 1 for success, 0 for failure
    char message[128]; // Additional details
    time_t timestamp;
};

/* Initialize the POSIX message queue (create if necessary) */
mqd_t init_msg_queue()
{
    struct mq_attr attr;
    attr.mq_flags = 0; // blocking by default
    attr.mq_maxmsg = MQ_MAX_MSG;
    attr.mq_msgsize = MQ_MSG_SIZE;
    attr.mq_curmsgs = 0;

    mqd_t mq = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0666, &attr);
    if (mq == (mqd_t)-1)
    {
        log_message("ERROR", "Failed to open POSIX message queue");
    }
    return mq;
}

/* Send a task message via the POSIX message queue */
int send_task_msg(mqd_t mq, const char *task, int result, const char *msg_text)
{
    struct task_msg message;
    message.pid = getpid();
    strncpy(message.task, task, sizeof(message.task) - 1);
    message.task[sizeof(message.task) - 1] = '\0';
    message.result = result;
    strncpy(message.message, msg_text, sizeof(message.message) - 1);
    message.message[sizeof(message.message) - 1] = '\0';
    message.timestamp = time(NULL);

    if (mq_send(mq, (const char *)&message, sizeof(message), 0) == -1)
    {
        char err[256];
        snprintf(err, sizeof(err), "Failed to send task message: %s", strerror(errno));
        log_message("ERROR", err);
        return -1;
    }
    return 0;
}

/* Close the POSIX message queue without unlinking it */
void close_msg_queue(mqd_t mq)
{
    mq_close(mq);
}

/* Cleanup the POSIX message queue: this function unlinks the queue.
   Use it only when you intend to shut down the entire system. */
void cleanup_msg_queue(mqd_t mq)
{
    mq_close(mq);
    mq_unlink(MQ_NAME);
}