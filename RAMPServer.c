/***************************************************************************
 *
 * DTC-1200 & STC-1200 Digital Transport Controllers for
 * Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2018, RTZ Professional Audio, LLC
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
#include "RAMPServer.h"
#include "IPCServer.h"
#include "STC1200.h"

/* External Data Items */
extern SYSDATA g_sysData;

/* Static Function Prototypes */
static RAMP_SVR_OBJECT g_svr;

/* Static Function Prototypes */
static Void RAMPReaderTaskFxn(UArg a0, UArg a1);
static Void RAMPWriterTaskFxn(UArg arg0, UArg arg1);
static Void RAMPWorkerTaskFxn(UArg arg0, UArg arg1);
static RAMP_ACK* GetAckBuf(uint8_t acknak);

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

    /* 400 kbps or 10 Mbps baud rate */
    uint32_t baudRate = (GPIO_read(Board_DIPSW_CFG1) == 0) ? 400000 : 250000;

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
    uartParams.baudRate       = baudRate;
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
    taskParams.priority  = 8;
    taskParams.arg0      = (UArg)&g_svr;
    taskParams.arg1      = 0;
    Task_create((Task_FuncPtr)RAMPWriterTaskFxn, &taskParams, &eb);

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 700;
    taskParams.priority  = 8;
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
// Return pointer to ACK buffer based on the sequence number.
//*****************************************************************************

RAMP_ACK* GetAckBuf(uint8_t seqnum)
{
    size_t index = (size_t)((seqnum - 1) % MAX_WINDOW);

    return &g_svr.ackBuf[index];
}

//*****************************************************************************
// This function blocks until an IPC message is available in the rx queue or
// the timeout expires. A return FALSE value indicates the timeout expired
// or a buffer never became available for the receiver within the timeout
// period specified.
//*****************************************************************************

Bool RAMP_pend(RAMP_FCB *fcb, RAMP_MSG* msg, UInt32 timeout)
{
    UInt key;
    RAMP_ELEM* elem;

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
        memcpy(fcb, &(elem->fcb), sizeof(RAMP_FCB));
        memcpy(msg, &(elem->msg), sizeof(RAMP_MSG));

        /* post the semaphore */
        Semaphore_post(g_svr.rxFreeSem);

        return TRUE;
    }

    return FALSE;
}

//*****************************************************************************
// This function posts a message to the transmit queue. A return FALSE value
// indicates the timeout expired or a buffer never became available for
// transmission within the timeout period specified.
//*****************************************************************************

Bool RAMP_post(RAMP_FCB *fcb, RAMP_MSG* msg, UInt32 timeout)
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

        /* copy FCB & MSG to element */
        memcpy(&(elem->fcb), fcb, sizeof(RAMP_FCB));

        if (msg)
            memcpy(&(elem->msg), msg, sizeof(RAMP_MSG));
        else
            memset(&(elem->msg), 0, sizeof(RAMP_MSG));

        /* put message on txDataQueue */
        if (fcb->type & F_PRIORITY)
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
    UInt key;
    RAMP_ELEM* elem;
    void* textbuf;
    uint16_t textlen;

    //RAMP_SVR_OBJECT* obj = (RAMP_SVR_OBJECT*)arg0;

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
        if ((elem->fcb.type & FRAME_TYPE_MASK) == TYPE_MSG_USER)
        {
            textbuf = GrGetScreenBuffer();
            textlen = GrGetScreenBufferSize();
        }
        else
        {
            textbuf = &(elem->msg);
            textlen = sizeof(RAMP_MSG);
        }

        /* Transmit the packet frame out */
        RAMP_TxFrame(g_svr.uartHandle, &(elem->fcb), textbuf, textlen);

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
}

//*****************************************************************************
// The reader task reads RAMP packets and stores these in the receive
// buffer queue for processing messages from the peer. The rxDataSem
// semaphore is signaled to indicate data is available to the IPCServer
// task that dispatches all the messages between the two peer nodes.
//*****************************************************************************

Void RAMPReaderTaskFxn(UArg arg0, UArg arg1)
{
    int rc;
    UInt key;
    RAMP_ELEM* elem;

    //RAMP_SVR_OBJECT* obj = (RAMP_SVR_OBJECT*)arg0;

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
        if (elem == (RAMP_ELEM*)(g_svr.rxFreeQue))
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

            rc = RAMP_RxFrame(g_svr.uartHandle, &(elem->fcb), &(elem->msg), sizeof(RAMP_MSG));

            /* Zero means packet received successfully */
            if (rc == 0)
                break;

            if (rc > ERR_TIMEOUT)
            {
                g_svr.rxErrors++;

                System_printf("RAMP RxError %d\n", rc);
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
}

//*****************************************************************************
//
//*****************************************************************************

Void RAMPWorkerTaskFxn(UArg arg0, UArg arg1)
{
    RAMP_FCB fcb;
    RAMP_MSG msg;

    //RAMP_SVR_OBJECT* obj = (RAMP_SVR_OBJECT*)arg0;

    while (1)
    {
        /* Wait for a RAMP message from peer */

        if (!RAMP_pend(&fcb, &msg, 1000))
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
                RAMP_Handle_datagram(&fcb, &msg);
            else
                RAMP_Handle_message(&fcb, &msg);
        }
        else if ((fcb.type & FRAME_TYPE_MASK) == TYPE_MSG_USER)
        {
            /* User defined messages are used to indicate the
             * message contains a single large frame of display
             * buffer memory. In this case the OLED display data
             * is received directly into the display buffer.
             */

        }
        else if ((fcb.type & FRAME_TYPE_MASK) == TYPE_MSG_ACK)
        {
            uint8_t acknak = fcb.acknak;

            /* Handle ACK response from peer */
            if ((acknak < MIN_SEQ_NUM) || (acknak > MAX_SEQ_NUM))
            {
                System_printf("IPC invalid ACK seqnum\n");
                System_flush();
                continue;

            }

            RAMP_ACK* ack = GetAckBuf(acknak);

            /* Save the reply MSG+ACK in the ACK buffer */
            memcpy(&(ack->msg), &msg, sizeof(RAMP_MSG));

            ack->flags  = 0x01;
            ack->type   = fcb.type;
            ack->acknak = acknak;
            ack->retry  = 0;

            /* Notify any pending transactions blocked that a MSG+ACK was received */

            size_t index = (size_t)((acknak - 1) % MAX_WINDOW);

            UInt mask = Event_Id_00 << index;

            Event_post(g_svr.ackEvent, mask);
        }
    }
}

//*****************************************************************************
//
//*****************************************************************************

Bool RAMP_Send_Display(UInt32 timeout)
{
    RAMP_FCB fcb;

    fcb.type    = MAKETYPE(0, TYPE_MSG_USER);
    fcb.acknak  = 0;
    fcb.seqnum  = RAMP_GetTxSeqNum();
    fcb.address = 0;

    return RAMP_post(&fcb, NULL, timeout);
}

//*****************************************************************************
//
//*****************************************************************************

Bool RAMP_Send_Message(RAMP_MSG* msg, UInt32 timeout)
{
    RAMP_FCB fcb;

    fcb.type    = MAKETYPE(0, TYPE_MSG_ONLY);
    fcb.acknak  = 0;
    fcb.seqnum  = RAMP_GetTxSeqNum();
    fcb.address = 0;

    return RAMP_post(&fcb, msg, timeout);
}

//*****************************************************************************
//
//*****************************************************************************

Bool RAMP_Transaction(RAMP_MSG* txmsg, RAMP_MSG* rxmsg, UInt32 timeout)
{
    RAMP_FCB fcb;
    RAMP_ACK* ack;

    fcb.type    = MAKETYPE(F_ACKNAK, TYPE_MSG_ONLY);
    fcb.acknak  = 0;
    fcb.seqnum  = RAMP_GetTxSeqNum();
    fcb.address = 0;

    ack = GetAckBuf(fcb.seqnum);

    ack->flags  = 0x01;          /* ACK pending flag */
    ack->retry  = 5;
    ack->acknak = fcb.seqnum;
    ack->type   = fcb.type;

    /* post the message to the transmit queue. We use the
     * transmit sequence number as our unique identifier
     * in the received message to locate the corresponding
     * response packet when it's received later by the
     * reader task.
     */

    if (!RAMP_post(&fcb, txmsg, timeout))
    {
        /* ACK no longer pending */
        ack->flags = 0x00;
        return FALSE;
    }

    /* Now block until we timeout or the selected bit fires */
    UInt events = Event_pend(g_svr.ackEvent, Event_Id_NONE, 0xFFFF, timeout);

    if (events)
    {
        /* Return reply in the callers buffer */
        if (rxmsg)
            memcpy(rxmsg, &(ack->msg), sizeof(RAMP_MSG));

        /* ACK no longer pending */
        ack->flags = 0x00;
        return TRUE;
    }

    return FALSE;
}

// End-Of-File
