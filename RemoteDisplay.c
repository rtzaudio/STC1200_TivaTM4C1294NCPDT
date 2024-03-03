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
#include <ti/drivers/I2C.h>
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
#include "drivers/offscrmono.h"

/* PMX42 Board Header file */
#include "Board.h"
#include "STC1200.h"
#include "Utils.h"
#include "IPCServer.h"
#include "RAMPServer.h"
#include "RemoteTask.h"

/* Static Function Prototypes */
static void DrawInfo(void);
static void DrawTapeTime(void);
static void DrawTrackAssign(void);
static void DrawTimeTop(void);
static void DrawTimeMiddle(void);
static void DrawTimeEdit(void);
static void DrawTimeBottom(void);
static void DrawTrackSetAll(void);
static void DrawTapeSpeedSet(void);
static void DrawStandbyMonSet(void);

/* Helpers */
static void GrSetRect(tRectangle* rect,
                      int16_t XMin, int16_t YMin,
                      int16_t XMax, int16_t YMax);

static void GrInflateRect(tRectangle* rect,
                      int16_t XMin, int16_t YMin,
                      int16_t XMax, int16_t YMax);

/* External Global Data */
extern tContext g_context;
extern tFont *g_psFontWDseg7bold24pt;
extern tFont *g_psFontWDseg7bold20pt;
extern tFont *g_psFontWDseg7bold18pt;
extern tFont *g_psFontWDseg7bold16pt;
extern tFont *g_psFontWDseg7bold14pt;
extern tFont *g_psFontWDseg7bold12pt;
extern tFont *g_psFontWDseg7bold10pt;
extern tFont *g_psFontWMonospace10pt;

//*****************************************************************************
// Graphics Helpers
//*****************************************************************************

void GrSetRect(tRectangle* rect,
               int16_t XMin, int16_t YMin,
               int16_t XMax, int16_t YMax)
{
    rect->i16XMin = XMin;
    rect->i16YMin = YMin;
    rect->i16XMax = XMax;
    rect->i16YMax = YMax;
}

void GrInflateRect(tRectangle* rect,
               int16_t XMin, int16_t YMin,
               int16_t XMax, int16_t YMax)
{
    rect->i16XMin += XMin;
    rect->i16YMin += YMin;
    rect->i16XMax += XMax;
    rect->i16YMax += YMax;
}

//*****************************************************************************
// Clear the display screen
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
    case VIEW_TAPE_TIME:
        DrawTapeTime();
        break;

    case VIEW_TRACK_ASSIGN:
        DrawTrackAssign();
        break;

    case VIEW_TRACK_SET_ALL:
        DrawTrackSetAll();
        break;

    case VIEW_STANDBY_SET_ALL:
        DrawStandbyMonSet();
        break;

    case VIEW_TAPE_SPEED_SET:
        DrawTapeSpeedSet();
        break;

    case VIEW_INFO:
        DrawInfo();
        break;

    default:
        break;
   }

    GrFlush(&g_context);
}

//*****************************************************************************
//
//*****************************************************************************

void DrawInfo(void)
{
    char buf[64];
    int32_t x, y;
    int32_t len;
    //int32_t width;
    int32_t height;
    uint32_t spacing = 4;

    /* Set foreground pixel color on to 0x01 */
    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);

    /* Use fixed system font */
    GrContextFontSet(&g_context, g_psFontFixed6x8);
    height = GrStringHeightGet(&g_context);

    x = SCREEN_WIDTH/2;
    y = 10;

    /* Display firmware version */
    len = sprintf(buf, "STC-1200 v%d.%02d.%d",
                  FIRMWARE_VER, FIRMWARE_REV, FIRMWARE_BUILD);
    GrStringDrawCentered(&g_context, buf, len, x, y, false);

    /* Display the ref clock frequency */
    y += (height + spacing);
    len = sprintf(buf, "REF %.2f Hz", g_sys.ref_freq);
    GrStringDrawCentered(&g_context, buf, len, x, y, false);

    /* Display the IP address */
    y += (height + spacing);
    if (strlen(g_sys.ipAddr) == 0)
        len = sprintf(buf, "IP (no network)");
    else
        len = sprintf(buf, "IP %s", g_sys.ipAddr);
    GrStringDrawCentered(&g_context, buf, len, x, y, false);
}

//*****************************************************************************
// Display the home tape time/position screen. This is the main application
// screen that users see in normal operation mode.
//*****************************************************************************

void DrawTapeTime(void)
{
    /* Draw the top line showing current mode/speed */
    DrawTimeTop();

    /* Draw the current tape position time in the middle */
    if (g_sys.remoteMode == REMOTE_MODE_EDIT)
        DrawTimeEdit();
    else
        DrawTimeMiddle();

    /* Draw bottom line with current locate point time */
    DrawTimeBottom();
}


void DrawTimeTop(void)
{
    char buf[64];
    int32_t x, y;
    int32_t len;
    int32_t width;

    /*
     * Draw the current transport mode text on top line
     */

    if (IsLocatorSearching())
    {
        len = sprintf(buf, "SEARCH");
    }
    else if (IsLocatorAutoLoop())
    {
        len = sprintf(buf, "LOOP");
    }
    else
    {
        switch(g_sys.transportMode & MODE_MASK)
        {
        case MODE_HALT:
            len = sprintf(buf, "HALT");
            break;

        case MODE_THREAD:
            len = sprintf(buf, "THREAD");
            break;

        case MODE_STOP:
            len = sprintf(buf, "STOP");
            break;

        case MODE_PLAY:
            if (g_sys.transportMode & M_RECORD)
                len = sprintf(buf, "PLAY (REC)");
            else
                len = sprintf(buf, "PLAY");
            break;

        case MODE_FWD:
            if (g_sys.transportMode & M_LIBWIND)
                len = sprintf(buf, "FWD (LIB)");
            else
                len = sprintf(buf, "FWD");
            break;

        case MODE_REW:
            if (g_sys.transportMode & M_LIBWIND)
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

    /* Mono */
    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);
    GrStringDraw(&g_context, buf, -1, x, y, 1);

    /* Draw current tape speed active */
    if (g_sys.varispeedMode)
    {
        len = sprintf(buf, "%u", (uint32_t)g_sys.ref_freq);
    }
    else
    {
        len = sprintf(buf, "%s IPS", (g_sys.tapeSpeed == 30) ? "30" : "15");
    }

    width = GrStringWidthGet(&g_context, buf, len);
    x = (SCREEN_WIDTH - 1) - width;
    GrStringDraw(&g_context, buf, -1, x, y, 1);
}


void DrawTimeBottom(void)
{
    char buf[64];
    int32_t x, y;
    int32_t len;
    int32_t width;
    int32_t height;
    tRectangle rect, rect2;
    TAPETIME tapeTime;

    /*
     *  Bottom line - show current locate memory time
     */

    GrContextFontSet(&g_context, g_psFontFixed6x8);
    height = GrStringHeightGet(&g_context);

    x = 0;
    y = SCREEN_HEIGHT - height - 1;

    len = sprintf(buf, "M:%02u", g_sys.cueIndex);
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

    if (IsCuePointFlags(g_sys.cueIndex, CF_ACTIVE))
    {
        CuePointTimeGet(g_sys.cueIndex, &tapeTime);
        int ch = (tapeTime.flags & F_PLUS) ? '+' : '-';
        snprintf(buf, sizeof(buf)-1, "%c%1u:%02u:%02u:%1u", ch, tapeTime.hour, tapeTime.mins, tapeTime.secs, tapeTime.tens);
        GrStringDraw(&g_context, buf, -1, x, y, 0);
    }
    else
    {
        GrStringDraw(&g_context, " -:--:--:-", -1, x, y, 0);
    }

    /* Display locate progress bar */

    if (!IsLocatorSearching())
    {
        if (g_sys.transportMode & M_RECORD)
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
            sprintf(buf, "%d%%", g_sys.searchProgress);
            GrStringDraw(&g_context, buf, -1, 100, y, 0);
        }
        else
        {
            /* Draw progress bar */

            x = 35;
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

            float progress = (float)g_sys.searchProgress * 0.01f;

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


void DrawTimeMiddle(void)
{
    char buf[64];
    int32_t x, y;
    int32_t len;
    int32_t width;
    int32_t height;

    /*
     * Draw the big time digits centered
     */

    /* Normal Mono */
    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);

    if (g_sys.cfgSTC.showLongTime)
    {
        GrContextFontSet(&g_context, g_psFontWDseg7bold18pt);
        height = GrStringHeightGet(&g_context);

        len = sprintf(buf, "%1u:%02u:%02u:",
                 g_sys.tapeTime.hour,
                 g_sys.tapeTime.mins,
                 g_sys.tapeTime.secs);

        width = GrStringWidthGet(&g_context, buf, len);

        x = 12;
        y = (SCREEN_HEIGHT / 2) - ((height / 2) + 5);
        GrStringDraw(&g_context, buf, len, x, y, 0);

        GrContextFontSet(&g_context, g_psFontWDseg7bold10pt);
        len = sprintf(buf, "%02u", g_sys.tapeTime.frame);
        GrStringDraw(&g_context, buf, len, x+width, y+1, 0);

        /* Draw the sign in a different font as 7-seg does not have these chars */
        GrContextFontSet(&g_context, g_psFontCm14); //g_psFontCmss12);
        len = sprintf(buf, "%c", (g_sys.tapeTime.flags & F_PLUS) ? '+' : '-');
        GrStringDrawCentered(&g_context, buf, len, 6, y+6, 1);

        y += height + 4;
        x = 13;
        GrContextFontSet(&g_context, g_psFontFixed6x8);
        GrStringDraw(&g_context, "HR", -1, x, y, 0);
        GrStringDraw(&g_context, "MIN", -1, x + 24, y, 0);
        GrStringDraw(&g_context, "SEC", -1, x + 57, y, 0);
        GrStringDraw(&g_context, "FRM", -1, x + 85, y, 0);
    }
    else
    {
        /*
         * Standard hour, mins, secs time display format
         */

        GrContextFontSet(&g_context, g_psFontWDseg7bold18pt);
        height = GrStringHeightGet(&g_context);

        len = sprintf(buf, "%1u:%02u:%02u:%1u",
                 g_sys.tapeTime.hour,
                 g_sys.tapeTime.mins,
                 g_sys.tapeTime.secs,
                 g_sys.tapeTime.tens);

        x = (SCREEN_WIDTH / 2) - 3;
        y = (SCREEN_HEIGHT / 2) - 5;
        GrStringDrawCentered(&g_context, buf, len, x, y, FALSE);

        /* Draw the sign in a different font as 7-seg does not have these chars */
        GrContextFontSet(&g_context, g_psFontCm14);
        len = sprintf(buf, "%c", (g_sys.tapeTime.flags & F_PLUS) ? '+' : '-');
        GrStringDrawCentered(&g_context, buf, len, 6, y-3, FALSE);

        y += height - 5;
        x = 13;
        GrContextFontSet(&g_context, g_psFontFixed6x8);
        GrStringDraw(&g_context, "HR", -1, x, y, 0);
        GrStringDraw(&g_context, "MIN", -1, x + 24, y, 0);
        GrStringDraw(&g_context, "SEC", -1, x + 57, y, 0);
        GrStringDraw(&g_context, "TEN", -1, x + 85, y, 0);
    }
}


void DrawTimeEdit(void)
{
    char buf[64];
    int32_t x, y;
    int32_t len;
    //int32_t width;
    int32_t height;

    /*
     * Draw the big time digits centered
     */

    /* Normal Mono */
    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);

    /*
     * Standard hour, mins, secs time display format
     */

    //GrContextFontSet(&g_context, g_psFontWDseg7bold18pt);
    GrContextFontSet(&g_context, g_psFontFixed6x8);
    height = GrStringHeightGet(&g_context);

    len = sprintf(buf, "ENTER TIME");
    x = (SCREEN_WIDTH / 2) - 3;
    y = (SCREEN_HEIGHT / 2) - 13;
    GrStringDrawCentered(&g_context, buf, len, x, y, FALSE);

    char sign = (g_sys.tapeTime.flags & F_PLUS) ? '+' : '-';

    len = sprintf(buf, "%c %1u:%02u:%02u:%u",
                  sign,
                  g_sys.editTime.hour,
                  g_sys.editTime.mins,
                  g_sys.editTime.secs,
                  g_sys.editTime.tens);

    x = (SCREEN_WIDTH / 2) - 3;
    y = (SCREEN_HEIGHT / 2);
    GrStringDrawCentered(&g_context, buf, len, x-5, y, FALSE);

    y += height + 3;

    len = sprintf(buf, "  H MM SS T");
    GrStringDrawCentered(&g_context, buf, len, x-5, y, FALSE);
}

//*****************************************************************************
// Draw the track assignment screen for the current channel.
//*****************************************************************************

void DrawTrackAssign(void)
{
    int32_t x, y;
    int32_t len;
    int32_t trackNum;
    int32_t height;
    tRectangle rect, rect2;
    char buf[64];

    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);

    GrContextFontSet(&g_context, g_psFontFixed6x8);

    if (!g_sys.trackCount || !g_sys.dcsFound)
    {
        x = SCREEN_WIDTH / 2;
        y = SCREEN_HEIGHT / 2;
        GrStringDrawCentered(&g_context, "No DCS-1200", -1, x, y, TRUE);
        GrStringDrawCentered(&g_context, "Track Controller!", -1, x, y+12, TRUE);
        return;
    }

    trackNum = g_sys.remoteTrackNum;

    /*** DRAW SAFE/READY MODE AREA ***/

    GrSetRect(&rect, 2, 2, 41, 19);
    GrRectDraw(&g_context, &rect);

    /* Draw inner hi-light rect if active edit field */
    if ((g_sys.remoteFieldIndex == FIELD_TRACK_ARM) && (!g_sys.remoteViewSelect))
    {
        rect2 = rect;
        GrInflateRect(&rect2, 1, 1, -1, -1);
        GrRectDraw(&g_context, &rect2);
    }

    x = rect.i16XMin + ((rect.i16XMax - rect.i16XMin) / 2) + 2;
    y = rect.i16YMin + ((rect.i16YMax - rect.i16YMin) / 2) + 1;

    if (g_sys.trackState[trackNum] & STC_T_RECORD)
    {
        GrRectFill(&g_context, &rect);
        strcpy(buf, "REC");
    }
    else
    {
        strcpy(buf, (g_sys.trackState[trackNum] & STC_T_READY) ? "RDY" : "SAFE");
    }

    GrStringDrawCentered(&g_context, buf, -1, x, y, TRUE);

    /*** REPRO/SYNC/INPUT MODE AREA ***/

    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);

    switch(g_sys.trackState[trackNum] & STC_TRACK_MASK)
    {
    case STC_TRACK_REPRO:       /* track is in repro mode */
        strcpy(buf, "REPR");
        break;
    case STC_TRACK_SYNC:        /* track is in sync mode  */
        strcpy(buf, "SYNC");
        break;
    case STC_TRACK_INPUT:       /* track is in input mode */
        strcpy(buf, "INP");
        break;
    default:
        buf[0] = '\0';
        break;
    }

    GrSetRect(&rect, 2, 23, 41, 40);

    if (g_sys.trackState[trackNum] & STC_T_STANDBY)
    {
        GrContextForegroundSetTranslated(&g_context, 1);
        GrContextBackgroundSetTranslated(&g_context, 0);

        GrRectFill(&g_context, &rect);

        GrContextForegroundSetTranslated(&g_context, 0);
        GrContextBackgroundSetTranslated(&g_context, 1);

        strcpy(buf, "INP");
    }
    else
    {
        GrRectDraw(&g_context, &rect);
    }

    /* Draw inner hi-light rect if active edit field */
    if ((g_sys.remoteFieldIndex == FIELD_TRACK_MODE) && (!g_sys.remoteViewSelect))
    {
        rect2 = rect;
        GrInflateRect(&rect2, 1, 1, -1, -1);
        GrRectDraw(&g_context, &rect2);
    }

    x = rect.i16XMin + ((rect.i16XMax - rect.i16XMin) / 2) + 2;
    y = rect.i16YMin + ((rect.i16YMax - rect.i16YMin) / 2) + 1;

    GrStringDrawCentered(&g_context, buf, -1, x, y, FALSE);

    /*** DRAW STANDBY MONITOR AREA ***/

    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);

    GrSetRect(&rect, 2, 44, 41, 61);
    GrRectDraw(&g_context, &rect);

    x = rect.i16XMin + ((rect.i16XMax - rect.i16XMin) / 2) + 2;
    y = rect.i16YMin + ((rect.i16YMax - rect.i16YMin) / 2) + 1;

    /* Test track standby monitor enable flag */
    if (g_sys.trackState[trackNum] & STC_T_STANDBY)
    {
        GrRectFill(&g_context, &rect);

        GrContextForegroundSetTranslated(&g_context, 0);
        GrContextBackgroundSetTranslated(&g_context, 1);
    }

    /* Draw inner hi-light rect if active edit field */
    if ((g_sys.remoteFieldIndex == FIELD_TRACK_MONITOR) && (!g_sys.remoteViewSelect))
    {
        rect2 = rect;
        GrInflateRect(&rect2, 1, 1, -1, -1);
        GrRectDraw(&g_context, &rect2);

        if (g_sys.trackState[trackNum] & STC_T_STANDBY)
        {
            GrContextForegroundSetTranslated(&g_context, 0);
            GrContextBackgroundSetTranslated(&g_context, 1);
        }
        else
        {
            GrContextForegroundSetTranslated(&g_context, 1);
            GrContextBackgroundSetTranslated(&g_context, 0);
        }
    }

    if (g_sys.trackState[trackNum] & STC_T_MONITOR)
        GrStringDrawCentered(&g_context, "MON", -1, x, y, FALSE);
    else
        GrStringDrawCentered(&g_context, "TAPE", -1, x, y, TRUE);

    /*** DRAW LARGE TRACK NUMBER AREA ***/

    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);

    GrSetRect(&rect, 45, 2, 125, 61);
    GrRectDraw(&g_context, &rect);

    /* Draw inner hi-light rect if active edit field */
    if ((g_sys.remoteFieldIndex == FIELD_TRACK_NUM) && (!g_sys.remoteViewSelect))
    {
        rect2 = rect;
        GrInflateRect(&rect2, 1, 1, -1, -1);
        GrRectDraw(&g_context, &rect2);
    }

    x = 85;
    /* Draw channel number heading label */
    y = 12;
    GrContextFontSet(&g_context, g_psFontFixed6x8);
    height = GrStringHeightGet(&g_context);
    len = sprintf(buf, "TRACK");
    GrStringDrawCentered(&g_context, buf, len, x, y, TRUE);
    /* Draw the current edit channel number */
    y = 33;
    GrContextFontSet(&g_context, g_psFontWDseg7bold18pt);
    height = GrStringHeightGet(&g_context);
    len = sprintf(buf, "%u", trackNum + 1);
    GrStringDrawCentered(&g_context, buf, len, x, y, TRUE);
    y += height;

    /*** Draw Tape Time ***/

#if 0
    GrContextFontSet(&g_context, g_psFontFixed6x8);
    height = GrStringHeightGet(&g_context);

    int ch = (g_sys.tapeTime.flags & F_PLUS) ? '+' : '-';

    len = snprintf(buf, sizeof(buf)-1, "%c%1u:%02u:%02u:%1u",
             ch,
             g_sys.tapeTime.hour,
             g_sys.tapeTime.mins,
             g_sys.tapeTime.secs,
             g_sys.tapeTime.tens);

    x = rect.i16XMin + ((rect.i16XMax - rect.i16XMin) / 2);
    GrStringDrawCentered(&g_context, buf, len, x, y, FALSE);
#endif

    GrContextFontSet(&g_context, g_psFontFixed6x8);
    height = GrStringHeightGet(&g_context);

    if (g_sys.remoteTrackNumSelect)
    {
        GrSetRect(&rect2, rect.i16XMin, rect.i16YMax-12, rect.i16XMax, rect.i16YMax);
        GrRectFill(&g_context, &rect2);

        GrContextForegroundSetTranslated(&g_context, 0);
        GrContextBackgroundSetTranslated(&g_context, 1);

        len = snprintf(buf, sizeof(buf)-1, "JOG+CLICK");
        x = rect.i16XMin + ((rect.i16XMax - rect.i16XMin) / 2);
        GrStringDrawCentered(&g_context, buf, len, x, y+3, FALSE);
    }
    else
    {
        len = snprintf(buf, sizeof(buf)-1, (g_sys.standbyMonitor) ? "STANDBY" : "TAPE");
        x = rect.i16XMin + ((rect.i16XMax - rect.i16XMin) / 2);
        GrStringDrawCentered(&g_context, buf, len, x, y, FALSE);
    }
}

//*****************************************************************************
//
//*****************************************************************************

void MenuDraw(char* heading, MenuOption* menu, size_t count, size_t index)
{
    int32_t i, w;
    int32_t len;
    char buf[32];
    tRectangle rect;
    int32_t maxwidth = 0;

    MenuOption* mp = menu;

    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);

    if (g_sys.remoteViewSelect)
    {
        GrSetRect(&rect, 0, 0, SCREEN_WIDTH, 10);
        GrRectFill(&g_context, &rect);

        /* Normal Mono */
        GrContextForegroundSetTranslated(&g_context, 0);
        GrContextBackgroundSetTranslated(&g_context, 1);
    }

    GrContextFontSet(&g_context, g_psFontFixed6x8);
    GrStringDrawCentered(&g_context, heading, -1, CENTER_X, 5, TRUE);

    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);

    for (i=0, mp=menu; i < count; i++, mp++)
    {
        len = sprintf(buf, "%s", mp->text);

        if ((w = GrStringWidthGet(&g_context, buf, len)) > maxwidth)
            maxwidth = w;

        GrStringDrawCentered(&g_context, buf, len, mp->x, mp->y, TRUE);
    }

    /* Show the item hi-light box around current menu item */
    if (g_sys.remoteFieldIndex < count)
    {
        /* Don't show menu item highlight box if view
         * select is currently active.
         */
        if (!g_sys.remoteViewSelect)
        {
            w = (maxwidth >> 1) + 10;
            mp = menu + index;
            GrSetRect(&rect, mp->x-w, mp->y-5, mp->x+w-1, mp->y+5);
            GrRectDraw(&g_context, &rect);
        }
    }
}

//*****************************************************************************
//
//*****************************************************************************

void DrawTrackSetAll(void)
{
    static MenuOption menuOptions[] = {
        30, 25, "INPUT",
        30, 35, "SYNC",
        30, 45, "REPRO",
        98, 25, "SAFE",
        98, 35, "READY",
    };

    MenuDraw("SET ALL TRACKS TO",
             menuOptions,
             sizeof(menuOptions)/sizeof(MenuOption),
             g_sys.remoteFieldIndex);
}

//*****************************************************************************
//
//*****************************************************************************

void DrawStandbyMonSet(void)
{
    static MenuOption menuOptions[] = {
        CENTER_X, 25, "SET ALL",
        CENTER_X, 35, "CLEAR ALL",
        CENTER_X, 25, "STANDBY OFF",
        CENTER_X, 35, "STANDBY OFF",
    };

    MenuDraw("TRACK STANDBY MONITOR",
             menuOptions,
             sizeof(menuOptions)/sizeof(MenuOption),
             g_sys.remoteFieldIndex);
}

//*****************************************************************************
//
//*****************************************************************************

void DrawTapeSpeedSet(void)
{
    static MenuOption menuOptions[] = {
        CENTER_X, 25, "LO-SPEED",
        CENTER_X, 35, "HI-SPEED",
    };

    MenuDraw("TAPE SPEED SELECT",
             menuOptions,
             sizeof(menuOptions)/sizeof(MenuOption),
             g_sys.remoteFieldIndex);
}

// End-Of-File
