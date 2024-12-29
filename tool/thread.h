// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#pragma once

#if defined(_WIN32)

    #define AP_THREAD_WIN32

    #include "safe_windows.h"

    struct Mutex
    {
        CRITICAL_SECTION m_NativeHandle;
    };

#elif defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__)

    #define AP_THREAD_POSIX

    #include <pthread.h>
    struct Mutex
    {
        pthread_mutex_t  m_NativeHandle;
    };

#else
    #error "Unsupported platform"
#endif

#include <external/thread.h>

typedef thread_id_t*    HThread;
typedef Mutex*          HMutex;

// ****************************************************************
// Mutex

HMutex  MutexCreate();
void    MutexDestroy(HMutex mutex);
void    MutexLock(HMutex mutex);
bool    MutexTryLock(HMutex mutex);
void    MutexUnlock(HMutex mutex);

struct ScopedMutexLock
{
    HMutex mutex;

    ScopedMutexLock(HMutex _mutex)
    : mutex(_mutex)
    {
        MutexLock(mutex);
    }

    ~ScopedMutexLock()
    {
        MutexUnlock(mutex);
    }
};

#define SCOPED_MUTEX(MUTEX) ScopedMutexLock _scoped(MUTEX)


// ****************************************************************
// Thread
