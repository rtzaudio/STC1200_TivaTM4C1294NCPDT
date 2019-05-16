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
#include <ti/ndk/inc/stkmain.h>
#include <ti/ndk/inc/tools/cgiparse.h>
#include <ti/ndk/inc/tools/cgiparsem.h>
#include <ti/ndk/inc/tools/console.h>

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
static Int sendIndexHtml(SOCKET htmlSock, int length);
static Int sendConfigHtml(SOCKET htmlSock, int length);
static int cgiConfig(SOCKET htmlSock, int ContentLength, char *pArgs);

#define html(str) httpSendClientStr(htmlSock, (char *)str)

//*****************************************************************************
// Main Entry Point
//*****************************************************************************

Void AddWebFiles(Void)
{
    efs_createfile("index.html", 0, (UINT8 *)&sendIndexHtml);
    efs_createfile("config.html", 0, (UINT8 *)&sendConfigHtml);
    efs_createfile("config.cgi", 0, (UINT8 *)&cgiConfig);
}

Void RemoveWebFiles(Void)
{
    efs_destroyfile("config.cgi");
    efs_destroyfile("config.html");
    efs_destroyfile("index.html");
}

//*****************************************************************************
// CGI Callback Functions
//*****************************************************************************

static Int sendIndexHtml(SOCKET htmlSock, int length)
{
    Char buf[MAX_RESPONSE_SIZE];
    Char serialnum[64];

    /*  Format the 64 bit GUID as a string */
    GetHexStr(serialnum, g_sysData.ui8SerialNumber, 16);

    html("<!DOCTYPE html>\r\n");
    html("<html>\r\n");
    html("<title>STC-1200 | home</title>\r\n");
    html("<meta charset=\"utf-8\">\r\n");
    html("<head>\r\n");
    html("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\r\n");
    html("<link rel=\"stylesheet\" type=\"text/css\" href=\"css/style.css\">\r\n");
    html("</head>\r\n");
    html("<body>\r\n");
    html("<div class=\"container wrapper\">\r\n");
    html("  <div id=\"top\">\r\n");
    html("    <img class=\"imgr\" src=\"images/MM1200_01.png\" />\r\n");
    html("    <h1>STC-1200</h1>\r\n");
    html("    <p>Tape Machine Services</p>\r\n");
    html("  </div>\r\n");
    html("  <div class=\"wrapper\">\r\n");
    html("    <div id=\"menubar\">\r\n");
    html("      <ul id=\"menulist\">\r\n");
    html("        <li class=\"menuitem active\" onclick=\"window.location.href='index.html'\">Home\r\n");
    html("        <li class=\"menuitem\" onclick=\"window.location.href='config.html'\">Configure\r\n");
    html("        <li class=\"menuitem\" onclick=\"window.location.href='remote.html'\">Remote\r\n");
    html("      </ul>\r\n");
    html("    </div>\r\n");
    html("    <div id=\"main\">\r\n");
    System_sprintf(buf,  "    <p>Firmware Version: %d.%02d.%03d</p>\r\n", FIRMWARE_VER, FIRMWARE_REV, FIRMWARE_BUILD);
    html(buf);
    System_sprintf(buf,  "    <p>PCB Serial#: %s</p>\r\n", serialnum);
    html(buf);
    System_sprintf(buf,  "    <p>IP Address: %s</p>\r\n", g_sysData.ipAddr);
    html(buf);
    System_sprintf(buf,  "    <p>Tape Speed: %d IPS</p>\r\n", g_sysData.tapeSpeed);
    html(buf);
    System_sprintf(buf,  "    <p>Encoder Errors: %d</p>\r\n", g_sysData.qei_error_cnt);
    html(buf);
    html("    </div>\r\n");
    html("  </div>\r\n");
    html("  <div id=\"bottom\">\r\n");
    html("    Copyright &copy; 2019, RTZ Professional Audio, LLC\r\n");
    html("  </div>\r\n");
    html("</div>\r\n");
    html("</body>\r\n");
    html("</html>\r\n");

    return 1;
}

static Int sendConfigHtml(SOCKET htmlSock, int length)
{
    Char buf[MAX_RESPONSE_SIZE];

    html("<!DOCTYPE html>\r\n");
    html("<html>\r\n");
    html("<title>STC-1200 | config</title>\r\n");
    html("<meta charset=\"utf-8\">\r\n");
    html("<head>\r\n");
    html("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\r\n");
    html("<link rel=\"stylesheet\" type=\"text/css\" href=\"css/style.css\">\r\n");
    html("</head>\r\n");
    html("<body>\r\n");
    html("<div class=\"container wrapper\">\r\n");
    html("  <div id=\"top\">\r\n");
    html("    <img class=\"imgr\" src=\"images/MM1200_01.png\" />\r\n");
    html("    <h1>STC-1200</h1>\r\n");
    html("    <p>Tape Machine Services</p>\r\n");
    html("  </div>\r\n");
    html("  <div class=\"wrapper\">\r\n");
    html("    <div id=\"menubar\">\r\n");
    html("      <ul id=\"menulist\">\r\n");
    html("        <li class=\"menuitem\" onclick=\"window.location.href='index.html'\">Home\r\n");
    html("        <li class=\"menuitem active\" onclick=\"window.location.href='config.html'\">Configure\r\n");
    html("        <li class=\"menuitem\" onclick=\"window.location.href='remote.html'\">Remote\r\n");
    html("      </ul>\r\n");
    html("    </div>\r\n");
    html("    <div id=\"main\">\r\n");
    html("      <form action=\"config.cgi\" method=\"post\">\r\n");
    html("        <p class=\"bold\">General Settings</p>\r\n");
    System_sprintf(buf, "        <input type=\"checkbox\" name=\"longtime\" value=\"yes\" %s> Remote displays long tape time format?\r\n",
                       g_sysParms.showLongTime ? "checked" : "");
    html(buf);
    html("        <br />\r\n");
    System_sprintf(buf, "        <input type=\"checkbox\" name=\"blink\" value=\"yes\" %s> Blink machines 7-seg display during locates?\r\n",
                   g_sysParms.searchBlink ? "checked" : "");
    html(buf);
    html("        <p class=\"bold\">Locator Settings</p>\r\n");

    System_sprintf(buf, "         Jog near velocity:<br><input type=\"text\" name=\"jognear\" value=\"%u\"> <br />\r\n", g_sysParms.jog_vel_near);
    html(buf);

    System_sprintf(buf, "         Jog mid velocity:<br><input type=\"text\" name=\"jogmid\" value=\"%u\"> <br />\r\n", g_sysParms.jog_vel_mid);
    html(buf);

    System_sprintf(buf, "         Jog far velocity:<br><input type=\"text\" name=\"jogfar\" value=\"%u\"> <br />\r\n", g_sysParms.jog_vel_far);
    html(buf);

    html("        <br />\r\n");
    html("        <input type=\"submit\" name=\"submit\" value=\"Save\">\r\n");
    html("        <input type=\"reset\" reset=\"submit\" value=\"Reset\">\r\n");
    html("      </form>\r\n");
    html("    </div>\r\n");
    html("  </div>\r\n");
    html("  <div id=\"bottom\">\r\n");
    html("    Copyright &copy; 2019, RTZ Professional Audio, LLC\r\n");
    html("  </div>\r\n");
    html("</div>\r\n");
    html("</body>\r\n");
    html("</html>\r\n");

    return 1;
}

//*****************************************************************************
// CGI Handler Functions
//*****************************************************************************

// This function processes the sample CGI from off the config
// page on the HTTP server.
//
// CGI Functions must return 1 if the socket is left open,
// and zero if the socket is closed. This example always
// returns 1.
//
static int cgiConfig(SOCKET htmlSock, int ContentLength, char *pArgs )
{
    char    *buffer, *key, *value;
    int     len;
    int     parseIndex;
    int     val;

    // CGI Functions can now support URI arguments as well if the
    // pArgs pointer is not NULL, and the ContentLength were zero,
    // we could parse the arguments off of pArgs instead.

    // First, allocate a buffer for the request
    buffer = (char*) mmBulkAlloc( ContentLength );
    if ( !buffer )
        goto ERROR;

    // Now read the data from the client
    len = recv( htmlSock, buffer, ContentLength, MSG_WAITALL );
    if ( len < 1 )
        goto ERROR;

    // Setup to parse the post data
    parseIndex = 0;
    buffer[ContentLength] = '\0';

    g_sysParms.showLongTime = false;
    g_sysParms.searchBlink  = false;

    // Process request variables until there are none left
    do
    {
        key   = cgiParseVars(buffer, &parseIndex);
        value = cgiParseVars(buffer, &parseIndex);

        if (!strcmp("longtime", key))
            g_sysParms.showLongTime = (strcmp(value, "yes") == 0) ? true : false;
        else if (!strcmp("blink", key))
            g_sysParms.searchBlink = (strcmp(value, "yes") == 0) ? true : false;
        else if (!strcmp("jognear", key))
        {
            val = atoi(value);
            if ((val >= 0) && (val <= 300))
                g_sysParms.jog_vel_near = (uint32_t)val;
        }
        else if (!strcmp("jogmid", key))
        {
            val = atoi(value);
            if ((val >= 0) && (val <= 500))
                g_sysParms.jog_vel_mid = (uint32_t)val;
        }
        else if (!strcmp("jogfar", key))
        {
            val = atoi(value);
            if ((val >= 0) && (val <= 1100))
                g_sysParms.jog_vel_far = (uint32_t)val;
        }
    } while(parseIndex != -1);

    // Output the data we read in...
    httpSendStatusLine(htmlSock, HTTP_OK, CONTENT_TYPE_HTML);
    // CRLF before entity
    html(CRLF);

    /* Write system parameters to EPROM */
    SysParamsWrite(&g_sysParms);

    // Send the updated page
    sendConfigHtml(htmlSock, 0);

ERROR:
    if (buffer)
        mmBulkFree(buffer);

    return 1;
}

/* End-Of-File */
