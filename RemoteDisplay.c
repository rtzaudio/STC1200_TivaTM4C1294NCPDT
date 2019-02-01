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

/* Static Function Prototypes */
static void DrawTimeTop(void);
static void DrawTimeMiddle(void);
static void DrawTimeBottom(void);
static int GetHexStr(char* pTextBuf, uint8_t* pDataBuf, int len);

/* External Global Data */
extern tContext g_context;
extern tFont *g_psFontWDseg7bold24pt;
extern tFont *g_psFontWDseg7bold20pt;
extern tFont *g_psFontWDseg7bold18pt;
extern tFont *g_psFontWDseg7bold16pt;
extern tFont *g_psFontWDseg7bold14pt;
extern tFont *g_psFontWDseg7bold12pt;
extern tFont *g_psFontWDseg7bold10pt;
extern SYSDATA g_sysData;
extern SYSPARMS g_sysParms;

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
    /* Draw the top line showing current mode/speed */
    DrawTimeTop();

    /* Draw the current tape position time in the middle */
    if (g_sysData.remoteMode != REMOTE_MODE_EDIT)
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

    /* Mono */
    GrContextForegroundSetTranslated(&g_context, 1);
    GrContextBackgroundSetTranslated(&g_context, 0);
    GrStringDraw(&g_context, buf, -1, x, y, 1);

    /* Draw current tape speed active */
    len = sprintf(buf, "%s IPS", (g_sysData.tapeSpeed == 30) ? "30" : "15");
    width = GrStringWidthGet(&g_context, buf, len);
    x = (SCREEN_WIDTH - 1) - width;
    GrStringDraw(&g_context, buf, -1, x, y, 1);
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

    if (g_sysParms.showLongTime)
    {
        GrContextFontSet(&g_context, g_psFontWDseg7bold16pt);
        height = GrStringHeightGet(&g_context);

        len = sprintf(buf, "%1u:%02u:%02u:%1u:",
                 g_sysData.tapeTime.hour,
                 g_sysData.tapeTime.mins,
                 g_sysData.tapeTime.secs,
                 g_sysData.tapeTime.tens);

        width = GrStringWidthGet(&g_context, buf, len);

        x = 11;
        y = (SCREEN_HEIGHT / 2) - ((height / 2) + 5);
        GrStringDraw(&g_context, buf, len, x, y, 0);

        GrContextFontSet(&g_context, g_psFontWDseg7bold10pt);
        len = sprintf(buf, "%02u", g_sysData.tapeTime.frame);
        GrStringDraw(&g_context, buf, len, x+width, y, 0);

        /* Draw the sign in a different font as 7-seg does not have these chars */
        GrContextFontSet(&g_context, g_psFontCmss14b);
        len = sprintf(buf, "%c", (g_sysData.tapeTime.flags & F_PLUS) ? '+' : '-');
        GrStringDrawCentered(&g_context, buf, len, 5, y+6, 1);

        y += height + 5;
        GrContextFontSet(&g_context, g_psFontFixed6x8);
        GrStringDraw(&g_context, "HR", -1, 12, y, 0);
        GrStringDraw(&g_context, "MIN", -1, 33, y, 0);
        GrStringDraw(&g_context, "SEC", -1, 63, y, 0);
        GrStringDraw(&g_context, "TEN", -1, 88, y, 0);
    }
    else
    {
        /*
         * Standard hour, mins, secs time display format
         */

        GrContextFontSet(&g_context, g_psFontWDseg7bold18pt);
        height = GrStringHeightGet(&g_context);

        len = sprintf(buf, "%1u:%02u:%02u",
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

    len = sprintf(buf, "LOC-%02u", g_sysData.currentMemIndex+1);
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

    if (CuePointGet(g_sysData.currentMemIndex, NULL) & CF_SET)
    {
        CuePointGetTime(g_sysData.currentMemIndex, &tapeTime);
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
