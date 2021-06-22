/* ============================================================================
 *
 * DTC-1200 & STC-1200 Digital Transport Controllers for
 * Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2020, RTZ Professional Audio, LLC
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
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Diags.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
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
#include <ti/sysbios/family/arm/m3/Hwi.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/SDSPI.h>
#include <ti/drivers/UART.h>

#include <file.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

/* STC1200 Board Header file */
#include "STC1200.h"
#include "Board.h"
#include "IPCFrame.h"
#include "STC1200TCP.h"
#include "TrackConfig.h"

/* Default AT45DB parameters structure */
const TRACK_Params TRACK_defaultParams = {
    0,   /* dummy */
};

static uint8_t s_seqnum = IPC_MIN_SEQ;

//*****************************************************************************
// Track Controller Construction/Destruction
//*****************************************************************************

TRACK_Handle TRACK_construct(TRACK_Object *obj, UART_Handle uartHandle,
                             TRACK_Params *params)
{
    /* Initialize the object's fields */
    obj->uartHandle = uartHandle;

    GateMutex_construct(&(obj->gate), NULL);

    return (TRACK_Handle)obj;
}

TRACK_Handle TRACK_create(UART_Handle uartHandle, TRACK_Params *params)
{
    TRACK_Handle handle;
    Error_Block eb;

    Error_init(&eb);

    handle = Memory_alloc(NULL, sizeof(TRACK_Object), NULL, &eb);

    if (handle == NULL)
        return NULL;

    handle = TRACK_construct(handle, uartHandle, params);

    return handle;
}

Void TRACK_delete(TRACK_Handle handle)
{
    TRACK_destruct(handle);

    Memory_free(NULL, handle, sizeof(TRACK_Object));
}

Void TRACK_destruct(TRACK_Handle handle)
{
    Assert_isTrue((handle != NULL), NULL);

    GateMutex_destruct(&(handle->gate));
}

Void TRACK_Params_init(TRACK_Params *params)
{
    Assert_isTrue(params != NULL, NULL);

    *params = TRACK_defaultParams;
}

//*****************************************************************************
// Issue a command to the DCS controller and get any reply.
//*****************************************************************************

int TRACK_Command(TRACK_Handle handle,
                  DCS_IPCMSG_HDR* request,
                  DCS_IPCMSG_HDR* reply)
{
    int rc;
    IPC_FCB rxFCB;
    IPC_FCB txFCB;
    IArg key;

    /* DIP switch 1 must be on to use track controller */
    if (GPIO_read(Board_DIPSW_CFG1) != 0)
        return IPC_ERR_TIMEOUT;

    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    /* Set message only op-code and message length */
    //msg.hdr.opcode = DCS_OP_SET_TRACKS;
   // msg.hdr.msglen = sizeof(DCS_IPCMSG_SET_TRACKS);

    /* Setup FCB for message only type frame */
    txFCB.type   = IPC_MAKETYPE(0, IPC_MSG_ONLY);
    txFCB.seqnum = s_seqnum;
    txFCB.acknak = 0;

    /* Send IPC command/data to track controller */
    rc = IPC_TxFrame(handle->uartHandle, &txFCB, request, request->msglen);

    if (rc == IPC_ERR_SUCCESS)
    {
        /* Try to read ack/nak response back */
        rc = IPC_RxFrame(handle->uartHandle, &rxFCB, reply, &(reply->msglen));

        if (rc == IPC_ERR_SUCCESS)
        {
            /* increment sequence number on reply */
            s_seqnum = IPC_INC_SEQ(s_seqnum);
        }
        else
        {
            System_printf("TRACK_Command() error %d\n", rc);
            System_flush();
        }
    }

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);

    return rc;
}

//*****************************************************************************
// Set speed to 15 or 30
//*****************************************************************************

bool Track_SetTapeSpeed(int speed)
{
    int rc;
    bool success = false;
    DCS_IPCMSG_SET_SPEED msg;

    msg.hdr.opcode = DCS_OP_SET_SPEED;
    msg.hdr.msglen = sizeof(DCS_IPCMSG_SET_SPEED);

    msg.tapeSpeed =  (speed >= 30) ? 1 : 0;

    /* mirror this here for debug */
    g_sys.tapeSpeed = speed;

    rc = TRACK_Command(g_sys.handleDCS,
                       (DCS_IPCMSG_HDR*)&msg,
                       (DCS_IPCMSG_HDR*)&msg);

    if (rc == IPC_ERR_SUCCESS)
    {
        success = true;
    }

    return success;
}

bool Track_GetCount(uint32_t* count)
{
    int rc;
    bool success = false;
    DCS_IPCMSG_GET_NUMTRACKS msg;

    *count = 0;

    msg.hdr.opcode = DCS_OP_GET_NUMTRACKS;
    msg.hdr.msglen = sizeof(DCS_IPCMSG_GET_NUMTRACKS);

    rc = TRACK_Command(g_sys.handleDCS,
                       (DCS_IPCMSG_HDR*)&msg,
                       (DCS_IPCMSG_HDR*)&msg);

    if (rc == IPC_ERR_SUCCESS)
    {
        *count = (uint32_t)msg.numTracks;
        success = true;
    }

    return success;
}

bool Track_ApplyState(size_t track, uint8_t state)
{
    int rc;
    bool success = false;
    DCS_IPCMSG_SET_TRACK msg;

    if (track >= DCS_NUM_TRACKS)
        return false;

    msg.hdr.opcode = DCS_OP_SET_TRACK;
    msg.hdr.msglen = sizeof(DCS_IPCMSG_SET_TRACK);

    msg.trackNum   = track;
    msg.trackState = state;

    rc = TRACK_Command(g_sys.handleDCS,
                       (DCS_IPCMSG_HDR*)&msg,
                       (DCS_IPCMSG_HDR*)&msg);

    if (rc == IPC_ERR_SUCCESS)
        success = true;

    return success;
}

bool Track_ApplyAllStates(uint8_t* trackStates)
{
    int rc;
    bool success = false;
    DCS_IPCMSG_SET_TRACKS msg;

    msg.hdr.opcode = DCS_OP_SET_TRACKS;
    msg.hdr.msglen = sizeof(DCS_IPCMSG_SET_TRACKS);

    memcpy(msg.trackState, trackStates, DCS_NUM_TRACKS);

    rc = TRACK_Command(g_sys.handleDCS,
                       (DCS_IPCMSG_HDR*)&msg,
                       (DCS_IPCMSG_HDR*)&msg);

    if (rc == IPC_ERR_SUCCESS)
        success = true;

    return success;
}

bool Track_GetState(size_t track, uint8_t* trackState)
{
    if (track >= MAX_TRACKS)
        return false;

    if (trackState)
        *trackState = g_sys.trackState[track];

    return true;
}

bool Track_SetState(size_t track, uint8_t trackState)
{
    if (track >= MAX_TRACKS)
        return false;

    g_sys.trackState[track] = trackState;

    Track_ApplyState(track, trackState);

    return true;
}

bool Track_SetAll(uint8_t mode, uint8_t flags)
{
    size_t i;

    for (i=0; i < MAX_TRACKS; i++)
        g_sys.trackState[i] = (mode & STC_TRACK_MASK) | flags;

    /* Update DCS channel switcher states */
    Track_ApplyAllStates(g_sys.trackState);

    return true;
}

bool Track_SetModeAll(uint8_t mode)
{
    size_t i;
    uint8_t mask;

    for (i=0; i < MAX_TRACKS; i++)
    {
        mask = g_sys.trackState[i] & ~(STC_TRACK_MASK);
        /* Store the new track flags, mode preserved */
        g_sys.trackState[i] = (mode & STC_TRACK_MASK) | mask;
    }

    /* Update DCS channel switcher states */
    Track_ApplyAllStates(g_sys.trackState);

    return true;
}

bool Track_MaskAll(uint8_t setmask, uint8_t clearmask)
{
    size_t i;
    uint8_t mode;
    uint8_t mask;

    for (i=0; i < MAX_TRACKS; i++)
    {
        mode = g_sys.trackState[i] & STC_TRACK_MASK;
        mask = g_sys.trackState[i] & ~(STC_TRACK_MASK);
        /* Clear any bits in the clear mask */
        mask &= ~(clearmask);
        /* Set any bits in the set mask */
        mask |= setmask;
        /* Store the new track flags, mode preserved */
        g_sys.trackState[i] = mode | mask;
    }

    /* Update DCS channel switcher states */
    Track_ApplyAllStates(g_sys.trackState);

    return true;
}

bool Track_ToggleMaskAll(uint8_t flags)
{
    size_t i;
    uint8_t mode;
    uint8_t mask;

    for (i=0; i < MAX_TRACKS; i++)
    {
        mode = g_sys.trackState[i] & STC_TRACK_MASK;
        mask = g_sys.trackState[i] ^ flags;
        /* Store the new track flags, mode preserved */
        g_sys.trackState[i] = mode | (mask & ~(STC_TRACK_MASK));
    }

    /* Update DCS channel switcher states */
    Track_ApplyAllStates(g_sys.trackState);

    return true;
}

/* End-Of-File */
