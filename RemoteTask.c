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
#include "AD9837.h"
#include "STC1200.h"
#include "IPCServer.h"
#include "RAMPServer.h"
#include "CLITask.h"

/* Static Module Globals */
static uint32_t s_uScreenNum = 0;

/* External Global Data */
extern tContext g_context;
extern Mailbox_Handle g_mailboxRemote;
extern SYSDAT g_sys;
extern SYSCFG g_cfg;

/* Static Function Prototypes */
static void HandleButtonPress(uint32_t mask, uint32_t cue_flags);
static void HandleDigitPress(size_t index, uint32_t cue_flags);
static void HandleJogwheelPress(uint32_t flags);
static void HandleJogwheelMotion(uint32_t velocity, int direction);
static Void RemoteTaskFxn(UArg arg0, UArg arg1);
static void RemoteSetMode(uint32_t mode);
static void ResetDigitBuf(void);
static int StrToTapeTime(char *digits, TAPETIME* tapetime);
static void CompleteEditTimeState();

//*****************************************************************************
// Initialize the remote display task
//*****************************************************************************

Bool Remote_Task_startup(void)
{
    Error_Block eb;
    Task_Params taskParams;

    Error_init(&eb);

    Task_Params_init(&taskParams);

    taskParams.stackSize = 2048;
    taskParams.priority  = 10;
    taskParams.arg0      = 0;
    taskParams.arg1      = 0;

    Task_create((Task_FuncPtr)RemoteTaskFxn, &taskParams, &eb);

    return TRUE;
}

void Remote_PostSwitchPress(uint32_t mode, uint32_t flags)
{
    RAMP_MSG msg;

    msg.type   = MSG_TYPE_SWITCH;
    msg.opcode = OP_SWITCH_REMOTE;

    msg.param1.U = mode;
    msg.param2.U = flags;

    Mailbox_post(g_mailboxRemote, &msg, 100);
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
    g_sys.digitCount = 0;

    memset(&g_sys.digitBuf, 0, sizeof(g_sys.digitBuf));
}

/*****************************************************************************
 * This function set the appropriate LOC button LED bit flag on and clears
 * all other LOC button LED bits so only one LOC LED is on at a time.
 *****************************************************************************/

void SetLocateButtonLED(size_t index)
{
    uint32_t mask = 0;

    static uint32_t tab[10] = {
        L_LOC0, L_LOC1, L_LOC2, L_LOC3, L_LOC4,
        L_LOC5, L_LOC6, L_LOC7, L_LOC8, L_LOC9
    };

    mask = tab[index % 10];

    if (g_sys.remoteMode == REMOTE_MODE_CUE)
        mask |= L_CUE;
    else if (g_sys.remoteMode == REMOTE_MODE_STORE)
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
    g_sys.ledMaskRemote &= ~(clearMask);

    /* Set any bits in the set mask */
    g_sys.ledMaskRemote |= setMask;

    /* Restore interrupts */
    Hwi_restore(key);
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
    uint32_t cue_flags;

    g_sys.cueIndex = 0;

    g_sys.remoteMode = REMOTE_MODE_UNDEFINED;

    /* Initialize LOC-1 memory as return to zero at CUE point 1 */
    g_sys.remoteModePrev = REMOTE_MODE_CUE;
    RemoteSetMode(REMOTE_MODE_CUE);
    HandleDigitPress(0, 0);

    ResetDigitBuf();

    if (!RAMP_Server_init()) {
        System_abort("RAMP server init failed");
    }

    while (TRUE)
    {
        /* Wait for a message up to 1 second */
        if (!Mailbox_pend(g_mailboxRemote, &msg, 100))
        {
            /* DIP switch #2 must be on to enable tx data to remote */
            if (GPIO_read(Board_DIPSW_CFG2) == 0)
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

                /* Cancel any locate/loop in progress */
                if (IsLocating())
                    LocateCancel();

                /* Send the transport command button mask to the DTC */
                ipc.type     = IPC_TYPE_NOTIFY;
                ipc.opcode   = OP_NOTIFY_BUTTON;
                ipc.param1.U = mask;
                ipc.param2.U = 0;

                IPC_Notify(&ipc, 0);
            }
            else if (msg.opcode == OP_SWITCH_REMOTE)
            {
                /* A locate or other mode button was pressed */
                g_sys.shiftAltButton = (msg.param1.U & SW_ALT) ? true : false;
                g_sys.shiftRecButton = (msg.param2.U & SW_REC) ? true : false;

                cue_flags = 0;

                if (g_sys.autoMode)
                    cue_flags |= CF_AUTO_PLAY;

                if (g_sys.shiftRecButton)
                    cue_flags |= CF_AUTO_REC;

                HandleButtonPress(msg.param1.U, cue_flags);

                /* Signal the TCP worker thread switch press event */
                Event_post(g_eventTransport, Event_Id_02);
            }
            else if (msg.opcode == OP_SWITCH_JOGWHEEL)
            {
                /* Jog wheel switch button was pressed */
                HandleJogwheelPress(msg.param1.U);
            }
            break;

        case MSG_TYPE_JOGWHEEL:
            if (msg.opcode == OP_JOGWHEEL_MOTION)
            {
                /* Jog wheel was turned */
                HandleJogwheelMotion(msg.param1.U, msg.param2.I);
            }
            break;

        default:
            break;
        }
    }
}

//*****************************************************************************
// This function sets the current remote operation mode.
//*****************************************************************************

void RemoteSetMode(uint32_t mode)
{
    if (mode == REMOTE_MODE_UNDEFINED)
    {
        /*
         * No mode active, reset everything
         */

        ResetDigitBuf();

        /* No mode active */
        SetButtonLedMask(0, L_CUE | L_STORE | L_EDIT);

        g_sys.remoteModePrev = g_sys.remoteMode;

        g_sys.remoteMode = REMOTE_MODE_UNDEFINED;
    }
    else if (g_sys.remoteMode == mode)
    {
        /*
         * Same mode requested, cancel the current mode
         */

        g_sys.editState = EDIT_BEGIN;

        ResetDigitBuf();

        /* Set mode to undefined */
        g_sys.remoteMode = REMOTE_MODE_UNDEFINED;

        /* Update the button LEDs */
        switch(mode)
        {
        case REMOTE_MODE_CUE:
            g_sys.remoteModeLast = REMOTE_MODE_UNDEFINED;
            SetButtonLedMask(0, L_CUE);
            break;

        case REMOTE_MODE_STORE:
            g_sys.remoteMode = g_sys.remoteModePrev;
            SetButtonLedMask(0, L_STORE);
            break;

        case REMOTE_MODE_EDIT:
            g_sys.remoteMode = g_sys.remoteModeLast;
            SetButtonLedMask(0, L_EDIT);
            break;
        }

        SetLocateButtonLED(g_sys.cueIndex);
    }
    else
    {
        /*
         * New mode requested, set LED's accordingly
         */

        g_sys.remoteModePrev = g_sys.remoteMode;

        /* Save the new mode */
        g_sys.remoteMode = mode;

        /* Setup new mode requested */
        switch(mode)
        {
        case REMOTE_MODE_CUE:
            g_sys.remoteModeLast = mode;
            SetButtonLedMask(L_CUE, L_CUE | L_STORE | L_EDIT);
            break;

        case REMOTE_MODE_STORE:
            g_sys.remoteModeLast = mode;
            SetButtonLedMask(L_STORE, L_CUE | L_STORE | L_EDIT);
            break;

        case REMOTE_MODE_EDIT:
            /* Reset digit input buffer */
            ResetDigitBuf();
            /* Reset the edit time structure */
            g_sys.editTime.hour  = 0;
            g_sys.editTime.mins  = 0;
            g_sys.editTime.secs  = 0;
            g_sys.editTime.frame = 0;
            g_sys.editTime.flags = F_PLUS;
            /* Begin at hour entry state */
            g_sys.editState = EDIT_BEGIN;
            /* Set new button LED state */
            SetButtonLedMask(L_EDIT, L_CUE | L_STORE | L_EDIT);
            break;

        default:
            SetButtonLedMask(0, L_CUE | L_STORE | L_EDIT);
            break;
        }

        SetLocateButtonLED(g_sys.cueIndex);
    }
}

//*****************************************************************************
// Handle button press events from DRC remote
//*****************************************************************************

void HandleButtonPress(uint32_t mask, uint32_t cue_flags)
{
    /* Handle numeric digit/locate buttons */
    if (mask & SW_LOC0) {
        HandleDigitPress(0, cue_flags);
    } else if (mask & SW_LOC1) {
        HandleDigitPress(1, cue_flags);
    } else if (mask & SW_LOC2) {
        HandleDigitPress(2, cue_flags);
    } else if (mask & SW_LOC3) {
        HandleDigitPress(3, cue_flags);
    } else if (mask & SW_LOC4) {
        HandleDigitPress(4, cue_flags);
    } else if (mask & SW_LOC5) {
        HandleDigitPress(5, cue_flags);
    } else if (mask & SW_LOC6) {
        HandleDigitPress(6, cue_flags);
    } else if (mask & SW_LOC7) {
        HandleDigitPress(7, cue_flags);
    } else if (mask & SW_LOC8) {
        HandleDigitPress(8, cue_flags);
    } else if (mask & SW_LOC9) {
        HandleDigitPress(9, cue_flags);
    } else if (mask & SW_CUE) {
        /* Switch to CUE mode */
        RemoteSetMode(REMOTE_MODE_CUE);
    }
    else if (mask & SW_STORE)
    {
        /* Switch to STORE mode */
        RemoteSetMode(REMOTE_MODE_STORE);
    }
    else if (mask & SW_EDIT)
    {
        /* Switch to EDIT mode */
        RemoteSetMode(REMOTE_MODE_EDIT);
    }
    else if (mask & SW_MENU)
    {
        /* ALT+MENU to zero reset system position */
        if (g_sys.shiftAltButton)
        {
            /* Reset system position to zero */
            PositionZeroReset();
        }
        else
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
    }
    else if (mask & SW_AUTO)
    {
        /* toggle auto play mode */
        if (!g_sys.autoMode)
        {
            g_sys.autoMode = TRUE;
            SetButtonLedMask(L_AUTO, 0);
        }
        else
        {
            g_sys.autoMode = FALSE;
            SetButtonLedMask(0, L_AUTO);
        }
    }
    else if (mask & SW_ALT)
    {
        if (g_cfg.showLongTime)
            g_cfg.showLongTime = false;
        else
            g_cfg.showLongTime = true;
    }

    // Notify the software remote of status change
    Event_post(g_eventTransport, Event_Id_02);
}

//*****************************************************************************
// This handler is called for any digit keys (0-9) pressed on the remote.
//*****************************************************************************

void HandleDigitPress(size_t index, uint32_t cue_flags)
{
    int len;
    char digit;
     static char digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };

    if (g_sys.remoteMode == REMOTE_MODE_CUE)
    {
        /*
         * Remote is in CUE mode and a LOC-# button was pressed
         */

        g_sys.cueIndex = index;

        SetLocateButtonLED(index);

        /* Begin locate search */
        LocateSearch(index, cue_flags);
    }
    else if (g_sys.remoteMode == REMOTE_MODE_STORE)
    {
        /*
         * Remote is in STORE mode and a LOC-# button was pressed
         */
        g_sys.cueIndex = index;

        SetLocateButtonLED(index);

        /* Store the current locate point */
        CuePointSet(index, g_sys.tapePosition, CF_ACTIVE);

        /* Return to previous Cue or default mode */
        RemoteSetMode(g_sys.remoteModePrev);

        SetLocateButtonLED(g_sys.cueIndex);
    }
    else if (g_sys.remoteMode == REMOTE_MODE_EDIT)
    {
        /*
         * Remote is in EDIT mode and a digit 0-9 was pressed.
         */

        if (g_sys.editState == EDIT_BEGIN)
        {
            g_sys.editTime.frame = 0;
            g_sys.editTime.tens  = 0;
            g_sys.editTime.secs  = 0;
            g_sys.editTime.mins  = 0;
            g_sys.editTime.hour  = 0;
            g_sys.editTime.flags = F_PLUS;

            g_sys.editState = EDIT_DIGITS;

            memset(&g_sys.editTime, 0, sizeof(g_sys.editTime));

            ResetDigitBuf();
        }

        digit = digits[index];

        if (g_sys.digitCount >= MAX_DIGITS_BUF)
            g_sys.digitCount = 0;

        g_sys.digitBuf[g_sys.digitCount++] = digit;
        g_sys.digitBuf[g_sys.digitCount] = 0;

        len = StrToTapeTime(g_sys.digitBuf, &g_sys.editTime);

        if (len >= 6)
        {
            CompleteEditTimeState();
        }
    }
    else
    {
        g_sys.cueIndex = index;

        SetLocateButtonLED(index);
    }
}

//*****************************************************************************
// Finished the time edit mode and stores the current result in the
// cue point currently being edited.
//*****************************************************************************

void CompleteEditTimeState(void)
{
    int count;
    int ipos;

    /* Get count of digits entered */
    count = g_sys.digitCount;

    /* Reset the digit counter */
    ResetDigitBuf();

    /* Exit EDIT mode back to previous state */
    RemoteSetMode(REMOTE_MODE_EDIT);

    /* Only store value if digits were entered */
    if (count)
    {
        /* Convert H:MM:SS time to total seconds */
        TapeTimeToPosition(&g_sys.editTime, &ipos);

        /* Store the position at current memory index */
        CuePointSet(g_sys.cueIndex, ipos, CF_ACTIVE);
    }

    memset(&g_sys.editTime, 0, sizeof(g_sys.editTime));

    g_sys.editState = EDIT_BEGIN;
}

//*****************************************************************************
// Parses a alphanumeric string of digits and converts it to tape time.
//*****************************************************************************

int StrToTapeTime(char *digits, TAPETIME* tapetime)
{
    int  dcount;
    char buf[8];

    if (!digits)
        return 0;

    memset(buf, 0, sizeof(buf));

    dcount = strlen(digits);

    switch(dcount)
    {
    case 1:
        /* parse tens units */
        buf[0] = digits[0];
        buf[1] = 0;
        tapetime->tens = atoi(buf);
        break;

    case 2:
        /* parse tens units */
        buf[0] = digits[0];
        buf[1] = 0;
        tapetime->tens = atoi(buf);
        /* parse secs units */
        buf[0] = digits[1];
        buf[1] = 0;
        tapetime->secs = atoi(buf);
        break;

    case 3:
        /* parse tens units */
        buf[0] = digits[0];
        buf[1] = 0;
        tapetime->tens = atoi(buf);
        /* parse secs units */
        buf[0] = digits[1];
        buf[1] = digits[2];
        buf[2] = 0;
        tapetime->secs = atoi(buf);
        break;

    case 4:
        /* parse tens units */
        buf[0] = digits[0];
        buf[1] = 0;
        tapetime->tens = atoi(buf);
        /* parse secs units */
        buf[0] = digits[1];
        buf[1] = digits[2];
        buf[2] = 0;
        tapetime->secs = atoi(buf);
        /* parse mins units */
        buf[0] = digits[3];
        buf[1] = 0;
        tapetime->mins = atoi(buf);
        break;

    case 5:
        /* parse tens units */
        buf[0] = digits[0];
        buf[1] = 0;
        tapetime->tens = atoi(buf);
        /* parse secs units */
        buf[0] = digits[1];
        buf[1] = digits[2];
        buf[2] = 0;
        tapetime->secs = atoi(buf);
        /* parse mins units */
        buf[0] = digits[3];
        buf[1] = digits[4];
        buf[2] = 0;
        tapetime->mins = atoi(buf);
        break;

    case 6:
        /* parse tens units */
        buf[0] = digits[0];
        buf[1] = 0;
        tapetime->tens = atoi(buf);
        /* parse secs units */
        buf[0] = digits[1];
        buf[1] = digits[2];
        buf[2] = 0;
        tapetime->secs = atoi(buf);
        /* parse mins units */
        buf[0] = digits[3];
        buf[1] = digits[4];
        buf[2] = 0;
        tapetime->mins = atoi(buf);
        /* parse hour unit */
        buf[0] = digits[5];
        buf[1] = 0;
        tapetime->hour = atoi(buf);
        break;

    default:
        /* length error */
        dcount = 0;
        break;
    }

    return dcount;
}

//*****************************************************************************
// This handler is called when the user presses the jog wheel down to close
// the switch within jog wheel encoder.
//*****************************************************************************

void HandleJogwheelPress(uint32_t flags)
{
//    uint32_t cue_flags = 0;
//    size_t index = g_sysData.cueIndex;

    switch (g_sys.remoteMode)
    {
    case REMOTE_MODE_EDIT:
        CompleteEditTimeState();
        break;
#if 0
    case REMOTE_MODE_CUE:
        SetLocateButtonLED(index);

        if (g_sys.autoMode)
            cue_flags |= CF_AUTO_PLAY;

        if (g_sys.shiftRecButton)
            cue_flags |= CF_AUTO_REC;

        /* Begin locate search */
        LocateSearch(index, cue_flags);
        break;

    case REMOTE_MODE_STORE:
        break;
#endif
    default:
        if (!g_sys.varispeedMode)
        {
            /* Enable vari-speed mode */
            g_sys.varispeedMode = true;
        }
        else
        {
            /* Reset ref frequency to default */
            g_sys.ref_freq = REF_FREQ;

            /* Calculate the 32-bit frequency divisor */
            uint32_t freqCalc = AD9837_freqCalc(g_sys.ref_freq);

            /* Program the DSS ref clock with new value */
            AD9837_adjustFreqMode32(FREQ0, FULL, freqCalc);
            AD9837_adjustFreqMode32(FREQ1, FULL, freqCalc);

            /* Disable vari-speed mode */
            g_sys.varispeedMode = false;
        }
        break;
    }
}

//*****************************************************************************
// This handler is called when the remote jog wheel is turning with the
// direction and velocity.
//*****************************************************************************

void HandleJogwheelMotion(uint32_t velocity, int direction)
{
    if (g_sys.varispeedMode)
    {
        float freq;
        float step;

        if (velocity >= 12)
            step = 1000.0f;
        else if (velocity >= 8)
            step = 500.0f;
        else if (velocity >= 5)
            step = 100.0f;
        else if (velocity >= 3)
            step = 10.0f;
        else
            step = 1.0f;

        freq = g_sys.ref_freq;

        if (direction > 0)
        {
            if ((freq + step) < REF_FREQ_MAX)
                freq += step;
            else
                freq = REF_FREQ_MAX;
        }
        else
        {
            if (step > freq)
                freq = REF_FREQ_MIN;
            else if ((freq - step) > REF_FREQ_MIN)
                freq -= step;
            else
                freq = REF_FREQ_MIN;
        }

        /* Calculate the 32-bit frequency divisor */
        uint32_t freqCalc = AD9837_freqCalc(freq);
        /* Program the DSS ref clock with new value */
        AD9837_adjustFreqMode32(FREQ0, FULL, freqCalc);
        AD9837_adjustFreqMode32(FREQ1, FULL, freqCalc);

        g_sys.ref_freq = freq;
    }
}

// End-Of-File
