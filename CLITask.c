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
#include <ti/mw/fatfs/ff.h>

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
#include "PositionTask.h"
#include "SMPTE.h"
#include "RAMPMessage.h"
#include "IPCMessage.h"
#include "IPCCommands.h"
#include "RemoteTask.h"
#include "xmodem.h"

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
MK_CMD(cls);
MK_CMD(help);
MK_CMD(about);
MK_CMD(stop);
MK_CMD(play);
MK_CMD(rew);
MK_CMD(fwd);
MK_CMD(cue);
MK_CMD(store);
MK_CMD(rtz);
MK_CMD(zero);
MK_CMD(speed);
MK_CMD(smpte);
MK_CMD(time);
MK_CMD(date);
MK_CMD(stat);
MK_CMD(cfg);
MK_CMD(dir);
MK_CMD(cd);
MK_CMD(cwd);
MK_CMD(md);
MK_CMD(ren);
MK_CMD(del);
MK_CMD(copy);
MK_CMD(xmdm);

/* The dispatch table */
#define CMD(func, help) {#func, cmd_ ## func, help}

cmd_t dispatch[] = {
    CMD(ip,     "Display IP address"),
    CMD(mac,    "Display MAC address"),
    CMD(sn,     "Display serial number"),
    CMD(cls,    "Clear the screen"),
    CMD(help,   "Display this help"),
    CMD(about,  "About the system"),
    CMD(stop,   "Transport STOP mode"),
    CMD(play,   "Transport PLAY {rec} mode"),
    CMD(rew,    "Transport REW {lib} mode"),
    CMD(fwd,    "Transport FWD {lib} mode"),
    CMD(cue,    "Transport locator cue {0-9}"),
    CMD(store,  "Transport locator store {0-9}"),
    CMD(rtz,    "Transport return to zero"),
    CMD(zero,   "Tape time counter zero reset"),
    CMD(speed,  "Tape speed display/set"),
    CMD(smpte,  "SMPTE generator {start|stop}"),
    CMD(time,   "Time display/set"),
    CMD(date,   "Date display/set"),
    CMD(stat,   "Displays machine status"),
    CMD(cfg,    "Configuration {save|load|reset}"),
    CMD(dir,    "List directory"),
    CMD(cd,     "Change directory"),
    CMD(cwd,    "Display current working directory"),
    CMD(md,     "Make a directory"),
    CMD(ren,    "Rename a file or directory"),
    CMD(del,    "Remove a file or directory"),
    CMD(copy,   "Copy a file to a new file"),
    CMD(xmdm,   "XMODEM send/receive file"),
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
static const char *s_delim = " ://\\\n";
static char s_cmdbuf[MAX_CHARS+3];
static char s_cmdprev[MAX_CHARS+3];

static int   s_argc = 0;
static char* s_argv[MAX_ARGS];
static char  s_args[MAX_ARGS][MAX_ARG_LEN];

/* Current Working Directory */
static char s_cwd[MAX_PATH] = "\\";

/*** Function Prototypes ***/
static int parse_args(char *buf);
static void parse_cmd(char *buf);
static void _perror(FRESULT res);
static char* _getcwd(void);
static FRESULT _dirlist(char* path);
static FRESULT _checkcmd(FRESULT res);
static void _fmt_commas(uint32_t n, char *out);

//*****************************************************************************
// Initialize the UART and open the tty serial port.
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
    uartParams.writeDataMode  = UART_DATA_BINARY;
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
// Startup the command line task
//*****************************************************************************

Bool CLI_startup(void)
{
    Error_Block eb;
    Task_Params taskParams;

    Error_init(&eb);

    Task_Params_init(&taskParams);

    taskParams.stackSize = 3072;
    taskParams.priority  = 11;
    taskParams.arg0      = 0;
    taskParams.arg1      = 0;

    Task_create((Task_FuncPtr)CLITaskFxn, &taskParams, &eb);

    return TRUE;
}

//*****************************************************************************
// The main command line interface task.
//*****************************************************************************

Void CLITaskFxn(UArg arg0, UArg arg1)
{
    uint8_t ch;
    int cnt = 0;

    f_chdir("0://.");

    /* Get the current working directory of the SD drive */
    _getcwd();

    /* Display initial welcome and prompt */
    CLI_home();
    CLI_about();
    CLI_puts("\nEnter 'help' to view a list of valid commands\n");
    CLI_prompt();

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
                CLI_prompt();
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
                    if (isalnum((int)ch) || strchr(s_delim, (int)ch) || (ch == '.') || (ch == '*'))
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
// Low level tty interface and helper functions
//*****************************************************************************

void CLI_about(void)
{
    CLI_printf("STC-1200 [Version %d.%02d.%03d]\n", FIRMWARE_VER, FIRMWARE_REV, FIRMWARE_BUILD);
    CLI_puts("(C) 2021 RTZ Professional Audio. All Rights Reserved.\n");
}

int CLI_getc(void)
{
    int ch;

    /* Read a character from the console */
    if (UART_read(s_handleUart, &ch, 1) == 1)
        return ch;

    return -1;
}

void CLI_putc(int ch)
{
    if (ch == '\n')
    {
        ch = '\r';
        UART_write(s_handleUart, &ch, 1);
        ch = '\n';
    }

    UART_write(s_handleUart, &ch, 1);
}

void CLI_puts(char* s)
{
    int i;
    int l = strlen(s);

    for (i=0; i < l; i++)
        CLI_putc(*s++);
}

void CLI_printf(const char *fmt, ...)
{
    va_list arg;
    static char buf[128];
    va_start(arg, fmt);
    vsnprintf(buf, sizeof(buf)-1, fmt, arg);
    va_end(arg);
    CLI_puts(buf);
}

void CLI_prompt(void)
{
    CLI_putc(LF);
    CLI_puts(s_cwd);
    CLI_putc('>');
}

void CLI_home(void)
{
    CLI_printf(VT100_HOME);
    CLI_printf(VT100_CLS);
}

void CLI_crlf(int n)
{
    CLI_emit('\n', n);
}

void CLI_emit(char c, int n)
{
    if (n > 0)
    {
        do {
            CLI_putc(c);
        } while(--n);
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
// FILE SYSTEM HELPER FUNCTIONS
//*****************************************************************************

void _fmt_commas(uint32_t n, char *out)
{
    int c;
    char buf[32];
    char *p;

    sprintf(buf, "%u", n);

    c = 2 - strlen(buf) % 3;

    for (p=buf; *p != 0; p++)
    {
       *out++ = *p;

       if (c == 1)
           *out++ = ',';

       c = (c + 1) % 3;
    }

    *--out = 0;
}

/* List files in a directory with time, date, size and file name */

FRESULT _dirlist(char* path)
{
    FRESULT res;
    DIR dir;
    uint32_t numdirs = 0L;
    uint32_t numfiles = 0L;
    uint32_t bytes = 0L;
    FATFS *fs;
    DWORD fre_clust;
    DWORD fre_sect;
    DWORD tot_sect;
    char buf[_MAX_LFN];
    char tmp[32];
    static FILINFO fno;

    /* Open the directory */
    if ((res = f_opendir(&dir, path)) != FR_OK)
    {
        CLI_printf("%s\n", FS_GetErrorStr(res));
    }
    else
    {
        for (;;)
        {
            /* Read a directory item */
            res = f_readdir(&dir, &fno);

            /* Break on error or end of dir */
            if (res != FR_OK || fno.fname[0] == 0)
                break;

            if (fno.fattrib & AM_SYS)
                continue;

            bytes += (uint64_t)fno.fsize;

            /* Print the file date */
            FS_GetDateStr(fno.fdate, buf, sizeof(buf));
            CLI_puts(buf);
            CLI_emit(' ', 2);

            FS_GetTimeStr(fno.ftime, buf, sizeof(buf));
            CLI_puts(buf);

            if (fno.fattrib & AM_DIR)
            {
                sprintf(buf, "%-15s", "<DIR>");
                ++numdirs;
            }
            else
            {
                _fmt_commas(fno.fsize, tmp);
                sprintf(buf, "%15s", tmp);
                ++numfiles;
            }

            if (fno.lfname)
                CLI_printf("    %s %s\n", buf, fno.lfname);
            else
                CLI_printf("    %s %s\n", buf, fno.fname);
        }

        f_closedir(&dir);

        sprintf(buf, "\n\t%8d File(s)", numfiles);
        CLI_puts(buf);

        _fmt_commas(bytes, tmp);
        sprintf(buf, "  %s bytes\n", tmp);
        CLI_puts(buf);

        sprintf(buf, "\t%8d Dir(s)", numdirs);
        CLI_puts(buf);

        /* Get volume information and free clusters of drive 1 */
        if ((res = f_getfree("0:", &fre_clust, &fs)) == FR_OK)
        {
            /* Get total sectors and free sectors */
            tot_sect = (fs->n_fatent - 2) * fs->csize;
            fre_sect = fre_clust * fs->csize;

            _fmt_commas(fre_sect/2, tmp);
            sprintf(buf, "  %s bytes free\n", tmp);
            CLI_puts(buf);

            /* Print the free space (assuming 512 bytes/sector) */
            //CLI_printf("%10lu KiB total drive space\n", tot_sect/2);
        }
        else
        {
            CLI_putc('\n');
        }
    }

    return res;
}

/* Update and return the current working directory */

char* _getcwd(void)
{
    FRESULT res;

    res = f_getcwd(s_cwd, sizeof(s_cwd)-1);

    if (res != FR_OK)
    {
        s_cwd[0] = 0;
    }

    return s_cwd;
}

/* Print file system error message */

void _perror(FRESULT res)
{
    CLI_printf("%s\n", FS_GetErrorStr(res));
}

/* Acknowledge successful command with new line, or error message */

FRESULT _checkcmd(FRESULT res)
{
    if (res == FR_OK)
        CLI_putc('\n');
    else
        _perror(res);

    return res;
}

//*****************************************************************************
// BASIC CLI COMMANDS
//*****************************************************************************

void cmd_help(int argc, char *argv[])
{
    char name[16];
    int x, len;
    int i = NUM_CMDS;

    CLI_puts("\nAvailable Commands:\n\n");

    //while(i--)

    for(i=0; i < NUM_CMDS; i++)
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

        CLI_printf(" %-10s%s\n", name, cmd.doc);
    }
}

void cmd_about(int argc, char *argv[])
{
    CLI_about();
}

void cmd_cls(int argc, char *argv[])
{
    CLI_home();
}

//*****************************************************************************
// GENERAL SYSTEM COMMANDS
//*****************************************************************************

void cmd_sn(int argc, char *argv[])
{
    char serialnum[64];
    /*  Format the 64 bit GUID as a string */
    GetHexStr(serialnum, g_sys.ui8SerialNumber, 16);
    CLI_printf("%s\n", serialnum);
}

void cmd_ip(int argc, char *argv[])
{
    CLI_printf("%s\n", g_sys.ipAddr);
}

void cmd_mac(int argc, char *argv[])
{
    char mac[32];
    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
            g_sys.ui8MAC[0], g_sys.ui8MAC[1], g_sys.ui8MAC[2],
            g_sys.ui8MAC[3], g_sys.ui8MAC[4], g_sys.ui8MAC[5]);
    CLI_printf("%s\n", mac);
}

void cmd_stat(int argc, char *argv[])
{
    CLI_printf("\nSystem Status\n\n");
    CLI_printf("  Tape roller tach   : %u\n", (uint32_t)g_sys.tapeTach);
    CLI_printf("  Tape roller errors : %u\n", g_sys.qei_error_cnt);
    CLI_printf("  Encoder position   : %d\n", g_sys.tapePosition);
    CLI_printf("  Tape Speed         : %d IPS\n", g_sys.tapeSpeed);
    CLI_printf("  RTC clock type     : %s\n", (g_sys.rtcFound) ? "RTC ext" : "cpu");
    CLI_printf("  DCS controller     : ");
    if (g_sys.dcsFound)
        CLI_printf("%d track\n", g_sys.trackCount);
    else
        CLI_printf("(n/a)\n");
    CLI_printf("  SMPTE controller   : ");
    if (g_sys.smpteFound)
        CLI_printf("found\n", g_sys.trackCount);
    else
        CLI_printf("(n/a)\n");
}

void cmd_cfg(int argc, char *argv[])
{
    if (argc == 1)
    {
        if (strcmp(argv[0], "save") == 0)
        {
            ConfigSave(1);
            CLI_puts("Configuration Saved\n");
        }
        else if (strcmp(argv[0], "load") == 0)
        {
            ConfigLoad(1);
            CLI_puts("Configuration Loaded\n");
        }
        else if (strcmp(argv[0], "reset") == 0)
        {
            ConfigReset(1);
            CLI_puts("Configuration Reset\n");
        }
        else
        {
            CLI_puts("Invalid Option\n");
        }
    }
    else
    {
        CLI_puts("Usage: cfg {save|reset|load}\n");
    }
}

//*****************************************************************************
// TAPE TRANSPORT COMMANDS
//*****************************************************************************

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
    CLI_printf("%u IPS\n", g_sys.tapeSpeed);
}

void cmd_rtz(int argc, char *argv[])
{
    LocateSearch(CUE_POINT_HOME, 0);
}

void cmd_zero(int argc, char *argv[])
{
    PositionZeroReset();
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

    if (g_sys.remoteMode != REMOTE_MODE_CUE)
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

    if (g_sys.remoteMode != REMOTE_MODE_STORE)
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

void cmd_smpte(int argc, char *argv[])
{
    if (argc < 1)
    {
        CLI_puts("Missing Argument\n");
        return;
    }

    CLI_puts("SMPTE generator ");

    if (!g_sys.smpteFound)
    {
        CLI_puts("not installed!\n");
        return;
    }

    if (strcmp(argv[0], "start") == 0)
    {
        SMPTE_generator_start();
        CLI_puts("START\n");
    }
    else if (strcmp(argv[0], "stop") == 0)
    {
        SMPTE_generator_stop();
        CLI_puts("STOP\n");
    }
    else if (strcmp(argv[0], "resume") == 0)
    {
        SMPTE_generator_resume();
        CLI_puts("RESUME\n");
    }
    else if (strcmp(argv[0], "revid") == 0)
    {
        char buf[16];
        uint16_t revid = 0;
        SMPTE_get_revid(&revid);
        sprintf(buf, "REVID %04X\n", revid);
        CLI_puts(buf);
    }
    else
    {
        CLI_puts("\n\nUsage: smpte {start|stop|resume|revid}\n");
    }
}

//*****************************************************************************
// TIME AND DATE COMMANDS
//*****************************************************************************

void cmd_time(int argc, char *argv[])
{
    RTCC_Struct ts;

    if (argc == 0)
    {
        if (!RTC_IsRunning())
            return;

        RTC_GetDateTime(&ts);

        CLI_printf("Current time: %d:%02d:%02d\n", ts.hour, ts.min, ts.sec);
    }
    else if (argc == 3)
    {
        /* Get current time/date */
        RTC_GetDateTime(&ts);

        ts.hour = (uint8_t)atoi(argv[0]);
        ts.min  = (uint8_t)atoi(argv[1]);
        ts.sec  = (uint8_t)atoi(argv[2]);

        if (RTC_IsValidTime(&ts))
        {
            RTC_SetDateTime(&ts);
            CLI_printf("Time set!\n");
        }
        else
        {
            CLI_printf("Invalid time entered\n");
        }
    }
    else
    {
        CLI_printf("Enter time as: hh:mm:ss\n");
    }
}

void cmd_date(int argc, char *argv[])
{
    RTCC_Struct ts;

    if (argc == 0)
    {
        if (!RTC_IsRunning())
            return;

        RTC_GetDateTime(&ts);

        CLI_printf("Current date: %d/%d/%d\n", ts.month+1, ts.date+1, ts.year+2000);
    }
    else if (argc == 3)
    {
        /* Get current time/date */
        RTC_GetDateTime(&ts);

        ts.month   = (uint8_t)(atoi(argv[0]) - 1);
        ts.date    = (uint8_t)(atoi(argv[1]) - 1);
        ts.year    = (uint8_t)(atoi(argv[2]) - 2000);
        ts.weekday = (uint8_t)((ts.date % 7) + 1);

        if (RTC_IsValidDate(&ts))
        {
            RTC_SetDateTime(&ts);
            CLI_printf("Date set!\n");
        }
        else
        {
            CLI_printf("Invalid date entered\n");
        }
    }
    else
    {
        CLI_printf("Enter date as: mm/dd/yyyy\n");
    }
}

//*****************************************************************************
// FILE SYSTEM COMMANDS
//*****************************************************************************

void cmd_dir(int argc, char *argv[])
{
    static char buf[MAX_PATH];

    CLI_printf("\n Directory of %s\n\n", s_cwd);

    strcpy(buf, ".");

    if (argc >= 1)
    {
        strncpy(buf, argv[0], sizeof(buf)-1);
        buf[sizeof(buf)-1] = 0;
    }

    _dirlist(buf);
}

void cmd_cwd(int argc, char *argv[])
{
    CLI_printf("%s\n", _getcwd());
}

void cmd_cd(int argc, char *argv[])
{
    FRESULT res;

    if (argc == 1)
    {
        /* Attempt to change to the directory path */
        res = f_chdir(argv[0]);

        /* Read back the current directory we're in */
        _getcwd();

        _checkcmd(res);
    }
    else
    {
        _getcwd();

        CLI_printf("%s\n", s_cwd);
    }
}

void cmd_md(int argc, char *argv[])
{
    FRESULT res;

    if (argc == 1)
    {
        /* Attempt to make a directory */
        res = f_mkdir(argv[0]);

        _checkcmd(res);
    }
}

void cmd_ren(int argc, char *argv[])
{
    FRESULT res;

    if (argc == 2)
    {
        /* Rename/Move a file or directory */
        res = f_rename(argv[0], argv[1]);

        _checkcmd(res);
    }
}

void cmd_del(int argc, char *argv[])
{
    FRESULT res;

    if (argc == 1)
    {
        /* Attempt to make a directory */
        res = f_unlink(argv[0]);

        _checkcmd(res);
    }
}

void cmd_copy(int argc, char *argv[])
{
    FIL fsrc, fdst;
    FRESULT res;
    UINT br, bw;
    BYTE buf[256];
    char src[128];
    char dest[128];

    if (argc != 2)
    {
        CLI_printf("Source and destination name are required\n");
        return;
    }

    strcpy(src, "0:");
    strncpy(&src[2], argv[0], sizeof(src)-4);

    /* Open source file on the drive 1 */
    res = f_open(&fsrc, src, FA_READ);

    if (res != FR_OK)
    {
        _checkcmd(res);
        return;
    }

    strcpy(dest, "0:");
    strncpy(&dest[2], argv[1], sizeof(dest)-4);

    /* Create destination file on the drive 0 */
    res = f_open(&fdst, dest, FA_WRITE | FA_CREATE_ALWAYS);

    if (res != FR_OK)
    {
        f_close(&fsrc);
        _checkcmd(res);
        return;
    }

    CLI_puts("Copying...");

    /* Copy source to destination */
    for (;;)
    {
        /* Read a chunk of data from the source file */
        res = f_read(&fsrc, buf, sizeof(buf), &br);

        if (br == 0)
            break;      /* error or eof */

        /* Write it to the destination file */
        res = f_write(&fdst, buf, br, &bw);

        if (bw < br)
        {
            /* error or disk full */
            _checkcmd(res);
            break;
        }
    }

    CLI_puts("done\n");

    f_close(&fsrc);
    f_close(&fdst);
}

//*****************************************************************************
// FILE TRANSFER COMMANDS
//*****************************************************************************

void cmd_xmdm(int argc, char *argv[])
{
    int rc = 0;
    FIL fp;
    FRESULT res = FR_OK;

    char *eraseEOL = VT100_ERASE_EOL;

    CLI_putc('\n');

    if (argc != 2)
    {
        CLI_printf("XMODEM Usage:\n\n");
        CLI_printf("xmdm s {filename}\t[sends a file]\n");
        CLI_printf("xmdm r {filename}\t[receives a file]\n");
        return;
    }

    if (toupper(*argv[0]) == 'R')
    {
        /* Receive a file */
        if ((res = f_open(&fp, argv[1], FA_WRITE|FA_OPEN_ALWAYS)) == FR_OK)
        {
            CLI_printf("XMODEM Receive Ready\n");

            /* Receive file via XMODEM */
            rc = xmodem_receive(s_handleUart, &fp);

            f_close(&fp);

            if (rc != XMODEM_SUCCESS)
            {
                /* Delete the file, it's not valid */
                f_unlink(argv[1]);

                CLI_printf("\r%s\rReceive Error %d\n", eraseEOL, rc);
            }
            else
            {
                CLI_printf("\r%s\rReceive Complete\n", eraseEOL);
            }
        }
        else
        {
            _perror(res);
        }
    }
    else if (toupper(*argv[0]) == 'S')
    {
        /* Send a file */
        if ((res = f_open(&fp, argv[1], FA_READ)) == FR_OK)
        {
            CLI_printf("XMODEM Send Ready\n");

            /* Send file via XMODEM */
            rc = xmodem_send(s_handleUart, &fp);

            f_close(&fp);

            if (rc != XMODEM_SUCCESS)
            {
                CLI_printf("\r%s\rSend Error %d\n", eraseEOL, rc);
            }
            else
            {
                CLI_printf("\r%s\rSend Complete\n", eraseEOL);
            }
        }
        else
        {
            _perror(res);
        }
    }
    else
    {
        CLI_printf("Invalid Option\n");
    }
}

// End-Of-File
