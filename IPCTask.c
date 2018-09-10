/*
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
 */

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Gate.h>
#include <xdc/runtime/Memory.h>

#include <ti/sysbios/BIOS.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SDSPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>

#include <file.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include <driverlib/sysctl.h>

#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Queue.h>

/* PMX42 Board Header file */

#include "STC1200.h"
#include "Board.h"
#include "IPCTask.h"

/* External Data Items */

/* Global Data Items */

static IPCMSG_SERVER g_server;

/* The following objects are created statically. */
static Semaphore_Handle sem;
static Queue_Handle freeQueue;

/* Static Function Prototypes */

//*****************************************************************************
//
//*****************************************************************************

int IPC_init(void)
{
    Int i;
    IPCMSG *msg;
    Error_Block eb;
    UART_Params uartParams;

    /* Open the UART for binary mode */

    UART_Params_init(&uartParams);

    uartParams.readMode       = UART_MODE_BLOCKING;
    uartParams.writeMode      = UART_MODE_BLOCKING;
    uartParams.readTimeout    = 1000;                   // 1 second read timeout
    uartParams.writeTimeout   = BIOS_WAIT_FOREVER;
    uartParams.readCallback   = NULL;
    uartParams.writeCallback  = NULL;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.writeDataMode  = UART_DATA_BINARY;
    uartParams.readDataMode   = UART_DATA_BINARY;
    uartParams.readEcho       = UART_ECHO_OFF;
    uartParams.baudRate       = 115200;
    uartParams.stopBits       = UART_STOP_ONE;
    uartParams.parityType     = UART_PAR_NONE;

    g_server.uartHandle = UART_open(Board_UART_IPC, &uartParams);

    if (g_server.uartHandle == NULL)
        System_abort("Error initializing UART\n");

    /* Create the tx queues */

    freeQueue = Queue_create(NULL, NULL);

    Error_init(&eb);

    msg = (IPCMSG*)Memory_alloc(NULL, MAX_WINDOW * sizeof(IPCMSG), 0, &eb);

    if (msg == NULL)
        System_abort("Memory allocation failed");

    /* Put all messages on freeQueue */
    for (i=0; i < MAX_WINDOW; msg++, i++)
        Queue_put(freeQueue, (Queue_Elem *)msg);



    g_server.currseq   = 1;     /* current tx sequence# */
    g_server.expectseq = 0;     /* expected rx seq#     */
    g_server.lastseq   = 0;     /* last seq# rx'ed      */

    return 1;
}

//*****************************************************************************
//
//*****************************************************************************

Void IPCReaderTaskFxn(UArg arg0, UArg arg1)
{
    int rc;

    /* Now begin the main program command task processing loop */

    while (true)
    {
    	FCB *fcb = &g_server.rx.fcb;

    	fcb->textbuf = &g_server.rx.msg;
    	fcb->textlen = sizeof(IPCMSG);

        rc = RAMP_RxFrame(g_server.uartHandle, fcb);

        if (rc == ERR_TIMEOUT)
        	continue;

        if (rc != 0)
        {
        	System_printf("RAMP_RxFrame Error %d\n", rc);
        	System_flush();
        	continue;
        }

        /* Save the last sequence number received */

        g_server.lastseq = fcb->seqnum;

        /* We've received a valid frame, attempt to decode it */

        switch(fcb->type & FRAME_TYPE_MASK)
        {
        	case TYPE_ACK_ONLY:			/* ACK message frame only      */
        		break;

        	case TYPE_NAK_ONLY:			/* NAK message frame only      */
        		break;

        	case TYPE_MSG_ONLY:			/* message only frame          */
        		if (fcb->type & F_DATAGRAM)
        		{

        		}
        		break;

        	case TYPE_MSG_ACK:			/* piggyback message plus ACK  */
        		break;

        	case TYPE_MSG_NAK:			/* piggyback message plus NAK  */
        		break;

        	default:
        		break;
        }

        //msg = (IPCMSG *)Queue_get(freeQueue);

        /* Increment the frame sequence number */
        //g_RxFcb.seqnum = INC_SEQ_NUM(g_RxFcb.seqnum);
    }
}

//*****************************************************************************
//
//*****************************************************************************

Void IPCWriterTaskFxn(UArg arg0, UArg arg1)
{
#if 0
    IPCMSG msg;

    /* Now begin the main program command task processing loop */

    //RAMP_InitFcb(&g_txFcb);

    while (true)
    {
        /* Wait for semaphore to be posted by writer(). */
        Semaphore_pend(sem, BIOS_WAIT_FOREVER);

        FCB *fcb = &g_server.tx.fcb;

        fcb->textbuf = &msg;
        fcb->textlen = sizeof(msg);

        RAMP_TxFrame(g_server.uartHandle, fcb);

        /* Increment the frame sequence number */
        fcb->seqnum = INC_SEQ_NUM(fcb->seqnum);
    }
#endif
}

// End-Of-File
