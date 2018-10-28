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
#include <ti/drivers/SPI.h>
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

#include <stdint.h>
#include <stdbool.h>
#include <inc/hw_memmap.h>
#include <inc/hw_types.h>
#include <inc/hw_ints.h>
#include <inc/hw_gpio.h>

#include <driverlib/sysctl.h>
#include <driverlib/gpio.h>
#include <driverlib/sysctl.h>
#include <driverlib/qei.h>
#include <driverlib/pin_map.h>

/* STC-1200 Board Header file */
#include "Board.h"
#include "STC1200.h"
#include "MidiTask.h"
#include "IPCCommands.h"
#include "CLITask.h"

#define MAX_PACKET_SIZE     48

/* External Data Items */

//extern SYSDATA g_sysData;

/* Static Data Items */

/* Static Function Prototypes */

int Midi_RxCommand(UART_Handle handle, uint8_t* pbyDeviceID, uint8_t* pBuffer, size_t* puNumBytesRead);
int Midi_TxResponse(UART_Handle handle, uint8_t byDeviceID, uint8_t* pBuffer, size_t uNumBytesToWrite);

//*****************************************************************************
// MIDI MCC Task
//*****************************************************************************

Void MidiTaskFxn(UArg arg0, UArg arg1)
{
    int rc;
	UART_Params uartParams;
	UART_Handle handle;
	size_t	uNumBytesRead;
	uint8_t byDeviceID;

	static uint8_t rxBuffer[MAX_PACKET_SIZE];

    /* Open the UART for MIDI communications. The MIDI data
     * frame (1 start bit, 8 data bits, 1 stop bit) is a subset
     * of the data frames allowed by RS-232 both define a positive
     * voltage/current to be a 'logical 0'. A negative voltage is 
     * a 'logical 1' for RS-232 (note 1). A zero current is a
     * 'logical 1'. MIDI has a single baud rate 31250 baud.
     */
	UART_Params_init(&uartParams);

	uartParams.readMode       = UART_MODE_BLOCKING;
	uartParams.writeMode      = UART_MODE_BLOCKING;
	uartParams.readTimeout    = 2000;					// 2 second read timeout
	uartParams.writeTimeout   = BIOS_WAIT_FOREVER;
	uartParams.readCallback   = NULL;
	uartParams.writeCallback  = NULL;
	uartParams.readReturnMode = UART_RETURN_FULL;
	uartParams.writeDataMode  = UART_DATA_BINARY;
	uartParams.readDataMode   = UART_DATA_BINARY;
	uartParams.readEcho       = UART_ECHO_OFF;
	uartParams.baudRate       = 31250;
	uartParams.stopBits       = UART_STOP_ONE;
	uartParams.parityType     = UART_PAR_NONE;

	handle = UART_open(Board_UART_MIDI, &uartParams);

	if (handle == NULL)
	    System_abort("Error opening MIDI UART\n");

	byDeviceID = 0x00;

    while (true)
    {
    	/* Attempt to read a MIDI MCC command */

    	memset(&rxBuffer, 0, sizeof(rxBuffer));

    	rc = Midi_RxCommand(handle, &byDeviceID, rxBuffer, &uNumBytesRead);

        if (rc != 0)
        {
            if (rc < -1)
            {
                CLI_printf("MidiRxError %d\n", rc);
            }
        }
        else
    	{
            CLI_printf("MMC(%d)-", uNumBytesRead);

    		switch(rxBuffer[0])
    		{
                case MCC_STOP:
                    CLI_printf("STOP\n");
                    Transport_Stop();
                    break;

                case MCC_PLAY:
                    CLI_printf("PLAY\n");
                    Transport_Play();
                    break;

                case MCC_DEFERRED_PLAY:
                    CLI_printf("DEF-PLAY\n");
                    break;

                case MCC_FAST_FORWARD:
                    Transport_Fwd(0);
                    CLI_printf("FFWD\n");
                    break;

                case MCC_REWIND:
                    Transport_Rew(0);
                    CLI_printf("REW\n");
                    break;

                case MCC_RECORD_STROBE:
                    CLI_printf("REC-STROBE\n");
                    break;

                case MCC_RECORD_EXIT:
                    CLI_printf("REC-EXIT\n");
                    break;

                case MCC_RECORD_PAUSE:
                    CLI_printf("REC-PAUSE\n");
                    break;

                case MCC_PAUSE:
                    CLI_printf("PAUSE\n");
                    break;

                case MCC_EJECT:
                    CLI_printf("EJECT\n");
                    break;

                case MCC_CHASE:
                    CLI_printf("CHASE\n");
                    break;

                case MCC_COMMAND_ERROR_RESET:
                    CLI_printf("CMD ERROR RESET\n");
                    break;

                case MCC_MMC_RESET:
                    CLI_printf("RESET\n");
                    break;

                default:
                    CLI_printf("UNKNOWN %02x\n", rxBuffer[0]);
                    break;
    		}
    	}
    }
}

//*****************************************************************************
//
//*****************************************************************************

int Midi_RxCommand(UART_Handle handle, uint8_t* pbyDeviceID, uint8_t* pBuffer, size_t* puNumBytesRead)
{
	uint8_t b;
	size_t i;
    int rc = 0;

    *puNumBytesRead = 0;
    *pbyDeviceID = 0;
    
    i = 0;

    do {

        /* Read a byte looking for 0xF0 Preamble */
        if (UART_read(handle, &b, 1) != 1)
            return -1;  /* timeout */

        CLI_printf("%02x ", b);

        ++i;

    } while (b != 0xF0);

    /* Read the 0x7F Preamble */
    if (UART_read(handle, &b, 1) != 1)
        return -1;

    /* If not the second preamble 0x7F byte, then out of sync */
    if (b != 0x7F)
        return -3;
    
    /* Read the device ID */
    if (UART_read(handle, &b, 1) != 1)
        return -1;

    *pbyDeviceID = b;
        
    /* Read the MCC (motion control command) type byte */
    if (UART_read(handle, &b, 1) != 1)
        return -1;

    if (b != MIDI_MCC)
        return -4;
        
    /* Read the MIDI packet data up to the 0x7F end of packet marker */
    
    for (i=0; i < MAX_PACKET_SIZE; i++)
    {
        /* Read a command byte */
        if (UART_read(handle, &b, 1) != 1)
        {
            rc = -1;
            break;
        }

        /* End of packet 0x7F indicator */
    	if (b == 0x7F)
        {
    	    CLI_printf("\n");

            *puNumBytesRead = i;
            rc = 0;
    	    break;
    	}

    	/* Store the data byte read */
    	*pBuffer++ = b;
    }    
    
    return rc;
}

//*****************************************************************************
//
//*****************************************************************************

int Midi_TxResponse(UART_Handle handle, uint8_t byDeviceID, uint8_t* pBuffer, size_t uNumBytesToWrite)
{
	uint8_t b;

    /* Write the 0xF0 Preamble */      
    b = 0xF0;
    UART_write(handle, &b, 1);
    
    /* Write the 0x7F Preamble */      
    b = 0x7F;
    UART_write(handle, &b, 1);

    /* Write the Device ID */
    b = byDeviceID;
    UART_write(handle, &b, 1);

    /* Write the MCR (motion control response) byte */
    b = MIDI_MCR;
    UART_write(handle, &b, 1);

    /* Write the user response data */
   	if (UART_write(handle, &b, uNumBytesToWrite) != uNumBytesToWrite)
   		return -1;

   	/* Write the response packet termination indicator byte */
    b = 0x7F;
    UART_write(handle, &b, 1);

    return uNumBytesToWrite;
}

// End-Of-File
