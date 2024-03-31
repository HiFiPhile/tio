/**
******************************************************************************
* @file    cthread.c
* @author  john@nachtimwald.com
*          Zixun LI
* @version V1.0
* @date    28/01/2022
* @brief   pthread wrapper
*/
/* Includes ------------------------------------------------------------------*/
#include "cthread.h"
/* Private define ------------------------------------------------------------*/
#ifdef _WIN32
/* Private typedef -----------------------------------------------------------*/
typedef struct
{
    void *(*start_routine)(void *);
    void *start_arg;
} win_thread_start_t;
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static DWORD WINAPI win_thread_start(void *arg);
/* Private functions ---------------------------------------------------------*/
static DWORD WINAPI win_thread_start(void *arg)
{
    win_thread_start_t *data       = arg;
    void *(*start_routine)(void *) = data->start_routine;
    void *start_arg                = data->start_arg;

    free(data);

    start_routine(start_arg);
    return 0; /* ERROR_SUCCESS */
}

int pthread_create(pthread_t *thread, pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
    win_thread_start_t *data;

    (void)attr;

    if(thread == NULL || start_routine == NULL)
        return 1;

    data                = malloc(sizeof(*data));
    data->start_routine = start_routine;
    data->start_arg     = arg;

    *thread = CreateThread(NULL, 0, win_thread_start, data, 0, NULL);
    if(*thread == NULL)
        return 1;
    return 0;
}

int pthread_join(pthread_t thread, void **value_ptr)
{
    (void)value_ptr;
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return 0;
}

int pthread_detach(pthread_t thread)
{
    CloseHandle(thread);
    return 0;
}

void pthread_exit(void *res)
{
    ExitThread((int)((intptr_t)res));
}

int pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr)
{
    (void)attr;

    if(mutex == NULL)
        return 1;

    InitializeCriticalSection(mutex);
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    if(mutex == NULL)
        return 1;
    DeleteCriticalSection(mutex);
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    if(mutex == NULL)
        return 1;
    EnterCriticalSection(mutex);
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    if(mutex == NULL)
        return 1;
    LeaveCriticalSection(mutex);
    return 0;
}

int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t *attr)
{
    (void)attr;
    if(cond == NULL)
        return 1;
    InitializeConditionVariable(cond);
    return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
    /* Windows does not have a destroy for conditionals */
    (void)cond;
    return 0;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    if(cond == NULL || mutex == NULL)
        return 1;
    if(!SleepConditionVariableCS(cond, mutex, INFINITE))
        return 1;
    return 0;
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime)
{
    if(cond == NULL || mutex == NULL)
        return 1;
    if(!SleepConditionVariableCS(cond, mutex, timespec_to_ms(abstime)))
        return 1;
    return 0;
}

int pthread_cond_signal(pthread_cond_t *cond)
{
    if(cond == NULL)
        return 1;
    WakeConditionVariable(cond);
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond)
{
    if(cond == NULL)
        return 1;
    WakeAllConditionVariable(cond);
    return 0;
}

#endif

void ms_to_timespec(struct timespec *ts, unsigned int ms)
{
    if(ts == NULL)
        return;
    ts->tv_sec  = ms / 1000;
    ts->tv_nsec = (ms % 1000) * 1000000;
}

int timespec_to_ms(const struct timespec *ts)
{
    if(ts == NULL)
        return 0;
    return (int)(ts->tv_sec * 1000 + ts->tv_nsec / 1000000);
}