/***************************************************************************
 *
 * DTC-1200 & STC-1200 Digital Transport Controllers for
 * Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2020, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 ***************************************************************************
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
 *
 ***************************************************************************/

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
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/gates/GateMutex.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/SDSPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>

#include <driverlib/sysctl.h>

#include <file.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "IPCServer.h"
#include "Board.h"

/* Global Data Items */
IPCSVR_OBJECT g_ipc;

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

    /* Create the queues needed */
    g_ipc.txFreeQue = Queue_create(NULL, NULL);
    g_ipc.txDataQue = Queue_create(NULL, NULL);
    g_ipc.rxFreeQue = Queue_create(NULL, NULL);
    g_ipc.rxDataQue = Queue_create(NULL, NULL);

    /* Create semaphores needed */
    g_ipc.txFreeSem = Semaphore_create(IPC_MAX_WINDOW, NULL, NULL);
    g_ipc.txDataSem = Semaphore_create(0, NULL, NULL);
    g_ipc.rxFreeSem = Semaphore_create(IPC_MAX_WINDOW, NULL, NULL);
    g_ipc.rxDataSem = Semaphore_create(0, NULL, NULL);

    Error_init(&eb);
    g_ipc.ackEvent  = Event_create(NULL, NULL);

    //g_ipc.datagramHandlerFxn    = NULL;
    //g_ipc.transactionHandlerFxn = NULL;

    /*
     * Allocate and Initialize TRANSMIT Buffer Memory
     */

    Error_init(&eb);

    g_ipc.txBuf = (IPC_ELEM*)Memory_alloc(NULL, sizeof(IPC_ELEM) * IPC_MAX_WINDOW, 0, &eb);

    if (g_ipc.txBuf == NULL)
        System_abort("TxBuf allocation failed");

    msg = g_ipc.txBuf;

    /* Put all tx message buffers on the freeQueue */
    for (i=0; i < IPC_MAX_WINDOW; i++, msg++) {
        Queue_enqueue(g_ipc.txFreeQue, (Queue_Elem*)msg);
    }

    /*
     * Allocate and Initialize RECEIVE Buffer Memory
     */

    Error_init(&eb);

    g_ipc.rxBuf = (IPC_ELEM*)Memory_alloc(NULL, sizeof(IPC_ELEM) * IPC_MAX_WINDOW, 0, &eb);

    if (g_ipc.rxBuf == NULL)
        System_abort("RxBuf allocation failed");

    msg = g_ipc.rxBuf;

    /* Put all tx message buffers on the freeQueue */
    for (i=0; i < IPC_MAX_WINDOW; i++, msg++) {
        Queue_enqueue(g_ipc.rxFreeQue, (Queue_Elem*)msg);
    }

    /*
     * Allocate and ACK RECEIVE Buffer Memory
     */

    Error_init(&eb);

    g_ipc.ackBuf = (IPC_ACK*)Memory_alloc(NULL, sizeof(IPC_ACK) * IPC_MAX_WINDOW, 0, &eb);

    if (g_ipc.ackBuf == NULL)
        System_abort("AckBuf allocation failed");

    /* Initialize Server Data Items */

    g_ipc.txErrors      = 0;
    g_ipc.txCount       = 0;
    g_ipc.txNumFreeMsgs = IPC_MAX_WINDOW;
    g_ipc.txNextSeq     = IPC_MIN_SEQ;      /* current tx sequence# */

    g_ipc.rxErrors      = 0;
    g_ipc.rxCount       = 0;
    g_ipc.rxNumFreeMsgs = IPC_MAX_WINDOW;
    g_ipc.rxLastSeq     = 0;                /* last seq# accepted   */
    g_ipc.rxExpectedSeq = IPC_MIN_SEQ;      /* expected recv seq#   */

    return TRUE;
}


Bool IPC_Server_startup(void)
{
    Error_Block eb;
    Task_Params taskParams;

    UART_Params uartParams;

    /* Open the UART for binary mode */

    UART_Params_init(&uartParams);

    uartParams.readMode       = UART_MODE_BLOCKING;
    uartParams.writeMode      = UART_MODE_BLOCKING;
    uartParams.readTimeout    = 2000;                   // 1 second read timeout
    uartParams.writeTimeout   = BIOS_WAIT_FOREVER;
    uartParams.readCallback   = NULL;
    uartParams.writeCallback  = NULL;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.writeDataMode  = UART_DATA_BINARY;
    uartParams.readDataMode   = UART_DATA_BINARY;
    uartParams.readEcho       = UART_ECHO_OFF;
    uartParams.baudRate       = 250000;
    uartParams.stopBits       = UART_STOP_ONE;
    uartParams.parityType     = UART_PAR_NONE;

    g_ipc.uartHandle = UART_open(Board_UART_IPC_A, &uartParams);

    if (g_ipc.uartHandle == NULL)
        System_abort("Error initializing UART\n");

    /*
     * Finally, create the reader, writer and worker tasks
     */

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 800;
    taskParams.priority  = 6;

    if (!Task_create((Task_FuncPtr)IPCWriterTaskFxn, &taskParams, &eb))
        System_abort("IPC Task create failed\n");

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 800;
    taskParams.priority  = 6;

    if (!Task_create((Task_FuncPtr)IPCReaderTaskFxn, &taskParams, &eb))
        System_abort("IPC Task create failed\n");

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 800;
    taskParams.priority  = 10;

    if (!Task_create((Task_FuncPtr)IPCWorkerTaskFxn, &taskParams, &eb))
        System_abort("IPC Task create failed\n");

    return TRUE;
}

//*****************************************************************************
// This function returns the next available transmit and increments the
// counter to the next frame sequence number.
//*****************************************************************************

uint8_t IPC_GetTxSeqNum(void)
{
    /* increment sequence number atomically */
    UInt key = Hwi_disable();

    /* Get the next frame sequence number */
    uint8_t seqnum = g_ipc.txNextSeq;

    /* Increment the servers sequence number */
    g_ipc.txNextSeq = IPC_INC_SEQ(seqnum);

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

Bool IPC_Message_pend(IPC_MSG* msg, IPC_FCB* fcb, UInt32 timeout)
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
        memcpy(msg, &(elem->msg), sizeof(IPC_MSG));
        memcpy(fcb, &(elem->fcb), sizeof(IPC_FCB));

        /* post the semaphore */
        Semaphore_post(g_ipc.rxFreeSem);

        return TRUE;
    }

    return FALSE;
}

//*****************************************************************************
// This function posts a message to the transmit queue. A return FALSE value
// indicates the timeout expired or a buffer never became available for
// transmission within the timeout period specified.
//*****************************************************************************

Bool IPC_Message_post(IPC_MSG* msg, IPC_FCB* fcb, UInt32 timeout)
{
    UInt key;
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

        /* decrement the numFreeMsgs */
        g_ipc.txNumFreeMsgs--;

        /* re-enable ints */
        Hwi_restore(key);

        /* copy msg to element */
        memcpy(&(elem->msg), msg, sizeof(IPC_MSG));
        memcpy(&(elem->fcb), fcb, sizeof(IPC_FCB));

        /* put message on txDataQueue */
        if (fcb->type & IPC_F_PRIORITY)
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
// This packet writer task waits for any message to appear in the
// outgoing transmit message queue and transmits all items from the queue.
//*****************************************************************************

Void IPCWriterTaskFxn(UArg arg0, UArg arg1)
{
    UInt key;
    IPC_ELEM* elem;

    /* Begin the packet transmit task loop */

    while (TRUE)
    {
        /* Wait for a packet in the tx queue */
        Semaphore_pend(g_ipc.txDataSem, BIOS_WAIT_FOREVER);

        /* Get the message from txDataQue */
        elem = Queue_get(g_ipc.txDataQue);

        /* Transmit the packet! */

        IPC_FrameTx(g_ipc.uartHandle, &(elem->fcb), &(elem->msg), sizeof(IPC_MSG));

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
// The reader task reads IPC packets and stores these in the receive
// buffer queue for processing messages from the peer. The rxDataSem
// semaphore is signaled to indicate data is available to the IPCServer
// task that dispatches all the messages between the two peer nodes.
//*****************************************************************************

Void IPCReaderTaskFxn(UArg arg0, UArg arg1)
{
    int rc;
    uint16_t rxlen;
    UInt key;
    IPC_ELEM* elem;

    /* Begin the packet receive task loop */

    while (TRUE)
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
            rxlen = sizeof(IPC_MSG);
            rc = IPC_FrameRx(g_ipc.uartHandle, &(elem->fcb), &(elem->msg), &rxlen);

            /* Zero means packet received successfully */
            if (rc == 0)
                break;

            if (rc > IPC_ERR_TIMEOUT)
            {
                g_ipc.rxErrors++;

                System_printf("IPC RxError %d\n", rc);
                System_flush();
            }
        }

        /* Packet received, save the sequence number received */
        g_ipc.rxLastSeq = elem->fcb.seqnum;

        /* Increment the total packets received count */
        g_ipc.rxCount++;

        /*Put message on rxDataQueue */
        if (elem->fcb.type & IPC_F_PRIORITY)
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

Void IPCWorkerTaskFxn(UArg arg0, UArg arg1)
{
    IPC_FCB fcb;
    IPC_MSG msg;

    while (1)
    {
        /* Wait for a IPC message from peer */

        if (!IPC_Message_pend(&msg, &fcb, 1000))
        {
            /* Timeout, no message received, check to see if we have
             * any messages with ACK pending that haven't been
             * acknowledge yet. If so, then retransmit until
             * for max number of retries.
             */
            continue;
        }

        /* We've received a valid IPC message frame. Check the
         * message type received as follows:
         *
         *  IPC_MSG_ONLY - This is a request for data that requires
         *                 a response message with ACK to the peer.
         *                 If the F_DATAGRAM type flag is set, the
         *                 message does not require an ACK response
         *                 and the message data can be processed.
         *
         *  IPC_MSG_ACK  - This is a response to a request for data
         *                 from peer. The ACK indicates the request
         *                 was processed and data returned in msg.
         */

        if ((fcb.type & IPC_TYPE_MASK) == IPC_MSG_ONLY)
        {
            if (fcb.type & IPC_F_DATAGRAM)
                IPC_Handle_datagram(&msg, &fcb);
            else
                IPC_Handle_transaction(&msg, &fcb, 2000);
        }
        else if ((fcb.type & IPC_TYPE_MASK) == IPC_MSG_ACK)
        {
            /* Handle MSG+ACK response from peer */

            uint8_t acknak = fcb.acknak;

            if ((acknak < IPC_MIN_SEQ) || (acknak > IPC_MAX_SEQ))
            {
                System_printf("IPC invalid ACK seqnum\n");
                System_flush();
                continue;
            }

            size_t index = (size_t)((acknak - 1) % IPC_MAX_WINDOW);

            /* Save the reply MSG+ACK in the ACK buffer */
            memcpy(&g_ipc.ackBuf[index].msg, &msg, sizeof(IPC_MSG));

            /* Notify any pending transactions blocked that a MSG+ACK was received */

            UInt mask = Event_Id_00 << index;

            Event_post(g_ipc.ackEvent, mask);
        }
    }
}

//*****************************************************************************
//
//*****************************************************************************

Bool IPC_Notify(IPC_MSG* msg, UInt32 timeout)
{
    IPC_FCB fcb;

    fcb.type    = IPC_MAKETYPE(IPC_F_DATAGRAM, IPC_MSG_ONLY);
    fcb.acknak  = 0;
    fcb.seqnum  = 0;
    fcb.rsvd    = 0;

    return IPC_Message_post(msg, &fcb, timeout);
}

//*****************************************************************************
//
//*****************************************************************************

Bool IPC_Transaction(IPC_MSG* msgTx, IPC_MSG* msgRx, UInt32 timeout)
{
    IPC_FCB fcb;

    memset(msgRx, 0, sizeof(IPC_MSG));

    fcb.type    = IPC_MAKETYPE(IPC_F_ACKNAK, IPC_MSG_ONLY);
    fcb.acknak  = 0;
    fcb.seqnum  = IPC_GetTxSeqNum();
    fcb.rsvd    = 0;

    size_t index = (fcb.seqnum - 1) % IPC_MAX_WINDOW;

    g_ipc.ackBuf[index].flags  = 0x01;          /* ACK pending flag */
    g_ipc.ackBuf[index].retry  = 5;
    g_ipc.ackBuf[index].acknak = fcb.seqnum;
    g_ipc.ackBuf[index].type   = fcb.type;

    /* post the message to the transmit queue. We use the
     * transmit sequence number as our unique identifier
     * in the received message to locate the corresponding
     * response packet when it's received later by the
     * reader task.
     */

    if (!IPC_Message_post(msgTx, &fcb, timeout))
    {
        /* ACK no longer pending */
        g_ipc.ackBuf[index].flags = 0x00;
        return FALSE;
    }

    /* Now block until we timeout or the selected bit fires */
    UInt events = Event_pend(g_ipc.ackEvent, Event_Id_NONE, 0xFFFF, timeout);

    if (events)
    {
        /* ACK no longer pending */
        g_ipc.ackBuf[index].flags = 0x00;

        /* Return reply in the callers buffer */
        if (msgRx)
        {
            msgRx->type   = g_ipc.ackBuf[index].msg.type;
            msgRx->opcode = g_ipc.ackBuf[index].msg.opcode;
            msgRx->param1 = g_ipc.ackBuf[index].msg.param1;
            msgRx->param2 = g_ipc.ackBuf[index].msg.param2;
        }

        return TRUE;
    }

    return FALSE;
}

// End-Of-File
