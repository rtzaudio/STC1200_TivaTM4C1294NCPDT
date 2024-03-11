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
#include "SMPTE.h"
#include "Board.h"
#include "Utils.h"
#include "RemoteTask.h"
#include "RAMPMessage.h"
#include "IPCServer.h"
#include "IPCCommands.h"

static bool rec_arm = false;
static bool rec_active = false;

static const char strCopyright[] = "Copyright &copy; 2021-2024, RTZ Professional Audio\r\n";

/* Static CGI callback functions */
static Int sendIndexHtml(SOCKET htmlSock, int length);
static Int sendConfigHtml(SOCKET htmlSock, int length);
static int cgiConfig(SOCKET htmlSock, int ContentLength, char *pArgs);
static Int sendRemoteHtml(SOCKET htmlSock, int length);
static int cgiRemote(SOCKET htmlSock, int ContentLength, char *pArgs);

static void emitHeader(SOCKET htmlSock, int menu_active, char* page_title);
static void emitFooter(SOCKET htmlSock);
static char *getstrYesNo(int f);
static char *getstrFPS(void);
static char *getstrSMPTEMode(void);

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
//
//*****************************************************************************

char* getstrFPS(void)
{
    char *p;

    switch(g_sys.cfgSTC.smpteFPS)
    {
    case SMPTE_CTL_FPS24:
        p = "24";
        break;
    case SMPTE_CTL_FPS25:
        p = "25";
        break;
    case SMPTE_CTL_FPS30:
        p = "30";
        break;
    case SMPTE_CTL_FPS30D:
        p = "30D";
        break;
    default:
        p = "0";
        break;
    }

    return p;
}

char* getstrSMPTEMode(void)
{
    char *p;

    switch(g_sys.smpteMode)
    {
    case STC_SMPTE_OFF:         /* smpte module off           */
        p = "Ready";
        break;
    case STC_SMPTE_ENCODER:     /* master stripe mode active  */
        p = "Master";
        break;
    case STC_SMPTE_SLAVE:       /* slave mode decode active   */
        p = "Slave";
        break;
    default:
        p = "N/A";
        break;
    }

    return p;
}

char* getstrYesNo(int f)
{
    return f ? "\"yes\"" : "\"no\"";
}

//*****************************************************************************
//
//*****************************************************************************

void emitHeader(SOCKET htmlSock, int menu_active, char* page_title)
{
    char buf[128];

    html("<!DOCTYPE html>\r\n");
    html("<html>\r\n");
    System_sprintf(buf, "<title>STC-1200 | %s</title>\r\n", page_title);
    html(buf);
    html("<meta charset=\"utf-8\">\r\n");
    html("<head>\r\n");
    html("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\r\n");
    html("<link rel=\"stylesheet\" type=\"text/css\" href=\"css/style.css\">\r\n");
    html("</head>\r\n");
    html("<body>\r\n");
    html("<div class=\"container wrapper\">\r\n");
    html("<div id=\"top\">\r\n");
    html("<h1>STC-1200</h1>\r\n");
    html("<p>admin server</p>\r\n");
    html("</div>\r\n");

    if (menu_active >= 0)
    {
        html("<div class=\"wrapper\">\r\n");
        html("<div id=\"menubar\">\r\n");
        html("<ul id=\"menulist\">\r\n");
        if (menu_active >= 0)
        System_sprintf(buf, "<li class=\"menuitem %s\" onclick=\"window.location.href='index.html'\">Home\r\n",
             (menu_active == 0) ? "active" : "");
        html(buf);
        System_sprintf(buf, "<li class=\"menuitem %s\" onclick=\"window.location.href='config.html'\">Configure\r\n",
             (menu_active == 1) ? "active" : "");
        html(buf);
        System_sprintf(buf, "<li class=\"menuitem %s\" onclick=\"window.location.href='remote.html'\">Remote\r\n",
             (menu_active == 2) ? "active" : "");
        html(buf);
        html("</ul>\r\n");
        html("</div>\r\n");
    }
}

void emitFooter(SOCKET htmlSock)
{
    html("</div>\r\n");
    html("</div>\r\n");
    html("<div id=\"bottom\">\r\n");
    html(strCopyright);
    html("</div>\r\n");
    html("</div>\r\n");
    html("</body>\r\n");
    html("</html>\r\n");
}

//*****************************************************************************
// CGI Index Page Callback Function
//*****************************************************************************

static Int sendIndexHtml(SOCKET htmlSock, int length)
{
    Char buf[MAX_RESPONSE_SIZE];
    Char serialnum[64];
    Char mac[32];
    Char timestr[16];
    Char datestr[16];

    /* Format the 128-bit serial number as a string */
    GetSerialNumStr(serialnum, g_sys.ui8SerialNumberSTC);
    /* Format the MAC address as a string */
    GetMACAddrStr(mac, g_sys.ui8MAC);
    /* get current date & time */
    RTC_GetTimeStr(&g_sys.timeDate, timestr);
    RTC_GetDateStr(&g_sys.timeDate, datestr);

    /* Send header portion of the html */
    emitHeader(htmlSock, 0, "home");

    html("<div id=\"main\">\r\n");
    html("<fieldset>\r\n");
    html("<legend class=\"bold\">System Summary</legend>\r\n");

    System_sprintf(buf, "<p>STC Firmware: v%d.%02d.%03d</p>\r\n",
                   FIRMWARE_VER, FIRMWARE_REV, FIRMWARE_BUILD);
    html(buf);

    System_sprintf(buf, "<p>DTC Firmware: v%d.%02d.%03d</p>\r\n",
                   g_sys.dtcVersion >> 16, g_sys.dtcVersion & 0xFFFF, g_sys.dtcBuild);
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
    html("<p>DCS track controller: ");
    if (g_sys.dcsFound)
        System_sprintf(buf, "%d tracks</p>\r\n", g_sys.trackCount);
    else
        System_sprintf(buf, "N/A</p>\r\n");
    html(buf);
    html("<p>SMPTE time code card: ");
    if (g_sys.smpteFound)
    {
        System_sprintf(buf, "%s %s fps</p>\r\n", getstrSMPTEMode(), getstrFPS());
        html(buf);
    }
    else
    {
        html("N/A</p>\r\n");
    }
    System_sprintf(buf, "<p>MIDI device ID: %d</p>\r\n", g_sys.cfgSTC.midiDevID);
    html(buf);
    System_sprintf(buf, "<p>System date: %s</p>\r\n", datestr);
    html(buf);
    System_sprintf(buf, "<p>System time: %s</p>\r\n", timestr);
    html(buf);
    html("</fieldset><br /><br />\r\n");

    emitFooter(htmlSock);

    return 1;
}

//*****************************************************************************
// CGI Config Page Callback Function
//*****************************************************************************

static Int sendConfigHtml(SOCKET htmlSock, int length)
{
    Char buf[MAX_RESPONSE_SIZE];

    /* Send header portion of the html */
    emitHeader(htmlSock, 1, "config");

    html("<div id=\"main\">\r\n");
    html("<form action=\"config.cgi\" method=\"post\">\r\n");

    /* GENERAL Settings */

    html("<fieldset>\r\n");
    html("<legend class=\"bold\">General Settings</legend>\r\n");
    System_sprintf(buf, "<input type=\"checkbox\" name=\"longtime\" value=\"yes\" %s> Wired remote displays time in timecode form<br />\r\n", g_sys.cfgSTC.showLongTime ? "checked" : "");
    html(buf);
    System_sprintf(buf, "<input type=\"checkbox\" name=\"blink\" value=\"yes\" %s> Blink transport 7-seg display during locate<br />\r\n", g_sys.cfgSTC.searchBlink ? "checked" : "");
    html(buf);
    html("</fieldset><br />\r\n");

    /* MIDI Settings */

    html("<fieldset>\r\n");
    html("<legend class=\"bold\">MIDI Settings</legend>\r\n");
    System_sprintf(buf, "Device ID:<br><input type=\"text\" name=\"devid\" value=\"%u\"> <br />\r\n", g_sys.cfgSTC.midiDevID);
    html(buf);
    html("</fieldset><br />\r\n");

    /* SMPTE Settings */

    html("<fieldset>\r\n");
    html("<legend class=\"bold\">SMPTE Settings</legend>\r\n");
    html("<label for=\"smpteRef\">Master ref clock frequency:</label><br>\r\n");
    System_sprintf(buf, "<input type=\"text\" name=\"refClock\" value=\"%u\"> <br />\r\n", (uint32_t)g_sys.cfgSTC.ref_freq);
    html(buf);
    html("<label for=\"frameRate\">Default frame rate:</label><br>\r\n");
    System_sprintf(buf, "<input type=\"text\" name=\"frameRate\" value=\"%s\"> <br />\r\n", getstrFPS());
    html(buf);
    html("</fieldset><br />\r\n");

    /* LOCATOR Settings */

    html("<fieldset>\r\n");
    html("<legend class=\"bold\">Locator Settings</legend>\r\n");
    System_sprintf(buf, "Jog near velocity:<br><input type=\"text\" name=\"jognear\" value=\"%u\"><br />\r\n", g_sys.cfgSTC.jog_vel_near);
    html(buf);
    System_sprintf(buf, "Jog mid velocity:<br><input type=\"text\" name=\"jogmid\" value=\"%u\"><br />\r\n", g_sys.cfgSTC.jog_vel_mid);
    html(buf);
    System_sprintf(buf, "Jog far velocity:<br><input type=\"text\" name=\"jogfar\" value=\"%u\"><br />\r\n", g_sys.cfgSTC.jog_vel_far);
    html(buf);
    html("</fieldset><br />\r\n");

    /* TRANSPORT Settings */

    html("<fieldset>\r\n");
    html("<legend class=\"bold\">Transport Settings</legend>\r\n");
    System_sprintf(buf, "Velocity detect threshold:<br><input type=\"text\" name=\"velDet\" value=\"%u\"><br />\r\n", g_sys.cfgDTC.vel_detect_threshold);
    html(buf);
    System_sprintf(buf, "Record pulse strobe time:<br><input type=\"text\" name=\"strobeTime\" value=\"%u\"><br />\r\n", g_sys.cfgDTC.record_pulse_time);
    html(buf);
    System_sprintf(buf, "Record hold settle time:<br><input type=\"text\" name=\"settleTime\" value=\"%u\"><br />\r\n", g_sys.cfgDTC.rechold_settle_time);
    html(buf);
    System_sprintf(buf, "Transport button debounce time:<br><input type=\"text\" name=\"debounce\" value=\"%u\"><br />\r\n", g_sys.cfgDTC.debounce);
    html(buf);
    html("</fieldset><br />\r\n");

    /* TENSION Settings */

    html("<fieldset>\r\n");
    html("<legend class=\"bold\">Tension Settings</legend>\r\n");
    /* Supply */
    html("<fieldset>\r\n");
    html("<legend class=\"bold\">Supply</legend>\r\n");
    System_sprintf(buf, "Stop:<br><input type=\"text\" name=\"stop_supply_tension\" value=\"%u\"><br />\r\n", g_sys.cfgDTC.stop_supply_tension);
    html(buf);
    System_sprintf(buf, "Shuttle:<br><input type=\"text\" name=\"shuttle_supply_tension\" value=\"%u\"><br />\r\n", g_sys.cfgDTC.shuttle_supply_tension);
    html(buf);
    System_sprintf(buf, "Play-LO:<br><input type=\"text\" name=\"play_lo_supply_tension\" value=\"%u\"><br />\r\n", g_sys.cfgDTC.play_lo_supply_tension);
    html(buf);
    System_sprintf(buf, "Play-HI:<br><input type=\"text\" name=\"play_hi_supply_tension\" value=\"%u\"><br />\r\n", g_sys.cfgDTC.play_hi_supply_tension);
    html(buf);
    System_sprintf(buf, "Thread:<br><input type=\"text\" name=\"thread_supply_tension\" value=\"%u\"><br />\r\n", g_sys.cfgDTC.thread_supply_tension);
    html(buf);
    html("</fieldset><br />\r\n");
    /* Takeup */
    html("<fieldset>\r\n");
    html("<legend class=\"bold\">Takeup</legend>\r\n");
    System_sprintf(buf, "Stop:<br><input type=\"text\" name=\"stop_takeup_tension\" value=\"%u\"><br />\r\n", g_sys.cfgDTC.stop_takeup_tension);
    html(buf);
    System_sprintf(buf, "Shuttle:<br><input type=\"text\" name=\"shuttle_takeup_tension\" value=\"%u\"><br />\r\n", g_sys.cfgDTC.shuttle_takeup_tension);
    html(buf);
    System_sprintf(buf, "Play-LO:<br><input type=\"text\" name=\"play_lo_takeup_tension\" value=\"%u\"><br />\r\n", g_sys.cfgDTC.play_lo_takeup_tension);
    html(buf);
    System_sprintf(buf, "Play-HI:<br><input type=\"text\" name=\"play_hi_takeup_tension\" value=\"%u\"><br />\r\n", g_sys.cfgDTC.play_hi_takeup_tension);
    html(buf);
    System_sprintf(buf, "Thread:<br><input type=\"text\" name=\"thread_takeup_tension\" value=\"%u\"><br />\r\n", g_sys.cfgDTC.thread_takeup_tension);
    html(buf);
    html("</fieldset><br />\r\n");

    html("<fieldset>\r\n");
    html("<legend class=\"bold\">Tension Sensor</legend>\r\n");
    html("</fieldset><br />\r\n");

    html("<fieldset>\r\n");
    html("<legend class=\"bold\">Reeling Radius</legend>\r\n");
    html("</fieldset><br />\r\n");

    html("</fieldset><br />\r\n");

    /* STOP Mode */

    html("<fieldset>\r\n");
    html("<legend class=\"bold\">Stop Mode</legend>\r\n");
    html("<label for \"stopTorque\">Dynamic stop brake torque:</label>\r\n");
    System_sprintf(buf, "<input type=\"text\" name=\"stopTorque\" value=\"%d\"> <br />\r\n",
                   g_sys.cfgDTC.stop_brake_torque);
    html(buf);
    System_sprintf(buf, "<input type=\"checkbox\" name=\"stopLifters\" value=\"yes\"> Leave lifters engaged at stop<br />\r\n",
                   getstrYesNo(g_sys.cfgDTC.sysflags & DTC_SF_LIFTER_AT_STOP));
    html(buf);
    System_sprintf(buf, "<input type=\"checkbox\" name=\"stopBrakes\" value=%s> Leave brakes engaged at stop<br />\r\n",
                   getstrYesNo(g_sys.cfgDTC.sysflags & DTC_SF_BRAKES_AT_STOP));
    html(buf);
    System_sprintf(buf, "<input type=\"checkbox\" name=\"stopEOT\" value=%s> Stop at end-of-tape sense<br />\r\n",
                   getstrYesNo(g_sys.cfgDTC.sysflags & DTC_SF_STOP_AT_TAPE_END));
    html(buf);
    html("</fieldset><br />\r\n");

    /* PLAY Mode */

#if 0
    /* Play Boost LO-Speed */
    html("<fieldset>\r\n");
    html("<legend class=\"bold\">Play Boost LO-Speed</legend>\r\n");
    html("<label for \"playPgainLO\">P-Gain:</label><br>\r\n");
    System_printf(buf, "<input type=\"text\" name=\"playPgainLO\" value=\"%f\"> <br />\r\n", g_sys.cfgDTC.play_lo_boost_pgain);
    html(buf);
    html("<label for \"playIgainLO\">I-Gain:</label><br>\r\n");
    System_printf(buf, "<input type=\"text\" name=\"playIgainLO\" value=\"%f\"> <br />\r\n", g_sys.cfgDTC.play_lo_boost_igain);
    html(buf);
    html("<label for \"playDgainLO\">D-Gain:</label><br>\r\n");
    html("<input type=\"text\" name=\"playDgainLO\" value=\"0\"> <br />\r\n");
    html("</fieldset><br />\r\n");

    /* Play Boost HI-Speed */
    html("<fieldset>\r\n");
    html("<legend class=\"bold\">Play Boost HI-Speed</legend>\r\n");
    html("<label for \"playPgainHI\">P-Gain:</label><br>\r\n");
    html("<input type=\"text\" name=\"playPgainHI\" value=\"0\"> <br />\r\n");
    html("<label for \"playIgainHI\">I-Gain:</label><br>\r\n");
    html("<input type=\"text\" name=\"playIgainHI\" value=\"0\"> <br />\r\n");
    html("<label for \"playDgainHI\">D-Gain:</label><br>\r\n");
    html("<input type=\"text\" name=\"playDgainHI\" value=\"0\"> <br />\r\n");
    html("</fieldset><br />\r\n");

    /* Play Mode */
    html("<fieldset>\r\n");
    html("<legend class=\"bold\">Play Mode</legend>\r\n");
    html("<label for \"settlePinch\">Pinch roller settle time after engage:</label><br>\r\n");
    html("<input type=\"text\" name=\"settlePinch\" value=\"0\"> <br />\r\n");
    html("<label for \"settlePlay\">Delay entering play after shuttle:</label><br>\r\n");
    html("<input type=\"text\" name=\"settlePlay\" value=\"0\"> <br />\r\n");
    html("<label for \"settleBrake\">Brake settle time after engage:</label><br>\r\n");
    html("<input type=\"text\" name=\"settleBrake\" value=\"0\"> <br />\r\n");
    html("<br />\r\n");
    html("<input type=\"checkbox\" name=\"brakesStopPlay\" value=\"yes\" > Use brakes to stop play mode<br />\r\n");
    html("<input type=\"checkbox\" name=\"pinchEngage\" value=\"yes\" > Engage pinch roller at play<br />\r\n");
    html("</fieldset><br />\r\n");

    /* Shuttle Mode */
    html("<fieldset>\r\n");
    html("<legend class=\"bold\">Shuttle Mode</legend>\r\n");
    html("<label for \"settlePinch\">FWD shuttle back-tension gain:</label><br>\r\n");
    html("<input type=\"text\" name=\"\" value=\"0\"> <br />\r\n");
    html("<label for \"settlePlay\">REW shuttle back-tension gain:</label><br>\r\n");
    html("<input type=\"text\" name=\"\" value=\"0\"> <br />\r\n");
    html("<label for \"settleBrake\">Shuttle mode max velocity:</label><br>\r\n");
    html("<input type=\"text\" name=\"\" value=\"0\"> <br />\r\n");
    html("<label for \"settleBrake\">Library wind mode velocity:</label><br>\r\n");
    html("<input type=\"text\" name=\"\" value=\"0\"> <br />\r\n");
    html("<label for \"settleBrake\">Auto-Slow wind velocity:</label><br>\r\n");
    html("<input type=\"text\" name=\"\" value=\"0\"> <br />\r\n");
    html("<label for \"settleBrake\">Auto-slow triggers at offset:</label><br>\r\n");
    html("<input type=\"text\" name=\"\" value=\"0\"> <br />\r\n");
    html("<label for \">Auto-slow triggers at offset:</label><br>\r\n");
    html("<input type=\"text\" name=\"\" value=\"0\"> <br />\r\n");
    html("<label for \">Auto-Slow min velocity to trigger:</label><br>\r\n");
    html("<input type=\"text\" name=\"\" value=\"0\"> <br />\r\n");
    html("<label for \">Lifter settle time after engage:</label><br>\r\n");
    html("<input type=\"text\" name=\"\" value=\"0\"> <br />\r\n");
    html("</fieldset><br />\r\n");

    /* Shuttle Servo PID */
    html("<fieldset>\r\n");
    html("<legend class=\"bold\">Shuttle Servo PID</legend>\r\n");
    html("<label for \"shuttlePgain\">P-Gain:</label><br>\r\n");
    html("<input type=\"text\" name=\"shuttlePgain\" value=\"0\"> <br />\r\n");
    html("<label for \"shuttleIgain\">I-Gain:</label><br>\r\n");
    html("<input type=\"text\" name=\"shuttleIgain\" value=\"0\"> <br />\r\n");
    html("<label for \"shuttleDgain\">D-Gain:</label><br>\r\n");
    html("<input type=\"text\" name=\"shuttleDgain\" value=\"0\"> <br />\r\n");
    html("</fieldset><br />\r\n");
#endif

    /* End of form Save/Reset buttons */
    html("<input class=\"btn\" type=\"submit\" name=\"submit\" value=\"Save\">\r\n");
    html("<input class=\"btn\" type=\"reset\" name=\"submit\" value=\"Reset\">\r\n");
    html("</form>\r\n");

    emitFooter(htmlSock);

    return 1;
}

//*****************************************************************************
// CGI Handler Functions
//*****************************************************************************

// This function processes the CGI from off the config
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

    g_sys.cfgSTC.showLongTime = false;
    g_sys.cfgSTC.searchBlink  = false;

    // Process request variables until there are none left
    do
    {
        key   = cgiParseVars(buffer, &parseIndex);
        value = cgiParseVars(buffer, &parseIndex);

        if (!strcmp("longtime", key))
            g_sys.cfgSTC.showLongTime = (strcmp(value, "yes") == 0) ? true : false;
        else if (!strcmp("blink", key))
            g_sys.cfgSTC.searchBlink = (strcmp(value, "yes") == 0) ? true : false;
        else if (!strcmp("jognear", key))
        {
            val = atoi(value);
            if ((val >= 0) && (val <= 300))
                g_sys.cfgSTC.jog_vel_near = (uint32_t)val;
        }
        else if (!strcmp("jogmid", key))
        {
            val = atoi(value);
            if ((val >= 0) && (val <= 500))
                g_sys.cfgSTC.jog_vel_mid = (uint32_t)val;
        }
        else if (!strcmp("jogfar", key))
        {
            val = atoi(value);
            if ((val >= 0) && (val <= 1100))
                g_sys.cfgSTC.jog_vel_far = (uint32_t)val;
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
// CGI Remote Page Callback Function
//*****************************************************************************

static Int sendRemoteHtml(SOCKET htmlSock, int length)
{
    int i;
    char ch;
    uint32_t lampmask = L_LOC1;
    Char buf[MAX_RESPONSE_SIZE];

    emitHeader(htmlSock, -1, "remote");

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
#if 0
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
#endif

    for(i=0; i < 10; i++)
    {
        ch = (g_sys.ledMaskRemote & lampmask) ? '1' : '0';

        System_sprintf(buf, "<input class=\"btn%c\" type=\"submit\" name=\"loc%d\" value=\"LOC-%d\">\r\n", ch, i+1, i+1);
        html(buf);

        if (i == 4)
            html("<br />\r\n");

        lampmask <<= 1;
    }

    html("</fieldset>\r\n");
    html("</form>\r\n");
    //html("</div>\r\n");
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
