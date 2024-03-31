/**
******************************************************************************
* @file    semaphore.h
* @author  Zixun LI
* @version V1.0
* @date    04/10/2023
* @brief   Waitable semaphore
*/
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _SEM_H
#define _SEM_H
/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
/* Exported defines --------------------------------------------------------- */
/* Exported types ------------------------------------------------------------*/
/* Event handle */
typedef struct _SEM_Handle_t *SEM_Handle_t;
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
#ifdef _WIN32
# define SEM_HANDLE void*
#else
# define SEM_HANDLE int
#endif
/* Exported functions ------------------------------------------------------- */
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
SEM_Handle_t SEM_Init(uint32_t InitVal, bool Event);

/**
 * @fn int SEM_Deinit(SEM_t hSem)
 *
 * @brief De-initialize semaphore.
 *
 * @param hSem Semaphore handle.
 *
 * @retval 0 on success, -1 on fail.
 */
int SEM_Deinit(SEM_Handle_t hSem);

/**
 * @fn int SEM_Give(SEM_t hSem)
 *
 * @brief Release a semaphore.
 *
 * @param hSem Semaphore handle.
 *
 * @retval 0 on success, -1 on fail.
 */
int SEM_Give(SEM_Handle_t hSem);

/**
 * @fn int SEM_Take(SEM_t hSem)
 *
 * @brief Take a semaphore.
 *
 * @param hSem Semaphore handle.
 *
 * @retval 0 on success, -1 on fail.
 */
int SEM_Take(SEM_Handle_t hSem);

/**
 * @fn SEM_HANDLE SEM_GetWaitable(SEM_t hSem)
 *
 * @brief Get a waitable handle.
 *
 * @param hSem Semaphore handle.
 *
 * @retval Handle on success, -1 on fail.
 */
SEM_HANDLE SEM_GetWaitable(SEM_Handle_t hSem);

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
int SEM_Wait(SEM_Handle_t hSem, int Timeout, bool Take);

#endif