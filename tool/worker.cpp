// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#include "worker.h"
#include <stdlib.h>
#include <stdio.h>

#include <external/thread.h>

struct WorkerJob
{
    struct WorkerJob*  next;
    void                (*fn)(void* ctx);
    void*               ctx;
};

struct Worker
{
    thread_ptr_t    thread;
    HMutex          mutex;
    WorkerJob*     jobs;
    int             run;
};

static int WorkerThread(void* ctx)
{
    Worker* w = (Worker*)ctx;

    thread_timer_t timer;
    thread_timer_init( &timer );
    while (w->run)
    {
        WorkerJob* job = 0;
        {
            SCOPED_MUTEX(w->mutex);
            job = w->jobs;
            if (job)
            {
                w->jobs = w->jobs->next;
            }
        }

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

Worker* WorkerStart(HMutex mutex)
{
    Worker* w   = (Worker*)malloc(sizeof(Worker));
    w->run      = 1;
    w->jobs     = 0;
    w->mutex    = mutex;
    w->thread   = mg_thread_create(WorkerThread, (void*)w, 2 * (1024*1024));
    return w;
}

void WorkerStop(Worker* w)
{
    {
        SCOPED_MUTEX(w->mutex);
        w->run = 0;
    }
    thread_join(w->thread);
}

void WorkerPushJob(Worker* w, FWorkerCallback fn, void* ctx)
{
    WorkerJob* job = (WorkerJob*)malloc(sizeof(WorkerJob));
    job->fn = fn;
    job->ctx = ctx;
    job->next = 0;

    SCOPED_MUTEX(w->mutex);

    WorkerJob* last = w->jobs;
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
}
