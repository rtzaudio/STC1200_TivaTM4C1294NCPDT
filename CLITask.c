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
#include <xdc/runtime/Memory.h>

#include <ti/sysbios/BIOS.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/gates/GateMutex.h>

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
#include <stdbool.h>
#include <time.h>

#include <driverlib/sysctl.h>
#include <driverlib/hibernate.h>

#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/hal/Seconds.h>

#include "STC1200.h"
#include "Board.h"
#include "Utils.h"
#include "CLITask.h"
#include "SMPTE.h"
#include "RAMPMessage.h"
#include "IPCMessage.h"
#include "IPCCommands.h"
#include "RemoteTask.h"

extern SYSDATA g_sysData;
extern SYSPARMS g_sysParms;

//*****************************************************************************
// Type Definitions
//*****************************************************************************

typedef struct {
    const char* name;
    void (*func)(int, char**);
    const char* doc;
} cmd_t;

#define MK_CMD(x) void cmd_ ## x (int, char**)

//*****************************************************************************
// CLI Function Handle Declarations
//*****************************************************************************

MK_CMD(ip);
MK_CMD(mac);
MK_CMD(sn);
MK_CMD(smpte);
MK_CMD(cls);
MK_CMD(help);
MK_CMD(about);
MK_CMD(stop);
MK_CMD(play);
MK_CMD(rew);
MK_CMD(fwd);
MK_CMD(speed);
MK_CMD(time);
MK_CMD(date);
MK_CMD(cue);
MK_CMD(store);
MK_CMD(rtz);
MK_CMD(stat);

/* The dispatch table */
#define CMD(func, help) {#func, cmd_ ## func, help}

cmd_t dispatch[] = {
    CMD(ip, "Displays IP address"),
    CMD(mac, "Displays MAC address"),
    CMD(sn, "Displays serial number"),
    CMD(smpte, "SMPTE generator {start|stop}"),
    CMD(cls, "Clear the screen"),
    CMD(help, "Display this help"),
    CMD(about, "About the system"),
    CMD(stop, "Transport STOP mode"),
    CMD(play, "Transport PLAY {rec} mode"),
    CMD(rew, "Transport REW {lib} mode"),
    CMD(fwd, "Transport FWD {lib} mode"),
    CMD(speed, "Display tape speed"),
    CMD(time, "Display time"),
    CMD(date, "Display date"),
    CMD(cue, "Locator cue {0-9}"),
    CMD(store, "Locator store {0-9}"),
    CMD(rtz, "Return to zero"),
    CMD(stat, "Displays machine status"),
};

#define NUM_CMDS    (sizeof(dispatch)/sizeof(cmd_t))

//*****************************************************************************
// Static and External Data Items
//*****************************************************************************

#define MAX_CHARS       80
#define MAX_ARGS        8
#define MAX_ARG_LEN     16
#define MAX_PATH        256

/*** Static Data Items ***/
static UART_Handle s_handleUart;
static const char *s_delim = " ://\n";
static char s_cmdbuf[MAX_CHARS+3];
static char s_cmdprev[MAX_CHARS+3];

static int   s_argc = 0;
static char* s_argv[MAX_ARGS];
static char  s_args[MAX_ARGS][MAX_ARG_LEN];

/* Current Working Directory */
//static char s_cwd[MAX_PATH] = "\\";
//static char s_drive = '0';

/*** Function Prototypes ***/
static int parse_args(char *buf);
static void parse_cmd(char *buf);
static bool IsClockRunning(void);

/*** External Data Items ***/
extern SYSDATA g_sysData;
extern SYSPARMS g_sysParms;

//*****************************************************************************
//
//*****************************************************************************

int CLI_init(void)
{
    UART_Params uartParams;

    UART_Params_init(&uartParams);

    uartParams.readMode       = UART_MODE_BLOCKING;
    uartParams.writeMode      = UART_MODE_BLOCKING;
    uartParams.readTimeout    = 1000;                   // 1 second read timeout
    uartParams.writeTimeout   = BIOS_WAIT_FOREVER;
    uartParams.readCallback   = NULL;
    uartParams.writeCallback  = NULL;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.writeDataMode  = UART_DATA_TEXT;
    uartParams.readDataMode   = UART_DATA_BINARY;
    uartParams.readEcho       = UART_ECHO_OFF;
    uartParams.baudRate       = 115200;
    uartParams.stopBits       = UART_STOP_ONE;
    uartParams.parityType     = UART_PAR_NONE;

    s_handleUart = UART_open(Board_UART_RS232_COM1, &uartParams);

    if (s_handleUart == NULL)
        System_abort("Error initializing UART\n");

    return 1;
}

//*****************************************************************************
//
//*****************************************************************************

Bool CLI_startup(void)
{
    Error_Block eb;
    Task_Params taskParams;

    Error_init(&eb);

    Task_Params_init(&taskParams);

    taskParams.stackSize = 1500;
    taskParams.priority  = 2;
    taskParams.arg0      = 0;
    taskParams.arg1      = 0;

    Task_create((Task_FuncPtr)CLITaskFxn, &taskParams, &eb);

    return TRUE;
}

//*****************************************************************************
//
//*****************************************************************************

void CLI_putc(int ch)
{
    UART_write(s_handleUart, &ch, 1);
}

void CLI_puts(char* s)
{
    int l = strlen(s);
    UART_write(s_handleUart, s, l);
}

void CLI_printf(const char *fmt, ...)
{
    va_list arg;
    static char buf[128];
    va_start(arg, fmt);
    System_vsnprintf(buf, sizeof(buf)-1, fmt, arg);
    va_end(arg);
    UART_write(s_handleUart, buf, strlen(buf));
}

void CLI_prompt(void)
{
    CLI_putc(CRET);
    CLI_putc(LF);
    CLI_putc('>');
    CLI_putc(' ');
}

//*****************************************************************************
//
//*****************************************************************************

Void CLITaskFxn(UArg arg0, UArg arg1)
{
    uint8_t ch;
    int cnt = 0;

    CLI_printf(VT100_HOME);
    CLI_printf(VT100_CLS);

    CLI_printf("STC-1200 v%d.%02d.%03d\n\n", FIRMWARE_VER, FIRMWARE_REV, FIRMWARE_BUILD);
    CLI_puts("Enter 'help' to view a list valid commands\n\n> ");

    while (true)
    {
        /* Read a character from the console */
        if (UART_read(s_handleUart, &ch, 1) == 1)
        {
            if (ch == CRET)
            {
                if (cnt)
                {
                    CLI_putc(CRET);
                    CLI_putc(LF);
                    /* save command for previous recall */
                    strcpy(s_cmdprev, s_cmdbuf);
                    /* parse new command and execute */
                    parse_cmd(s_cmdbuf);
                    /* reset the command buffer */
                    s_cmdbuf[0] = 0;
                    cnt = 0;
                }
                CLI_putc(CRET);
                CLI_putc(LF);
                CLI_putc('>');
                CLI_putc(' ');
            }
            else if (ch == BKSPC)
            {
                if (cnt)
                {
                    s_cmdbuf[--cnt] = 0;

                    CLI_putc(BKSPC);
                    CLI_putc(' ');
                    CLI_putc(BKSPC);
                }
            }
            else if (ch == CTL_Z)
            {
                /* restore previous command */
                strcpy(s_cmdbuf, s_cmdprev);
                cnt = strlen(s_cmdbuf);
                CLI_printf("%s", s_cmdbuf);
            }
            else
            {
                if (cnt < MAX_CHARS)
                {
                    if (isalnum((int)ch) || strchr(s_delim, (int)ch))
                    {
                        s_cmdbuf[cnt++] = tolower(ch);
                        s_cmdbuf[cnt] = 0;

                        CLI_putc((int)ch);
                    }
                }
            }
        }
    }
}

//*****************************************************************************
//
//*****************************************************************************

int parse_args(char *buf)
{
    int argc = 0;

    const char* tok = strtok(NULL, s_delim);

    if (!tok)
        return 0;

    while (tok != NULL)
    {
        s_argv[argc] = strncpy(s_args[argc], tok, MAX_ARG_LEN-1);

        if (++argc >= MAX_ARGS)
            break;

        tok = strtok(NULL, s_delim);
    }

    return argc;
}

void parse_cmd(char *buf)
{
    char* tok = strtok(buf, s_delim);

    if (!tok)
        return;

    /* parse args into array */
    s_argc = parse_args(tok);

    int i = NUM_CMDS;

    while(i--)
    {
        cmd_t cur = dispatch[i];

        if (!strncmp(tok, cur.name, strlen(cur.name)))
        {
            cur.func(s_argc, s_argv);
            return;
        }
    }

    CLI_puts("Command not found.\n");
}

//*****************************************************************************
// Time/Date Helper Functions
//*****************************************************************************

bool IsClockRunning(void)
{
    bool running = true;

    if (g_sysData.rtcFound)
        running = MCP79410_IsRunning(g_sysData.handleRTC);

    if (!running)
        CLI_printf("clock not running - set time/date first\n");

    return running;
}

bool IsValidTime(struct tm *p)
{
    if (((p->tm_sec < 0) || (p->tm_sec > 59)) ||
        ((p->tm_min < 0) || (p->tm_min > 59)) ||
        ((p->tm_hour < 0) || (p->tm_hour > 23)))
    {
        return false;
    }

    return true;
}

bool IsValidDate(struct tm *p)
{
    // Is valid data read?
    if(((p->tm_mday < 1) || (p->tm_mday > 31)) ||
       ((p->tm_mon < 0) || (p->tm_mon > 11)) ||
       ((p->tm_year < 100) || (p->tm_year > 199)))
    {
        return false;
    }

    return true;
}

//*****************************************************************************
// CLI Command Handlers
//*****************************************************************************

void cmd_help(int argc, char *argv[])
{
    char name[16];
    int x, len;
    int i = NUM_CMDS;

    CLI_puts("\nAvailable Commands:\n\n");

    while(i--)
    {
        cmd_t cmd = dispatch[i];

        len = strlen(cmd.name);

        for (x=0; x < len; x++)
        {
            name[x] = toupper(cmd.name[x]);
            name[x+1] = 0;

            if (x >= sizeof(name)-1)
                break;
        }

        CLI_printf("%-10s%s\n", name, cmd.doc);
    }
}

void cmd_about(int argc, char *argv[])
{
    CLI_printf("STC-1200 v%d.%02d.%03d\n", FIRMWARE_VER, FIRMWARE_REV, FIRMWARE_BUILD);
    CLI_puts("Copyright (C) 2016-2021, RTZ Professional Audio\n");
}

void cmd_cls(int argc, char *argv[])
{
    CLI_puts(VT100_CLS);
    CLI_puts(VT100_HOME);
}

void cmd_ip(int argc, char *argv[])
{
    CLI_printf("%s\n", g_sysData.ipAddr);
}

void cmd_mac(int argc, char *argv[])
{
    char mac[32];
    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
            g_sysData.ui8MAC[0], g_sysData.ui8MAC[1], g_sysData.ui8MAC[2],
            g_sysData.ui8MAC[3], g_sysData.ui8MAC[4], g_sysData.ui8MAC[5]);
    CLI_printf("%s\n", mac);
}

void cmd_sn(int argc, char *argv[])
{
    char serialnum[64];
    /*  Format the 64 bit GUID as a string */
    GetHexStr(serialnum, g_sysData.ui8SerialNumber, 16);
    CLI_printf("%s\n", serialnum);
}

void cmd_smpte(int argc, char *argv[])
{
    if (argc < 1)
    {
        CLI_puts("Missing Argument\n");
        return;
    }

    CLI_puts("SMPTE generator ");

    if (strcmp(argv[0], "start") == 0)
    {
        SMPTE_stripe_start();
        CLI_puts("START\n");
    }
    else
    {
        SMPTE_stripe_stop();
        CLI_puts("STOP\n");
    }
}

void cmd_stop(int argc, char *argv[])
{
    CLI_puts("STOP\n");
    Transport_PostButtonPress(S_STOP);
}

void cmd_play(int argc, char *argv[])
{
    uint32_t mask = S_PLAY;

    if (argc)
    {
        if (strcmp(argv[0], "rec") == 0)
            mask |= S_REC;
    }

    CLI_printf("PLAY%s\n", (mask & S_REC) ? "-REC" : "");

    Transport_PostButtonPress(mask);
}

void cmd_fwd(int argc, char *argv[])
{
    uint32_t mask = 0;

    if (argc)
    {
        if (strcmp(argv[0], "lib") == 0)
            mask |= M_LIBWIND;
    }

    CLI_printf("FWD%s\n", (mask & M_LIBWIND) ? "-LIB" : "");

    Transport_Fwd(0, mask);
}

void cmd_rew(int argc, char *argv[])
{
    uint32_t mask = 0;

    if (argc)
    {
        if (strcmp(argv[0], "lib") == 0)
            mask |= M_LIBWIND;
    }

    CLI_printf("REW%s\n", (mask & M_LIBWIND) ? "-LIB" : "");

    Transport_Rew(0, mask);
}

void cmd_speed(int argc, char *argv[])
{
    CLI_printf("%u IPS\n", g_sysData.tapeSpeed);
}

void cmd_rtz(int argc, char *argv[])
{
    LocateSearch(CUE_POINT_HOME, 0);
}

void cmd_cue(int argc, char *argv[])
{
    int loc = 0;

    if (!argc)
    {
        CLI_printf("CUE MODE\n");
        Remote_PostSwitchPress(SW_CUE, 0);
        return;
    }

    loc = atoi(argv[0]);

    if (g_sysData.remoteMode != REMOTE_MODE_CUE)
        Remote_PostSwitchPress(SW_CUE, 0);

    CLI_printf("SEARCH TO CUE MEMORY %d\n", loc);

    switch(loc)
    {
    case 0:
        Remote_PostSwitchPress(SW_LOC0, 0);
        break;
    case 1:
        Remote_PostSwitchPress(SW_LOC1, 0);
        break;
    case 2:
        Remote_PostSwitchPress(SW_LOC2, 0);
        break;
    case 3:
        Remote_PostSwitchPress(SW_LOC3, 0);
        break;
    case 4:
        Remote_PostSwitchPress(SW_LOC4, 0);
        break;
    case 5:
        Remote_PostSwitchPress(SW_LOC5, 0);
        break;
    case 6:
        Remote_PostSwitchPress(SW_LOC6, 0);
        break;
    case 7:
        Remote_PostSwitchPress(SW_LOC7, 0);
        break;
    case 8:
        Remote_PostSwitchPress(SW_LOC8, 0);
        break;
    case 9:
        Remote_PostSwitchPress(SW_LOC9, 0);
        break;
    }
}

void cmd_store(int argc, char *argv[])
{
    int loc = 0;

    if (!argc)
    {
        CLI_printf("STORE MODE\n");
        Remote_PostSwitchPress(SW_STORE, 0);
        return;
    }

    loc = atoi(argv[0]);
    CLI_printf("STORE TO MEMORY to %d\n", loc);

    if (g_sysData.remoteMode != REMOTE_MODE_STORE)
        Remote_PostSwitchPress(SW_STORE, 0);

    switch(loc)
    {
    case 0:
        Remote_PostSwitchPress(SW_LOC0, 0);
        break;
    case 1:
        Remote_PostSwitchPress(SW_LOC1, 0);
        break;
    case 2:
        Remote_PostSwitchPress(SW_LOC2, 0);
        break;
    case 3:
        Remote_PostSwitchPress(SW_LOC3, 0);
        break;
    case 4:
        Remote_PostSwitchPress(SW_LOC4, 0);
        break;
    case 5:
        Remote_PostSwitchPress(SW_LOC5, 0);
        break;
    case 6:
        Remote_PostSwitchPress(SW_LOC6, 0);
        break;
    case 7:
        Remote_PostSwitchPress(SW_LOC7, 0);
        break;
    case 8:
        Remote_PostSwitchPress(SW_LOC8, 0);
        break;
    case 9:
        Remote_PostSwitchPress(SW_LOC9, 0);
        break;
    }
}

void cmd_stat(int argc, char *argv[])
{
    CLI_printf("\nPosition Status\n\n");
    CLI_printf("  tape roller tach   : %u\n", (uint32_t)g_sysData.tapeTach);
    CLI_printf("  tape roller errors : %u\n", g_sysData.qei_error_cnt);
    CLI_printf("  encoder position   : %d\n", g_sysData.tapePosition);
}

void cmd_time(int argc, char *argv[])
{
    char timeFmt[] = "Current time: %d:%02d:%02d\n";
    char timeSet[] = "Time set!\n";
    char timeAs[]  = "Enter time as: hh:mm:ss\n";

    if (g_sysData.rtcFound)
    {
        RTCC_Struct ts;

        if (argc == 0)
        {
            if (!IsClockRunning())
                return;

            MCP79410_GetTime(g_sysData.handleRTC, &ts);

            CLI_printf(timeFmt, ts.hour, ts.min, ts.sec);
        }
        else if (argc == 3)
        {
            /* Get current time/date */
            MCP79410_GetTime(g_sysData.handleRTC, &ts);

            ts.hour    = (uint8_t)atoi(argv[0]);
            ts.min     = (uint8_t)atoi(argv[1]);
            ts.sec     = (uint8_t)atoi(argv[2]);

            MCP79410_SetHourFormat(g_sysData.handleRTC, H24);                // Set hour format to military time standard
            MCP79410_EnableVbat(g_sysData.handleRTC);                        // Enable battery backup
            MCP79410_SetTime(g_sysData.handleRTC, &ts);
            MCP79410_EnableOscillator(g_sysData.handleRTC);                  // Start clock by enabling oscillator

            CLI_puts(timeSet);
        }
        else
        {
            CLI_puts(timeAs);
        }
    }
    else
    {
        struct tm stime;

        if (argc == 0)
        {
            // Get the latest time.
            HibernateCalendarGet(&stime);

            // Is valid data read?
            if (!IsValidTime(&stime))
            {
                CLI_puts("Time has not been set yet\n");
            }
            else
            {
                CLI_printf(timeFmt, stime.tm_hour, stime.tm_min, stime.tm_sec);
            }
        }
        else if (argc == 3)
        {
            // Get the latest date and time.
            HibernateCalendarGet(&stime);

            // Set the time values that are to be updated.
            stime.tm_hour = atoi(argv[0]);
            stime.tm_min  = atoi(argv[1]);
            stime.tm_sec  = atoi(argv[2]);

            // Update the calendar logic of hibernation module.
            HibernateCalendarSet(&stime);

            CLI_puts(timeSet);
        }
        else
        {
            CLI_puts(timeAs);
        }
    }
}

void cmd_date(int argc, char *argv[])
{
    char dateFmt[] = "Current date: %d/%d/%d\n";
    char dateSet[] = "Date set!\n";
    char dateAs[]  = "Enter date as: mm/dd/yyyy\n";

    if (g_sysData.rtcFound)
     {
        RTCC_Struct ts;

        if (argc == 0)
        {
            if (!IsClockRunning())
                return;

            MCP79410_GetTime(g_sysData.handleRTC, &ts);

            CLI_printf(dateFmt, ts.month, ts.date, ts.year+2000);
        }
        else if (argc == 3)
        {
            /* Get current time/date */
            MCP79410_GetTime(g_sysData.handleRTC, &ts);

            ts.month   = (uint8_t)atoi(argv[0]);
            ts.date    = (uint8_t)atoi(argv[1]);
            ts.year    = (uint8_t)(atoi(argv[2]) - 2000);
            ts.weekday = (uint8_t)((ts.date % 7) + 1);

            MCP79410_SetHourFormat(g_sysData.handleRTC, H24);                // Set hour format to military time standard
            MCP79410_EnableVbat(g_sysData.handleRTC);                        // Enable battery backup
            MCP79410_SetTime(g_sysData.handleRTC, &ts);
            MCP79410_EnableOscillator(g_sysData.handleRTC);                  // Start clock by enabling oscillator

            CLI_puts(dateSet);
        }
        else
        {
            CLI_puts(dateAs);
        }
     }
    else
    {
        struct tm stime;

        if (argc == 0)
        {
            // Get the latest time.
            HibernateCalendarGet(&stime);

            // Is valid data read?
            if (!IsValidDate(&stime))
            {
                CLI_puts("Date has not been set yet\n");
            }
            else
            {
                CLI_printf(dateFmt,
                           stime.tm_mon,
                           stime.tm_mday,
                           stime.tm_year + 1900);
            }
        }
        else if (argc == 3)
        {
            // Get the latest date and time.
            HibernateCalendarGet(&stime);

            // Set the date values that are to be updated.
            stime.tm_mon  = atoi(argv[0]);
            stime.tm_mday = atoi(argv[1]);
            stime.tm_year = atoi(argv[2]) - 1900;

            // Update the calendar logic of hibernation module.
            HibernateCalendarSet(&stime);

            CLI_puts(dateSet);
        }
        else
        {
            CLI_puts(dateAs);
        }
    }
}

// End-Of-File
