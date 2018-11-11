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

/* Various Local Constants */
#define LAST_SCREEN     1

#define MODE_UNDEFINED  0
#define MODE_CUE        1
#define MODE_STORE      2

#define SCREEN_TIME     0
#define SCREEN_MENU     1
#define SCREEN_ABOUT    2

/* Static Module Globals */
static uint32_t s_uScreenNum = 0;
static uint32_t s_searchMode = MODE_UNDEFINED;

/* External Global Data */
extern tContext g_context;
extern tFont *g_psFontWDseg7bold24pt;
extern tFont *g_psFontWDseg7bold20pt;
extern tFont *g_psFontWDseg7bold18pt;
extern tFont *g_psFontWDseg7bold16pt;
extern Mailbox_Handle g_mailboxRemote;
extern SYSDATA g_sysData;
extern SYSPARMS g_sysParms;

/* Static Function Prototypes */
static void ClearDisplay(void);
static void DrawScreen(uint32_t uScreenNum);
static void DrawTapeTime(void);
static void DrawAbout(void);
static void DrawMenu(void);
static int GetHexStr(char* pTextBuf, uint8_t* pDataBuf, int len);
static void HandleSwitchPress(uint32_t bits);
static void HandleJogwheel(uint32_t bits);
static Void RemoteTaskFxn(UArg arg0, UArg arg1);
static void HandleSetSearchMemory(size_t index);
static void HandleSetSearchMode(uint32_t mode);
static void BlinkLocateButtonLED(size_t index);

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

    g_sysData.currentCueIndex = 0;
    g_sysData.currentCueBank  = 0;

    /* Initialize LOC-1 memory as RTZ and select CUE mode */
    s_searchMode = MODE_UNDEFINED;
    HandleSetSearchMode(MODE_CUE);
    HandleSetSearchMemory(0);

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
                HandleSwitchPress(msg.param1.U);
            }
            else if (msg.opcode == OP_SWITCH_JOGWHEEL)
            {
                //CLI_printf("jog press\n");
            }
            break;

        case MSG_TYPE_JOGWHEEL:
            if (msg.opcode == OP_JOGWHEEL_MOTION)
            {
                //HandleJogwheelNotify();
                //CLI_printf("jog %u, %d\n", msg.param1.U, msg.param2.I);
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

void HandleSwitchPress(uint32_t bits)
{
    if (bits & SW_LOC1) {
        HandleSetSearchMemory(0);
    } else if (bits & SW_LOC2) {
        HandleSetSearchMemory(1);
    } else if (bits & SW_LOC3) {
        HandleSetSearchMemory(2);
    } else if (bits & SW_LOC4) {
        HandleSetSearchMemory(3);
    } else if (bits & SW_LOC5) {
        HandleSetSearchMemory(4);
    } else if (bits & SW_LOC6) {
        HandleSetSearchMemory(5);
    } else if (bits & SW_LOC7) {
        HandleSetSearchMemory(6);
    } else if (bits & SW_LOC8) {
        HandleSetSearchMemory(7);
    } else if (bits & SW_STORE) {
        HandleSetSearchMode(MODE_STORE);
    } else if (bits & SW_CUE)  {
        HandleSetSearchMode(MODE_CUE);
    } else if (bits & SW_SET)  {

    } else if (bits & SW_ESC)  {
        if (g_sysParms.showLongTime)
            g_sysParms.showLongTime = FALSE;
        else
            g_sysParms.showLongTime = TRUE;

    } else if (bits & SW_PREV) {

    } else if (bits & SW_NEXT) {

    } else if (bits & SW_MENU) {
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
    } else if (bits & SW_EDIT) {

    }
}

//*****************************************************************************
//
//*****************************************************************************

void HandleSetSearchMode(uint32_t mode)
{
    if (s_searchMode == mode)
    {
        if (mode == MODE_STORE)
            SetButtonLedMask(0, L_STORE);
        else
            SetButtonLedMask(0, L_CUE);

        s_searchMode = MODE_UNDEFINED;
    }
    else
    {
        s_searchMode = mode;

        if (mode == MODE_STORE)
            SetButtonLedMask(L_STORE, L_CUE | L_STORE);
        else if (mode == MODE_CUE)
            SetButtonLedMask(L_CUE, L_CUE | L_STORE);
        else
            SetButtonLedMask(0, L_CUE | L_STORE);
    }
}

//*****************************************************************************
//
//*****************************************************************************

void HandleSetSearchMemory(size_t index)
{
    g_sysData.currentCueIndex = index;

    SetLocateButtonLED(index);

    if (s_searchMode == MODE_CUE)
    {
        /* Begin locate search */
        LocateSearch(index);
    }
    else if (s_searchMode == MODE_STORE)
    {
        /* Store the current locate point */
        CuePointSet(index, g_sysData.tapePosition);
    }
}

/*****************************************************************************
 * This functions set the appropriate LOC button LED bit flag on and
 * clears all other LOC button LED bits so only one LED is on.
 *****************************************************************************/

void SetLocateButtonLED(size_t index)
{
    uint32_t mask = 0;

    size_t shift = index % 8;

    mask = L_LOC1 << shift;

    if (s_searchMode == MODE_CUE)
        mask |= L_CUE;
    else if (s_searchMode == MODE_STORE)
        mask |= L_STORE;

    SetButtonLedMask(mask, 0x00FF);
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

//*****************************************************************************
// Format a data buffer into an ascii hex string.
//*****************************************************************************

int GetHexStr(char* pTextBuf, uint8_t* pDataBuf, int len)
{
    char fmt[8];
    uint32_t i;
    int32_t l;

    *pTextBuf = 0;
    strcpy(fmt, "%02X");

    for (i=0; i < len; i++)
    {
        l = sprintf(pTextBuf, fmt, *pDataBuf++);
        pTextBuf += l;

        if (((i % 2) == 1) && (i != (len-1)))
        {
            l = sprintf(pTextBuf, "-");
            pTextBuf += l;
        }
    }

    return strlen(pTextBuf);
}

//*****************************************************************************
//
//*****************************************************************************

void ClearDisplay()
{
    tRectangle rect = {0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1};
    GrContextForegroundSetTranslated(&g_context, 0);
    GrContextBackgroundSetTranslated(&g_context, 0);
    GrRectFill(&g_context, &rect);
}

//*****************************************************************************
// Display the current measurement screen data
//*****************************************************************************

void DrawScreen(uint32_t uScreenNum)
{
    ClearDisplay();

    switch(uScreenNum)
    {
    case SCREEN_TIME:
        DrawTapeTime();
        break;

    case SCREEN_ABOUT:
        DrawAbout();
        break;

    case SCREEN_MENU:
        DrawMenu();
        break;

    default:
        break;
   }

    GrFlush(&g_context);
}

//*****************************************************************************
//
//*****************************************************************************

void DrawMenu(void)
{
    int len;
    char buf[64];

    /* Set foreground pixel color on to 0x01 */
    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);

    /* Setup font */
    uint32_t y = 4;
    uint32_t height;

    /* Use fixed system font */
    GrContextFontSet(&g_context, g_psFontFixed6x8);
    height = GrStringHeightGet(&g_context);

    len = sprintf(buf, "MENU");
    GrStringDrawCentered(&g_context, buf, len, SCREEN_WIDTH/2, y, FALSE);
    y += (height/2) + 4;
}

//*****************************************************************************
// Draw the welome screen with version info
//*****************************************************************************

void DrawAbout(void)
{
    int len;
    char buf[64];

    /* Set foreground pixel color on to 0x01 */
    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);

    //tRectangle rect = {0, 0, SCREEN_WIDTH-1, SCREEN_HEIGHT-1};
    //GrRectDraw(&g_context, &rect);

    /* Setup font */
    uint32_t y;
    uint32_t height;
    uint32_t spacing = 2;

    /* Display the program version/revision */
    GrContextFontSet(&g_context, g_psFontCm28b);
    height = GrStringHeightGet(&g_context);
    y = 12;
    len = sprintf(buf, "STC-1200");
    GrStringDrawCentered(&g_context, buf, len, SCREEN_WIDTH/2, y, FALSE);
    y += (height/2) + 4;

    /* Switch to fixed system font */
    GrContextFontSet(&g_context, g_psFontFixed6x8);
    height = GrStringHeightGet(&g_context);

    sprintf(buf, "Firmware v%d.%02d", FIRMWARE_VER, FIRMWARE_REV);
    GrStringDraw(&g_context, buf, -1, 25, y, 0);
    y += height + spacing + 4;

    /* Get the serial number string and display it */

    GetHexStr(buf, &g_sysData.ui8SerialNumber[0], 8);
    GrStringDraw(&g_context, buf, -1, 8, y, 0);
    y += height + spacing;

    GetHexStr(buf, &g_sysData.ui8SerialNumber[8], 8);
    GrStringDraw(&g_context, buf, -1, 8, y, 0);
    y += height + spacing;
}

//*****************************************************************************
// Display the home tape time/position screen. This is the main application
// screen that users see in normal operation mode.
//*****************************************************************************

void DrawTapeTime(void)
{
    char buf[64];
    int32_t x, y;
    int32_t len;
    int32_t width;
    int32_t height;
    tRectangle rect, rect2;
    TAPETIME tapeTime;

    /*
     * Draw the current transport mode text on top line
     */

    if (LocateIsSearching())
    {
        len = sprintf(buf, "SEARCH");
    }
    else
    {
        switch(g_sysData.transportMode & MODE_MASK)
        {
        case MODE_HALT:
            len = sprintf(buf, "HALT");
            break;

        case MODE_STOP:
            len = sprintf(buf, "STOP");
            break;

        case MODE_PLAY:
            if (g_sysData.transportMode & M_RECORD)
                len = sprintf(buf, "PLAY (REC)");
            else
                len = sprintf(buf, "PLAY");
            break;

        case MODE_FWD:
            if (g_sysData.transportMode & M_LIBWIND)
                len = sprintf(buf, "FWD (LIB)");
            else
                len = sprintf(buf, "FWD");
            break;

        case MODE_REW:
            if (g_sysData.transportMode & M_LIBWIND)
                len = sprintf(buf, "REW (LIB)");
            else
                len = sprintf(buf, "REW");
            break;

        default:
            break;
        }
    }

    x = 0;
    y = 2;

    /* Top line fixed system font */
    GrContextFontSet(&g_context, g_psFontFixed6x8);
    height = GrStringHeightGet(&g_context);

    /* Mono */
    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);
    //width = GrStringWidthGet(&g_context, buf, len);
    GrStringDraw(&g_context, buf, -1, x, y, 1);

    /*
     * Draw current tape speed active
     */

    len = sprintf(buf, "%s IPS", (g_sysData.tapeSpeed == 30) ? "30" : "15");
    width = GrStringWidthGet(&g_context, buf, len);
    x = (SCREEN_WIDTH - 1) - width;
    GrStringDraw(&g_context, buf, -1, x, y, 1);
    y += height;

    /*
     * Draw the big time digits centered
     */

    /* Normal Mono */
    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);

    if (g_sysParms.showLongTime)
    {
        /* Extended hour, mins, secs and tenth seconds display format */

        GrContextFontSet(&g_context, g_psFontWDseg7bold18pt);
        height = GrStringHeightGet(&g_context);

        len = sprintf(buf, "%c%1u:%02u:%02u:%02u",
                (g_sysData.tapeTime.flags & F_PLUS) ? '+' : '-',
                 g_sysData.tapeTime.hour,
                 g_sysData.tapeTime.mins,
                 g_sysData.tapeTime.secs,
                 g_sysData.tapeTime.tens);

        x = (SCREEN_WIDTH / 2) - 3;
        y = (SCREEN_HEIGHT / 2) - 5;
        GrStringDrawCentered(&g_context, buf, len, x, y, 1);

        /* Draw the sign in a different font as 7-seg does not have these chars */
        GrContextFontSet(&g_context, g_psFontCmss14b);
        len = sprintf(buf, "%c", (g_sysData.tapeTime.flags & F_PLUS) ? '+' : '-');
        GrStringDrawCentered(&g_context, buf, len, 4, y-3, 1);

        y += height - 5;
        GrContextFontSet(&g_context, g_psFontFixed6x8);
        GrStringDraw(&g_context, "HR", -1, 11, y, 0);
        GrStringDraw(&g_context, "MIN", -1, 35, y, 0);
        GrStringDraw(&g_context, "SEC", -1, 69, y, 0);
        GrStringDraw(&g_context, "TEN", -1, 101, y, 0);
    }
    else
    {
        /* Standard hour, mins, secs time display format */

        GrContextFontSet(&g_context, g_psFontWDseg7bold18pt);
        height = GrStringHeightGet(&g_context);

        len = sprintf(buf, "%c%1u:%02u:%02u",
                (g_sysData.tapeTime.flags & F_PLUS) ? '+' : '-',
                 g_sysData.tapeTime.hour,
                 g_sysData.tapeTime.mins,
                 g_sysData.tapeTime.secs);

        x = (SCREEN_WIDTH / 2) - 3;
        y = (SCREEN_HEIGHT / 2) - 5;
        GrStringDrawCentered(&g_context, buf, len, x, y, FALSE);

        /* Draw the sign in a different font as 7-seg does not have these chars */
        GrContextFontSet(&g_context, g_psFontCmss14b);
        len = sprintf(buf, "%c", (g_sysData.tapeTime.flags & F_PLUS) ? '+' : '-');
        GrStringDrawCentered(&g_context, buf, len, 15, y-3, FALSE);

        y += height - 5;
        x = 27;
        GrContextFontSet(&g_context, g_psFontFixed6x8);
        GrStringDraw(&g_context, "HR", -1, x, y, 0);
        GrStringDraw(&g_context, "MIN", -1, x + 24, y, 0);
        GrStringDraw(&g_context, "SEC", -1, x + 58, y, 0);
    }

    /*
     *  Bottom line - show current locate memory time
     */

    GrContextFontSet(&g_context, g_psFontFixed6x8);
    height = GrStringHeightGet(&g_context);

    x = 0;
    y = SCREEN_HEIGHT - height - 1;

    len = sprintf(buf, "LOC-%u", g_sysData.currentCueIndex+1);
    width = GrStringWidthGet(&g_context, buf, len);

    rect.i16XMin = x;
    rect.i16YMin = y;
    rect.i16XMax = width + 1;
    rect.i16YMax = y + height;

    GrContextForegroundSetTranslated(&g_context, 0);
    GrContextBackgroundSetTranslated(&g_context, 1);

    width = GrStringWidthGet(&g_context, buf, len);
    GrStringDraw(&g_context, buf, -1, x+1, y+1, 1);

    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);

    GrRectDraw(&g_context, &rect);

    /* Get the cue point tape time and display it */

    x = width + 6;
    y = y + 1;

    if (CuePointGet(g_sysData.currentCueIndex, NULL) & CF_SET)
    {
        CuePointGetTime(g_sysData.currentCueIndex, &tapeTime);
        int ch = (tapeTime.flags & F_PLUS) ? '+' : '-';
        sprintf(buf, "%c%u:%02u:%02u", ch, tapeTime.hour, tapeTime.mins, tapeTime.secs);
        GrStringDraw(&g_context, buf, -1, x, y, 0);
    }
    else
    {
        GrStringDraw(&g_context, " -:--:--", -1, x, y, 0);
    }

    /* Display locate progress bar */

    if (!LocateIsSearching())
    {
        if (g_sysData.transportMode & M_RECORD)
        {
            GrContextFontSet(&g_context, g_psFontFixed6x8);
            height = GrStringHeightGet(&g_context);

            len = sprintf(buf, "RECORD");
            width = GrStringWidthGet(&g_context, buf, len);

            x = (SCREEN_WIDTH - 1) - width;
            y = SCREEN_HEIGHT - height - 1;

            rect.i16XMin = x;
            rect.i16YMin = y;
            rect.i16XMax = x + width + 1;
            rect.i16YMax = y + height;

            GrContextForegroundSetTranslated(&g_context, 0);
            GrContextBackgroundSetTranslated(&g_context, 1);

            GrStringDraw(&g_context, buf, -1, x+1, y+1, 1);

            GrContextForegroundSetTranslated(&g_context, 1);
            GrContextBackgroundSetTranslated(&g_context, 0);

            GrRectDraw(&g_context, &rect);
        }
    }
    else
    {
        if (0)
        {
            /* Draw progress as text only */
            GrContextFontSet(&g_context, g_psFontFixed6x8);
            sprintf(buf, "%d%%", g_sysData.searchProgress);
            GrStringDraw(&g_context, buf, -1, 100, y, 0);
        }
        else
        {
            /* Draw progress bar */

            x = 40;
            height -= 2;

            rect.i16XMin = SCREEN_WIDTH - x;
            rect.i16YMin = y;
            rect.i16XMax = SCREEN_WIDTH - 1;
            rect.i16YMax = y + height;

            GrContextForegroundSetTranslated(&g_context, 1);
            GrContextBackgroundSetTranslated(&g_context, 0);
            GrRectDraw(&g_context, &rect);

            rect2 = rect;

            rect2.i16XMin += 2;
            rect2.i16YMin += 2;
            rect2.i16XMax -= 2;
            rect2.i16YMax -= 2;

            int32_t x1 = rect2.i16XMin;
            int32_t x2 = rect2.i16XMax;

            float progress = (float)g_sysData.searchProgress * 0.01f;

            x = (int16_t)((float)(x2 - x1) * progress) + x1;

            if (x > x2)
                x = x2;

            if (x < x1)
                x = x1;

            rect2.i16XMax = (int16_t)x - 1;

            GrContextForegroundSetTranslated(&g_context, 1);
            GrContextBackgroundSetTranslated(&g_context, 0);

            GrRectFill(&g_context, &rect2);
        }
    }
}

// End-Of-File
