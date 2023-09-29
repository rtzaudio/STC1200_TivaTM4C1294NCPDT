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
#include <ti/sysbios/gates/GateMutex.h>
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
#include "STC1200TCP.h"
#include "SMPTE.h"
#include "Board.h"
#include "Utils.h"
#include "RemoteTask.h"
#include "RAMPMessage.h"
#include "IPCServer.h"
#include "IPCCommands.h"

static bool rec_arm = false;
static bool rec_active = false;

/* Static CGI callback functions */
static Int sendIndexHtml(SOCKET htmlSock, int length);
static Int sendConfigHtml(SOCKET htmlSock, int length);
static int cgiConfig(SOCKET htmlSock, int ContentLength, char *pArgs);
static Int sendRemoteHtml(SOCKET htmlSock, int length);
static int cgiRemote(SOCKET htmlSock, int ContentLength, char *pArgs);

#define html(str) httpSendClientStr(htmlSock, (char *)str)

//*****************************************************************************
// Main Entry Point
//*****************************************************************************

Void AddWebFiles(Void)
{
    efs_createfile("index.html", 0, (UINT8 *)&sendIndexHtml);
    efs_createfile("config.html", 0, (UINT8 *)&sendConfigHtml);
    efs_createfile("config.cgi", 0, (UINT8 *)&cgiConfig);
    efs_createfile("remote.html", 0, (UINT8 *)&sendRemoteHtml);
    efs_createfile("remote.cgi", 0, (UINT8 *)&cgiRemote);
}

Void RemoveWebFiles(Void)
{
    efs_destroyfile("remote.cgi");
    efs_destroyfile("remote.html");
    efs_destroyfile("config.cgi");
    efs_destroyfile("config.html");
    efs_destroyfile("index.html");
}

//*****************************************************************************
// CGI Callback Functions
//*****************************************************************************

static const char strCopyright[] = "Copyright &copy; 2021-2023, RTZ Professional Audio\r\n";

static Int sendIndexHtml(SOCKET htmlSock, int length)
{
    Char buf[MAX_RESPONSE_SIZE];
    Char serialnum[64];
    Char mac[32];

    /* Format the 128-bit serial number as a string */
    GetSerialNumStr(serialnum, g_sys.ui8SerialNumberSTC);

    /* Format the MAC address as a string */
    GetMACAddrStr(mac, g_sys.ui8MAC);

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
    html("<div id=\"top\">\r\n");
    html("<h1>STC-1200</h1>\r\n");
    html("<p>Tape Machine Admin</p>\r\n");
    html("</div>\r\n");
    html("<div class=\"wrapper\">\r\n");
    html("<div id=\"menubar\">\r\n");
    html("<ul id=\"menulist\">\r\n");
    html("<li class=\"menuitem active\" onclick=\"window.location.href='index.html'\">Home\r\n");
    html("<li class=\"menuitem\" onclick=\"window.location.href='config.html'\">Configure\r\n");
    html("<li class=\"menuitem\" onclick=\"window.location.href='remote.html'\">Remote\r\n");
    html("</ul>\r\n");
    html("</div>\r\n");
    html("<div id=\"main\">\r\n");

    html("<fieldset>\r\n");
    html("<legend class=\"bold\">System Summary</legend>\r\n");

    System_sprintf(buf, "<p>Firmware version: %d.%02d.%03d</p>\r\n", FIRMWARE_VER, FIRMWARE_REV, FIRMWARE_BUILD);
    html(buf);
    System_sprintf(buf, "<p>PCB serial#: %s</p>\r\n", serialnum);
    html(buf);
    System_sprintf(buf, "<p>MAC address: %s</p>\r\n", mac);
    html(buf);
    System_sprintf(buf, "<p>IP address: %s</p>\r\n", g_sys.ipAddr);
    html(buf);
    System_sprintf(buf, "<p>Tape speed: %d IPS</p>\r\n", g_sys.tapeSpeed);
    html(buf);
    System_sprintf(buf, "<p>Roller encoder errors: %d</p>\r\n", g_sys.qei_error_cnt);
    html(buf);
    System_sprintf(buf, "<p>Time of day clock: %s</p>\r\n", g_sys.rtcFound ? "RTC" : "CPU");
    html(buf);

    html("<p>DCS track controller: ");
    if (g_sys.dcsFound)
        System_sprintf(buf, "%d tracks</p>\r\n", g_sys.trackCount);
    else
        System_sprintf(buf, "N/A</p>\r\n");
    html(buf);

    html("<p>SMPTE time code card: ");
    if (g_sys.smpteFound)
    {
        switch(g_sys.smpteMode)
        {
        case STC_SMPTE_OFF:         /* smpte module off           */
            html("Ready");
            break;
        case STC_SMPTE_ENCODER:     /* master stripe mode active  */
            html("Master");
            break;
        case STC_SMPTE_SLAVE:       /* slave mode decode active   */
            html("Slave");
            break;
        default:
            html("N/A");
            break;
        }

        html(" ");

        switch(g_cfg.smpteFPS)
        {
        case SMPTE_CTL_FPS24:
            html("24");
            break;

        case SMPTE_CTL_FPS25:
            html("25");
            break;

        case SMPTE_CTL_FPS30:
            html("30");
            break;

        case SMPTE_CTL_FPS30D:
            html("30D");
            break;

        default:
            html("0");
            break;
        }
        html(" fps</p>\r\n");
    }
    else
    {
        html("N/A</p>\r\n");
    }
    html("</fieldset><br /><br />\r\n");

    html("</div>\r\n");
    html("</div>\r\n");
    html("<div id=\"bottom\">\r\n");
    html(strCopyright);
    html("</div>\r\n");
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
    html("<div id=\"top\">\r\n");
    html("<h1>STC-1200</h1>\r\n");
    html("<p>Tape Machine Admin</p>\r\n");
    html("</div>\r\n");
    html("<div class=\"wrapper\">\r\n");
    html("<div id=\"menubar\">\r\n");
    html("<ul id=\"menulist\">\r\n");
    html("<li class=\"menuitem\" onclick=\"window.location.href='index.html'\">Home\r\n");
    html("<li class=\"menuitem active\" onclick=\"window.location.href='config.html'\">Configure\r\n");
    html("<li class=\"menuitem\" onclick=\"window.location.href='remote.html'\">Remote\r\n");
    html("</ul>\r\n");
    html("</div>\r\n");

    html("<div id=\"main\">\r\n");
    html("<form action=\"config.cgi\" method=\"post\">\r\n");

    /* General Settings */

    html("<fieldset>\r\n");
    html("<legend class=\"bold\">General Settings</legend>\r\n");
    System_sprintf(buf, "<input type=\"checkbox\" name=\"longtime\" value=\"yes\" %s> Remote displays long tape time format?<br />\r\n", g_cfg.showLongTime ? "checked" : "");
    html(buf);
    System_sprintf(buf, "<input type=\"checkbox\" name=\"blink\" value=\"yes\" %s> Blink machines 7-seg display during locates?<br />\r\n", g_cfg.searchBlink ? "checked" : "");
    html(buf);
    html("</fieldset><br />\r\n");

    /* Locator Settings */

    html("<fieldset>\r\n");
    html("<legend class=\"bold\">Locator Settings</legend>\r\n");
    System_sprintf(buf, "Jog near velocity:<br><input type=\"text\" name=\"jognear\" value=\"%u\"><br />\r\n", g_cfg.jog_vel_near);
    html(buf);
    System_sprintf(buf, "Jog mid velocity:<br><input type=\"text\" name=\"jogmid\" value=\"%u\"><br />\r\n", g_cfg.jog_vel_mid);
    html(buf);
    System_sprintf(buf, "Jog far velocity:<br><input type=\"text\" name=\"jogfar\" value=\"%u\"><br />\r\n", g_cfg.jog_vel_far);
    html(buf);
    html("</fieldset><br /><br />\r\n");

    /* End of form Save/Reset buttons */

    html("<input class=\"btn\" type=\"submit\" name=\"submit\" value=\"Save\">\r\n");
    html("<input class=\"btn\" type=\"reset\" name=\"submit\" value=\"Reset\">\r\n");
    html("</form>\r\n");
    html("</div>\r\n");

    html("</div>\r\n");
    html("<div id=\"bottom\">\r\n");
    html(strCopyright);
    html("</div>\r\n");
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

    g_cfg.showLongTime = false;
    g_cfg.searchBlink  = false;

    // Process request variables until there are none left
    do
    {
        key   = cgiParseVars(buffer, &parseIndex);
        value = cgiParseVars(buffer, &parseIndex);

        if (!strcmp("longtime", key))
            g_cfg.showLongTime = (strcmp(value, "yes") == 0) ? true : false;
        else if (!strcmp("blink", key))
            g_cfg.searchBlink = (strcmp(value, "yes") == 0) ? true : false;
        else if (!strcmp("jognear", key))
        {
            val = atoi(value);
            if ((val >= 0) && (val <= 300))
                g_cfg.jog_vel_near = (uint32_t)val;
        }
        else if (!strcmp("jogmid", key))
        {
            val = atoi(value);
            if ((val >= 0) && (val <= 500))
                g_cfg.jog_vel_mid = (uint32_t)val;
        }
        else if (!strcmp("jogfar", key))
        {
            val = atoi(value);
            if ((val >= 0) && (val <= 1100))
                g_cfg.jog_vel_far = (uint32_t)val;
        }
    } while(parseIndex != -1);

    // Output the data we read in...
    httpSendStatusLine(htmlSock, HTTP_OK, CONTENT_TYPE_HTML);
    // CRLF before entity
    html(CRLF);

    /* Write system parameters to EPROM */
    ConfigSave(1);

    // Send the updated page
    sendConfigHtml(htmlSock, 0);

ERROR:
    if (buffer)
        mmBulkFree(buffer);

    return 1;
}

//*****************************************************************************
// Remote Handler Functions
//*****************************************************************************

static Int sendRemoteHtml(SOCKET htmlSock, int length)
{
    Char buf[MAX_RESPONSE_SIZE];

    html("<!DOCTYPE html>\r\n");
    html("<html>\r\n");
    html("<title>STC-1200 | remote</title>\r\n");
    html("<meta charset=\"utf-8\">\r\n");
    html("<head>\r\n");
    html("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\r\n");
    html("<link rel=\"stylesheet\" type=\"text/css\" href=\"css/style.css\">\r\n");
    html("</head>\r\n");
    html("<body>\r\n");
    html("<div class=\"container wrapper\">\r\n");
//    html("  <div id=\"top\">\r\n");
//    html("    <p>REMOTE</p>\r\n");
//    html("  </div>\r\n");
    html("<div class=\"wrapper\">\r\n");
    html("<div id=\"menubar\">\r\n");
    html("<ul id=\"menulist\">\r\n");
    html("<li class=\"menuitem\" onclick=\"window.location.href='index.html'\">Home\r\n");
    html("<li class=\"menuitem\" onclick=\"window.location.href='config.html'\">Configure\r\n");
    html("<li class=\"menuitem active\" onclick=\"window.location.href='remote.html'\">Remote\r\n");
    html("</ul>\r\n");
    html("</div>\r\n");
    html("<div id=\"main\">\r\n");
    html("<form action=\"remote.cgi\" method=\"post\">\r\n");

    html("<fieldset>\r\n");
    html("<legend>Transport</legend>\r\n");
    if (rec_arm)
    {
        System_sprintf(buf, "<input class=\"btnrec0\" type=\"submit\" name=\"rec\" value=\"REC\">\r\n");
    }
    else
    {
        if (rec_active) //g_sysData.ledMaskTransport & L_REC)
            System_sprintf(buf, "<input class=\"btnrec1\" type=\"submit\" name=\"rec\" value=\"REC\">\r\n");
        else
            System_sprintf(buf, "<input class=\"btn\" type=\"submit\" name=\"rec\" value=\"REC\">\r\n");
    }
    html(buf);
    html("<input class=\"btn\" type=\"submit\" name=\"play\" value=\"PLAY\">\r\n");
    html("<input class=\"btn\" type=\"submit\" name=\"rew\" value=\"REW\">\r\n");
    html("<input class=\"btn\" type=\"submit\" name=\"fwd\" value=\"FWD\">\r\n");
    html("<input class=\"btn\" type=\"submit\" name=\"stop\" value=\"STOP\">\r\n");
    html("</fieldset>\r\n");

    html("<fieldset>\r\n");
    html("<legend>Mode</legend>\r\n");
    System_sprintf(buf, "<input class=\"btn%c\" type=\"submit\" name=\"cue\" value=\"CUE\">\r\n", (g_sys.ledMaskRemote & L_CUE) ? '1' : '0');
    html(buf);
    System_sprintf(buf, "<input class=\"btn%c\" type=\"submit\" name=\"store\" value=\"STORE\">\r\n", (g_sys.ledMaskRemote & L_STORE) ? '1' : '0');
    html(buf);
    html("</fieldset>\r\n");

    html("<fieldset>\r\n");
    html("<legend>Locate</legend>\r\n");
    System_sprintf(buf, "<input class=\"btn%c\" type=\"submit\" name=\"loc1\" value=\"LOC-1\">\r\n", (g_sys.ledMaskRemote & L_LOC1) ? '1' : '0');
    html(buf);
    System_sprintf(buf, "<input class=\"btn%c\" type=\"submit\" name=\"loc2\" value=\"LOC-2\">\r\n", (g_sys.ledMaskRemote & L_LOC2) ? '1' : '0');
    html(buf);
    System_sprintf(buf, "<input class=\"btn%c\" type=\"submit\" name=\"loc3\" value=\"LOC-3\">\r\n", (g_sys.ledMaskRemote & L_LOC3) ? '1' : '0');
    html(buf);
    System_sprintf(buf, "<input class=\"btn%c\" type=\"submit\" name=\"loc4\" value=\"LOC-4\">\r\n", (g_sys.ledMaskRemote & L_LOC4) ? '1' : '0');
    html(buf);
    System_sprintf(buf, "<input class=\"btn%c\" type=\"submit\" name=\"loc5\" value=\"LOC-5\"><br />\r\n", (g_sys.ledMaskRemote & L_LOC5) ? '1' : '0');
    html(buf);
    System_sprintf(buf, "<input class=\"btn%c\" type=\"submit\" name=\"loc6\" value=\"LOC-6\">\r\n", (g_sys.ledMaskRemote & L_LOC6) ? '1' : '0');
    html(buf);
    System_sprintf(buf, "<input class=\"btn%c\" type=\"submit\" name=\"loc7\" value=\"LOC-7\">\r\n", (g_sys.ledMaskRemote & L_LOC7) ? '1' : '0');
    html(buf);
    System_sprintf(buf, "<input class=\"btn%c\" type=\"submit\" name=\"loc8\" value=\"LOC-8\">\r\n", (g_sys.ledMaskRemote & L_LOC8) ? '1' : '0');
    html(buf);
    System_sprintf(buf, "<input class=\"btn%c\" type=\"submit\" name=\"loc9\" value=\"LOC-9\">\r\n", (g_sys.ledMaskRemote & L_LOC9) ? '1' : '0');
    html(buf);
    System_sprintf(buf, "<input class=\"btn%c\" type=\"submit\" name=\"loc0\" value=\"LOC-0\">\r\n", (g_sys.ledMaskRemote & L_LOC0) ? '1' : '0');
    html(buf);
    html("</fieldset>\r\n");

    html("</form>\r\n");
    html("</div>\r\n");
    html("</div>\r\n");
    //html("<div id=\"bottom\">\r\n");
    //html(strCopyright);
    //html("</div>\r\n");
    html("</div>\r\n");
    html("</body>\r\n");
    html("</html>\r\n");

    return 1;
}

static int cgiRemote(SOCKET htmlSock, int ContentLength, char *pArgs )
{
    char    *buffer, *key, *value;
    int     len;
    int     parseIndex;
    //int     val;

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

    // Process request variables until there are none left
    do
    {
        key   = cgiParseVars(buffer, &parseIndex);
        value = cgiParseVars(buffer, &parseIndex);

        (void)value;

        /*** Transport Buttons ***/
        if (!strcmp("stop", key))
        {
            Transport_PostButtonPress(S_STOP);
            rec_arm = rec_active = false;
        }
        else if (!strcmp("play", key))
        {
            if (rec_arm)
            {
                Transport_PostButtonPress(S_PLAY|S_REC);
                rec_active = true;
                rec_arm = false;
            }
            else
            {
                Transport_Play(0);
                rec_arm = rec_active = false;
            }
        }
        else if (!strcmp("rew", key))
        {
            Transport_PostButtonPress(S_REW);
            rec_arm = rec_active = false;
        }
        else if (!strcmp("fwd", key))
        {
            Transport_PostButtonPress(S_FWD);
            rec_arm = rec_active = false;
        }
        else if (!strcmp("rec", key))
        {
            if (rec_arm)
                rec_arm = false;
            else
                rec_arm = true;
        }
        else
        {
            rec_arm = rec_active = false;

            /*** Mode Buttons ***/
            if (!strcmp("cue", key))
            {
                Remote_PostSwitchPress(SW_CUE, 0);
            }
            else if (!strcmp("store", key))
            {
                Remote_PostSwitchPress(SW_STORE, 0);
            }
            /*** Locate Buttons ***/
            if (!strcmp("loc1", key))
            {
                Remote_PostSwitchPress(SW_LOC1, 0);
            }
            else if (!strcmp("loc2", key))
            {
                Remote_PostSwitchPress(SW_LOC2, 0);
            }
            else if (!strcmp("loc3", key))
            {
                Remote_PostSwitchPress(SW_LOC3, 0);
            }
            else if (!strcmp("loc4", key))
            {
                Remote_PostSwitchPress(SW_LOC4, 0);
            }
            else if (!strcmp("loc5", key))
            {
                Remote_PostSwitchPress(SW_LOC5, 0);
            }
            else if (!strcmp("loc6", key))
            {
                Remote_PostSwitchPress(SW_LOC6, 0);
            }
            else if (!strcmp("loc7", key))
            {
                Remote_PostSwitchPress(SW_LOC7, 0);
            }
            else if (!strcmp("loc8", key))
            {
                Remote_PostSwitchPress(SW_LOC8, 0);
            }
            else if (!strcmp("loc9", key))
            {
                Remote_PostSwitchPress(SW_LOC9, 0);
            }
            else if (!strcmp("loc0", key))
            {
                Remote_PostSwitchPress(SW_LOC0, 0);
            }
        }
    } while(parseIndex != -1);

    // Output the data we read in...
    httpSendStatusLine(htmlSock, HTTP_OK, CONTENT_TYPE_HTML);
    // CRLF before entity
    html(CRLF);

    // Send the updated page
    sendRemoteHtml(htmlSock, 0);

ERROR:
    if (buffer)
        mmBulkFree(buffer);

    return 1;
}

/* End-Of-File */
