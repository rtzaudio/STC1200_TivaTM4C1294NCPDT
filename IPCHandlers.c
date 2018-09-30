/* ============================================================================
 *
 * DTC-1200 & STC-1200 Digital Transport Controllers for
 * Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2018, RTZ Professional Audio, LLC
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

#include <file.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include <driverlib/sysctl.h>
#include <IPCServer.h>

/* Board Header file */
#include "Board.h"
#include "CLITask.h"

/* External Data Items */

/* Global Data Items */

//*****************************************************************************
// This handler processes application specific datagram messages received
// from the peer. No response is required for datagrams.
//*****************************************************************************

Bool IPC_Handle_datagram(IPCMSG* msg, RAMP_FCB* fcb)
{
    //uint32_t param1 = msg->param1.U;

    if (msg->type != IPC_TYPE_NOTIFY)
        return FALSE;

    switch(msg->opcode)
    {
    case OP_NOTIFY_BUTTON:
#if 0
        CLI_printf("BUTTON: %02x ", msg->param1.U);
        if (param1 & S_STOP)
            CLI_printf("STOP");
        else if (param1 & S_PLAY)
            CLI_printf("PLAY");
        else if (param1 & S_REC)
            CLI_printf("REC");
        else if (param1 & S_REW)
            CLI_printf("REW");
        else if (param1 & S_FWD)
            CLI_printf("FWD");
        else if (param1 & S_LDEF)
            CLI_printf("LDEF");
        else if (param1 & S_TAPEOUT)
            CLI_printf("TAPE OUT");
        else if (param1 & S_TAPEIN)
            CLI_printf("TAPE IN");
        CLI_printf("\n");
#endif
        break;

    case OP_NOTIFY_TRANSPORT:
        CLI_printf("TRANSPORT: %02x\n", msg->param1.U);
        break;
    }

    return TRUE;
}

//*****************************************************************************
// This handler processes an incoming transaction request from a peer
// message received. It executes the command requested and returns the
// results in a MSG+ACK reply to indicate completion.
//*****************************************************************************

Bool IPC_Handle_transaction(IPCMSG* msg, RAMP_FCB* fcb, UInt32 timeout)
{
    RAMP_FCB fcbReply;
    IPCMSG msgReply;

    /* Copy incoming message to outgoing reply for default values */
    memcpy(&msgReply, msg, sizeof(IPCMSG));

    /* Execute the transaction type request */

    switch(msg->type)
    {
        /* The STC doesn't support any incoming transactions
         * from the DTC. In general, the DTC acts as a slave
         * to the DTC.
         */
    }

    /* Send the response MSG+ACK with command results returned */

    fcbReply.type    = MAKETYPE(F_ACKNAK, TYPE_MSG_ACK);
    fcbReply.acknak  = fcb->seqnum;
    fcbReply.address = fcb->address;
    fcbReply.seqnum  = IPC_GetTxSeqNum();

    return IPC_Message_post(&msgReply, &fcbReply, timeout);
}

// End-Of-File
