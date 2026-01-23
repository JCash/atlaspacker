// Copyright (c) 2021-2026 Mathias Westerdahl
// Licensed under the MIT License. See http://opensource.org/licenses/MIT
// https://github.com/JCash/atlaspacker

#include "worker.h"
#include "thread.h"

#include <stdlib.h>
#include <stdio.h>

#include <external/thread.h>

struct WorkerJob
{
    struct WorkerJob* next;

    void*           ctx;
    FWorkerProcess  process;
    FWorkerCallback finished;
    int             result;
};

struct Worker
{
    thread_ptr_t    thread;
    HMutex          mutex;  // TODO: Use its own mutex!!
    WorkerJob*      jobs;
    WorkerJob*      finished;
    int             run;
};

static WorkerJob* AllocJob(FWorkerProcess process, FWorkerCallback finished, void* job_ctx)
{
    WorkerJob* job = (WorkerJob*)malloc(sizeof(WorkerJob));
    job->process = process;
    job->finished= finished;
    job->ctx     = job_ctx;
    job->result  = 0;
    job->next    = 0;
    return job;
}

static void FreeJob(WorkerJob* job)
{
    free((void*)job);
}

static void AddLast(WorkerJob** list, WorkerJob* job)
{
    WorkerJob* last = *list;
    if (!last)
    {
        *list = job;
    }
    else
    {
        while(last->next)
        {
            last = last->next;
        }
        last->next = job;
    }
}

static WorkerJob* PopJob(WorkerJob** list)
{
    WorkerJob* job = *list;
    if (job)
    {
        *list = (*list)->next;
    }
    return job;
}

static bool WorkerProcessOneJob(Worker* w)
{
    WorkerJob* job = 0;
    {
        SCOPED_MUTEX(w->mutex);
        job = PopJob(&w->jobs);
    }

    if (!job)
        return false;

    job->result = job->process(job->ctx);

    {
        SCOPED_MUTEX(w->mutex);
        if (job->finished)
            AddLast(&w->finished, job);
        else
            FreeJob(job);
    }
    return true;
}

static int WorkerThread(void* ctx)
{
    Worker* w = (Worker*)ctx;

    thread_timer_t timer;
    thread_timer_init( &timer );

    while (true)
    {
        {
            SCOPED_MUTEX(w->mutex);
            if (!w->run)
                break;
        }

        WorkerProcessOneJob(w);

        // TODO: Add wake on signal instead!
        thread_timer_wait(&timer, 1000); // nanoseconds
    }

    thread_timer_term( &timer );
    return 0;
}

HWorker WorkerCreateNoThread()
{
    Worker* w   = (Worker*)malloc(sizeof(Worker));
    w->run      = 1;
    w->jobs     = 0;
    w->finished = 0;
    w->mutex    = MutexCreate();
    w->thread   = 0;
    return w;
}

Worker* WorkerCreate()
{
    Worker* w = WorkerCreateNoThread();
    w->thread = mg_thread_create(WorkerThread, (void*)w, 2 * (1024*1024));
    return w;
}

void WorkerDestroy(Worker* w)
{
    if (w->thread)
    {
        {
            SCOPED_MUTEX(w->mutex);
            w->run = 0;
        }
        thread_join(w->thread);
    }
    MutexDestroy(w->mutex);
    free((void*)w);
}

void WorkerPushJob(Worker* w, FWorkerProcess process, FWorkerCallback finished, void* job_ctx)
{
    WorkerJob* job = AllocJob(process, finished, job_ctx);
    SCOPED_MUTEX(w->mutex);
    AddLast(&w->jobs, job);
}

void WorkerUpdate(Worker* w)
{
    if (!w->thread) // not threaded
    {
        while(WorkerProcessOneJob(w))
        {
            //
        }
    }

    // Process the finished jobs
    WorkerJob* job = 0;
    {
        SCOPED_MUTEX(w->mutex);
        job = w->finished;
        w->finished = 0;
    }

    while (job)
    {
        if (job->finished)
            job->finished(job->result, job->ctx);
        WorkerJob* next = job->next;
        FreeJob(job);
        job = next;
    }
}
