/* ============================================================================
 *
 * DTC-1200 Digital Transport Controller for Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016, RTZ Professional Audio, LLC
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
 * ============================================================================ */

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
#include <ti/sysbios/gates/GateMutex.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/SDSPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>

/* Tivaware Driver files */
#include <driverlib/eeprom.h>
#include <driverlib/fpu.h>

/* Generic Includes */
#include <file.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* XDCtools Header files */
#include "STC1200.h"
#include "Board.h"
#include "IPCToDTC.h"
#include "..\DTC1200_TivaTM4C123AE6PM\IPCCMD_DTC1200.h"

//*****************************************************************************
// This task handles IPC messages from the STC via Board_UART_IPC_B.
//*****************************************************************************

IPCCMD_Handle IPCToDTC_Open()
{
    UART_Handle uartHandle;
    UART_Params uartParams;
    IPCCMD_Handle ipcHandle;
    IPCCMD_Params ipcParams;

    /* Open the UART for binary mode */

    UART_Params_init(&uartParams);

    /* RS-232 port-B 115200,N,8,1 with 2-sec read timeout */
    uartParams.readMode       = UART_MODE_BLOCKING;
    uartParams.writeMode      = UART_MODE_BLOCKING;
    uartParams.readTimeout    = 2000;
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

    uartHandle = UART_open(Board_UART_IPC_B, &uartParams);

    if (uartHandle == NULL)
        return NULL;

    /* Create the IPC command object over UART handle */

    IPCCMD_Params_init(&ipcParams);
    ipcParams.uartHandle = uartHandle;

    ipcHandle = IPCCMD_create(&ipcParams);

    if (ipcHandle == NULL)
        UART_close(uartHandle);

    return ipcHandle;
}

/* This reads all DTC configuration parameters currently in memory
 * and fills all values into the structure pointed to by 'cfg'.
 */

int IPCToDTC_ConfigGet(IPCCMD_Handle handle, DTC_CONFIG_DATA* cfg)
{
    int rc;
    IPCMSG_HDR request;
    DTC_IPCMSG_CONFIG_GET reply;

    request.opcode = DTC_OP_CONFIG_GET;
    request.msglen = sizeof(IPCMSG_HDR);

    reply.hdr.msglen = sizeof(DTC_IPCMSG_CONFIG_GET);

    rc = IPCCMD_Transaction(handle, &request, &reply.hdr);

    if (rc == IPC_ERR_SUCCESS)
    {
        memcpy(cfg, &reply.cfg, sizeof(DTC_CONFIG_DATA));
    }

    return rc;
}

/* This sets all DTC configuration parameters to the values passed in
 * the structure 'cfg'. All runtime parameters in memory are overwritten.
 */
int IPCToDTC_ConfigSet(IPCCMD_Handle handle, DTC_CONFIG_DATA* cfg)
{
    int rc;
    IPCMSG_HDR reply;
    DTC_IPCMSG_CONFIG_SET request;

    request.hdr.opcode = DTC_OP_CONFIG_SET;
    request.hdr.msglen = sizeof(DTC_IPCMSG_CONFIG_SET);

    memcpy(&request.cfg, cfg, sizeof(DTC_CONFIG_DATA));

    reply.msglen = sizeof(IPCMSG_HDR);

    rc = IPCCMD_Transaction(handle, &request.hdr, &reply);

    return rc;
}

/*  0 = recall DTC config from EPROM to memory
 *  1 = store DTC config in memory to EPROM
 *  2 = reset DTC config data to defaults
 */
int IPCToDTC_ConfigEPROM(IPCCMD_Handle handle, int store)
{
    int rc;
    DTC_IPCMSG_CONFIG_EPROM msg;

    msg.hdr.opcode = DTC_OP_CONFIG_EPROM;
    msg.hdr.msglen = sizeof(DTC_IPCMSG_CONFIG_EPROM);
    msg.store = store;      /* 0=recall, 1=store, 2=reset */
    msg.status = 0;

    rc = IPCCMD_Transaction(handle, &msg.hdr, &msg.hdr);

    return rc;
}

/* End-Of-File */

