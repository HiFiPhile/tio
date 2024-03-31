/**
******************************************************************************
* @file    ssServer.h
* @author  Zixun LI
* @version V1.0
* @date    28/01/2022
* @brief   simple socket server
*/
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _SS_SERVER_H
#define _SS_SERVER_H
/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "cthread.h"
#include "fifo.h"
/* Exported defines --------------------------------------------------------- */
/* Maximum concurrent client */
#ifndef SS_MAX_CLIENTS
#define SS_MAX_CLIENTS 4
#endif
/* Maximum received length */
#ifndef SS_MAX_RX_LENGTH
#define SS_MAX_RX_LENGTH 65535
#endif
/* Send timeout in millisecond */
#ifndef SS_XFER_TIMEO
#define SS_XFER_TIMEO 100
#endif
/* Client alive timeout in second */
#ifndef SS_ALIVE_TIMEO
#define SS_ALIVE_TIMEO 300
#endif
/* Exported types ------------------------------------------------------------*/
/* SS event type */
typedef enum
{
    SS_Event_Conn,
    SS_Event_Disc,
    SS_Event_Info,
    SS_Event_Error,
} SS_Event_t;
/* SS handle type */
typedef struct SS_Handle SS_Handle_t;
/* Client connection info type */
typedef struct SS_Client_Conn SS_Client_Conn_t;
/* Callback on event */
typedef void (*SS_EventCb)(const SS_Handle_t* hSS, SS_Event_t Event, const SS_Client_Conn_t* Conn, const char* Msg);
/* Callback on message received */
typedef void (*SS_MessageCb)(const SS_Client_Conn_t* Conn, const char* Msg, size_t Len);
/* Client connection info */
struct SS_Client_Conn
{
    pthread_mutex_t Lock;
    intptr_t        Sock;
    uint32_t        Address;
    uint32_t        Time;
    uint16_t        Port;
};
/* SS handle */
struct SS_Handle
{
    SS_EventCb       OnEvent;
    SS_MessageCb     OnMessage;
    intptr_t         Sock;
    uint32_t         Address;
    uint16_t         Port;
    FIFO_Handle_t    TxQueue;
    bool             Binary;
    SS_Client_Conn_t Clients[SS_MAX_CLIENTS];
};
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
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
                   uint16_t TxLen, bool Binary, bool Blocking);

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
void SS_SendMessage(SS_Handle_t* hSS, intptr_t Sock, const char* Msg, size_t Len);

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
int SS_SendMessage_Async(SS_Handle_t* hSS, intptr_t Sock, const char* Msg, size_t Len, bool Blocking);

/**
 * @fn void SS_Reply(const SS_Client_Conn_t* Conn, const char* Msg, size_t Len)
 *
 * @brief Reply message to client, can only be used in message received callback
 *
 * @param Conn  Connection information.
 * @param Msg   Message to send.
 * @param Len   Message length, 0 to use strlen().
 */
void SS_Reply(const SS_Client_Conn_t* Conn, const char* Msg, size_t Len);

#endif
