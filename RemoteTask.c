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

/* Various Local Constants */
#define LAST_SCREEN     1

/* Static Module Globals */
uint32_t s_uScreenNum = 0;

/* External Global Data */
extern tContext g_context;
extern tFont *g_psFontWDseg7bold24pt;
extern Mailbox_Handle g_mailboxRemote;
extern SYSDATA g_sysData;

/* Static Function Prototypes */
static void ClearDisplay(void);
static void DrawScreen(uint32_t uScreenNum);
static void DrawTapeTime(void);
static void DrawWelcome(void);
static int GetHexStr(char* pTextBuf, uint8_t* pDataBuf, int len);

static void HandleSwitchPress(uint32_t bits);

static Void RemoteTaskFxn(UArg arg0, UArg arg1);

//*****************************************************************************
// Initialize the remote display task
//*****************************************************************************

Bool Remote_Task_init(void)
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
    RAMP_MSG msg;

    g_sysData.currentCueIndex = 0;
    g_sysData.ledMaskButton |= L_LOC1;

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
            if (msg.opcode == OP_DISPLAY_REFRESH)
                DrawScreen(s_uScreenNum);
            break;

        case MSG_TYPE_SWITCH:
            if (msg.opcode == OP_SWITCH_TRANSPORT)
            {
                IPCMSG ipc;

                /* Convert DRC switch bits to STC/DTC bit mask form */
                uint32_t mask = xlate_to_dtc_transport_switch_mask(msg.param1.U);

                /* Send the transport button mask to the DTC */
                ipc.type     = IPC_TYPE_NOTIFY;
                ipc.opcode   = OP_NOTIFY_BUTTON;
                ipc.param1.U = (uint32_t)mask;
                ipc.param2.U = 0;

                IPC_Notify(&ipc, 0);
            }
            else if (msg.opcode == OP_SWITCH_REMOTE)
            {
                HandleSwitchPress(msg.param1.U);
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
        g_sysData.currentCueIndex = 0;
    } else if (bits & SW_LOC2) {
        g_sysData.currentCueIndex = 1;
    } else if (bits & SW_LOC3) {
        g_sysData.currentCueIndex = 2;
    } else if (bits & SW_LOC4) {
        g_sysData.currentCueIndex = 3;
    } else if (bits & SW_LOC5) {
        g_sysData.currentCueIndex = 4;
    } else if (bits & SW_LOC6) {
        g_sysData.currentCueIndex = 5;
    } else if (bits & SW_LOC7) {
        g_sysData.currentCueIndex = 6;
    } else if (bits & SW_LOC8) {
        g_sysData.currentCueIndex = 7;
    }
    else if (bits & SW_STORE) {
        CuePointStore(g_sysData.currentCueIndex);
    } else if (bits & SW_CUE)  {
        LocateSearch(g_sysData.currentCueIndex);
    } else if (bits & SW_SET)  {

    } else if (bits & SW_ESC)  {

    } else if (bits & SW_PREV) {

    } else if (bits & SW_MENU) {

    } else if (bits & SW_NEXT) {

    } else if (bits & SW_EDIT) {

    }

    SetLocButtonLED(g_sysData.currentCueIndex);
}

//*****************************************************************************
//
//*****************************************************************************

void SetLocButtonLED(size_t index)
{
    uint32_t mask = 0;

    size_t shift = index % 8;

    //mask = L_LOC1 << shift;
#if 1
    switch(index % 8)
    {
    case 0:
        mask = L_LOC1;
        break;
    case 1:
        mask = L_LOC2;
        break;
    case 2:
        mask = L_LOC3;
        break;
    case 3:
        mask = L_LOC4;
        break;
    case 4:
        mask = L_LOC5;
        break;
    case 5:
        mask = L_LOC6;
        break;
    case 6:
        mask = L_LOC7;
        break;
    case 7:
        mask = L_LOC8;
        break;
    }
#endif
    g_sysData.ledMaskButton = (g_sysData.ledMaskButton & 0xFF00) | mask;
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
//
//*****************************************************************************

void ClearDisplay()
{
    tRectangle rect = {0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1};
    GrContextForegroundSetTranslated(&g_context, 0);
    GrContextBackgroundSetTranslated(&g_context, 0);
    GrRectFill(&g_context, &rect);
    GrFlush(&g_context);
}

//*****************************************************************************
// Display the current measurement screen data
//*****************************************************************************

void DrawScreen(uint32_t uScreenNum)
{
    ClearDisplay();

    switch(uScreenNum)
    {
    case 0:
        DrawTapeTime();
        break;

    case 1:
        DrawWelcome();
        break;

    default:
        break;
   }

    GrFlush(&g_context);
}

//*****************************************************************************
// Display the normal tape time/position screen
//*****************************************************************************

void DrawTapeTime(void)
{
    char buf[64];
    int len;
    uint32_t y = 0;
    uint32_t height;
    uint32_t width;
    uint32_t spacing = 0;
    tRectangle rect;
    TAPETIME tapeTime;

    /* Top line fixed system font in inverse */
    GrContextFontSet(&g_context, g_psFontFixed6x8);
    height = GrStringHeightGet(&g_context);

    /* Inverse Mono */
    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);

    y = 2;

    if (g_sysData.searching)
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
                len = sprintf(buf, "REC");
            else
                len = sprintf(buf, "PLAY");
            break;

        case MODE_FWD:
            len = sprintf(buf, "FWD");
            break;

        case MODE_REW:
            len = sprintf(buf, "REW");
            break;

        default:
            break;
        }
    }

    /*
     * Draw current tape speed active
     */

    //width = GrStringWidthGet(&g_context, buf, len);
    GrStringDraw(&g_context, buf, -1, 0, y, 1);
    len = sprintf(buf, "%s IPS", (g_sysData.transportMode & 0x8000) ? "30" : "15");
    width = GrStringWidthGet(&g_context, buf, len);
    GrStringDraw(&g_context, buf, -1, (SCREEN_WIDTH - 1) - width, y, 1);
    y += height + spacing;

    /* Normal Mono */
    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);

    /*
     * Draw the big time digits centered
     */

    GrContextFontSet(&g_context, g_psFontWDseg7bold24pt);
    //GrContextFontSet(&g_context, g_psFontCm30b);
    height = GrStringHeightGet(&g_context);

    len = sprintf(buf, "%c%1u:%02u:%02u",
            (g_sysData.tapeTime.flags & F_PLUS) ? '+' : '-',
            g_sysData.tapeTime.hour,
            g_sysData.tapeTime.mins,
            g_sysData.tapeTime.secs);

    //GrStringDraw(&g_context, buf, -1, 10, y, 0);
    GrStringDrawCentered(&g_context, buf, len, (SCREEN_WIDTH/2)-4, SCREEN_HEIGHT/2, FALSE);
    y += height + spacing;

    /* Draw the sign in a different font as 7-seg does not have these chars */
    GrContextFontSet(&g_context, g_psFontCmss14b);
    height = GrStringHeightGet(&g_context);
    len = sprintf(buf, "%c", (g_sysData.tapeTime.flags & F_PLUS) ? '+' : '-');
    GrStringDrawCentered(&g_context, buf, len, 6, (SCREEN_HEIGHT/2)-3, FALSE);

    /*
     *  Bottom line - show current locate memory time
     */

    GrContextFontSet(&g_context, g_psFontFixed6x8);
    height = GrStringHeightGet(&g_context);

    y = SCREEN_HEIGHT - height - 1;

    len = sprintf(buf, "LOC-%u", g_sysData.currentCueIndex+1);
    width = GrStringWidthGet(&g_context, buf, len);

    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);

    rect.i16XMin = 0;
    rect.i16YMin = y;
    rect.i16XMax = width + 1;
    rect.i16YMax = y + height;

    GrContextForegroundSetTranslated(&g_context, 0);
    GrContextBackgroundSetTranslated(&g_context, 1);

    width = GrStringWidthGet(&g_context, buf, len);
    GrStringDraw(&g_context, buf, -1, 1, y+1, 1);

    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);

    GrRectDraw(&g_context, &rect);

    /* Get the cue point tape time and display it */

    if (CuePointGet(g_sysData.currentCueIndex, NULL) & CF_SET)
    {
        CuePointGetTime(g_sysData.currentCueIndex, &tapeTime);
        sprintf(buf, "%u:%02u:%02u", tapeTime.hour, tapeTime.mins, tapeTime.secs);
        GrStringDraw(&g_context, buf, -1, width+6, y+1, 0);
    }
    else
    {
        GrStringDraw(&g_context, "-:--:--", -1, width+6, y+1, 0);
    }
}




//*****************************************************************************
// Draw the welome screen with version info
//*****************************************************************************

void DrawWelcome(void)
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

    GrFlush(&g_context);
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

// End-Of-File
