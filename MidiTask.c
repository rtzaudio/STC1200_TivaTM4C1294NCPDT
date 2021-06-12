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
#include <ti/sysbios/gates/GateMutex.h>

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
#include "IPCMessage.h"
#include "CLITask.h"

/* External Data Items */

extern SYSDAT g_sys;

/* Static Data Items */

static UART_Handle g_handleMidi;
static Mailbox_Handle g_mailboxMidi = NULL;
static MIDI_SERVICE g_midi;

/* Static Function Prototypes */

static Void MidiWriterTaskFxn(UArg arg0, UArg arg1);
static Void MidiReaderTaskFxn(UArg arg0, UArg arg1);
static int Midi_RxCommand(UART_Handle handle, uint8_t* pbyDeviceID, uint8_t* pBuffer, size_t* puNumBytesRead);
static int Midi_TxResponse(UART_Handle handle, uint8_t byDeviceID, uint8_t* pBuffer, size_t uNumBytesToWrite);

//*****************************************************************************
// MIDI Task Initialize
//*****************************************************************************

Bool Midi_Server_init(void)
{
    Error_Block eb;
    Mailbox_Params mboxParams;

    Error_init(&eb);
    Mailbox_Params_init(&mboxParams);
    g_mailboxMidi = Mailbox_create(sizeof(MidiMessage), 16, &mboxParams, &eb);

    if (g_mailboxMidi == NULL) {
        System_abort("MIDI Mailbox create failed\n");
    }

    return TRUE;
}

//*****************************************************************************
// MIDI Task Startup
//*****************************************************************************

Bool Midi_Server_startup(void)
{
    Error_Block eb;
    Task_Params taskParams;
    UART_Params uartParams;


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
    uartParams.readTimeout    = 2000;                   // 2 second read timeout
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

    g_handleMidi = UART_open(Board_UART_MIDI, &uartParams);

    if (g_handleMidi == NULL)
        System_abort("Error opening MIDI UART\n");

    /* Startup the MIDI server tasks */

    Error_init(&eb);
    Task_Params_init(&taskParams);

    taskParams.stackSize = 800;
    taskParams.priority  = 5;
    taskParams.arg0      = 0;
    taskParams.arg1      = 0;

    if (!Task_create((Task_FuncPtr)MidiReaderTaskFxn, &taskParams, &eb))
        System_abort("MidiReaderTaskFxn create failed\n");

    Error_init(&eb);
    Task_Params_init(&taskParams);

    taskParams.stackSize = 800;
    taskParams.priority  = 5;
    taskParams.arg0      = 0;
    taskParams.arg1      = 0;

    if (!Task_create((Task_FuncPtr)MidiWriterTaskFxn, &taskParams, &eb))
        System_abort("MidiWriterTaskFxn create failed\n");

    return TRUE;
}

//*****************************************************************************
// Set the next or immediate transport mode requested.
//*****************************************************************************

Bool MidiQueueResponse(MidiMessage* msg)
{
    return Mailbox_post(g_mailboxMidi, msg, BIOS_WAIT_FOREVER);
}

//*****************************************************************************
//
//*****************************************************************************

Void MidiWriterTaskFxn(UArg arg0, UArg arg1)
{
    MidiMessage msgMidi;

    while (TRUE)
    {
        Mailbox_pend(g_mailboxMidi, &msgMidi, BIOS_WAIT_FOREVER);

        Midi_TxResponse(g_handleMidi, g_midi.deviceID, msgMidi.data, msgMidi.length);
    }
}

//*****************************************************************************
// MIDI MCC Task
//*****************************************************************************

#define DEBUG_MIDI   0

Void MidiReaderTaskFxn(UArg arg0, UArg arg1)
{
    int rc = 0;
	size_t	uNumBytesRead;

	static uint8_t rxBuffer[MIDI_MAX_PACKET_SIZE];


    while (true)
    {
#if (DEBUG_MIDI > 0)
        uint8_t b;

        /* Read a byte looking for 0xF0 Preamble */
        if (UART_read(g_handleMidi, &b, 1) == 1)
        {
            CLI_printf("%02x ", b);

            if (b == 0xF7)
            {
                CLI_printf("\n");
                rc = 0;
            }

            if ((rc++ % 16) == 15)
                CLI_printf("\n");
        }
#endif
    	/* Attempt to read a MIDI MCC command */

    	memset(&rxBuffer, 0, sizeof(rxBuffer));

    	rc = Midi_RxCommand(g_handleMidi, &g_midi.deviceID, rxBuffer, &uNumBytesRead);

        if (rc != 0)
        {
            if (rc < -1)
            {
#if (DEBUG_MIDI > 0)
                CLI_printf("MidiRxError %d\n", rc);
#endif
            }
        }
        else
    	{
#if (DEBUG_MIDI > 1)
            CLI_printf("MMC(%d)-", g_midi.deviceID);
#endif
    		switch(rxBuffer[0])
    		{
                case MCC_STOP:
#if (DEBUG_MIDI > 1)
                    CLI_printf("STOP\n");
#endif
                    Transport_Stop();
                    break;

                case MCC_PLAY:
#if (DEBUG_MIDI > 1)
                    CLI_printf("PLAY\n");
                    Transport_Play(0);
#endif
                    break;

                case MCC_DEFERRED_PLAY:
#if (DEBUG_MIDI > 1)
                    CLI_printf("DEF-PLAY\n");
#endif
                    break;

                case MCC_FAST_FORWARD:
                    Transport_Fwd(0, 0);
#if (DEBUG_MIDI > 1)
                    CLI_printf("FFWD\n");
#endif
                    break;

                case MCC_REWIND:
                    Transport_Rew(0, 0);
#if (DEBUG_MIDI > 1)
                    CLI_printf("REW\n");
#endif
                    break;

                case MCC_RECORD_STROBE:
                    Transport_Play(M_RECORD);
#if (DEBUG_MIDI > 1)
                    CLI_printf("REC-STROBE\n");
#endif
                    break;

                case MCC_RECORD_EXIT:
                    Transport_Play(0);
#if (DEBUG_MIDI > 1)
                    CLI_printf("REC-EXIT\n");
#endif
                    break;

                case MCC_RECORD_PAUSE:
#if (DEBUG_MIDI > 1)
                    CLI_printf("REC-PAUSE\n");
#endif
                    break;

                case MCC_PAUSE:
#if (DEBUG_MIDI > 1)
                    CLI_printf("PAUSE\n");
#endif
                    break;

                case MCC_EJECT:
#if (DEBUG_MIDI > 1)
                    CLI_printf("EJECT\n");
#endif
                    break;

                case MCC_CHASE:
#if (DEBUG_MIDI > 1)
                    CLI_printf("CHASE\n");
#endif
                    break;

                case MCC_COMMAND_ERROR_RESET:
#if (DEBUG_MIDI > 1)
                    CLI_printf("CMD ERROR RESET\n");
#endif
                    break;

                case MCC_MMC_RESET:
#if (DEBUG_MIDI > 1)
                    CLI_printf("RESET\n");
#endif
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
            return MIDI_ERR_TIMEOUT;
        ++i;
    } while (b != 0xF0);

    /* Read the 0x7F Preamble */
    if (UART_read(handle, &b, 1) != 1)
        return MIDI_ERR_TIMEOUT;

    /* If not preamble 0x7F byte, then out of sync */
    if (b != 0x7F)
        return MIDI_ERR_FRAME_BEGIN;

    /* Read the device ID */
    if (UART_read(handle, &b, 1) != 1)
        return MIDI_ERR_TIMEOUT;

    *pbyDeviceID = b;

    /* Read the MCC (motion control command) type byte */
    if (UART_read(handle, &b, 1) != 1)
        return MIDI_ERR_TIMEOUT;

    if (b != MIDI_MCC)
        return MIDI_ERR_MMC_INVALID;

    /* Read the MIDI packet data up to the 0x7F end of packet marker */
    
    rc = MIDI_ERR_RX_OVERFLOW;

    for (i=0; i < MIDI_MAX_PACKET_SIZE; i++)
    {
        /* Read a command byte */
        if (UART_read(handle, &b, 1) != 1)
        {
            rc = MIDI_ERR_TIMEOUT;
            break;
        }

        /* End of packet 0xF7 indicator */
    	if (b == 0xF7)
        {
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
   	if (UART_write(handle, pBuffer, uNumBytesToWrite) != uNumBytesToWrite)
   		return MIDI_ERR_TIMEOUT;

   	/* Write the response packet termination indicator byte */
    b = 0xF7;
    UART_write(handle, &b, 1);

    return uNumBytesToWrite;
}

// End-Of-File
