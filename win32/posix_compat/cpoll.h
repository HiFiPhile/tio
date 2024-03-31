/**
******************************************************************************
* @file    cpoll.h
* @author  Zixun LI
* @version V1.0
* @date    05/10/2023
* @brief   poll wrapper
*/
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _CPOLL_H
#define _CPOLL_H
/* Includes ------------------------------------------------------------------*/
#ifndef _WIN32
# include <poll.h>
#else
# include <Windows.h>
#endif
/* Exported defines --------------------------------------------------------- */
#ifndef _WIN32
#define POLL_IN   POLLIN
#define POLL_PRI  POLLPRI
#define POLL_OUT  POLLOUT
#define POLL_ERR  POLLERR
#define POLL_HUP  POLLHUP
#define POLL_NVAL POLLNVAL
#else
#define POLL_IN   0x01       /* There is data to read. */
#define POLL_PRI  0x02       /* There is urgent data to read. */
#define POLL_OUT  0x04       /* Writing now will not block. */
#define POLL_ERR  0x08       /* An error occured. */
#define POLL_HUP  0x10       /* Shutdown or close happened. */
#define POLL_NVAL 0x20       /* Invalid file descriptor. */
#endif
/* Exported types ------------------------------------------------------------*/
#ifndef _WIN32
typedef struct pollfd pollfd_t;
#else
typedef struct _pollfd_t
{
    HANDLE fd;
    short events;
    short revents;
}pollfd_t;

typedef unsigned int nfds_t;
#endif
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
#ifdef _WIN32
int poll(pollfd_t *fds, nfds_t nfds, int timeout);
#endif

#endif
