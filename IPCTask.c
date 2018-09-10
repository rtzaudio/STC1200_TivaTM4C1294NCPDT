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
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>

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

IPCSVR_OBJECT g_server;

/* Static Function Prototypes */

//*****************************************************************************
//
//*****************************************************************************

Bool IPC_Server_init(IPCSVR_OBJECT* pSvr)
{
    Int i;
    FCBMSG *msg;
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

    pSvr->uartHandle = UART_open(Board_UART_IPC, &uartParams);

    if (pSvr->uartHandle == NULL)
        System_abort("Error initializing UART\n");

    /* Create the tx queues */

    pSvr->txFreeQue = Queue_create(NULL, NULL);
    pSvr->txDataQue = Queue_create(NULL, NULL);

    Error_init(&eb);
    msg = (FCBMSG*)Memory_alloc(NULL, MAX_WINDOW * sizeof(FCBMSG), 0, &eb);

    if (msg == NULL)
        System_abort("Memory allocation failed");

    /* Put all tx message buffers on the freeQueue */
    for (i=0; i < MAX_WINDOW; i++, msg++) {
        Queue_enqueue(pSvr->txFreeQue, (Queue_Elem*)msg);
    }

    /* Initialize Server Data Items */
    pSvr->numFreeMsgs = MAX_WINDOW;
    pSvr->currSeq     = 1;     /* current tx sequence# */
    pSvr->expectSeq   = 0;     /* expected rx seq#     */
    pSvr->lastSeq     = 0;     /* last seq# rx'ed      */

    return TRUE;
}

//*****************************************************************************
//
//*****************************************************************************

Bool IPC_Send_message(IPCSVR_OBJECT* pSvr, IPCMSG* pMsg, UInt32 timeout)
{
    UInt key;
    FCBMSG* elem;

    if (Semaphore_pend(pSvr->txFreeSem, timeout))
    {
        /* perform the dequeue and decrement numFreeMsgs atomically */
        key = Hwi_disable();

        /* get a message from the free queue */
        elem = Queue_dequeue(pSvr->txFreeQue);

        /* Make sure that a valid pointer was returned. */
        if (elem == (FCBMSG*)(pSvr->txFreeQue))
        {
            Hwi_restore(key);
            return FALSE;
        }

        /* decrement the numFreeMsgs */
        pSvr->numFreeMsgs--;

        /* re-enable ints */
        Hwi_restore(key);

        /* copy msg to elem */
        memcpy(&(elem->msg), pMsg, sizeof(IPCMSG));

        /* put message on dataQueue */
        Queue_put(pSvr->txDataQue, (Queue_Elem *)elem);

        /* post the semaphore */
        Semaphore_post(pSvr->txDataSem);

        return TRUE;          /* success */
    }

    return FALSE;         /* error */
}

//*****************************************************************************
//
//*****************************************************************************

UInt32 IPC_Send_datagram(IPCSVR_OBJECT* pSvr, IPCMSG* pMsg, UInt32 timeout)
{
	return 0;
}

//*****************************************************************************
//
//*****************************************************************************

Void IPCWriterTaskFxn(UArg arg0, UArg arg1)
{
#if 0
    UInt key;
    FCBMSG* elem;
    Queue_Handle dataQue;
    Queue_Handle freeQue;
    Semaphore_Handle dataSem;
    Semaphore_Handle freeSem;

    /* Now begin the main program command task processing loop */

    while (TRUE)
    {
    	if (Semaphore_pend(dataSem, timeout))
    	{
            /* get message from dataQue */
            elem = Queue_get(pSvr->txDataQue);

            /* copy message to user supplied pointer */
            memcpy(msg, elem + 1, obj->msgSize);

            /* perform the enqueue and increment numFreeMsgs atomically */
            key = Hwi_disable();

            /* put message on freeQue */
            Queue_enqueue(freeQue, (Queue_Elem *)elem);

            /* increment numFreeMsgs */
            obj->numFreeMsgs++;

            /* re-enable ints */
            Hwi_restore(key);

            /* post the semaphore */
            Semaphore_post(freeSem);

            return TRUE;
        }

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

        g_server.lastSeq = fcb->seqnum;

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

// End-Of-File
