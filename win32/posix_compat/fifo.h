/**
******************************************************************************
* @file    fifo.h
* @author  Zixun LI
* @version V1.0
* @date    31/01/2022
* @brief   thread-safe FIFO
*/
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _FIFO_H
#define _FIFO_H
/* Includes ------------------------------------------------------------------*/
/* Exported defines --------------------------------------------------------- */
/* Exported types ------------------------------------------------------------*/

/* FIFO handle */
typedef struct _FIFO_t *FIFO_Handle_t;
typedef enum
{
    FIFO_Free,
    FIFO_Available,
}FIFO_EVENT_t;
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
#ifndef WAIT_HANDLE
#ifdef _WIN32
# define WAIT_HANDLE void*
#else
# define WAIT_HANDLE int
#endif
#endif
/* Exported functions ------------------------------------------------------- */
/**
 * @fn FIFO_Handle_t FIFO_Init(unsigned int Length)
 *
 * @brief Initialize FIFO.
 *
 * @param Length Maximum FIFO length, 0 if unlimited.
 *
 * @retval FIFO handle.
 */
FIFO_Handle_t FIFO_Init(unsigned int Length);

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
int FIFO_Push_Blocking(FIFO_Handle_t hFIFO, void *Data);

/**
 * @fn void *FIFO_Pop_Blocking(FIFO_Handle_t hFIFO)
 *
 * @brief Pop element from FIFO, blocking if empty.
 *
 * @param hFIFO FIFO handle.
 *
 * @retval Pointer to data on success, NULL on fail.
 */
void *FIFO_Pop_Blocking(FIFO_Handle_t hFIFO);

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
int FIFO_Push(FIFO_Handle_t hFIFO, void *Data);

/**
 * @fn void *FIFO_Pop(FIFO_Handle_t hFIFO)
 *
 * @brief Pop element from FIFO.
 *
 * @param hFIFO FIFO handle.
 *
 * @retval Pointer to data on success, NULL on fail.
 */
void *FIFO_Pop(FIFO_Handle_t hFIFO);

/**
 * @fn WAIT_HANDLE FIFO_GetWaitable(FIFO_Handle_t hFIFO, FIFO_EVENT_t Event)
 *
 * @brief Get event waitable handle.
 *
 * @param hFIFO FIFO handle.
 * @param Event Event to wait.
 *
 * @retval Handle on success, NULL on fail.
 */
WAIT_HANDLE FIFO_GetWaitable(FIFO_Handle_t hFIFO, FIFO_EVENT_t Event);

#endif