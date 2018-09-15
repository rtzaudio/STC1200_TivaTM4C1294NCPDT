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
#include "CLITask.h"

/* External Data Items */

/* Global Data Items */

static IPCSVR_OBJECT g_ipc;

/* Static Function Prototypes */

static Void IPCReaderTaskFxn(UArg a0, UArg a1);
static Void IPCWriterTaskFxn(UArg arg0, UArg arg1);
static Void IPCWorkerTaskFxn(UArg arg0, UArg arg1);

//*****************************************************************************
// This function initializes the IPC server and creates all it's worker
// threads. It also allocates all receive and transmit buffers, queues and
// semaphores needed to manage the queues and tasks.
//*****************************************************************************

Bool IPC_Server_init(void)
{
    Int i;
    IPC_ELEM* msg;
    Error_Block eb;
    UART_Params uartParams;
    Task_Params taskParams;

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

    g_ipc.uartHandle = UART_open(Board_UART_IPC, &uartParams);

    if (g_ipc.uartHandle == NULL)
        System_abort("Error initializing UART\n");

    /* Create the queues needed */
    g_ipc.txFreeQue = Queue_create(NULL, NULL);
    g_ipc.txDataQue = Queue_create(NULL, NULL);
    g_ipc.rxFreeQue = Queue_create(NULL, NULL);
    g_ipc.rxDataQue = Queue_create(NULL, NULL);

    /* Create semaphores needed */
    g_ipc.txFreeSem = Semaphore_create(MAX_WINDOW, NULL, NULL);
    g_ipc.txDataSem = Semaphore_create(0, NULL, NULL);
    g_ipc.rxFreeSem = Semaphore_create(MAX_WINDOW, NULL, NULL);
    g_ipc.rxDataSem = Semaphore_create(0, NULL, NULL);

    /*
     * Allocate and Initialize TRANSMIT Buffer Memory
     */

    Error_init(&eb);

    g_ipc.txMsgBuf = (IPC_ELEM*)Memory_alloc(NULL, sizeof(IPC_ELEM) * MAX_WINDOW, 0, &eb);

    if (g_ipc.txMsgBuf == NULL)
        System_abort("TxBuf allocation failed");

    msg = g_ipc.txMsgBuf;

    /* Put all tx message buffers on the freeQueue */
    for (i=0; i < MAX_WINDOW; i++, msg++) {
        Queue_enqueue(g_ipc.txFreeQue, (Queue_Elem*)msg);
    }

    /*
     * Allocate and Initialize RECEIVE Buffer Memory
     */

    Error_init(&eb);

    g_ipc.rxMsgBuf = (IPC_ELEM*)Memory_alloc(NULL, sizeof(IPC_ELEM) * MAX_WINDOW, 0, &eb);

    if (g_ipc.rxMsgBuf == NULL)
        System_abort("RxBuf allocation failed");

    msg = g_ipc.rxMsgBuf;

    /* Put all tx message buffers on the freeQueue */
    for (i=0; i < MAX_WINDOW; i++, msg++) {
        Queue_enqueue(g_ipc.rxFreeQue, (Queue_Elem*)msg);
    }

    /* Initialize Server Data Items */

    g_ipc.txErrors      = 0;
    g_ipc.txCount       = 0;
    g_ipc.txNumFreeMsgs = MAX_WINDOW;
    g_ipc.txCurrSeq     = MIN_SEQ_NUM;      /* current tx sequence# */

    g_ipc.rxErrors      = 0;
    g_ipc.rxCount       = 0;
    g_ipc.rxNumFreeMsgs = MAX_WINDOW;
    g_ipc.rxLastSeq     = 0;                /* last seq# accepted   */
    g_ipc.rxExpectedSeq = MIN_SEQ_NUM;      /* expected recv seq#   */

    /*
     * Finally, create the reader, writer and worker tasks
     */

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 700;
    taskParams.priority  = 6;
    Task_create((Task_FuncPtr)IPCWriterTaskFxn, &taskParams, &eb);

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 700;
    taskParams.priority  = 6;
    Task_create((Task_FuncPtr)IPCReaderTaskFxn, &taskParams, &eb);

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 1024;
    taskParams.priority  = 5;
    Task_create((Task_FuncPtr)IPCWorkerTaskFxn, &taskParams, &eb);

    return TRUE;
}

//*****************************************************************************
// This packet writer task waits for any message to appear in the
// outgoing transmit message queue.
//*****************************************************************************

Void IPCWriterTaskFxn(UArg arg0, UArg arg1)
{
    UInt key;
    IPC_ELEM* elem;

    /* Begin the packet transmit task loop */

    while (TRUE)
    {
        /* Wait for a packet in the tx queue */
    	if (!Semaphore_pend(g_ipc.txDataSem, 1000))
    	{
    	    /* Timeout, nothing to send */
    	    continue;
    	}

    	/* Get the message from txDataQue */
        elem = Queue_get(g_ipc.txDataQue);

        /* Transmit the packet! */
        RAMP_TxFrame(g_ipc.uartHandle, &(elem->fcb), &(elem->msg), sizeof(IPCMSG));

        /* Perform the enqueue and increment numFreeMsgs atomically */
        key = Hwi_disable();

        /* Put message buffer back on the free queue */
        Queue_enqueue(g_ipc.txFreeQue, (Queue_Elem *)elem);

        /* Increment numFreeMsgs */
        g_ipc.txNumFreeMsgs++;

        /* Increment total number of packets transmitted */
        g_ipc.txCount++;

        /* re-enable ints */
        Hwi_restore(key);

        /* post the semaphore */
        Semaphore_post(g_ipc.txFreeSem);
    }
}

//*****************************************************************************
// This function posts a message to the transmit queue. A return FALSE value
// indicates the timeout expired or a buffer never became available for
// transmission within the timeout period specified.
//*****************************************************************************

Bool IPC_post_message(IPCMSG* msg, FCB* fcb, UInt32 timeout)
{
    UInt key;
    uint8_t seqnum;
    IPC_ELEM* elem;

    /* Wait for a free transmit buffer and timeout if necessary */
    if (Semaphore_pend(g_ipc.txFreeSem, timeout))
    {
        /* perform the dequeue and decrement numFreeMsgs atomically */
        key = Hwi_disable();

        /* get a message from the free queue */
        elem = Queue_dequeue(g_ipc.txFreeQue);

        /* Make sure that a valid pointer was returned. */
        if (elem == (IPC_ELEM*)(g_ipc.txFreeQue))
        {
            Hwi_restore(key);
            return FALSE;
        }

        /* Get the next frame sequence number */
        seqnum = g_ipc.txCurrSeq;

        /* Increment the servers sequence number */
        g_ipc.txCurrSeq = INC_SEQ_NUM(seqnum);

        /* decrement the numFreeMsgs */
        g_ipc.txNumFreeMsgs--;

        /* re-enable ints */
        Hwi_restore(key);

        /* Set and return the sequence number! */
        fcb->seqnum = seqnum;

        /* copy msg to element */
        memcpy(&(elem->msg), msg, sizeof(IPCMSG));
        memcpy(&(elem->fcb), fcb, sizeof(FCB));

        /* put message on txDataQueue */
        if (fcb->type & F_PRIORITY)
            Queue_putHead(g_ipc.txDataQue, (Queue_Elem *)elem);
        else
            Queue_put(g_ipc.txDataQue, (Queue_Elem *)elem);

        /* post the semaphore */
        Semaphore_post(g_ipc.txDataSem);

        return TRUE;          /* success */
    }

    return FALSE;         /* error */
}

//*****************************************************************************
//
//*****************************************************************************

Void IPCReaderTaskFxn(UArg arg0, UArg arg1)
{
    int rc;
    UInt key;
    IPC_ELEM* elem;

    /* Begin the packet receive task loop */

    while (true)
    {
        /* Wait for a free receive buffer if necessary */
        if (!Semaphore_pend(g_ipc.rxFreeSem, 1000))
        {
            /* See if any packets have not been ACK'ed
             * and re-send if necessary.
             */
            continue;
        }

        /* perform the dequeue and decrement numFreeMsgs atomically */
        key = Hwi_disable();

        /* get a rx buffer from the free queue */
        elem = Queue_dequeue(g_ipc.rxFreeQue);

        /* Make sure that a valid pointer was returned. */
        if (elem == (IPC_ELEM*)(g_ipc.rxFreeQue))
        {
            Hwi_restore(key);
            continue;
        }

        /* decrement the numFreeMsgs */
        g_ipc.rxNumFreeMsgs--;

        /* re-enable ints */
        Hwi_restore(key);

        /* Buffer allocated, wait for a packet from peer */

        while (1)
        {
            /* Attempt to read a frame from the peer */

            rc = RAMP_RxFrame(g_ipc.uartHandle, &(elem->fcb), &(elem->msg), sizeof(IPCMSG));

            /* Zero means packet received successfully */
            if (rc == 0)
                break;

            if (rc > ERR_TIMEOUT)
            {
                g_ipc.rxErrors++;

                System_printf("RAMP_RxFrame Error %d\n", rc);
                System_flush();
            }
        }

        /* Packet received, save the sequence number received */
        g_ipc.rxLastSeq = elem->fcb.seqnum;

        /* Increment the total packets received count */
        g_ipc.rxCount++;

        /*Put message on rxDataQueue */
        if (elem->fcb.type & F_PRIORITY)
            Queue_putHead(g_ipc.rxDataQue, (Queue_Elem*)elem);
        else
            Queue_put(g_ipc.rxDataQue, (Queue_Elem*)elem);

        /* post the semaphore */
        Semaphore_post(g_ipc.rxDataSem);
    }
}

//*****************************************************************************
//
//*****************************************************************************

Bool IPC_pend_message(IPCMSG* msg, FCB* fcb, UInt32 timeout)
{
    UInt key;
    IPC_ELEM* elem;

    if (Semaphore_pend(g_ipc.rxDataSem, timeout))
    {
        /* get message from dataQue */
        elem = Queue_get(g_ipc.rxDataQue);

        /* perform the enqueue and increment numFreeMsgs atomically */
        key = Hwi_disable();

        /* put message on freeQue */
        Queue_enqueue(g_ipc.rxFreeQue, (Queue_Elem *)elem);

        /* increment numFreeMsgs */
        g_ipc.rxNumFreeMsgs++;

        /* re-enable ints */
        Hwi_restore(key);

        /* return message and fcb data to caller */
        memcpy(msg, &(elem->msg), sizeof(IPCMSG));
        memcpy(fcb, &(elem->fcb), sizeof(FCB));

        /* post the semaphore */
        Semaphore_post(g_ipc.rxFreeSem);

        return TRUE;
    }

    return FALSE;
}

//*****************************************************************************
//
//*****************************************************************************

Void IPCWorkerTaskFxn(UArg arg0, UArg arg1)
{
    FCB fcb;
    IPCMSG msg;

    while (1)
    {
        /* Wait for a RAMP message from peer */

        if (!IPC_pend_message(&msg, &fcb, 1000))
        {
            /* Timeout, no message received, check to see if we have
             * any messages with ACK pending that haven't been
             * acknowledge yet. If so, then retransmit until
             * for max number of retries.
             */
            continue;
        }

        /* We've received a valid RAMP message frame. Check the
         * message type received as follows:
         *
         *  TYPE_MSG_ONLY - This is a request for data that requires
         *                  a response message with ACK to the peer.
         *                  If the F_DATAGRAM type flag is set, the
         *                  message does not require an ACK response
         *                  and the message data can be processed.
         *
         *  TYPE_MSG_ACK  - This is a response to a request for data
         *                  from peer. The ACK indicates the request
         *                  was processed and returned in the msg.
         */

        if ((fcb.type & FRAME_TYPE_MASK) == TYPE_MSG_ONLY)
        {
            CLI_printf("Rx(%u)=%04x\n", fcb.seqnum, msg.opcode);

            if (fcb.type & F_DATAGRAM)
            {
                /* deliver the datagram and we're done */
            }
            else
            {
                /* begin transaction phase for request */
            }
        }
        else if ((fcb.type & FRAME_TYPE_MASK) == TYPE_MSG_ACK)
        {
            /* response to transaction received */
        }
    }
}

//*****************************************************************************
//
//*****************************************************************************

Bool IPC_Send_datagram(IPCMSG* msg, UInt32 timeout)
{
    FCB fcb;

    fcb.type    = MAKETYPE(F_DATAGRAM, TYPE_MSG_ONLY);
    fcb.acknak  = 0;
    fcb.address = 0;
    fcb.seqnum  = 0;

    return IPC_post_message(msg, &fcb, timeout);
}

//*****************************************************************************
//
//*****************************************************************************

Bool IPC_Send_transaction(IPCMSG* msg, UInt32 timeout)
{
    FCB fcb;

    fcb.type    = MAKETYPE(F_ACKNAK, TYPE_MSG_ONLY);
    fcb.acknak  = 0;
    fcb.address = 0;
    fcb.seqnum  = 0;

    return IPC_post_message(msg, &fcb, timeout);
}


// End-Of-File
