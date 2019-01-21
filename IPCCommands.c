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

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
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
#include <limits.h>

#include <stdint.h>
#include <stdbool.h>
#include <inc/hw_memmap.h>
#include <inc/hw_types.h>
#include <inc/hw_ints.h>
#include <inc/hw_gpio.h>

#include <driverlib/sysctl.h>
#include <driverlib/gpio.h>
#include <driverlib/sysctl.h>
#include <driverlib/qei.h>
#include <driverlib/pin_map.h>

#include "STC1200.h"
#include "Board.h"
#include "IPCServer.h"

/* External Data Items */

extern SYSDATA g_sysData;
extern Event_Handle g_eventQEI;
extern Mailbox_Handle g_mailboxLocate;

/* Static Function Prototypes */

#define IPC_TIMEOUT         2000

/*****************************************************************************
 * TRANSPORT COMMANDS TO DTC
 *****************************************************************************/

Bool Transport_Stop(void)
{
    IPC_MSG msg;

    msg.type     = IPC_TYPE_TRANSPORT;
    msg.opcode   = OP_MODE_STOP;
    msg.param1.U = 0;
    msg.param2.U = 0;

    return IPC_Notify(&msg, IPC_TIMEOUT);
}

Bool Transport_Play(uint32_t flags)
{
    IPC_MSG msg;

    msg.type     = IPC_TYPE_TRANSPORT;
    msg.opcode   = OP_MODE_PLAY;
    msg.param1.U = flags;
    msg.param2.U = 0;

    return IPC_Notify(&msg, IPC_TIMEOUT);
}

Bool Transport_Fwd(uint32_t velocity)
{
    IPC_MSG msg;

    msg.type     = IPC_TYPE_TRANSPORT;
    msg.opcode   = OP_MODE_FWD;
    msg.param1.U = velocity;
    msg.param2.U = 0;

    return IPC_Notify(&msg, IPC_TIMEOUT);
}

Bool Transport_Rew(uint32_t velocity)
{
    IPC_MSG msg;

    msg.type     = IPC_TYPE_TRANSPORT;
    msg.opcode   = OP_MODE_REW;
    msg.param1.U = velocity;
    msg.param2.U = 0;

    return IPC_Notify(&msg, IPC_TIMEOUT);
}

/*****************************************************************************
 * DTC-1200 TRANSPORT TRANSACTIONS
 *****************************************************************************/

Bool Transport_GetMode(uint32_t* mode, uint32_t* speed)
{
    IPC_MSG msgTx;
    IPC_MSG msgRx;

    msgTx.type     = IPC_TYPE_TRANSPORT;
    msgTx.opcode   = OP_TRANSPORT_GET_MODE;
    msgTx.param1.U = 0;
    msgTx.param2.U = 0;

    if (!IPC_Transaction(&msgTx, &msgRx, IPC_TIMEOUT))
        return FALSE;

    /* return current transport mode */
    *mode  = msgRx.param1.U;
    *speed = msgRx.param2.U;

    return TRUE;
}

/*****************************************************************************
 * DTC-1200 CONFIGURATION PARAMETERS
 *****************************************************************************/

Bool Config_SetShuttleVelocity(uint32_t velocity)
{
    IPC_MSG msgTx;
    IPC_MSG msgRx;

    msgTx.type     = IPC_TYPE_CONFIG;
    msgTx.opcode   = OP_SET_SHUTTLE_VELOCITY;
    msgTx.param1.U = velocity;
    msgTx.param2.U = 0;

    return IPC_Transaction(&msgTx, &msgRx, IPC_TIMEOUT);
}

Bool Config_GetShuttleVelocity(uint32_t* velocity)
{
    IPC_MSG msgTx;
    IPC_MSG msgRx;

    msgTx.type     = IPC_TYPE_CONFIG;
    msgTx.opcode   = OP_GET_SHUTTLE_VELOCITY;
    msgTx.param1.U = 0;
    msgTx.param2.U = 0;

    if (!IPC_Transaction(&msgTx, &msgRx, IPC_TIMEOUT))
        return FALSE;

    /* Return query results */
    *velocity = msgRx.param1.U;

    return TRUE;
}

/* End-Of-File */
