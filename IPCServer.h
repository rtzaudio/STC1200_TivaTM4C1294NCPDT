/*
 * IPC - Serial InterProcess Message Protocol
 *
 * Copyright (C) 2016-2020, RTZ Professional Audio, LLC. ALL RIGHTS RESERVED.
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 */

#ifndef _IPCTASK_H_
#define _IPCTASK_H_

#include "CRC16.h"
#include "IPCFrame.h"
#include "IPCMessage.h"

/*** IPC MESSAGE STRUCTURE *************************************************/

typedef struct _IPC_MSG {
    uint16_t        type;           /* the IPC message type code   */
    uint16_t        opcode;         /* application defined op code */
    union {
        int32_t     I;
        uint32_t    U;
        float       F;
    } param1;                       /* unsigned or float param1 */
    union {
        int32_t     I;
        uint32_t    U;
        float       F;
    }  param2;                      /* unsigned or float param2 */
} IPC_MSG;

/*** IPC TX/RX MESSAGE LIST ELEMENT STRUCTURES *****************************/

typedef struct _IPC_ELEM {
	Queue_Elem  elem;
	IPC_FCB     fcb;
    IPC_MSG     msg;
} IPC_ELEM;

typedef struct _IPC_ACK {
    uint8_t     flags;
    uint8_t     acknak;
    uint8_t     retry;
    uint8_t     type;
    IPC_MSG     msg;
} IPC_ACK;

/*** IPC MESSAGE SERVER OBJECT *********************************************/

typedef struct _IPCSVR_OBJECT {
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
    int					txNumFreeMsgs;
    int                 txErrors;
    uint32_t			txCount;
    uint8_t             txNextSeq;          /* next tx sequence# */
    int                 rxNumFreeMsgs;
    int                 rxErrors;
    uint32_t            rxCount;
    uint8_t             rxExpectedSeq;		/* expected recv seq#   */
    uint8_t             rxLastSeq;       	/* last seq# accepted   */
    /* callback handlers */
    //Bool (*datagramHandlerFxn)(IPC_MSG* msg, IPC_FCB* fcb);
    //Bool (*transactionHandlerFxn)(IPC_MSG* msg, IPC_FCB* fcb, UInt32 timeout);
    /* frame memory buffers */
    IPC_ELEM*           txBuf;
    IPC_ELEM*           rxBuf;
    IPC_ACK*            ackBuf;
} IPCSVR_OBJECT;

/*** IPC FUNCTION PROTOTYPES ***********************************************/

Bool IPC_Server_init(void);
Bool IPC_Server_startup(void);

uint8_t IPC_GetTxSeqNum(void);

/* Application specific callback handlers */
Bool IPC_Handle_datagram(IPC_MSG* msg, IPC_FCB* fcb);
Bool IPC_Handle_transaction(IPC_MSG* msg, IPC_FCB* fcb, UInt32 timeout);

/* IPC server internal use */
Bool IPC_Message_post(IPC_MSG* msg, IPC_FCB* fcb, UInt32 timeout);
Bool IPC_Message_pend(IPC_MSG* msg, IPC_FCB* fcb, UInt32 timeout);

/* High level functions to send messages */
Bool IPC_Notify(IPC_MSG* msg, UInt32 timeout);
Bool IPC_Transaction(IPC_MSG* msgTx, IPC_MSG* msgRx, UInt32 timeout);

#endif /* _IPCTASK_H_ */
