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
#include <DRC1200Task.h>
#include <math.h>
#include <stdbool.h>

/* Graphiclib Header file */
#include <grlib/grlib.h>
#include "drivers/offscrmono.h"

/* PMX42 Board Header file */
#include "Board.h"
#include "STC1200.h"
#include "RAMPServer.h"
#include "DRC1200Task.h"

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
static void ClearDisplay();
static void DisplayWelcome();
static void DrawScreen(uint32_t uScreenNum);
static int GetHexStr(char* pTextBuf, uint8_t* pDataBuf, int len);

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

Void DRC1200TaskFxn(UArg arg0, UArg arg1)
{
    RAMP_MSG msg;

    if (!RAMP_Server_init()) {
        System_abort("RAMP server init failed");
    }

    ClearDisplay();

    DisplayWelcome();

    while (TRUE)
    {
        /* Wait for a message up to 1 second */
        if (!Mailbox_pend(g_mailboxRemote, &msg, 1000))
        {
            /* No message, blink the LED */
            //GPIO_toggle(Board_STAT_LED1);
            continue;
        }

        switch(msg.type)
        {
        case MSG_TYPE_DISPLAY:
            if (msg.param1.U < LAST_SCREEN)
                s_uScreenNum = msg.param1.U;
            DrawScreen(s_uScreenNum);
            break;

        case MSG_TYPE_SWITCH:
            ++s_uScreenNum;
            if (s_uScreenNum > LAST_SCREEN)
                s_uScreenNum = 0;
            DrawScreen(s_uScreenNum);
            break;

        case MSG_TYPE_LED:
            if (s_uScreenNum)
                --s_uScreenNum;
            else if (!s_uScreenNum)
                s_uScreenNum = LAST_SCREEN;
            DrawScreen(s_uScreenNum);
            break;

        default:
            break;
        }
    }
}

//*****************************************************************************
// Format a data buffer into an ascii hex string.
//*****************************************************************************

int GetHexStr(char* pTextBuf, uint8_t* pDataBuf, int len)
{
	char fmt[8];
	uint32_t i;
	int32_t	l;

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
//
//*****************************************************************************

void DisplayWelcome()
{
	char buf[64];

	/* Set foreground pixel color on to 0x01 */
	GrContextForegroundSetTranslated(&g_context, 1);
	GrContextBackgroundSetTranslated(&g_context, 0);

    tRectangle rect = {0, 0, SCREEN_WIDTH-1, SCREEN_HEIGHT-1};
    GrRectDraw(&g_context, &rect);

    /* Setup font */

	uint32_t y;
	uint32_t height;
	uint32_t spacing = 2;

    /* Display the program version/revision */
    y = 4;
	GrContextFontSet(&g_context, g_psFontCm28b);
    height = GrStringHeightGet(&g_context);
    GrStringDraw(&g_context, "STC-1200", -1, 21, y, 0);
    y += height;

    /* Switch to fixed system font */
    GrContextFontSet(&g_context, g_psFontFixed6x8);
    height = GrStringHeightGet(&g_context);

    sprintf(buf, "Firmware v%d.%02d", FIRMWARE_VER, FIRMWARE_REV);
    GrStringDraw(&g_context, buf, -1, 25, y, 0);
    y += height + spacing + 2;

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
// Display the current measurement screen data
//*****************************************************************************

void DrawScreen(uint32_t uScreenNum)
{
	char buf[64];
	int len;
	uint32_t y = 0;
	uint32_t height;
	uint32_t width;
	uint32_t spacing = 0;
    //tRectangle rect;

	ClearDisplay();

	/* Set foreground pixel color on to 0x01 */
	GrContextForegroundSetTranslated(&g_context, 1);
	GrContextBackgroundSetTranslated(&g_context, 0);

    switch(uScreenNum)
    {
		/* Display Normal Tape Time Screen */
		case 0:
			/* Top line fixed system font in inverse */
			GrContextFontSet(&g_context, g_psFontFixed6x8);
			height = GrStringHeightGet(&g_context);

			/* Inverse Mono */
			//GrContextForegroundSetTranslated(&g_context, 0);
			//GrContextBackgroundSetTranslated(&g_context, 1);
			GrContextForegroundSetTranslated(&g_context, 1);
			GrContextBackgroundSetTranslated(&g_context, 0);

			len = sprintf(buf, "STOP");
			//width = GrStringWidthGet(&g_context, buf, len);
			GrStringDraw(&g_context, buf, -1, 0, y, 1);

			len = sprintf(buf, "30ips");
			width = GrStringWidthGet(&g_context, buf, len);
			GrStringDraw(&g_context, buf, -1, (SCREEN_WIDTH - 1) - width, y, 1);
			y += height + spacing;

			/* Normal Mono */
			GrContextForegroundSetTranslated(&g_context, 1);
			GrContextBackgroundSetTranslated(&g_context, 0);

			/* Now draw the big digits centered */

			GrContextFontSet(&g_context, g_psFontWDseg7bold24pt);
			//GrContextFontSet(&g_context, g_psFontCm30b);
			height = GrStringHeightGet(&g_context);

			len = sprintf(buf, "%c%02u:%02u:%02u",
					(g_sysData.tapeTime.flags & F_PLUS) ? '+' : '-',
					g_sysData.tapeTime.hour,
					g_sysData.tapeTime.mins,
					g_sysData.tapeTime.secs);

			//GrStringDraw(&g_context, buf, -1, 10, y, 0);
			GrStringDrawCentered(&g_context, buf, len, SCREEN_WIDTH/2, SCREEN_HEIGHT/2, FALSE);
			y += height + spacing;
			break;

		/* 4 Channel Measurement Data */
		case 1:
			/* Setup the font and get it's height */
			GrContextFontSet(&g_context,  g_psFontCmss18b);
		    height = GrStringHeightGet(&g_context);

			sprintf(buf, "%d.%02d", rand() % 1000 + 1, rand() % 10 + 1);
			GrStringDraw(&g_context, buf, -1, 0, y, 0);

			sprintf(buf, "%d.%02d", rand() % 1000 + 1, rand() % 10 + 1);
			GrStringDraw(&g_context, buf, -1, 63, y, 0);

			y += height + spacing;

			sprintf(buf, "%d.%02d", rand() % 1000 + 1, rand() % 10 + 1);
			GrStringDraw(&g_context, buf, -1, 0, y, 0);

			sprintf(buf, "%d.%02d", rand() % 1000 + 1, rand() % 10 + 1);
			GrStringDraw(&g_context, buf, -1, 63, y, 0);

			y += height + spacing;
			break;

		/* 1 Big Number Centered */
		case 2:
		    /* Top line fixed system font in inverse */
		    GrContextFontSet(&g_context, g_psFontFixed6x8);
		    height = GrStringHeightGet(&g_context);

			GrContextForegroundSetTranslated(&g_context, 0);
			GrContextBackgroundSetTranslated(&g_context, 1);

		    len = sprintf(buf, "CH-1");
			width = GrStringWidthGet(&g_context, buf, len);
			GrStringDraw(&g_context, buf, -1, 0, y, 1);

			GrContextForegroundSetTranslated(&g_context, 1);
			GrContextBackgroundSetTranslated(&g_context, 0);

		    sprintf(buf, "%d.%02dV", rand() % 999 + 1, rand() % 10 + 1);
			GrStringDraw(&g_context, buf, -1, width+4, y, 0);

			y += height + spacing;

			/* Now draw the big digits centered */

			GrContextFontSet(&g_context, g_psFontCm30b);
		    height = GrStringHeightGet(&g_context);
			len = sprintf(buf, "%d.%02dV", rand() % 999 + 1, rand() % 10 + 1);
			//GrStringDraw(&g_context, buf, -1, 10, y, 0);
			GrStringDrawCentered(&g_context, buf, len, SCREEN_WIDTH/2, SCREEN_HEIGHT/2, FALSE);

			y += height + spacing;
			break;

		default:
			break;
    }

    GrFlush(&g_context);
}

// End-Of-File