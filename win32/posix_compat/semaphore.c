/**
******************************************************************************
* @file    semaphore.c
* @author  Zixun LI
* @version V1.0
* @date    04/10/2023
* @brief   Waitable semaphore
*/
/* Includes ------------------------------------------------------------------*/
#define _GNU_SOURCE
#include <stdlib.h>
#ifdef _WIN32
# include <windows.h>
#elif __CYGWIN__
# include <unistd.h>
# include <poll.h>
# include <fcntl.h>
# include <pthread.h>
#else
# include <unistd.h>
# include <poll.h>
# include <sys/eventfd.h>
#endif
#include "semaphore.h"
/* Private defines --------------------------------------------------------- */
struct _SEM_Handle_t
{
#ifdef _WIN32
    HANDLE Hdl;
    uint32_t Cnt;
    CRITICAL_SECTION CS;
    bool Event;
#elif __CYGWIN__
    int fd[2];
    uint32_t Cnt;
    pthread_mutex_t Lock;
    bool Event;
#else
    int fd;
#endif
};
/* Private types ------------------------------------------------------------*/
/* Private constants --------------------------------------------------------*/
/* Private macro ------------------------------------------------------------*/
/* Private functions ------------------------------------------------------- */
/**
 * @fn SEM_Handle_t SEM_Init(uint32_t InitVal)
 *
 * @brief Initialize semaphore.
 *
 * @param InitVal Initial value.
 * @param Event True to create event, false to create semaphore.
 *
 * @retval Semaphore created on success, -1 on fail.
 */
SEM_Handle_t SEM_Init(uint32_t InitVal, bool Event)
{
    SEM_Handle_t hSem = malloc(sizeof(struct _SEM_Handle_t));
    if(!hSem) return (SEM_Handle_t)-1;

#ifdef _WIN32
    if(hSem->Hdl = CreateEvent(NULL, TRUE, FALSE, NULL), !hSem->Hdl)
        goto err;
    InitializeCriticalSection(&hSem->CS);
    if(InitVal)
    {
        SetEvent(hSem->Hdl);
        if(Event) InitVal = 1;
    }
    hSem->Cnt = InitVal;
    hSem->Event = Event;
#elif __CYGWIN__
    if(pipe2(hSem->fd, O_NONBLOCK) < 0)
        goto err;
    pthread_mutex_init(&hSem->Lock, NULL);
    if(InitVal)
    {
        write(hSem->fd[1], &(uint8_t){1}, 1);
        if(Event) InitVal = 1;
    }
    hSem->Cnt = InitVal;
    hSem->Event = Event;
#else
    if(hSem->fd = eventfd(InitVal, Event ? EFD_NONBLOCK : EFD_SEMAPHORE | EFD_NONBLOCK), hSem->fd < 0)
        goto err;
#endif

    return hSem;
err:
    free(hSem);
    return (SEM_Handle_t)-1;
}

/**
 * @fn int SEM_Deinit(SEM_Handle_t hSem)
 *
 * @brief De-initialize semaphore.
 *
 * @param hSem Semaphore handle.
 *
 * @retval 0 on success, -1 on fail.
 */
int SEM_Deinit(SEM_Handle_t hSem)
{
    if(!hSem) return -1;

#ifdef _WIN32
    CloseHandle(hSem->Hdl);
    DeleteCriticalSection(&hSem->CS);
#elif __CYGWIN__
    close(hSem->fd[0]);
    close(hSem->fd[1]);
    pthread_mutex_destroy(&hSem->Lock);
#else
    close(hSem->fd);
#endif

    free(hSem);
    return 0;
}

/**
 * @fn int SEM_Give(SEM_Handle_t hSem)
 *
 * @brief Release a semaphore.
 *
 * @param hSem Semaphore handle.
 *
 * @retval 0 on success, -1 on fail.
 */
int SEM_Give(SEM_Handle_t hSem)
{
    if(!hSem) return -1;

#ifdef _WIN32
    EnterCriticalSection(&hSem->CS);
    if(!hSem->Cnt)
        SetEvent(hSem->Hdl);
    if(hSem->Event)
        hSem->Cnt = 1;
    else if(hSem->Cnt < UINT32_MAX - 1)
        hSem->Cnt++;
    LeaveCriticalSection(&hSem->CS);
#elif __CYGWIN__
    pthread_mutex_lock(&hSem->Lock);
    if(!hSem->Cnt)
        write(hSem->fd[1], &(uint8_t){1}, 1);
    if(hSem->Event)
        hSem->Cnt = 1;
    else if(hSem->Cnt < UINT32_MAX - 1)
        hSem->Cnt++;
    pthread_mutex_unlock(&hSem->Lock);
#else
    write(hSem->fd, &(uint64_t){1}, sizeof(uint64_t));
#endif

    return 0;
}

/**
 * @fn int SEM_Take(SEM_Handle_t hSem)
 *
 * @brief Take a semaphore.
 *
 * @param hSem Semaphore handle.
 *
 * @retval 0 on success, -1 on fail.
 */
int SEM_Take(SEM_Handle_t hSem)
{
    if(!hSem) return -1;

    int ret = -1;
#ifdef _WIN32
    EnterCriticalSection(&hSem->CS);
    if(hSem->Cnt)
    {
        hSem->Cnt--;
        ret = 0;
    }
    if(!hSem->Cnt)
        ResetEvent(hSem->Hdl);
    LeaveCriticalSection(&hSem->CS);
#elif __CYGWIN__
    pthread_mutex_lock(&hSem->Lock);
    if(hSem->Cnt)
    {
        hSem->Cnt--;
        ret = 0;
    }
    if(!hSem->Cnt)
        read(hSem->fd[0], &(uint8_t){1}, 1);
    pthread_mutex_unlock(&hSem->Lock);
#else
    if(read(hSem->fd, &(uint64_t){1}, sizeof(uint64_t)) > 0)
        ret = 0;
#endif

    return ret;
}

/**
 * @fn SEM_HANDLE SEM_GetWaitable(SEM_Handle_t hSem)
 *
 * @brief Get a waitable handle.
 *
 * @param hSem Semaphore handle.
 *
 * @retval Handle on success, -1 on fail.
 */
SEM_HANDLE SEM_GetWaitable(SEM_Handle_t hSem)
{
    if(!hSem) return (SEM_HANDLE)-1;

#ifdef _WIN32
    return hSem->Hdl;
#elif __CYGWIN__
    return hSem->fd[0];
#else
    return hSem->fd;
#endif
}

/**
 * @fn int SEM_Wait(SEM_Handle_t hSem, int Timeout)
 *
 * @brief Wait a semaphore to be signaled.
 *
 * @param hSem Semaphore handle.
 * @param Timeout Timeout in milliseconds, negative for infinite.
 * @param Take True to wait until the semaphore is actually taken.
 *
 * @retval 1 on signaled, 0 on timeout, -1 on fail.
 */
int SEM_Wait(SEM_Handle_t hSem, int Timeout, bool Take)
{
    if(!hSem) return -1;

    /* Restart wait until sucessfully taken */
restart:
#ifdef _WIN32
    if(Timeout < 0) Timeout = INFINITE;
    DWORD ret = WaitForSingleObject(hSem->Hdl, Timeout);

    if(ret == WAIT_OBJECT_0)
    {
        if(Take && (SEM_Take(hSem) != 0))
            goto restart;
        return 1;
    }
    if(ret == WAIT_TIMEOUT)
        return 0;
    return -1;
#else
    struct pollfd pollfds[1];
# ifdef __CYGWIN__
    pollfds[0].fd = hSem->fd[0];
# else
    pollfds[0].fd = hSem->fd;
# endif
    pollfds[0].events = POLLIN;
    int ret = poll(pollfds, 1, Timeout);

    if(ret == 1)
    {
        if(Take && (SEM_Take(hSem) != 0))
            goto restart;
        return 1;
    }

    return ret;
#endif
}