// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#pragma once

#include "thread.h"

typedef struct Worker* HWorker;

typedef void (*FWorkerCallback)(void* ctx);

HWorker WorkerStart(HMutex mutex);
void    WorkerStop(HWorker worker);
void    WorkerPushJob(HWorker worker, FWorkerCallback cbk, void* cbk_ctx);
