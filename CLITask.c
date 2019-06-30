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
#include <xdc/runtime/Memory.h>

#include <ti/sysbios/BIOS.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SDSPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>

#include <file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include <driverlib/sysctl.h>

#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Queue.h>

#include "STC1200.h"
#include "Board.h"
#include "Utils.h"
#include "CLITask.h"

//*****************************************************************************
// Type Definitions
//*****************************************************************************

typedef union {
    char  *s;
    char   c;
    float  f;
} arg_t;

typedef struct {
    const char* name;
    void (*func)(arg_t*);
    const char* args;
    const char* doc;
} cmd_t;

#define MK_CMD(x) void cmd_ ## x (arg_t*)

//*****************************************************************************
// CLI Function Handle Declarations
//*****************************************************************************

#define CMDS 5

MK_CMD(ipaddr);
MK_CMD(macaddr);
MK_CMD(sernum);
MK_CMD(cls);
MK_CMD(help);

/* The dispatch table */
#define CMD(func, params, help) {#func, cmd_ ## func, params, help}

cmd_t dsp_table[CMDS] = {
    CMD(ipaddr, "", "Displays IP address"),
    CMD(macaddr, "", "Displays MAC address"),
    CMD(sernum, "", "Displays serial number"),
    CMD(cls, "", "Clear the screen"),
    CMD(help, "", "Display this help")
};

//*****************************************************************************
// Static and External Data Items
//*****************************************************************************

#define MAX_CHARS   80

/*** Static Data Items ***/

static UART_Handle s_handleUart;
static const char *delim = " \n(,);";
static char cmdbuf[MAX_CHARS+3];

extern SYSDATA g_sysData;
extern SYSPARMS g_sysParms;

/*** Function Prototypes ***/

static void parse(char *cmd);
static arg_t *args_parse(const char *s);

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

//*****************************************************************************
//
//*****************************************************************************

Void CLITaskFxn(UArg arg0, UArg arg1)
{
    uint8_t ch;
    int cnt = 0;

    /* Now begin the main program command task processing loop */

    CLI_printf(VT100_HOME);
    CLI_printf(VT100_CLS);

    CLI_printf("STC-1200 v%d.%02d.%03d\n\n", FIRMWARE_VER, FIRMWARE_REV, FIRMWARE_BUILD);
    CLI_puts("Enter 'help' to view a list valid commands\n\n");
    CLI_putc('>');

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
                    parse(cmdbuf);
                    cmdbuf[0] = 0;
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
                    cmdbuf[--cnt] = 0;
                    CLI_putc(BKSPC);
                    CLI_putc(' ');
                    CLI_putc(BKSPC);
                }
            }
            else
            {
                if (cnt < MAX_CHARS)
                {
                    if (isalnum((int)ch))
                    {
                        cmdbuf[cnt++] = ch;
                        cmdbuf[cnt] = 0;
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

void parse(char *cmd)
{
    const char* tok = strtok(cmd,delim);

    if (!tok)
        return;

    int i = CMDS;

    while(i--)
    {
        cmd_t cur = dsp_table[i];

        if (!strcmp(tok,cur.name))
        {
            arg_t *args = args_parse(cur.args);

            if (args == NULL && strlen(cur.args))
                return;//Error in argument parsing

            cur.func(args);

            free(args);
            return;
        }
    }

    puts("Command Not Found");
}

#define ESCAPE { free(args); CLI_puts("Bad Argument(s)\n"); return NULL; }

arg_t *args_parse(const char *s)
{
    int argc = strlen(s);

    arg_t *args = malloc(sizeof(arg_t)*argc);

    int i;

    for(i=0; i < argc; ++i)
    {
        char *tok;

        switch(s[i])
        {
            case 's':
                args[i].s = strtok(NULL,delim);
                if (!args[i].s)
                    ESCAPE;
                break;

            case 'c':
                tok = strtok(NULL,delim);
                if (!tok)
                    ESCAPE;
                args[i].c = tok[0];
                if (!islower(args[i].c))
                    ESCAPE;
                break;

            case 'f':
                tok = strtok(NULL,delim);
                if (sscanf(tok,"%f", &args[i].f)!=1)
                    ESCAPE;
                break;
        }
    }

    return args;
}
#undef ESCAPE

//*****************************************************************************
// CLI Command Handlers
//*****************************************************************************

void cmd_ipaddr(arg_t *args)
{
    CLI_printf("%s", g_sysData.ipAddr);
}

void cmd_macaddr(arg_t *args)
{
    char mac[32];
    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
            g_sysData.ui8MAC[0], g_sysData.ui8MAC[1], g_sysData.ui8MAC[2],
            g_sysData.ui8MAC[3], g_sysData.ui8MAC[4], g_sysData.ui8MAC[5]);
    CLI_printf("%s", mac);
}

void cmd_sernum(arg_t *args)
{
    char serialnum[64];
    /*  Format the 64 bit GUID as a string */
    GetHexStr(serialnum, g_sysData.ui8SerialNumber, 16);
    CLI_printf("%s", serialnum);
}

void cmd_cls(arg_t *args)
{
    CLI_puts(VT100_CLS);
    CLI_puts(VT100_HOME);
}

void cmd_help(arg_t *args)
{
    char tmp[100];
    int i=CMDS;

    CLI_puts("\nAvailable Commands:\n\n");

    while(i--)
    {
        cmd_t cmd=dsp_table[i];
        snprintf(tmp,100,"%s(%s)", cmd.name, cmd.args);
        CLI_printf("%10s\t- %s\n", tmp, cmd.doc);
    }
}

// End-Of-File
