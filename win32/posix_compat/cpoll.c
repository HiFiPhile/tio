/**
******************************************************************************
* @file    cpoll.c
* @author  Zixun LI
* @version V1.0
* @date    06/10/2023
* @brief   poll wrapper
*/
#ifdef _WIN32
/* Includes ------------------------------------------------------------------*/
#include <stdlib.h>
#include "cpoll.h"
/* Private defines --------------------------------------------------------- */
/* Private types ------------------------------------------------------------*/
/* Private constants --------------------------------------------------------*/
#define WAIT_HANDLE_MAX 64
/* Private macro ------------------------------------------------------------*/
/* Private functions ------------------------------------------------------- */
int poll(pollfd_t *fds, nfds_t nfds, int timeout)
{
    if(nfds > WAIT_HANDLE_MAX)
        return -1;

    int valid_fds = 0;
    for(int i = 0; i < nfds; i++)
    {
        if(fds[i].fd != NULL && fds[i].fd != INVALID_HANDLE_VALUE)
        {
            fds[i].revents = 0;
            valid_fds++;
        }
        else
        {
            fds[i].revents = POLL_NVAL;
        }
    }

    if(!valid_fds)
        return -1;

    HANDLE* fd_set = malloc(sizeof(HANDLE) * valid_fds);
    if(!fd_set) return -1;

    for(int i = 0, idx = 0; i < nfds; i++)
    {
        if(fds[i].revents == 0)
            fd_set[idx++] = fds[i].fd;
    }

    if(timeout < 0) timeout = INFINITE;

    DWORD ret = WaitForMultipleObjects(valid_fds, fd_set, FALSE, timeout);

    free(fd_set);

    if(ret >= WAIT_OBJECT_0 && ret <= WAIT_OBJECT_0 + WAIT_HANDLE_MAX - 1)
    {
        int cnt = 0;
        for(int i = 0; i < nfds; i++)
        {
            if(fds[i].revents == 0)
            {
                DWORD ret = WaitForSingleObject(fds[i].fd, 0);
                if(ret == WAIT_OBJECT_0)
                {
                    fds[i].revents = fds[i].events;
                    cnt++;
                }
            }
        }

        return cnt;
    }
    else if(ret == WAIT_TIMEOUT)
    {
        return 0;
    }
    else
    {
        for(int i = 0; i < nfds; i++)
        {
            if(fds[i].revents == 0)
            {
                DWORD ret = WaitForSingleObject(fds[i].fd, 0);
                if(ret == WAIT_FAILED)
                {
                    DWORD err = GetLastError();
                    if(err == ERROR_INVALID_HANDLE)
                        fds[i].revents = POLL_NVAL;
                    else
                        fds[i].revents = POLL_ERR;
                }
            }
        }

        return -1;
    }
}
#endif