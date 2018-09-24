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

/*
 *    ======== tcpEcho.c ========
 *    Contains BSD sockets code.
 */

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Gate.h>
#include <xdc/runtime/Memory.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SDSPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>

/* NDK BSD support */
#include <sys/socket.h>

#include <file.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include <driverlib/sysctl.h>

/* Graphiclib Header file */
#include <grlib/grlib.h>
#include <RAMPServer.h>
#include "drivers/offscrmono.h"

/* PMX42 Board Header file */
#include "Board.h"
#include "RAMP.h"
#include "STC1200.h"

/* External Data Items */

extern SYSDATA g_sysData;

/* Global Data Items */

UART_Handle g_handleUart422;
FCB g_txFcb;

/* Static Function Prototypes */

/* Global Data Items */
static RAMP_SVR_OBJECT g_svr;

/* Static Function Prototypes */
static Void RAMPReaderTaskFxn(UArg a0, UArg a1);
static Void RAMPWriterTaskFxn(UArg arg0, UArg arg1);
static Void RAMPWorkerTaskFxn(UArg arg0, UArg arg1);

//*****************************************************************************
// This function initializes the IPC server and creates all it's worker
// threads. It also allocates all receive and transmit buffers, queues and
// semaphores needed to manage the queues and tasks.
//*****************************************************************************

Bool RAMP_Server_init(void)
{
    Int i;
    RAMP_ELEM* msg;
    Error_Block eb;
    UART_Params uartParams;
    Task_Params taskParams;

    /*
     * Open the UART for RS-422 communications
     */

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

    g_svr.uartHandle = UART_open(Board_UART_RS422_REMOTE, &uartParams);

    if (g_svr.uartHandle == NULL)
        System_abort("Error initializing UART\n");

    /* Assert the RS-422 DE & RE pins */
    GPIO_write(Board_RS422_DE, PIN_HIGH);
    GPIO_write(Board_RS422_RE_N, PIN_LOW);

    /* Create the queues needed */
    g_svr.txFreeQue = Queue_create(NULL, NULL);
    g_svr.txDataQue = Queue_create(NULL, NULL);
    g_svr.rxFreeQue = Queue_create(NULL, NULL);
    g_svr.rxDataQue = Queue_create(NULL, NULL);

    /* Create semaphores needed */
    g_svr.txFreeSem = Semaphore_create(MAX_WINDOW, NULL, NULL);
    g_svr.txDataSem = Semaphore_create(0, NULL, NULL);
    g_svr.rxFreeSem = Semaphore_create(MAX_WINDOW, NULL, NULL);
    g_svr.rxDataSem = Semaphore_create(0, NULL, NULL);

    Error_init(&eb);
    g_svr.ackEvent  = Event_create(NULL, NULL);

    /*
     * Allocate and Initialize TRANSMIT Buffer Memory
     */

    Error_init(&eb);

    g_svr.txBuf = (RAMP_ELEM*)Memory_alloc(NULL, sizeof(RAMP_ELEM) * MAX_WINDOW, 0, &eb);

    if (g_svr.txBuf == NULL)
        System_abort("TxBuf allocation failed");

    msg = g_svr.txBuf;

    /* Put all tx message buffers on the freeQueue */
    for (i=0; i < MAX_WINDOW; i++, msg++) {
        Queue_enqueue(g_svr.txFreeQue, (Queue_Elem*)msg);
    }

    /*
     * Allocate and Initialize RECEIVE Buffer Memory
     */

    Error_init(&eb);

    g_svr.rxBuf = (RAMP_ELEM*)Memory_alloc(NULL, sizeof(RAMP_ELEM) * MAX_WINDOW, 0, &eb);

    if (g_svr.rxBuf == NULL)
        System_abort("RxBuf allocation failed");

    msg = g_svr.rxBuf;

    /* Put all tx message buffers on the freeQueue */
    for (i=0; i < MAX_WINDOW; i++, msg++) {
        Queue_enqueue(g_svr.rxFreeQue, (Queue_Elem*)msg);
    }

    /*
     * Allocate and ACK RECEIVE Buffer Memory
     */

    Error_init(&eb);

    g_svr.ackBuf = (RAMP_ACK*)Memory_alloc(NULL, sizeof(RAMP_ACK) * MAX_WINDOW, 0, &eb);

    if (g_svr.ackBuf == NULL)
        System_abort("AckBuf allocation failed");

    /* Initialize Server Data Items */

    g_svr.txErrors      = 0;
    g_svr.txCount       = 0;
    g_svr.txNumFreeMsgs = MAX_WINDOW;
    g_svr.txNextSeq     = MIN_SEQ_NUM;      /* current tx sequence# */

    g_svr.rxErrors      = 0;
    g_svr.rxCount       = 0;
    g_svr.rxNumFreeMsgs = MAX_WINDOW;
    g_svr.rxLastSeq     = 0;                /* last seq# accepted   */
    g_svr.rxExpectedSeq = MIN_SEQ_NUM;      /* expected recv seq#   */

    /*
     * Finally, create the reader, writer and worker tasks
     */

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 700;
    taskParams.priority  = 6;
    taskParams.arg0      = (UArg)&g_svr;
    taskParams.arg1      = 0;
    Task_create((Task_FuncPtr)RAMPWriterTaskFxn, &taskParams, &eb);

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 700;
    taskParams.priority  = 6;
    taskParams.arg0      = (UArg)&g_svr;
    taskParams.arg1      = 0;
    Task_create((Task_FuncPtr)RAMPReaderTaskFxn, &taskParams, &eb);

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 1500;
    taskParams.priority  = 10;
    taskParams.arg0      = (UArg)&g_svr;
    taskParams.arg1      = 0;
    Task_create((Task_FuncPtr)RAMPWorkerTaskFxn, &taskParams, &eb);

    return TRUE;
}

//*****************************************************************************
// This function returns the next available transmit and increments the
// counter to the next frame sequence number.
//*****************************************************************************

uint8_t RAMP_GetTxSeqNum(void)
{
    /* increment sequence number atomically */
    UInt key = Hwi_disable();

    /* Get the next frame sequence number */
    uint8_t seqnum = g_svr.txNextSeq;

    /* Increment the servers sequence number */
    g_svr.txNextSeq = INC_SEQ_NUM(seqnum);

    /* re-enable ints */
    Hwi_restore(key);

    return seqnum;
}

//*****************************************************************************
// This function blocks until an IPC message is available in the rx queue or
// the timeout expires. A return FALSE value indicates the timeout expired
// or a buffer never became available for the receiver within the timeout
// period specified.
//*****************************************************************************

Bool RAMP_pend(RAMP_ELEM* msg, UInt32 timeout)
{
#if 0
    UInt key;
    IPC_ELEM* elem;

    if (Semaphore_pend(g_svr.rxDataSem, timeout))
    {
        /* get message from dataQue */
        elem = Queue_get(g_svr.rxDataQue);

        /* perform the enqueue and increment numFreeMsgs atomically */
        key = Hwi_disable();

        /* put message on freeQue */
        Queue_enqueue(g_svr.rxFreeQue, (Queue_Elem *)elem);

        /* increment numFreeMsgs */
        g_svr.rxNumFreeMsgs++;

        /* re-enable ints */
        Hwi_restore(key);

        /* return message and fcb data to caller */
        memcpy(msg, &(elem->msg), sizeof(IPCMSG));
        memcpy(fcb, &(elem->fcb), sizeof(FCB));

        /* post the semaphore */
        Semaphore_post(g_svr.rxFreeSem);

        return TRUE;
    }
#endif
    return FALSE;
}

//*****************************************************************************
// This function posts a message to the transmit queue. A return FALSE value
// indicates the timeout expired or a buffer never became available for
// transmission within the timeout period specified.
//*****************************************************************************

Bool RAMP_post(RAMP_ELEM* msg, UInt32 timeout)
{
    UInt key;
    RAMP_ELEM* elem;

    /* Wait for a free transmit buffer and timeout if necessary */
    if (Semaphore_pend(g_svr.txFreeSem, timeout))
    {
        /* perform the dequeue and decrement numFreeMsgs atomically */
        key = Hwi_disable();

        /* get a message from the free queue */
        elem = Queue_dequeue(g_svr.txFreeQue);

        /* Make sure that a valid pointer was returned. */
        if (elem == (RAMP_ELEM*)(g_svr.txFreeQue))
        {
            Hwi_restore(key);
            return FALSE;
        }

        /* decrement the numFreeMsgs */
        g_svr.txNumFreeMsgs--;

        /* re-enable ints */
        Hwi_restore(key);

        /* Save pointer to data buffer */
        elem->textbuf = msg->textbuf;
        elem->textlen = msg->textlen;

        /* copy FCB info to element */
        memcpy(&(elem->fcb), &(msg->fcb), sizeof(FCB));

        /* put message on txDataQueue */
        if (msg->fcb.type & F_PRIORITY)
            Queue_putHead(g_svr.txDataQue, (Queue_Elem *)elem);
        else
            Queue_put(g_svr.txDataQue, (Queue_Elem *)elem);

        /* post the semaphore */
        Semaphore_post(g_svr.txDataSem);

        return TRUE;      /* success */
    }

    return FALSE;         /* error */
}

//*****************************************************************************
// This packet writer task waits for any message to appear in the
// outgoing transmit message queue and transmits all items from the queue.
//*****************************************************************************

Void RAMPWriterTaskFxn(UArg arg0, UArg arg1)
{
    RAMP_SVR_OBJECT* obj = (RAMP_SVR_OBJECT*)arg0;
#if 0
    UInt key;
    IPC_ELEM* elem;

    /* Begin the packet transmit task loop */

    while (TRUE)
    {
        /* Wait for a packet in the tx queue */
        if (!Semaphore_pend(g_svr.txDataSem, 1000))
        {
            /* Timeout, nothing to send */
            continue;
        }

        /* Get the message from txDataQue */
        elem = Queue_get(g_svr.txDataQue);

        /* Transmit the packet! */
        RAMP_TxFrame(g_svr.uartHandle, &(elem->fcb), &(elem->msg), sizeof(IPCMSG));

        /* Perform the enqueue and increment numFreeMsgs atomically */
        key = Hwi_disable();

        /* Put message buffer back on the free queue */
        Queue_enqueue(g_svr.txFreeQue, (Queue_Elem *)elem);

        /* Increment numFreeMsgs */
        g_svr.txNumFreeMsgs++;

        /* Increment total number of packets transmitted */
        g_svr.txCount++;

        /* re-enable ints */
        Hwi_restore(key);

        /* post the semaphore */
        Semaphore_post(g_svr.txFreeSem);
    }
#endif
}

//*****************************************************************************
// The reader task reads RAMP packets and stores these in the receive
// buffer queue for processing messages from the peer. The rxDataSem
// semaphore is signaled to indicate data is available to the IPCServer
// task that dispatches all the messages between the two peer nodes.
//*****************************************************************************

Void RAMPReaderTaskFxn(UArg arg0, UArg arg1)
{
    RAMP_SVR_OBJECT* obj = (RAMP_SVR_OBJECT*)arg0;
#if 0
    int rc;
    UInt key;
    IPC_ELEM* elem;

    /* Begin the packet receive task loop */

    while (TRUE)
    {
        /* Wait for a free receive buffer if necessary */
        if (!Semaphore_pend(g_svr.rxFreeSem, 1000))
        {
            /* See if any packets have not been ACK'ed
             * and re-send if necessary.
             */
            continue;
        }

        /* perform the dequeue and decrement numFreeMsgs atomically */
        key = Hwi_disable();

        /* get a rx buffer from the free queue */
        elem = Queue_dequeue(g_svr.rxFreeQue);

        /* Make sure that a valid pointer was returned. */
        if (elem == (IPC_ELEM*)(g_svr.rxFreeQue))
        {
            Hwi_restore(key);
            continue;
        }

        /* decrement the numFreeMsgs */
        g_svr.rxNumFreeMsgs--;

        /* re-enable ints */
        Hwi_restore(key);

        /* Buffer allocated, wait for a packet from peer */

        while (1)
        {
            /* Attempt to read a frame from the peer */

            rc = RAMP_RxFrame(g_svr.uartHandle, &(elem->fcb), &(elem->msg), sizeof(IPCMSG));

            /* Zero means packet received successfully */
            if (rc == 0)
                break;

            if (rc > ERR_TIMEOUT)
            {
                g_svr.rxErrors++;

                System_printf("RAMP_RxFrame Error %d\n", rc);
                System_flush();
            }
        }

        /* Packet received, save the sequence number received */
        g_svr.rxLastSeq = elem->fcb.seqnum;

        /* Increment the total packets received count */
        g_svr.rxCount++;

        /*Put message on rxDataQueue */
        if (elem->fcb.type & F_PRIORITY)
            Queue_putHead(g_svr.rxDataQue, (Queue_Elem*)elem);
        else
            Queue_put(g_svr.rxDataQue, (Queue_Elem*)elem);

        /* post the semaphore */
        Semaphore_post(g_svr.rxDataSem);
    }
#endif
}

//*****************************************************************************
//
//*****************************************************************************

Void RAMPWorkerTaskFxn(UArg arg0, UArg arg1)
{
    RAMP_SVR_OBJECT* obj = (RAMP_SVR_OBJECT*)arg0;

#if 0
    FCB fcb;
    IPCMSG msg;

    while (1)
    {
        /* Wait for a RAMP message from peer */

        if (!IPC_Message_pend(&msg, &fcb, 1000))
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
         *                  was processed and data returned in msg.
         */

        if ((fcb.type & FRAME_TYPE_MASK) == TYPE_MSG_ONLY)
        {
            if (fcb.type & F_DATAGRAM)
                IPC_Handle_datagram(&msg, &fcb);
            else
                IPC_Handle_transaction(&msg, &fcb, 1000);
        }
        else if ((fcb.type & FRAME_TYPE_MASK) == TYPE_MSG_ACK)
        {
            /* Handle MSG+ACK response from peer */

            uint8_t acknak = fcb.acknak;

            if ((acknak < MIN_SEQ_NUM) || (acknak > MAX_SEQ_NUM))
            {
                System_printf("IPC invalid ACK seqnum\n");
                System_flush();
                continue;
            }

            size_t index = (size_t)((acknak - 1) % MAX_WINDOW);

            /* Save the reply MSG+ACK in the ACK buffer */
            memcpy(&g_svr.ackBuf[index].msg, &msg, sizeof(IPCMSG));

            /* Notify any pending transactions blocked that a MSG+ACK was received */

            UInt mask = Event_Id_00 << index;

            Event_post(g_svr.ackEvent, mask);
        }
    }
#endif
}

// End-Of-File
