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

#ifndef __REMOTETASK_H
#define __REMOTETASK_H

#include "RAMP.h"

typedef struct _RAMP_ELEM {
    Queue_Elem  elem;
    FCB         fcb;
    size_t      textlen;
    void*       textbuf;
} RAMP_ELEM;

typedef struct _RAMP_ACK {
    uint8_t     status;
    uint8_t     acknak;
    uint8_t     retry;
    uint8_t     type;
} RAMP_ACK;

/*** CONSTANTS AND CONFIGURATION *******************************************/

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

Bool RAMP_Message_post(RAMP_ELEM* elem, UInt32 timeout);
Bool RAMP_Message_pend(RAMP_ELEM* elem, UInt32 timeout);

#endif /* __REMOTETASK_H */
