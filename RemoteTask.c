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
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SDSPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>

/* NDK BSD support */
#include <sys/socket.h>

#include <file.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include <driverlib/sysctl.h>

/* Graphiclib Header file */
#include <grlib/grlib.h>
#include "drivers/offscrmono.h"

/* PMX42 Board Header file */
#include "Board.h"
#include "RAMP.h"
#include "STC1200.h"

/* External Data Items */

extern SYSDATA g_sysData;

/* Global Data Items */

UART_Handle g_handleUart422;

FCB g_TxFcb;

/* Static Function Prototypes */


//*****************************************************************************
// This main RS-422 wired remote control task.
//*****************************************************************************

Void RemoteTaskFxn(UArg arg0, UArg arg1)
{
	UART_Params uartParams;

    /*
     * Open the UART for RS-422 communications
     */

	UART_Params_init(&uartParams);

	uartParams.readMode       = UART_MODE_BLOCKING;
	uartParams.writeMode      = UART_MODE_BLOCKING;
	uartParams.readTimeout    = 1000;					// 1 second read timeout
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

	g_handleUart422 = UART_open(Board_UART_RS422_REMOTE, &uartParams);

	if (g_handleUart422 == NULL)
	    System_abort("Error initializing UART\n");

	/* Assert the RS-422 DE & RE pins */
    GPIO_write(Board_RS422_DE, PIN_HIGH);
    GPIO_write(Board_RS422_RE_N, PIN_LOW);

    /* Now begin the main program command task processing loop */

	RAMP_InitFcb(&g_TxFcb);

    while (true)
    {
    	Task_sleep(2000);

        //GPIO_write(Board_STAT_LED2, Board_LED_ON);

    	/* Transmit a frame of the display buffer */
    	RAMP_FrameTx(g_handleUart422, &g_TxFcb, GrGetScreenBuffer(), GrGetScreenBufferSize());

    	//GPIO_write(Board_STAT_LED2, Board_LED_OFF);

    	/* Increment the frame sequence number */
    	g_TxFcb.seqnum = INC_SEQ_NUM(g_TxFcb.seqnum);
    }
}

// End-Of-File
