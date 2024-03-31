/**
******************************************************************************
* @file    ring.c
* @author  Zixun LI
* @version V1.0
* @date    30/03/2024
* @brief   thread-safe ring buffer
* @cite    https://www.snellman.net/blog/archive/2016-12-13-ring-buffers/
*/
/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include "ring.h"
#include "semaphore.h"
#include "cthread.h"
/* Private typedef -----------------------------------------------------------*/

/* RING handle */
struct _RING_t
{
    pthread_mutex_t Lock;
    SEM_Handle_t SemFree;
    SEM_Handle_t SemAvail;
    uint8_t* Buffer;
    uint32_t Size;
    uint32_t IdxRead;
    uint32_t IdxWrite;
};
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
#ifndef min
#define min(a, b)       ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b)       ((a) > (b) ? (a) : (b))
#endif
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static inline uint32_t mask(RING_Handle_t hRING, uint32_t val) { return val % hRING->Size; }
static inline uint32_t wrap(RING_Handle_t hRING, uint32_t val) { return val % (hRING->Size * 2); }
static inline uint32_t count(RING_Handle_t hRING)              { return wrap(hRING, hRING->IdxWrite - hRING->IdxRead); }
static inline bool empty(RING_Handle_t hRING)                  { return hRING->IdxRead == hRING->IdxWrite; }
static inline bool full(RING_Handle_t hRING)                   { return count(hRING) == hRING->Size; }
/* Private functions ---------------------------------------------------------*/

/**
 * @fn RING_Handle_t RING_Init(uint32_t Length)
 *
 * @brief Initialize ring buffer.
 *
 * @param Length Ring buffer size, must be <= 1 GiB.
 *
 * @retval Ring buffer handle, or NULL on fail.
 */
RING_Handle_t RING_Init(uint32_t Length)
{
    if(Length == 0 || Length > 0x40000000)
        return NULL;

    RING_Handle_t RING = malloc(sizeof(struct _RING_t));
    if(RING == NULL)
        return NULL;

    RING->Buffer = malloc(Length);
    if(RING->Buffer == NULL)
        return NULL;

    pthread_mutex_init(&RING->Lock, NULL);
    RING->SemFree = SEM_Init(1, true);
    RING->SemAvail = SEM_Init(0, true);

    RING->Size = Length;
    RING->IdxRead  = 0;
    RING->IdxWrite = 0;

    return RING;
}

/**
 * @fn RING_Handle_t RING_Deinit(RING_Handle_t hRING)
 *
 * @brief De-initialize ring buffer.
 *
 * @param hRING Ring buffer handle.
 *
 * @retval 0 on success, -1 on fail.
 */
int RING_Deinit(RING_Handle_t hRING)
{
    if(!hRING)
        return -1;

    SEM_Deinit(hRING->SemFree);
    SEM_Deinit(hRING->SemAvail);

    pthread_mutex_destroy(&hRING->Lock);

    free(hRING->Buffer);
    free(hRING);

    return 0;
}

/**
 * @fn uint32_t RING_Write(RING_Handle_t hRING, const void *Data, uint32_t Length)
 *
 * @brief Write element to ring buffer.
 *
 * @param hRING Ring buffer handle.
 * @param Data Pointer to data.
 * @param Length Data length.
 *
 * @retval Bytes written on success, 0 on fail.
 */
uint32_t RING_Write(RING_Handle_t hRING, const void *Data, uint32_t Length)
{
    const uint8_t* data = (const uint8_t*)Data;

    pthread_mutex_lock(&hRING->Lock);

    Length = min(Length, hRING->Size - count(hRING));
    if(Length)
    {
        uint32_t run = min(Length, hRING->Size - mask(hRING, hRING->IdxWrite));
        uint32_t rem = Length - run;

        memcpy(hRING->Buffer + mask(hRING, hRING->IdxWrite), data, run);
        memcpy(hRING->Buffer, data + run, rem);
        hRING->IdxWrite = wrap(hRING, hRING->IdxWrite + Length);

        SEM_Give(hRING->SemAvail);
        if(full(hRING))
            SEM_Take(hRING->SemFree);
    }

    pthread_mutex_unlock(&hRING->Lock);

    return Length;
}

/**
 * @fn uint32_t RING_Read(RING_Handle_t hRING, void *Data, uint32_t Length)
 *
 * @brief Read element from ring buffer.
 *
 * @param hRING Ring buffer handle.
 * @param Data Pointer to data.
 * @param Length Data length.
 *
 * @retval Bytes read on success, 0 on fail.
 */
uint32_t RING_Read(RING_Handle_t hRING, void *Data, uint32_t Length)
{
    uint8_t* data = (uint8_t*)Data;

    pthread_mutex_lock(&hRING->Lock);

    Length = min(Length, count(hRING));
    if(Length)
    {
        uint32_t run = min(Length, hRING->Size - mask(hRING, hRING->IdxRead));
        uint32_t rem = Length - run;

        memcpy(data, hRING->Buffer + mask(hRING, hRING->IdxRead), run);
        memcpy(data + run, hRING->Buffer, rem);
        hRING->IdxRead = wrap(hRING, hRING->IdxRead + Length);

        SEM_Give(hRING->SemFree);
        if(empty(hRING))
            SEM_Take(hRING->SemAvail);
    }

    pthread_mutex_unlock(&hRING->Lock);

    return Length;
}

/**
 * @fn int RING_Write_Blocking(RING_Handle_t hRING, void *Data)
 *
 * @brief Write element to ring buffer, blocking if full.
 *
 * @param hRING Ring buffer handle.
 * @param Data Pointer to data.
 * @param Length Data length.
 *
 * @retval 0 on success, others on fail.
 */
int RING_Write_Blocking(RING_Handle_t hRING, const void *Data, uint32_t Length)
{
    const uint8_t* data = (const uint8_t*)Data;

    while(Length)
    {
        if(SEM_Wait(hRING->SemFree, -1, false) == -1)
            return -1;

        uint32_t wLen = RING_Write(hRING, data, Length);

        Length -= wLen;
        data   += wLen;
    }

    return 0;
}

/**
 * @fn uint32_t RING_Read_Blocking(RING_Handle_t hRING)
 *
 * @brief Read element from ring buffer, blocking if empty.
 *
 * @param hRING Ring buffer handle.
 * @param Data Pointer to data.
 * @param Length Data length.
 *
 * @retval 0 on success, others on fail.
 */
int RING_Read_Blocking(RING_Handle_t hRING, void *Data, uint32_t Length)
{
    uint8_t* data = (uint8_t*)Data;

    while(Length)
    {
        if(SEM_Wait(hRING->SemAvail, -1, false) == -1)
            return -1;

        uint32_t rLen = RING_Read(hRING, data, Length);

        Length -= rLen;
        data   += rLen;
    }

    return 0;
}

/**
 * @fn uint32_t RING_Get_Count(RING_Handle_t hRING)
 *
 * @brief Get element count.
 *
 * @param hRING Ring buffer handle.
 *
 * @retval element count.
 */
uint32_t RING_Get_Count(RING_Handle_t hRING)
{
    uint32_t size;

    pthread_mutex_lock(&hRING->Lock);

    size = count(hRING);

    pthread_mutex_unlock(&hRING->Lock);

    return size;
}

/**
 * @fn uint32_t RING_Get_Free(RING_Handle_t hRING)
 *
 * @brief Get free element.
 *
 * @param hRING Ring buffer handle.
 *
 * @retval free element.
 */
uint32_t RING_Get_Free(RING_Handle_t hRING)
{
    uint32_t size;

    pthread_mutex_lock(&hRING->Lock);

    size = hRING->Size - count(hRING);

    pthread_mutex_unlock(&hRING->Lock);

    return size;
}

/**
 * @fn WAIT_HANDLE RING_GetWaitable(RING_Handle_t hRING, RING_EVENT_t Event)
 *
 * @brief Get event waitable handle.
 *
 * @param hRING RING handle.
 * @param Event Event to wait.
 *
 * @retval Handle on success, -1 on fail.
 */
WAIT_HANDLE RING_GetWaitable(RING_Handle_t hRING, RING_EVENT_t Event)
{
    if(Event == RING_Available)
        return SEM_GetWaitable(hRING->SemAvail);
    else if(Event == RING_Free)
        return SEM_GetWaitable(hRING->SemFree);
    else
        return (WAIT_HANDLE)-1;
}
