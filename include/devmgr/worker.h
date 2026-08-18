#ifndef DEVMGR_WORKER_H
#define DEVMGR_WORKER_H

#include "devmgr/firmware.h"

#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>

#define DEVMGR_WORKER_PATH_MAX 4096U

struct devmgr_validation_result {
    uint32_t job_id;
    int status;
    struct devmgr_firmware_image image;
};

struct devmgr_worker {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int event_fd;
    bool initialized;
    bool stop;
    bool job_pending;
    bool job_running;
    bool result_pending;
    uint32_t job_id;
    char job_path[DEVMGR_WORKER_PATH_MAX];
    struct devmgr_validation_result result;
};

int devmgr_worker_init(struct devmgr_worker *worker);
int devmgr_worker_submit_firmware(struct devmgr_worker *worker, uint32_t job_id,
                                  const char *path);
int devmgr_worker_get_event_fd(const struct devmgr_worker *worker);
int devmgr_worker_take_result(struct devmgr_worker *worker,
                              struct devmgr_validation_result *result);
void devmgr_worker_destroy(struct devmgr_worker *worker);

#endif
