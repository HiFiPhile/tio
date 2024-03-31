/**
******************************************************************************
* @file    ring.h
* @author  Zixun LI
* @version V1.0
* @date    30/03/2024
* @brief   thread-safe ring buffer
* @cite    https://www.snellman.net/blog/archive/2016-12-13-ring-buffers/
*/
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _RING_H
#define _RING_H
/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
/* Exported defines --------------------------------------------------------- */
/* Exported types ------------------------------------------------------------*/

/* Ring buffer handle */
typedef struct _RING_t *RING_Handle_t;
typedef enum
{
    RING_Free,
    RING_Available,
}RING_EVENT_t;
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
 * @fn RING_Handle_t RING_Init(uint32_t Length)
 *
 * @brief Initialize ring buffer.
 *
 * @param Length Ring buffer size, must be <= 1 GiB.
 *
 * @retval Ring buffer handle, or NULL on fail.
 */
RING_Handle_t RING_Init(uint32_t Length);

/**
 * @fn RING_Handle_t RING_Deinit(RING_Handle_t hRING)
 *
 * @brief De-initialize ring buffer.
 *
 * @param hRING Ring buffer handle.
 *
 * @retval 0 on success, -1 on fail.
 */
int RING_Deinit(RING_Handle_t hRING);

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
uint32_t RING_Write(RING_Handle_t hRING, const void *Data, uint32_t Length);

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
uint32_t RING_Read(RING_Handle_t hRING, void *Data, uint32_t Length);

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
int RING_Write_Blocking(RING_Handle_t hRING, const void *Data, uint32_t Length);

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
int RING_Read_Blocking(RING_Handle_t hRING, void *Data, uint32_t Length);

/**
 * @fn uint32_t RING_Get_Count(RING_Handle_t hRING)
 *
 * @brief Get element count.
 *
 * @param hRING Ring buffer handle.
 *
 * @retval element count.
 */
uint32_t RING_Get_Count(RING_Handle_t hRING);

/**
 * @fn uint32_t RING_Get_Free(RING_Handle_t hRING)
 *
 * @brief Get free element.
 *
 * @param hRING Ring buffer handle.
 *
 * @retval free element.
 */
uint32_t RING_Get_Free(RING_Handle_t hRING);

/**
 * @fn WAIT_HANDLE RING_GetWaitable(RING_Handle_t hRING, RING_EVENT_t Event)
 *
 * @brief Get event waitable handle.
 *
 * @param hRING Ring buffer handle.
 * @param Event Event to wait.
 *
 * @retval Handle on success, NULL on fail.
 */
WAIT_HANDLE RING_GetWaitable(RING_Handle_t hRING, RING_EVENT_t Event);

#endif
