// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#pragma once

#include "thread.h"

typedef struct Worker* HWorker;

typedef int  (*FWorkerProcess)(void* ctx);
typedef void (*FWorkerCallback)(int result, void* ctx);

HWorker WorkerStart(HMutex mutex);

void    WorkerStop(HWorker worker);
void    WorkerPushJob(HWorker worker, FWorkerProcess process, FWorkerCallback finished, void* job_ctx);

//
HWorker WorkerStartNoThread(HMutex mutex);
void    WorkerUpdate(HWorker worker);
