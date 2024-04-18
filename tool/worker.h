#pragma once

#include <thread.h>
#include <atlaspacker/project.h>

typedef struct worker worker;
worker* worker_start(thread_mutex_t mutex);
void    worker_stop(worker* worker);
void    worker_push_job(worker* worker, void (*fn)(void*), void* ctx);
