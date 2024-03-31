/**
******************************************************************************
* @file    fifo.c
* @author  Zixun LI
* @version V1.0
* @date    31/01/2022
* @brief   thread-safe FIFO
*/
/* Includes ------------------------------------------------------------------*/
#include <stdlib.h>
#include <limits.h>
#include "fifo.h"
#include "semaphore.h"
#include "cthread.h"
/* Private typedef -----------------------------------------------------------*/
/* FIFO element node */
typedef struct _FIFO_Node_t
{
    struct _FIFO_Node_t *Next;
    void *       Data;
}FIFO_Node_t;
/* FIFO handle */
struct _FIFO_t
{
    pthread_mutex_t Lock;
    SEM_Handle_t SemFree;
    SEM_Handle_t SemAvail;
    FIFO_Node_t *Head;
    FIFO_Node_t *Tail;
    unsigned int Length;
};
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void Push(FIFO_Handle_t hFIFO,void *Data);
static void* Pop(FIFO_Handle_t hFIFO);
/* Private functions ---------------------------------------------------------*/
/**
 * @fn FIFO_Handle_t FIFO_Init(unsigned int Length)
 *
 * @brief Initialize FIFO.
 *
 * @param Length Maximum FIFO length, 0 if unlimited.
 *
 * @retval FIFO handle.
 */
FIFO_Handle_t FIFO_Init(unsigned int Length)
{
    FIFO_Handle_t FIFO = malloc(sizeof(struct _FIFO_t));
    if(!Length)
    {
        Length = UINT_MAX;
    }

    pthread_mutex_init(&FIFO->Lock, NULL);
    FIFO->SemFree = SEM_Init(Length, false);
    FIFO->SemAvail = SEM_Init(0, false);

    FIFO->Length    = 0;
    FIFO->Head      = NULL;
    FIFO->Tail      = NULL;

    return FIFO;
}

/**
 * @fn void Push(FIFO_Handle_t hFIFO, void *Data)
 *
 * @brief Push element to FIFO.
 *
 * @param hFIFO FIFO handle.
 * @param Data Pointer to Data.
 */
static void Push(FIFO_Handle_t hFIFO, void *Data)
{
    FIFO_Node_t * node;

    node = malloc(sizeof(*node));
    node->Data = Data;
    node->Next = NULL;

    pthread_mutex_lock(&hFIFO->Lock);

    if(hFIFO->Head == NULL)
    {
        hFIFO->Head = node;
        hFIFO->Tail = node;
    }
    else
    {
        hFIFO->Tail->Next = node;
        hFIFO->Tail       = node;
    }
    hFIFO->Length++;

    pthread_mutex_unlock(&hFIFO->Lock);
}

/**
 * @fn void* Pop(FIFO_Handle_t hFIFO)
 *
 * @brief Pop element from FIFO.
 *
 * @param hFIFO FIFO handle.
 *
 * @retval Data.
 */
static void* Pop(FIFO_Handle_t hFIFO)
{
    FIFO_Node_t *node;
    void *data;

    pthread_mutex_lock(&hFIFO->Lock);

    node = hFIFO->Head;
    if(hFIFO->Head == hFIFO->Tail)
    {
        hFIFO->Head = NULL;
        hFIFO->Tail = NULL;
    }
    else
    {
        hFIFO->Head = node->Next;
    }
    hFIFO->Length--;

    pthread_mutex_unlock(&hFIFO->Lock);

    data = node->Data;
    free(node);

    return data;
}

/**
 * @fn int FIFO_Push_Blocking(FIFO_Handle_t hFIFO, void *Data)
 *
 * @brief Push element to FIFO, blocking if full.
 *
 * @param hFIFO FIFO handle.
 * @param Data Pointer to data.
 *
 * @retval 0 on success, others on fail.
 */
int FIFO_Push_Blocking(FIFO_Handle_t hFIFO, void *Data)
{
    SEM_Wait(hFIFO->SemFree, -1, true);

    Push(hFIFO, Data);

    SEM_Give(hFIFO->SemAvail);

    return 0;
}

/**
 * @fn void *FIFO_Pop_Blocking(FIFO_Handle_t hFIFO)
 *
 * @brief Pop element from FIFO, blocking if empty.
 *
 * @param hFIFO FIFO handle.
 *
 * @retval Pointer to data on success, NULL on fail.
 */
void *FIFO_Pop_Blocking(FIFO_Handle_t hFIFO)
{
    void* data;

    SEM_Wait(hFIFO->SemAvail, -1, true);

    data = Pop(hFIFO);

    SEM_Give(hFIFO->SemFree);

    return data;
}

/**
 * @fn int FIFO_Push(FIFO_Handle_t hFIFO, void *Data)
 *
 * @brief Push element to FIFO.
 *
 * @param hFIFO FIFO handle.
 * @param Data Pointer to data.
 *
 * @retval 0 on success, others on fail.
 */
int FIFO_Push(FIFO_Handle_t hFIFO, void *Data)
{
    if(SEM_Take(hFIFO->SemFree) != 0)
        return -1;

    Push(hFIFO, Data);

    SEM_Give(hFIFO->SemAvail);

    return 0;
}

/**
 * @fn void *FIFO_Pop(FIFO_Handle_t *hFIFO)
 *
 * @brief Pop element from FIFO.
 *
 * @param hFIFO FIFO handle.
 *
 * @retval Pointer to data on success, NULL on fail.
 */
void *FIFO_Pop(FIFO_Handle_t hFIFO)
{
    void* data;

    if(SEM_Take(hFIFO->SemAvail) != 0)
        return NULL;

    data = Pop(hFIFO);

    SEM_Give(hFIFO->SemFree);

    return data;
}

/**
 * @fn WAIT_HANDLE FIFO_GetWaitable(FIFO_Handle_t hFIFO, FIFO_EVENT_t Event)
 *
 * @brief Get event waitable handle.
 *
 * @param hFIFO FIFO handle.
 * @param Event Event to wait.
 *
 * @retval Handle on success, -1 on fail.
 */
WAIT_HANDLE FIFO_GetWaitable(FIFO_Handle_t hFIFO, FIFO_EVENT_t Event)
{
    if(Event == FIFO_Available)
        return SEM_GetWaitable(hFIFO->SemAvail);
    else if(Event == FIFO_Free)
        return SEM_GetWaitable(hFIFO->SemFree);
    else
        return (WAIT_HANDLE)-1;
}
