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

#include <file.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include <driverlib/sysctl.h>
/* Board Header file */
#include "STC1200.h"
#include "Board.h"
#include "IPCServer.h"
#include "RAMPServer.h"
#include "LocateTask.h"
#include "PositionTask.h"

/* External Data Items */
extern SYSDATA g_sysData;

/* Static Function Prototypes */
static uint32_t dtc_to_drc_lamp_mask(uint32_t bits);

//*****************************************************************************
// This handler processes an incoming transaction request from a peer
// message received. It executes the command requested and returns the
// results in a MSG+ACK reply to indicate completion with results.
//*****************************************************************************

Bool IPC_Handle_transaction(IPC_MSG* msg, IPC_FCB* fcb, UInt32 timeout)
{
    IPC_FCB fcbReply;
    IPC_MSG msgReply;

    /* Copy incoming message to outgoing reply for default values */
    memcpy(&msgReply, msg, sizeof(IPC_MSG));

    /* Execute the transaction type request */

    switch(msg->type)
    {
        /* The STC doesn't support any incoming transactions
         * from the DTC. In general, the DTC acts as a slave
         * to the STC.
         */
    }

    /* Send the response MSG+ACK with command results returned */

    fcbReply.type    = MAKETYPE(IPC_F_ACKNAK, IPC_MSG_ACK);
    fcbReply.acknak  = fcb->seqnum;
    fcbReply.rsvd    = fcb->rsvd;
    fcbReply.seqnum  = IPC_GetTxSeqNum();

    return IPC_Message_post(&msgReply, &fcbReply, timeout);
}

//*****************************************************************************
// This handler processes application specific datagram messages received
// from the peer DTC. No response is required for datagrams.
//*****************************************************************************

Bool IPC_Handle_datagram(IPC_MSG* msg, IPC_FCB* fcb)
{
    uint32_t param1;

    if (msg->type != IPC_TYPE_NOTIFY)
        return FALSE;

    switch(msg->opcode)
    {
    case OP_NOTIFY_BUTTON:
        param1 = msg->param1.U;
        if (param1 & S_STOP) {
            LocateCancel();
        } else if (param1 & S_PLAY) {
            LocateCancel();
        } else if (param1 & S_REW) {
            LocateCancel();
        } else if (param1 & S_FWD) {
            LocateCancel();
        } else if (param1 & S_LDEF) {
            LocateCancel();
        } else if (param1 & S_TAPEOUT) {
            LocateCancel();
        } else if (param1 & S_TAPEIN) {
            LocateCancel();
        }
        break;

    case OP_NOTIFY_LAMP:
        /* Update the current transport LED mask. The DTC also
         * sends the tape speed along with the LED/lamp mask.
         */
        g_sysData.ledMaskTransport = dtc_to_drc_lamp_mask(msg->param1.U);
        g_sysData.tapeSpeed = msg->param2.U;
        break;

    case OP_NOTIFY_TRANSPORT:
        /* The DTC sends this notification with the current
         * transport mode (stop, play, etc).
         */
        g_sysData.transportMode = msg->param1.U;
        break;
    }

    /* Signal the TCP worker a transport switch or LED state changed */
    Event_post(g_eventTransport, Event_Id_01);

    return TRUE;
}

//*****************************************************************************
// Convert lamp bits from DRC format to DTC format
//*****************************************************************************

/* DTC Lamp Driver Bits */
#define L_DTC_REC      0x01             // record indicator lamp
#define L_DTC_PLAY     0x02             // play indicator lamp
#define L_DTC_STOP     0x04             // stop indicator lamp
#define L_DTC_FWD      0x08             // forward indicator lamp
#define L_DTC_REW      0x10             // rewind indicator lamp

uint32_t dtc_to_drc_lamp_mask(uint32_t bits)
{
    uint32_t mask = 0;

    if (bits & L_DTC_REC)       /* DTC record lamp bit */
        mask |= L_REC;
    if (bits & L_DTC_PLAY)      /* DTC play lamp bit */
        mask |= L_PLAY;
    if (bits & L_DTC_STOP)      /* DTC stop lamp bit */
        mask |= L_STOP;
    if (bits & L_DTC_FWD)       /* DTC fwd lamp bit */
        mask |= L_FWD;
    if (bits & L_DTC_REW)       /* DTC rew lamp bit */
        mask |= L_REW;

    return mask;
}

// End-Of-File
