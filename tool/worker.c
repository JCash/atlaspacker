#include "worker.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct worker_job
{
    struct worker_job*  next;
    void                (*fn)(void* ctx);
    void*               ctx;
} worker_job;

typedef struct worker
{
    thread_ptr_t    thread;
    thread_mutex_t  mutex;
    worker_job*     jobs;
    int             run;
} worker;

static int WorkerThread(void* ctx)
{
    worker* w = (worker*)ctx;

    thread_timer_t timer;
    thread_timer_init( &timer );
    while (w->run)
    {
        worker_job* job = 0;
        thread_mutex_lock(&w->mutex);

        job = w->jobs;
        if (job)
        {
            w->jobs = w->jobs->next;
        }

        thread_mutex_unlock(&w->mutex);

        if (job)
        {
            job->fn(job->ctx);
        }
        free((void*)job);

        thread_timer_wait(&timer, 100); // nanoseconds
    }

    thread_timer_term( &timer );
    return 0;
}

worker* worker_start(thread_mutex_t mutex)
{
    worker* w = (worker*)malloc(sizeof(worker));
    w->run      = 1;
    w->mutex    = mutex;
    w->thread   = mg_thread_create(WorkerThread, (void*)w, 2 * (1024*1024));
    w->jobs     = 0;
    return w;
}

void worker_stop(worker* w)
{
    thread_mutex_lock(&w->mutex);
    w->run = 0;
    thread_mutex_unlock(&w->mutex);
    thread_join(w->thread);
}

void worker_push_job(worker* w, void (*fn)(void*), void* ctx)
{
    worker_job* job = (worker_job*)malloc(sizeof(worker_job));
    job->fn = fn;
    job->ctx = ctx;
    job->next = 0;

    thread_mutex_lock(&w->mutex);

    worker_job* last = w->jobs;
    if (!last)
    {
        w->jobs = job;
    }
    else
    {
        while(last->next)
        {
            last = last->next;
        }
    }

    thread_mutex_unlock(&w->mutex);
}
