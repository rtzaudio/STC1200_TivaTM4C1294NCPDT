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

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Gate.h>

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
#include <ti/drivers/UART.h>

#include <file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h>

/* Graphiclib Header file */
#include <grlib/grlib.h>
#include <RemoteTask.h>
#include <RemoteTask.h>
#include "drivers/offscrmono.h"

/* PMX42 Board Header file */
#include "Board.h"
#include "STC1200.h"
#include "IPCServer.h"
#include "RAMPServer.h"
#include "CLITask.h"

/* Static Module Globals */
static uint32_t s_uScreenNum = 0;

/* External Global Data */
extern tContext g_context;
extern Mailbox_Handle g_mailboxRemote;
extern SYSDATA g_sysData;
extern SYSPARMS g_sysParms;

/* Static Function Prototypes */
static void HandleButtonPress(uint32_t flags);
static void HandleJogwheelPress(uint32_t flags);
static void HandleJogwheelMotion(uint32_t flags);

static Void RemoteTaskFxn(UArg arg0, UArg arg1);
static void HandleDigitPress(size_t index);
static void HandleSetMode(uint32_t mode);
//static void BlinkLocateButtonLED(size_t index);
void ResetDigitBuf(void);

//*****************************************************************************
// Initialize the remote display task
//*****************************************************************************

Bool Remote_Task_startup(void)
{
    Error_Block eb;
    Task_Params taskParams;

    Error_init(&eb);

    Task_Params_init(&taskParams);

    taskParams.stackSize = 1500;
    taskParams.priority  = 10;
    taskParams.arg0      = 0;
    taskParams.arg1      = 0;

    Task_create((Task_FuncPtr)RemoteTaskFxn, &taskParams, &eb);

    return TRUE;
}

//*****************************************************************************
// This converts the DRC GPIO transport control switch mask to DTC equivalent
// switch mask form for the transport controls. The transport button pin
// order on the remote is different from the DTC transport pin assignments.
//*****************************************************************************

uint32_t xlate_to_dtc_transport_switch_mask(uint32_t mask)
{
    uint32_t bits = 0;

    if (mask & SW_REC)      /* DRC REC button */
        bits |= S_REC;
    if (mask & SW_PLAY)     /* DRC PLAY button */
        bits |= S_PLAY;
    if (mask & SW_REW)      /* DRC REW button */
        bits |= S_REW;
    if (mask & SW_FWD)      /* DRC FWD button */
        bits |= S_FWD;
    if (mask & SW_STOP)     /* DRC STOP button */
        bits |= S_STOP;

    return bits;
}

void ResetDigitBuf(void)
{
    g_sysData.digitCount = 0;

    memset(&g_sysData.digitBuf, 0, sizeof(g_sysData.digitBuf));
}

//*****************************************************************************
// DRC-1200 Wired Remote Controller Task
//
// This task handles all communications to the DRC-1200 wired remote via
// via the RS-422 port on the STC-1200 card. All of the OLED display drawing
// functions draw into an in-memory display buffer that gets sent to the
// OLED display in the DRC-1200 remote. The DRC-1200 is basically a dumb
// terminal display device.
//
//*****************************************************************************

Void RemoteTaskFxn(UArg arg0, UArg arg1)
{
    IPC_MSG ipc;
    RAMP_MSG msg;

    g_sysData.currentMemIndex = 0;

    g_sysData.remoteMode = REMOTE_MODE_UNDEFINED;

    /* Initialize LOC-1 memory as return to zero at CUE point 1 */
    HandleSetMode(REMOTE_MODE_CUE);
    HandleDigitPress(0);

    ResetDigitBuf();

    if (!RAMP_Server_init()) {
        System_abort("RAMP server init failed");
    }

    while (TRUE)
    {
        /* Wait for a message up to 1 second */
        if (!Mailbox_pend(g_mailboxRemote, &msg, 100))
        {
            DrawScreen(s_uScreenNum);
            continue;
        }

        switch(msg.type)
        {
        case MSG_TYPE_DISPLAY:
            /* Refresh the OLED display with contents of display.
             * buffer.
             */
            //if (msg.opcode == OP_DISPLAY_REFRESH)
            //    DrawScreen(s_uScreenNum);
            break;

        case MSG_TYPE_SWITCH:
            /* A transport button (stop, play, etc)  or locator button
             * was pressed on the DRC remote unit. Convert the DRC transport
             * button bit mask to DTC format and send the button press event
             * to the DTC to execute the new transport mode requested.
             */
            if (msg.opcode == OP_SWITCH_TRANSPORT)
            {
                /* Convert DRC switch bits to DTC bit mask form */
                uint32_t mask = xlate_to_dtc_transport_switch_mask(msg.param1.U);

                /* Cancel any search in progress */
                if (LocateIsSearching())
                {
                    LocateAbort();
                    Task_sleep(250);
                }
                /* Send the transport command button mask to the DTC */
                ipc.type     = IPC_TYPE_NOTIFY;
                ipc.opcode   = OP_NOTIFY_BUTTON;
                ipc.param1.U = mask;
                ipc.param2.U = 0;

                IPC_Notify(&ipc, 0);
            }
            else if (msg.opcode == OP_SWITCH_REMOTE)
            {
                g_sysData.shiftAltButton = (msg.param1.U & SW_ALT) ? true : false;
                g_sysData.shiftRecButton = (msg.param2.U & SW_REC) ? true : false;

                HandleButtonPress(msg.param1.U);
            }
            else if (msg.opcode == OP_SWITCH_JOGWHEEL)
            {
                HandleJogwheelPress(msg.param1.U);
            }
            break;

        case MSG_TYPE_JOGWHEEL:
            if (msg.opcode == OP_JOGWHEEL_MOTION)
            {
                HandleJogwheelMotion(msg.param1.U);
            }
            break;

        default:
            break;
        }
    }
}

//*****************************************************************************
// Handle button press events from DRC remote
//*****************************************************************************

void HandleButtonPress(uint32_t flags)
{
    if (flags & SW_LOC1) {
        HandleDigitPress(0);
    } else if (flags & SW_LOC2) {
        HandleDigitPress(1);
    } else if (flags & SW_LOC3) {
        HandleDigitPress(2);
    } else if (flags & SW_LOC4) {
        HandleDigitPress(3);
    } else if (flags & SW_LOC5) {
        HandleDigitPress(4);
    } else if (flags & SW_LOC6) {
        HandleDigitPress(5);
    } else if (flags & SW_LOC7) {
        HandleDigitPress(6);
    } else if (flags & SW_LOC8) {
        HandleDigitPress(7);
    } else if (flags & SW_LOC9) {
        HandleDigitPress(8);
    } else if (flags & SW_LOC0) {
        HandleDigitPress(9);
    }
    else if (flags & SW_CUE)
    {
        HandleSetMode(REMOTE_MODE_CUE);
    }
    else if (flags & SW_STORE)
    {
        HandleSetMode(REMOTE_MODE_STORE);
    }
    else if (flags & SW_EDIT)
    {
        HandleSetMode(REMOTE_MODE_EDIT);
    }
    else if (flags & SW_MENU)
    {
        if (s_uScreenNum == SCREEN_MENU)
        {
            s_uScreenNum = SCREEN_TIME;
            SetButtonLedMask(0, L_MENU);
        }
        else
        {
            s_uScreenNum = SCREEN_MENU;
            SetButtonLedMask(L_MENU, 0);
        }
    }
    else if (flags & SW_AUTO)
    {
        /* toggle auto play mode */
        if (!g_sysData.autoMode)
        {
            g_sysData.autoMode = TRUE;
            SetButtonLedMask(L_AUTO, 0);
        }
        else
        {
            g_sysData.autoMode = FALSE;
            SetButtonLedMask(0, L_AUTO);
        }
    }
    else if (flags & SW_ALT)
    {
        if (g_sysParms.showLongTime)
            g_sysParms.showLongTime = false;
        else
            g_sysParms.showLongTime = true;
    }
}


void HandleJogwheelPress(uint32_t flags)
{
    uint32_t cue_flags = 0;

    size_t index = g_sysData.currentMemIndex;

    switch (g_sysData.remoteMode)
    {
    case REMOTE_MODE_CUE:

        SetLocateButtonLED(index);

        if (g_sysData.autoMode)
            cue_flags |= CF_PLAY;

        if (g_sysData.shiftRecButton)
            cue_flags |= CF_REC;

        /* Begin locate search */
        LocateSearch(index, cue_flags);
        break;

    case REMOTE_MODE_STORE:
        break;
    }
}


void HandleJogwheelMotion(uint32_t flags)
{

}

//*****************************************************************************
//
//*****************************************************************************

void HandleSetMode(uint32_t mode)
{
    if (mode == REMOTE_MODE_UNDEFINED)
    {
        /*
         * No mode active, reset everything
         */

        ResetDigitBuf();

        /* No mode active */
        SetButtonLedMask(0, L_CUE | L_STORE | L_EDIT);

        g_sysData.remoteMode = REMOTE_MODE_UNDEFINED;
    }
    else if (g_sysData.remoteMode == mode)
    {
        /*
         * Same mode requested, cancel the current mode
         */

        g_sysData.editState = EDIT_BEGIN;

        /* Set mode to undefined */
        g_sysData.remoteMode = REMOTE_MODE_UNDEFINED;

        /* Update the button LEDs */
        switch(mode)
        {
        case REMOTE_MODE_CUE:
            g_sysData.remoteModeLast = REMOTE_MODE_UNDEFINED;
            SetButtonLedMask(0, L_CUE);
            break;

        case REMOTE_MODE_STORE:
            g_sysData.remoteModeLast = REMOTE_MODE_UNDEFINED;
            SetButtonLedMask(0, L_STORE);
            break;

        case REMOTE_MODE_EDIT:
            g_sysData.remoteMode = g_sysData.remoteModeLast;
            SetButtonLedMask(0, L_EDIT);
            break;
        }

        SetLocateButtonLED(g_sysData.currentMemIndex);
    }
    else
    {
        /*
         * New mode requested, set LED's accordingly
         */

        /* Save the new mode */
        g_sysData.remoteMode = mode;

        /* Setup new mode requested */
        switch(mode)
        {
        case REMOTE_MODE_CUE:
            g_sysData.remoteModeLast = mode;
            SetButtonLedMask(L_CUE, L_CUE | L_STORE | L_EDIT);
            break;

        case REMOTE_MODE_STORE:
            g_sysData.remoteModeLast = mode;
            SetButtonLedMask(L_STORE, L_CUE | L_STORE | L_EDIT);
            break;

        case REMOTE_MODE_EDIT:
            /* Reset digit input buffer */
            ResetDigitBuf();
            /* Reset the edit time structure */
            g_sysData.editTime.hour  = 0;
            g_sysData.editTime.mins  = 0;
            g_sysData.editTime.secs  = 0;
            g_sysData.editTime.frame = 0;
            g_sysData.editTime.flags = F_PLUS;
            /* Begin at hour entry state */
            g_sysData.editState = EDIT_BEGIN;
            /* Set new button LED state */
            SetButtonLedMask(L_EDIT, L_CUE | L_STORE | L_EDIT);
            break;

        default:
            SetButtonLedMask(0, L_CUE | L_STORE | L_EDIT);
            break;
        }

        SetLocateButtonLED(g_sysData.currentMemIndex);
    }
}

//*****************************************************************************
//
//*****************************************************************************

void HandleDigitPress(size_t index)
{
    int n;
    char digit;
    static char digits[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9', '0' };

    if (g_sysData.remoteMode == REMOTE_MODE_CUE)
    {
        /*
         * Remote is in CUE mode and a LOC-# button was pressed
         */
        uint32_t flags = 0;

        g_sysData.currentMemIndex = index;

        SetLocateButtonLED(index);

        if (g_sysData.autoMode)
            flags |= CF_PLAY;

        if (g_sysData.shiftRecButton)
            flags |= CF_REC;

        /* Begin locate search */
        LocateSearch(index, flags);
    }
    else if (g_sysData.remoteMode == REMOTE_MODE_STORE)
    {
        /*
         * Remote is in STORE mode and a LOC-# button was pressed
         */
        g_sysData.currentMemIndex = index;

        SetLocateButtonLED(index);

        /* Store the current locate point */
        CuePointSet(index, g_sysData.tapePosition);
    }
    else if (g_sysData.remoteMode == REMOTE_MODE_EDIT)
    {
        /*
         * Remote is in EDIT mode and a digit 0-9 was pressed.
         */

        digit = digits[index];

        if (g_sysData.digitCount >= MAX_DIGITS_BUF)
            g_sysData.digitCount = 0;

        g_sysData.digitBuf[g_sysData.digitCount++] = digit;
        g_sysData.digitBuf[g_sysData.digitCount] = 0;

        switch(g_sysData.editState)
        {
        case EDIT_BEGIN:
            g_sysData.digitCount = 0;
            g_sysData.editState  = EDIT_MINUTES;
            g_sysData.editTime.flags = F_PLUS;
            g_sysData.editTime.hour  = (digit == '1') ? 1 : 0;
            break;

        case EDIT_MINUTES:
            n = atoi(g_sysData.digitBuf);

            /* validate mins value */
            if (n > 59)
                n = 59;

            g_sysData.editTime.mins = (uint8_t)n;

            if (g_sysData.digitCount > 1)
            {
                g_sysData.digitCount = 0;
                g_sysData.editState  = EDIT_SECONDS;
            }
            break;

        case EDIT_SECONDS:
            n = atoi(g_sysData.digitBuf);

            /* validate secs value */
            if (n > 59)
                n = 59;

            g_sysData.editTime.secs = (uint8_t)n;

            if (g_sysData.digitCount > 1)
            {
                g_sysData.digitCount = 0;
                g_sysData.editState  = EDIT_TENTHS;
            }
            break;

        case EDIT_TENTHS:
            g_sysData.editTime.tens = (uint8_t)atoi(g_sysData.digitBuf);
            g_sysData.digitCount = 0;
            g_sysData.editState  = EDIT_BEGIN;

            /* Exit EDIT mode back to previous state */
            HandleSetMode(REMOTE_MODE_EDIT);

            /* Convert H:MM:SS time to total seconds */
            int ipos;
            TapeTimeToPosition(&g_sysData.editTime, &ipos);

            /* Store the position at current memory index */
            CuePointSet(g_sysData.currentMemIndex, ipos);
            break;

        default:
            g_sysData.digitCount = 0;
            g_sysData.editState  = EDIT_BEGIN;
            break;
        }
    }
    else
    {
        g_sysData.currentMemIndex = index;

        SetLocateButtonLED(index);
    }
}

/*****************************************************************************
 * This functions set the appropriate LOC button LED bit flag on and
 * clears all other LOC button LED bits so only one LED is on.
 *****************************************************************************/

void SetLocateButtonLED(size_t index)
{
    uint32_t mask = 0;

    static uint32_t tab[10] = {
        L_LOC1, L_LOC2, L_LOC3, L_LOC4, L_LOC5,
        L_LOC6, L_LOC7, L_LOC8, L_LOC9, L_LOC0
    };

    mask = tab[index % 10];

    if (g_sysData.remoteMode == REMOTE_MODE_CUE)
        mask |= L_CUE;
    else if (g_sysData.remoteMode == REMOTE_MODE_STORE)
        mask |= L_STORE;
    else
        mask = 0;

    SetButtonLedMask(mask, L_LOC_MASK);
}

/*****************************************************************************
 * This function sets or clears button LED bits on the remote.
 *****************************************************************************/

void SetButtonLedMask(uint32_t setMask, uint32_t clearMask)
{
    /* Atomic change bits */
    uint32_t key = Hwi_disable();

    /* Clear any bits in the clear mask */
    g_sysData.ledMaskButton &= ~(clearMask);

    /* Set any bits in the set mask */
    g_sysData.ledMaskButton |= setMask;

    /* Restore interrupts */
    Hwi_restore(key);
}

// End-Of-File
