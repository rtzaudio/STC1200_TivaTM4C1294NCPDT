/***************************************************************************
 *
 * STC-1200 Digital Search Timer Cue Controller for Ampex MM-1200.
 *
 * Copyright (C) 2016-2019, RTZ Professional Audio, LLC
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
#include <xdc/runtime/Memory.h>

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
#include <ti/drivers/SPI.h>
#include <ti/drivers/SDSPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>

/* TI-RTOS NDK files */
#include <ti/ndk/inc/netmain.h>

#include <file.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* STC1200 Board Header file */
#include "STC1200.h"
#include "Board.h"

extern SYSDATA g_sysData;
extern SYSPARMS g_sysParms;

/* Static CGI callback functions */

static int GetHexStr(char* textbuf, uint8_t* databuf, int len);
static Int sendIndexHtml(SOCKET s, int length);

//*****************************************************************************
// Main Entry Point
//*****************************************************************************

Void AddWebFiles(Void)
{
    efs_createfile("index.html", 0, (UINT8 *)&sendIndexHtml);
}

Void RemoveWebFiles(Void)
{
    efs_destroyfile("index.html");
}

//*****************************************************************************
// Helper Functions
//*****************************************************************************

int GetHexStr(char* textbuf, uint8_t* databuf, int len)
{
    char *p = textbuf;
    uint8_t *d;
    uint32_t i;
    int32_t l;

    /* Null output text buffer initially */
    *textbuf = 0;

    /* Make sure buffer length is not zero */
    if (!len)
        return 0;

    /* Read data bytes in reverse order so we print most significant byte first */
    d = databuf + (len-1);

    for (i=0; i < len; i++)
    {
        l = sprintf(p, "%02X", *d--);
        p += l;

        if (((i % 2) == 1) && (i != (len-1)))
        {
            l = sprintf(p, "-");
            p += l;
        }
    }

    return strlen(textbuf);
}


//*****************************************************************************
// CGI Callback Functions
//*****************************************************************************

Int sendIndexHtml(SOCKET s, int length)
{
    Char buf[128];
    Char serialnum[64];

    /*  Format the 64 bit GUID as a string */
    GetHexStr(serialnum, g_sysData.ui8SerialNumber, 16);

    //httpSendClientStr(s, "<!DOCTYPE html>");
    httpSendClientStr(s, "<html>");
    httpSendClientStr(s, "<title>STC-1200 | home</title>");
    httpSendClientStr(s, "<meta charset=""utf-8"">");
    httpSendClientStr(s, "<head>");
    httpSendClientStr(s, "<meta name=""viewport"" content=""width=device-width, initial-scale=1.0"">");
    httpSendClientStr(s, "<link rel=""stylesheet"" type=""text/css"" href=""css/style.css"">");
    httpSendClientStr(s, "</head>");
    httpSendClientStr(s, "<body>");
    httpSendClientStr(s, "  <ul class=""sidenav"">");
    httpSendClientStr(s, "    <li><a class=""active"" href=""index.html"">HOME</a></li>");
    httpSendClientStr(s, "    <li><a href=""config.html"">CONFIG</a></li>");
    httpSendClientStr(s, "    <li><a href=""remote.html"">REMOTE</a></li>");
    httpSendClientStr(s, "  </ul>");
    httpSendClientStr(s, "  <div class=""content"">");
    httpSendClientStr(s, "    <h1 class=""font-x2 btmspace-10"">MM1200 Server</h1>");
    System_sprintf(buf,  "    <p>STC Firmware v%d.%02d</p>", FIRMWARE_VER, FIRMWARE_REV);
    httpSendClientStr(s, buf);
    System_sprintf(buf,  "    <p>Serial# %s</p>", serialnum);
    httpSendClientStr(s, buf);
    System_sprintf(buf,  "    <p>Tape Speed %dips</p>", g_sysData.tapeSpeed);
    httpSendClientStr(s, buf);
    httpSendClientStr(s, "  </div>");
    httpSendClientStr(s, "</body>");
    httpSendClientStr(s, "</html>");
    return 1;
}

/* End-Of-File */
