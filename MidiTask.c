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
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Diags.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
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
extern SYSCFG g_cfg;

/* Static Data Items */

static Mailbox_Handle g_mailboxMidi = NULL;

/* Default MIDI parameters structure */
const MIDI_Params MIDI_defaultParams = {
    NULL,
    MIDI_DEVID_ALL_CALL
};

static MIDI_Handle g_midiHandle;

/* Static Function Prototypes */

static Void MasterTaskFxn(UArg arg0, UArg arg1);
static Void SlaveTaskFxn(UArg arg0, UArg arg1);
static int SlaveRxCommand(MIDI_Handle handle, uint8_t* pbyDeviceID, uint8_t* pBuffer, int* puNumBytesRead);
static int SlaveTxResponse(MIDI_Handle handle, uint8_t* pBuffer, int uNumBytesToWrite);

//*****************************************************************************
// MIDI Controller Construction/Destruction
//*****************************************************************************

MIDI_Handle MIDI_construct(MIDI_Object *obj, MIDI_Params *params)
{
    /* Initialize the object's fields */
    obj->uartHandle  = params->uartHandle;
    obj->deviceID    = params->deviceID;

    GateMutex_construct(&(obj->gate), NULL);

    return (MIDI_Handle)obj;
}

MIDI_Handle MIDI_create(MIDI_Params *params)
{
    MIDI_Handle handle;
    Error_Block eb;

    Error_init(&eb);

    handle = Memory_alloc(NULL, sizeof(MIDI_Object), NULL, &eb);

    if (handle == NULL)
        return NULL;

    handle = MIDI_construct(handle, params);

    return handle;
}

Void MIDI_delete(MIDI_Handle handle)
{
    MIDI_destruct(handle);

    Memory_free(NULL, handle, sizeof(MIDI_Object));
}

Void MIDI_destruct(MIDI_Handle handle)
{
    Assert_isTrue((handle != NULL), NULL);

    if (handle->uartHandle)
        UART_close(handle->uartHandle);

    GateMutex_destruct(&(handle->gate));
}

Void MIDI_Params_init(MIDI_Params *params)
{
    Assert_isTrue(params != NULL, NULL);

    *params = MIDI_defaultParams;
}

//*****************************************************************************
// MIDI Task Initialize
//*****************************************************************************

Bool MIDI_Server_init(void)
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

Bool MIDI_Server_startup(void)
{
    Error_Block eb;
    Task_Params taskParams;
    UART_Params uartParams;
    MIDI_Params midiParams;

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

    /* Create the MIDI server object */

    MIDI_Params_init(&midiParams);

    midiParams.uartHandle  = UART_open(Board_UART_MIDI, &uartParams);
    midiParams.deviceID    = g_cfg.midiDevID;

    g_midiHandle = MIDI_create(&midiParams);

    /* Startup the MIDI server tasks */

    Error_init(&eb);
    Task_Params_init(&taskParams);

    taskParams.stackSize = 800;
    taskParams.priority  = 5;
    taskParams.arg0      = 0;
    taskParams.arg1      = 0;

    if (!Task_create((Task_FuncPtr)SlaveTaskFxn, &taskParams, &eb))
        System_abort("SlaveTaskFxn create failed\n");

    Error_init(&eb);
    Task_Params_init(&taskParams);

    taskParams.stackSize = 800;
    taskParams.priority  = 5;
    taskParams.arg0      = 0;
    taskParams.arg1      = 0;

    if (!Task_create((Task_FuncPtr)MasterTaskFxn, &taskParams, &eb))
        System_abort("MasterTaskFxn create failed\n");

    return TRUE;
}

//*****************************************************************************
// Set the next or immediate transport mode requested.
//*****************************************************************************

Bool MidiQueueResponse(MidiMessage* msg)
{
    return Mailbox_post(g_mailboxMidi, msg, 500);
}

//*****************************************************************************
//
//*****************************************************************************

Void MasterTaskFxn(UArg arg0, UArg arg1)
{
    MidiMessage msgMidi;

    while (TRUE)
    {
        if (Mailbox_pend(g_mailboxMidi, &msgMidi, BIOS_WAIT_FOREVER))
        {
            SlaveTxResponse(g_midiHandle, msgMidi.data, msgMidi.length);
        }
    }
}

//*****************************************************************************
// MIDI MMC Slave Task
//*****************************************************************************

Void SlaveTaskFxn(UArg arg0, UArg arg1)
{
    bool reply;
    int rc = 0;
    uint8_t deviceID;
	int	numBytes;

	static uint8_t rxBuffer[MIDI_MAX_PACKET_SIZE];


    while (true)
    {
    	/* Read a MIDI MCC command */

    	memset(&rxBuffer, 0, sizeof(rxBuffer));

    	rc = SlaveRxCommand(g_midiHandle, &deviceID, rxBuffer, &numBytes);

        if (rc != 0)
        {
            if (rc < -1)
            {
                System_printf("MidiRxError %d\n", rc);
                System_flush();
            }

            continue;
        }

        reply = true;

  		switch(rxBuffer[0])
   		{
        case MCC_STOP:
            /* Change transport to stop mode */
            Transport_Stop();
            break;

        case MCC_PLAY:
        case MCC_DEFERRED_PLAY:
            /* Change transport to play mode */
            Transport_Play(0);
            break;

        case MCC_FAST_FORWARD:
            /* Change transport for fast forward mode */
            Transport_Fwd(0, 0);
            break;

        case MCC_REWIND:
            /* Change transport to rewind mode */
            Transport_Rew(0, 0);
            break;

        case MCC_RECORD_STROBE:
            /* Toggle Record, if in play */
            Transport_RecStrobe();
            break;

        case MCC_RECORD_EXIT:
            /* Exit record, but remain in play */
            Transport_RecExit();
            break;

        case MCC_RECORD_PAUSE:
            break;

        case MCC_PAUSE:
            break;

        case MCC_EJECT:
            break;

        case MCC_CHASE:
            break;

        case MCC_COMMAND_ERROR_RESET:
            break;

        case MCC_MMC_RESET:
            break;

        default:
            reply = false;
            System_printf("MIDI Cmd Unknown %d\n", rxBuffer[0]);
            System_flush();
            break;
   		}

   		if (reply)
   		{
   		    /* If closed loop, send the reply */
   		    SlaveTxResponse(g_midiHandle, rxBuffer, numBytes);
   		}
    }
}

//*****************************************************************************
// Receive a MIDI MCC command request from a master.
//*****************************************************************************

int SlaveRxCommand(MIDI_Handle handle, uint8_t* pbyDeviceID, uint8_t* pBuffer, int* puNumBytesRead)
{
	uint8_t b;
	int i;
    int rc = 0;

    *puNumBytesRead = 0;
    *pbyDeviceID = 0;
    
    i = 0;

    do {
        /* Read a byte looking for 0xF0 Preamble */
        if (UART_read(handle->uartHandle, &b, 1) != 1)
            return MIDI_ERR_TIMEOUT;
        ++i;
    } while (b != 0xF0);

    /* Read the 0x7F Preamble */
    if (UART_read(handle->uartHandle, &b, 1) != 1)
        return MIDI_ERR_TIMEOUT;

    /* If not preamble 0x7F byte, then out of sync */
    if (b != 0x7F)
        return MIDI_ERR_FRAME_BEGIN;

    /* Read the device ID */
    if (UART_read(handle->uartHandle, &b, 1) != 1)
        return MIDI_ERR_TIMEOUT;

    *pbyDeviceID = b;

    /* Read the MCC (motion control command) type byte */
    if (UART_read(handle->uartHandle, &b, 1) != 1)
        return MIDI_ERR_TIMEOUT;

    if (b != MIDI_MCC)
        return MIDI_ERR_MMC_INVALID;

    /* Read the MIDI packet data up to the 0x7F end of packet marker */
    
    rc = MIDI_ERR_RX_OVERFLOW;

    for (i=0; i < MIDI_MAX_PACKET_SIZE; i++)
    {
        /* Read a command byte */
        if (UART_read(handle->uartHandle, &b, 1) != 1)
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
// Send a MIDI MCR response packet back to the master.
//*****************************************************************************

int SlaveTxResponse(MIDI_Handle handle, uint8_t* pBuffer, int uNumBytesToWrite)
{
	uint8_t b;
	uint8_t hdr[4];

	hdr[0] = 0xF0;                  /* 0xF0 Preamble */
	hdr[1] = 0x7F;                  /* 0x7F Preamble */
	hdr[2] = handle->deviceID;      /* the device ID */
    hdr[3] = MIDI_MCR;              /* motion control response byte */

    /* Write the response packet header */
    if (UART_write(handle->uartHandle, hdr, 4) != 4)
        return MIDI_ERR_TIMEOUT;

    /* Write the user response data */
   	if (UART_write(handle->uartHandle, pBuffer, uNumBytesToWrite) != uNumBytesToWrite)
   		return MIDI_ERR_TIMEOUT;

   	/* Write the response packet termination indicator byte */
    b = 0xF7;
    UART_write(handle->uartHandle, &b, 1);

    return uNumBytesToWrite;
}

// End-Of-File
