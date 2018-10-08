/*
 * PMX42.h : created 5/18/2015
 *
 * Copyright (C) 2015, Robert E. Starr. ALL RIGHTS RESERVED.
 *
 * THIS MATERIAL CONTAINS  CONFIDENTIAL, PROPRIETARY AND TRADE
 * SECRET INFORMATION. NO DISCLOSURE OR USE OF ANY
 * PORTIONS OF THIS MATERIAL MAY BE MADE WITHOUT THE EXPRESS
 * WRITTEN CONSENT OF THE AUTHOR.
 */

#ifndef __RAMPSERVER_H
#define __RAMPSERVER_H

#include "RAMP.h"
#include "RAMPMessage.h"

/*** RAMP MESSAGE STRUCTURE ************************************************/

typedef struct _RAMP_MSG {
    uint16_t        type;           /* message type/class code     */
    uint16_t        opcode;         /* application defined op code */
    union {
        uint32_t    U;
        float       F;
    } param1;                       /* unsigned or float param1 */
    union {
        uint32_t    U;
        float       F;
    }  param2;                      /* unsigned or float param2 */
} RAMP_MSG;

/*** RAMP ELEMENT STRUCTURES ***********************************************/

typedef struct _RAMP_ELEM {
    Queue_Elem  elem;
    RAMP_FCB    fcb;
    RAMP_MSG    msg;
} RAMP_ELEM;

typedef struct _RAMP_ACK {
    uint8_t     flags;
    uint8_t     acknak;
    uint8_t     retry;
    uint8_t     type;
    RAMP_MSG    msg;
} RAMP_ACK;

/*** SERVER STRUCTURE ******************************************************/

typedef struct _RAMP_SVR_OBJECT {
    UART_Handle         uartHandle;
    /* tx queues and semaphores */
    Queue_Handle        txFreeQue;
    Queue_Handle        txDataQue;
    Semaphore_Handle    txDataSem;
    Semaphore_Handle    txFreeSem;
    Event_Handle        ackEvent;
    /* rx queues and semaphores */
    Queue_Handle        rxFreeQue;
    Queue_Handle        rxDataQue;
    Semaphore_Handle    rxDataSem;
    Semaphore_Handle    rxFreeSem;
    /* server data items */
    int                 txNumFreeMsgs;
    int                 txErrors;
    uint32_t            txCount;
    uint8_t             txNextSeq;          /* next tx sequence# */
    int                 rxNumFreeMsgs;
    int                 rxErrors;
    uint32_t            rxCount;
    uint8_t             rxExpectedSeq;      /* expected recv seq#   */
    uint8_t             rxLastSeq;          /* last seq# accepted   */
    /* frame memory buffers */
    RAMP_ELEM*          txBuf;
    RAMP_ELEM*          rxBuf;
    RAMP_ACK*           ackBuf;
} RAMP_SVR_OBJECT;

/*** FUNCTION PROTOTYPES ***************************************************/

Bool RAMP_Server_init(void);

Bool RAMP_pend(RAMP_FCB *fcb, RAMP_MSG* msg, UInt32 timeout);
Bool RAMP_post(RAMP_FCB *fcb, RAMP_MSG* msg, UInt32 timeout);

Bool RAMP_Send_Display(UInt32 timeout);
Bool RAMP_Send_Message(RAMP_MSG* msg, UInt32 timeout);
Bool RAMP_Transaction(RAMP_MSG* txMsg, RAMP_MSG* rxMsg, UInt32 timeout);

void RAMP_Handle_message(RAMP_FCB* fcb, RAMP_MSG* msg);
void RAMP_Handle_datagram(RAMP_FCB* fcb, RAMP_MSG* msg);

#endif /* __RAMPSERVER_H */
