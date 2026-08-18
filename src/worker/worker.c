#define _GNU_SOURCE

#include "devmgr/worker.h"

#include "devmgr/error.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>

static void notify_reactor(int event_fd)
{
    uint64_t value = 1U;
    ssize_t count;
    do {
        count = write(event_fd, &value, sizeof(value));
    } while (count < 0 && errno == EINTR);
}

static void *worker_main(void *opaque)
{
    struct devmgr_worker *worker = opaque;
    for (;;) {
        char path[DEVMGR_WORKER_PATH_MAX];
        uint32_t job_id;
        (void)pthread_mutex_lock(&worker->mutex);
        while (!worker->stop && !worker->job_pending)
            (void)pthread_cond_wait(&worker->condition, &worker->mutex);
        if (worker->stop) {
            (void)pthread_mutex_unlock(&worker->mutex);
            break;
        }
        job_id = worker->job_id;
        memcpy(path, worker->job_path, strlen(worker->job_path) + 1U);
        worker->job_pending = false;
        worker->job_running = true;
        (void)pthread_mutex_unlock(&worker->mutex);

        struct devmgr_validation_result result = {.job_id = job_id, .image = {.fd = -1}};
        result.status = devmgr_firmware_open(&result.image, path);

        (void)pthread_mutex_lock(&worker->mutex);
        worker->result = result;
        worker->job_running = false;
        worker->result_pending = true;
        (void)pthread_mutex_unlock(&worker->mutex);
        notify_reactor(worker->event_fd);
    }
    return NULL;
}

int devmgr_worker_init(struct devmgr_worker *worker)
{
    if (worker == NULL) return DEVMGR_ERROR_INVALID;
    memset(worker, 0, sizeof(*worker));
    worker->event_fd = -1;
    worker->result.image.fd = -1;
    if (pthread_mutex_init(&worker->mutex, NULL) != 0) return DEVMGR_ERROR_IO;
    if (pthread_cond_init(&worker->condition, NULL) != 0) {
        (void)pthread_mutex_destroy(&worker->mutex);
        return DEVMGR_ERROR_IO;
    }
    worker->event_fd = eventfd(0U, EFD_NONBLOCK | EFD_CLOEXEC);
    if (worker->event_fd < 0) {
        (void)pthread_cond_destroy(&worker->condition);
        (void)pthread_mutex_destroy(&worker->mutex);
        return DEVMGR_ERROR_IO;
    }
    if (pthread_create(&worker->thread, NULL, worker_main, worker) != 0) {
        (void)close(worker->event_fd);
        (void)pthread_cond_destroy(&worker->condition);
        (void)pthread_mutex_destroy(&worker->mutex);
        worker->event_fd = -1;
        return DEVMGR_ERROR_IO;
    }
    worker->initialized = true;
    return DEVMGR_OK;
}

int devmgr_worker_submit_firmware(struct devmgr_worker *worker, uint32_t job_id,
                                  const char *path)
{
    if (worker == NULL || !worker->initialized || job_id == 0U || path == NULL)
        return DEVMGR_ERROR_INVALID;
    size_t length = strlen(path);
    if (length == 0U || length >= sizeof(worker->job_path)) return DEVMGR_ERROR_LIMIT;
    (void)pthread_mutex_lock(&worker->mutex);
    if (worker->stop || worker->job_pending || worker->job_running || worker->result_pending) {
        (void)pthread_mutex_unlock(&worker->mutex);
        return DEVMGR_ERROR_BUSY;
    }
    memcpy(worker->job_path, path, length + 1U);
    worker->job_id = job_id;
    worker->job_pending = true;
    (void)pthread_cond_signal(&worker->condition);
    (void)pthread_mutex_unlock(&worker->mutex);
    return DEVMGR_OK;
}

int devmgr_worker_get_event_fd(const struct devmgr_worker *worker)
{
    return worker == NULL || !worker->initialized ? -1 : worker->event_fd;
}

int devmgr_worker_take_result(struct devmgr_worker *worker,
                              struct devmgr_validation_result *result)
{
    if (worker == NULL || result == NULL || !worker->initialized) return DEVMGR_ERROR_INVALID;
    (void)pthread_mutex_lock(&worker->mutex);
    if (!worker->result_pending) {
        (void)pthread_mutex_unlock(&worker->mutex);
        return DEVMGR_ERROR_NOT_FOUND;
    }
    *result = worker->result;
    worker->result.image.fd = -1;
    worker->result.image.data = NULL;
    worker->result.image.size = 0U;
    worker->result_pending = false;
    (void)pthread_mutex_unlock(&worker->mutex);
    return DEVMGR_OK;
}

void devmgr_worker_destroy(struct devmgr_worker *worker)
{
    if (worker == NULL || !worker->initialized) return;
    (void)pthread_mutex_lock(&worker->mutex);
    worker->stop = true;
    (void)pthread_cond_signal(&worker->condition);
    (void)pthread_mutex_unlock(&worker->mutex);
    (void)pthread_join(worker->thread, NULL);
    if (worker->result_pending) devmgr_firmware_close(&worker->result.image);
    (void)close(worker->event_fd);
    (void)pthread_cond_destroy(&worker->condition);
    (void)pthread_mutex_destroy(&worker->mutex);
    worker->initialized = false;
    worker->event_fd = -1;
}
