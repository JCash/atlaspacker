// Copyright (c) 2021-2026 Mathias Westerdahl
// Licensed under the MIT License. See http://opensource.org/licenses/MIT
// https://github.com/JCash/atlaspacker

#ifndef ATLASPACKER_TOOL_WORKER_H
#define ATLASPACKER_TOOL_WORKER_H

typedef struct Worker* HWorker;

typedef int  (*FWorkerProcess)(void* ctx);
typedef void (*FWorkerCallback)(int result, void* ctx);

// Creates a worker with a thread
HWorker WorkerCreate();
// Creates a worker with no thread
HWorker WorkerCreateNoThread();
// Joins the thread and then destroys the worker
void    WorkerDestroy(HWorker worker);
// Push a single job onto the worker
void    WorkerPushJob(HWorker worker, FWorkerProcess process, FWorkerCallback finished, void* job_ctx);
// Flush all finished jobs and do callbacks on the current thread
void    WorkerUpdate(HWorker worker);

#endif // ATLASPACKER_TOOL_WORKER_H
