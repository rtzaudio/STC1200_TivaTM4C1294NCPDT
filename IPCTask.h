/* ============================================================================
 *
 * DTC-1200 Digital Transport Controller for Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 * ============================================================================
 *
 * Copyright (c) 2014, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ============================================================================ */

#ifndef _IPCTASK_H_
#define _IPCTASK_H_

#include "RAMP.h"

/* IPC Message Structure */
typedef struct _IPCMSG {
    uint32_t    command;                    /* command code   */
    uint32_t    opcode;                     /* operation code */
} IPCMSG;

/* IPC Message List Entry Structure */
typedef struct _FCBMSG {
	Queue_Elem	elem;
	FCB			fcb;
    IPCMSG      msg;
} FCBMSG;

/* IPC Message Server Object */
typedef struct _IPCSVR_OBJECT {
	UART_Handle         uartHandle;
	Queue_Handle        txFreeQue;
    Queue_Handle        txDataQue;
    Semaphore_Handle    txDataSem;
    Semaphore_Handle    txFreeSem;
    int					numFreeMsgs;
    int					txCount;
    uint8_t             currSeq;            /* current tx sequence# */
    uint8_t             expectSeq;          /* expected recv seq#   */
    uint8_t             lastSeq;            /* last seq# accepted   */
    FCBMSG              tx[MAX_WINDOW+1];   /* resend tx queue buf  */
    FCBMSG              rx;
} IPCSVR_OBJECT;

/* Function Prototypes */

Bool IPC_Server_init(IPCSVR_OBJECT* pSvr);
Bool IPC_Send_message(IPCSVR_OBJECT* pSvr, IPCMSG* pMsg, UInt32 timeout);
UInt32 IPC_Send_datagram(IPCSVR_OBJECT* pSvr, IPCMSG* pMsg, UInt32 timeout);

Void IPCReaderTaskFxn(UArg a0, UArg a1);
Void IPCWriterTaskFxn(UArg arg0, UArg arg1);

#endif /* _IPCTASK_H_ */
