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
#include "RAMPServer.h"
#include "IPCServer.h"
#include "STC1200.h"

/* External Data Items */
extern SYSDATA g_sysData;

/* Static Function Prototypes */
static uint32_t xlate_to_dtc_transport_switch_mask(uint32_t mask);

//*****************************************************************************
// These handlers process messages received from the DRC wired remote.
//*****************************************************************************

void RAMP_Handle_datagram(RAMP_FCB* fcb, RAMP_MSG* msg)
{

}

void RAMP_Handle_message(RAMP_FCB* fcb, RAMP_MSG* msg)
{
    uint32_t mask;
    IPCMSG ipc;

    switch(msg->type)
    {
    case MSG_TYPE_SWITCH:
        if (msg->opcode == OP_SWITCH_PRESS)
        {
            /* Convert DRC switch bits to STC/DTC bit mask form */
            mask = xlate_to_dtc_transport_switch_mask(msg->param1.U);

            /* Send the transport button mask to the DTC */
            ipc.type     = IPC_TYPE_NOTIFY;
            ipc.opcode   = OP_NOTIFY_BUTTON;
            ipc.param1.U = (uint32_t)mask;
            ipc.param2.U = 0;

            IPC_Notify(&ipc, 0);
        }
        break;

    default:
        break;
    }
}

//*****************************************************************************
// This converts the DRC gpio switch mask to STC/DTC switch mask form
// for the transport controls. The gpio button order on the remote is
// different from the DTC transport control button assignments.
//*****************************************************************************

uint32_t xlate_to_dtc_transport_switch_mask(uint32_t mask)
{
    uint32_t bits = 0;

    if (mask & SW_REC)      /* REC button */
        bits |= S_REC;

    if (mask & SW_PLAY)     /* PLAY button */
        bits |= S_PLAY;

    if (mask & SW_REW)      /* REW button */
        bits |= S_REW;

    if (mask & SW_FWD)      /* FWD button */
        bits |= S_FWD;

    if (mask & SW_STOP)     /* STOP button */
        bits |= S_STOP;

    return bits;
}

// End-Of-File
