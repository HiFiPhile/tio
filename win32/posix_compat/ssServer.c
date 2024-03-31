/**
******************************************************************************
* @file    ssServer.c
* @author  Zixun LI
* @version V1.0
* @date    28/01/2022
* @brief   simple socket server
*/
/* Includes ------------------------------------------------------------------*/
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include "ssServer.h"
/* Private typedef -----------------------------------------------------------*/
typedef struct
{
    SS_Client_Conn_t *Conn;
    SS_Handle_t *     hSS;
} Conn_priv_t;

typedef struct
{
    intptr_t Sock;
    size_t   Len;
} Queue_Info_t;
/* Private define ------------------------------------------------------------*/
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void  LogEvent(const SS_Handle_t *hSS, SS_Event_t Event, const SS_Client_Conn_t *Conn, const char *format, ...);
static void *Client_Worker(void *ptr);
static void *Listener_Worker(void *ptr);
static void *Send_Worker(void *ptr);
/* Private functions ---------------------------------------------------------*/
static void LogEvent(const SS_Handle_t *hSS, SS_Event_t Event, const SS_Client_Conn_t *Conn, const char *format, ...)
{
    va_list args, args2;
    size_t  len;
    char *  buf;
    if(!hSS->OnEvent)
        return;

    va_start(args, format);
    va_copy(args2, args);
    len = vsnprintf(NULL, 0, format, args);
    buf = malloc(len + 1);
    va_end(args);
    vsprintf(buf, format, args2);
    va_end(args2);

    hSS->OnEvent(hSS, Event, Conn, buf);
    free(buf);
}

static void *Client_Worker(void *ptr)
{
    Conn_priv_t *priv = (Conn_priv_t *)ptr;
    SS_Handle_t *hSS  = priv->hSS;
    char *       buffer;
    int          len, tlen;

    buffer = malloc(SS_MAX_RX_LENGTH + 1);
    if(buffer == NULL)
    {
        LogEvent(hSS, SS_Event_Error, priv->Conn, "Error: buffer alloc failed.");
        free(priv);
        pthread_exit((void *)1);
    }

    LogEvent(hSS, SS_Event_Conn, priv->Conn, "New client: %d.%d.%d.%d:%d", (int)((priv->Conn->Address >> 24) & 0xff),
             (int)((priv->Conn->Address >> 16) & 0xff), (int)((priv->Conn->Address >> 8) & 0xff),
             (int)((priv->Conn->Address) & 0xff), priv->Conn->Port);

    tlen = 0;
    while(1)
    {
        len     = hSS->Binary ? SS_MAX_RX_LENGTH : SS_MAX_RX_LENGTH - tlen;
        int ret = recv(priv->Conn->Sock, &buffer[tlen], len, 0);
        if(ret > 0)
        {
            uint8_t tmp[SS_MAX_RX_LENGTH+1] = {0};
            memcpy(tmp, &buffer[tlen], ret);
            pthread_mutex_lock(&priv->Conn->Lock);
            priv->Conn->Time = 0;
            pthread_mutex_unlock(&priv->Conn->Lock);

            tlen += ret;
            if(tlen > SS_MAX_RX_LENGTH)
                tlen = 0;

            if(hSS->Binary)
            {
                if(hSS->OnMessage)
                {
                    hSS->OnMessage(priv->Conn, buffer, tlen);
                }
                tlen = 0;
            }
            else
            {
                if(buffer[tlen - 1] == '\n' || buffer[tlen - 1] == '\r')
                {
                    int next = 0;
                    for(int i = 0; i < tlen; i++)
                    {
                        if(buffer[i] == '\n' || buffer[i] == '\r' || buffer[i] == 0)
                        {
                            buffer[i] = 0;
                            if(hSS->OnMessage && i - next > 0)
                            {
                                hSS->OnMessage(priv->Conn, buffer + next, i - next);
                            }
                            next = i + 1;
                        }
                    }
                    tlen = 0;
                }
            }
            continue;
        }
        else if(ret < 0)
        {
#ifdef _WIN32
            int err = WSAGetLastError();
            if(err == WSAETIMEDOUT)
#else
            int err = errno;
            if(err == EAGAIN || err == ETIMEDOUT)
#endif

            {
                pthread_mutex_lock(&priv->Conn->Lock);
                priv->Conn->Time += SS_XFER_TIMEO;
                pthread_mutex_unlock(&priv->Conn->Lock);
                if(priv->Conn->Time > SS_ALIVE_TIMEO * 1000)
                {
                    LogEvent(hSS, SS_Event_Disc, priv->Conn, "Client: %d.%d.%d.%d:%d timeout.",
                             (int)((priv->Conn->Address >> 24) & 0xff), (int)((priv->Conn->Address >> 16) & 0xff),
                             (int)((priv->Conn->Address >> 8) & 0xff), (int)((priv->Conn->Address) & 0xff),
                             priv->Conn->Port);
                    break;
                }
                continue;
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    LogEvent(hSS, SS_Event_Disc, priv->Conn, "Close client: %d.%d.%d.%d:%d", (int)((priv->Conn->Address >> 24) & 0xff),
             (int)((priv->Conn->Address >> 16) & 0xff), (int)((priv->Conn->Address >> 8) & 0xff),
             (int)((priv->Conn->Address) & 0xff), priv->Conn->Port);

#ifdef _WIN32
    closesocket(priv->Conn->Sock);
#else
    close(priv->Conn->Sock);
#endif

    pthread_mutex_lock(&priv->Conn->Lock);
    for(int i = 0; i < SS_MAX_CLIENTS; i++)
    {
        if(hSS->Clients[i].Sock == priv->Conn->Sock)
        {
            hSS->Clients[i].Sock = -1;
            break;
        }
    }
    pthread_mutex_unlock(&priv->Conn->Lock);

    free(buffer);
    free(priv);
    pthread_exit(0);
    return NULL;
}

static void *Listener_Worker(void *ptr)
{
    SS_Handle_t *      hSS = (SS_Handle_t *)ptr;
    struct sockaddr_in client;
    socklen_t          len;
    intptr_t           sock;
    bool               accepted;
    Conn_priv_t *      priv;
    pthread_t          thread;

    len = sizeof(struct sockaddr_in);

    while(1)
    {
        sock = accept(hSS->Sock, (struct sockaddr *)&client, &len);
        if(sock < 0)
        {
#ifndef _WIN32
            int err = errno;
            if(err == EAGAIN || err == EWOULDBLOCK)
                continue;
#endif
            LogEvent(hSS, SS_Event_Error, NULL, "Error: socket accept failed.");
            pthread_exit((void *)1);
        }

        accepted = false;

        for(int i = 0; i < SS_MAX_CLIENTS; i++)
        {
            pthread_mutex_lock(&hSS->Clients[i].Lock);
            if(hSS->Clients[i].Sock == -1)
            {

                priv                = (Conn_priv_t *)malloc(sizeof(Conn_priv_t));
                priv->Conn          = &hSS->Clients[i];
                priv->Conn->Sock    = sock;
                priv->Conn->Address = ntohl(client.sin_addr.s_addr);
                priv->Conn->Port    = ntohs(client.sin_port);
                priv->Conn->Time    = 0;
                priv->hSS           = hSS;

                pthread_create(&thread, 0, Client_Worker, (void *)priv);
                pthread_detach(thread);

                accepted = true;
            }
            pthread_mutex_unlock(&hSS->Clients[i].Lock);
            if(accepted) break;
        }

        if(!accepted)
        {
#ifdef _WIN32
            closesocket(sock);
#else
            close(sock);
#endif
        }
    }

    return NULL;
}

static void *Send_Worker(void *ptr)
{
    SS_Handle_t * hSS = (SS_Handle_t *)ptr;
    void *        element;
    Queue_Info_t *info;
    void *        data;
    while(1)
    {
        element = FIFO_Pop_Blocking(hSS->TxQueue);

        info = (Queue_Info_t *)element;
        data = (void *)((intptr_t)element + sizeof(Queue_Info_t));

        for(int i = 0; i < SS_MAX_CLIENTS; i++)
        {
            pthread_mutex_lock(&hSS->Clients[i].Lock);
            if(hSS->Clients[i].Sock != -1)
            {
                if(info->Sock == 0 || hSS->Clients[i].Sock == info->Sock)
                {
                    if(send(hSS->Clients[i].Sock, data, (int)info->Len, MSG_NOSIGNAL) > 0)
                        hSS->Clients[i].Time = 0;
                }
            }
            pthread_mutex_unlock(&hSS->Clients[i].Lock);
        }

        free(element);
    }

    return NULL;
}

/**
 * @fn int SS_Server_Init(SS_Handle_t *hSS, uint32_t Addr, uint16_t Port, SS_EventCb CbEvent, SS_MessageCb CbMsg,
 * uint16_t TxLen, bool Binary, bool Blocking)
 *
 * @brief Initialize ssServer.
 *
 * @param hSS       Pointer to a server handle.
 * @param Addr      Listening address.
 * @param Port      Listening port.
 * @param CbEvent   Callback on event.
 * @param CbMsg     Callback on message received.
 * @param TxLen     Maximum send queue length, 0 if unlimited.
 * @param Binary    Binary Mode.
 * @param Blocking  Wait for working threads.
 *
 * @retval 0 on success, others on fail.
 */
int SS_Server_Init(SS_Handle_t *hSS, uint32_t Addr, uint16_t Port, SS_EventCb CbEvent, SS_MessageCb CbMsg,
                   uint16_t TxLen, bool Binary, bool Blocking)
{
    struct sockaddr_in address;
    pthread_t          thread_listen, thread_send;
    int                option;

    hSS->Address   = Addr;
    hSS->Port      = Port;
    hSS->OnEvent   = CbEvent;
    hSS->OnMessage = CbMsg;
    hSS->Binary    = Binary;

    for (int i = 0; i < SS_MAX_CLIENTS; i++)
    {
        pthread_mutex_init(&hSS->Clients[i].Lock, NULL);
    }

    hSS->TxQueue = FIFO_Init(TxLen);

#ifdef _WIN32
    WSADATA wsaData;
    if(WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        LogEvent(hSS, SS_Event_Error, NULL, "Error: WSAStartup failed");
        return -2;
    }
#endif

    /* create socket */
    hSS->Sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(hSS->Sock <= 0)
    {
        LogEvent(hSS, SS_Event_Error, NULL, "Error: cannot create socket");
        return -2;
    }

    option = 1;
    if(setsockopt(hSS->Sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&option, sizeof(option)) < 0)
    {
        LogEvent(hSS, SS_Event_Error, NULL, "Error: setsockopt(SO_REUSEADDR) failed");
        return -3;
    }

    option = 1;
    if(setsockopt(hSS->Sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&option, sizeof(option)) < 0)
    {
        LogEvent(hSS, SS_Event_Error, NULL, "Error: setsockopt(TCP_NODELAY) failed");
        return -3;
    }

#ifdef _WIN32
    DWORD timeout = SS_XFER_TIMEO;
#else
    struct timeval timeout;
    timeout.tv_sec = SS_XFER_TIMEO / 1000;
    timeout.tv_usec = (SS_XFER_TIMEO % 1000) * 1000;
#endif
    if(setsockopt(hSS->Sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout)) < 0)
    {
        LogEvent(hSS, SS_Event_Error, NULL, "Error: setsockopt(SO_SNDTIMEO) failed");
        return -3;
    }

    if(setsockopt(hSS->Sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)) < 0)
    {
        LogEvent(hSS, SS_Event_Error, NULL, "Error: setsockopt(SO_RCVTIMEO) failed");
        return -3;
    }

    /* bind socket to port */
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = htonl(hSS->Address);
    address.sin_port        = htons(hSS->Port);
    if(bind(hSS->Sock, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0)
    {
        LogEvent(hSS, SS_Event_Error, NULL, "Error: cannot bind socket to port %d", hSS->Port);
        return -4;
    }

    /* listen on port */
    if(listen(hSS->Sock, SOMAXCONN) < 0)
    {
        LogEvent(hSS, SS_Event_Error, NULL, "Error: cannot listen on port");
        return -5;
    }

    for(int i = 0; i < SS_MAX_CLIENTS; i++)
    {
        hSS->Clients[i].Sock = -1;
    }

    LogEvent(hSS, SS_Event_Info, NULL, "Ready and listening on %d.%d.%d.%d:%d", (int)((hSS->Address >> 24) & 0xff),
             (int)((hSS->Address >> 16) & 0xff), (int)((hSS->Address >> 8) & 0xff), (int)((hSS->Address) & 0xff),
             hSS->Port);

    pthread_create(&thread_listen, 0, Listener_Worker, (void *)hSS);
    pthread_create(&thread_send, 0, Send_Worker, (void *)hSS);

    if(Blocking)
    {
        pthread_join(thread_listen, NULL);
        pthread_join(thread_send, NULL);
    }
    else
    {
        pthread_detach(thread_listen);
        pthread_detach(thread_send);
    }

    return 0;
}

/**
 * @fn void SS_SendMessage(SS_Handle_t *hSS, intptr_t Sock, const char* Msg, size_t Len)
 *
 * @brief Send message.
 *
 * @param hSS   Pointer to the server handle that contains the configuration information for the specified server.
 * @param Sock  Socket handle of destination, 0 of broadcast.
 * @param Msg   Message to send.
 * @param Len   Message length, 0 to use strlen().
 */
void SS_SendMessage(SS_Handle_t *hSS, intptr_t Sock, const char *Msg, size_t Len)
{
    for(int i = 0; i < SS_MAX_CLIENTS; i++)
    {
        pthread_mutex_lock(&hSS->Clients[i].Lock);
        if(hSS->Clients[i].Sock != -1)
        {
            if(Sock == 0 || hSS->Clients[i].Sock == Sock)
            {
                if(send(hSS->Clients[i].Sock, Msg, (int)(Len ? Len : strlen(Msg)), MSG_NOSIGNAL))
                        hSS->Clients[i].Time = 0;
            }
        }
        pthread_mutex_unlock(&hSS->Clients[i].Lock);
    }
}

/**
 * @fn int SS_SendMessage_Async(SS_Handle_t *hSS, intptr_t Sock, const char* Msg, size_t Len, bool Blocking)
 *
 * @brief Send message asynchronously.
 *
 * @param hSS       Pointer to the server handle that contains the configuration information for the specified server.
 * @param Sock      Socket handle of destination, 0 of broadcast.
 * @param Msg       Message to send.
 * @param Len       Message length, 0 to use strlen().
 * @param Blocking  Block if send queue is full, otherwise fail.
 *
 * @retval 0 on success, others on fail.
 */
int SS_SendMessage_Async(SS_Handle_t *hSS, intptr_t Sock, const char *Msg, size_t Len, bool Blocking)
{
    size_t        len = Len ? Len : strlen(Msg);
    void *        element;
    Queue_Info_t *info;
    void *        data;
    int           ret = 0;

    element = malloc(sizeof(Queue_Info_t) + len);
    if(element == NULL)
    {
        LogEvent(hSS, SS_Event_Error, NULL, "Error: buffer alloc failed.");
        return -1;
    }

    info = (Queue_Info_t *)element;
    data = (void *)((intptr_t)element + sizeof(Queue_Info_t));

    info->Sock = Sock;
    info->Len  = len;

    memcpy(data, Msg, len);

    if(Blocking)
    {
        FIFO_Push_Blocking(hSS->TxQueue, element);
    }
    else
    {
        ret = FIFO_Push(hSS->TxQueue, element);
        if(ret != 0)
            free(element);
    }
    return ret;
}

/**
 * @fn void SS_Reply(const SS_Client_Conn_t* Conn, const char* Msg, size_t Len)
 *
 * @brief Reply message to client, can only be used in message received callback
 *
 * @param Conn  Connection information.
 * @param Msg   Message to send.
 * @param Len   Message length, 0 to use strlen().
 */
void SS_Reply(const SS_Client_Conn_t *Conn, const char *Msg, size_t Len)
{
    send(Conn->Sock, Msg, (int)(Len ? Len : strlen(Msg)), MSG_NOSIGNAL);
}
