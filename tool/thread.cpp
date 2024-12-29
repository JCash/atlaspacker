// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#define THREAD_IMPLEMENTATION
#include <external/thread.h>

#include <assert.h>
#include <stdio.h>

#include "thread.h"

#if defined(AP_THREAD_POSIX)
    #define CHECK_RET(ret) \
        if (ret) { \
            fprintf(stderr, "%s:%d failed: %d", __FUNCTION__, __LINE__, ret); \
            assert(ret == 0); \
        }
#else
    #define CHECK_RET(ret)
#endif


HMutex MutexCreate()
{
#if defined(AP_THREAD_POSIX)
    // thread_mutex_t* mutex = (thread_mutex_t*)malloc(sizeof(thread_mutex_t));
    // thread_mutex_init(mutex);
    // return mutex;

    pthread_mutexattr_t attr;
    int ret = pthread_mutexattr_init(&attr);

    // NOTE: We should perhaps consider non-recursive mutex:
    // from http://pubs.opengroup.org/onlinepubs/7908799/xsh/pthread_mutexattr_settype.html
    // It is advised that an application should not use a PTHREAD_MUTEX_RECURSIVE mutex
    // with condition variables because the implicit unlock performed for a pthread_cond_wait()
    // or pthread_cond_timedwait() may not actually release the mutex (if it had been locked
    // multiple times). If this happens, no other thread can satisfy the condition of the predicate.
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    assert(ret == 0);

    Mutex* mutex = new Mutex();

    ret = pthread_mutex_init(&mutex->m_NativeHandle, &attr);
    CHECK_RET(ret);
    ret = pthread_mutexattr_destroy(&attr);
    CHECK_RET(ret);

    return mutex;
#endif
}

void MutexDestroy(HMutex mutex)
{
    assert(mutex);
#if defined(AP_THREAD_POSIX)
    int ret = pthread_mutex_destroy(&mutex->m_NativeHandle);
    CHECK_RET(ret);
    delete mutex;
#endif
}

void MutexLock(HMutex mutex)
{
    assert(mutex);
#if defined(AP_THREAD_POSIX)
    int ret = pthread_mutex_lock(&mutex->m_NativeHandle);
    CHECK_RET(ret);
#endif
}

bool MutexTryLock(HMutex mutex)
{
    assert(mutex);
#if defined(AP_THREAD_POSIX)
    return (pthread_mutex_trylock(&mutex->m_NativeHandle) == 0) ? true : false;
#endif
}

void MutexUnlock(HMutex mutex)
{
    assert(mutex);
#if defined(AP_THREAD_POSIX)
    int ret = pthread_mutex_unlock(&mutex->m_NativeHandle);
    CHECK_RET(ret);
#endif
}
