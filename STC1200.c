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
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
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

/* Tivaware Driver files */
#include <driverlib/eeprom.h>
#include <driverlib/sysctl.h>

/* Graphiclib Header file */
#include <grlib/grlib.h>
#include "drivers/offscrmono.h"

/* STC1200 Board Header file */
#include "Board.h"
#include "DisplayTask.h"
#include "RemoteTask.h"
#include "STC1200.h"

/* Global STC-1200 System data */
SYSDATA g_sysData;
SYSPARMS g_sysParms;

/* Handles created dynamically */
Mailbox_Handle g_mailboxDisplay = NULL;
Mailbox_Handle mailboxCommand = NULL;

Event_Handle g_eventQEI;

/* Static Function Prototypes */

//*****************************************************************************
// Main Entry Point
//*****************************************************************************

int main(void)
{
	Task_Params taskParams;
    Mailbox_Params mboxParams;
    Error_Block eb;

    /* Call board init functions */
    Board_initGeneral();
    Board_initGPIO();
    Board_initI2C();
    Board_initSPI();
    Board_initUART();
    Board_initEMAC();

    /* Initialize a 1 BPP off-screen OLED display buffer that will draw into */
    GrOffScreenMonoInit();

    /* Deassert the Atmega88 reset line */
    GPIO_write(Board_RESET_AVR_N, PIN_HIGH);

    GPIO_write(Board_TAPE_DIR, PIN_HIGH);
    GPIO_write(Board_MOTION_FWD, PIN_HIGH);
	GPIO_write(Board_MOTION_REW, PIN_HIGH);

    /* Turn on the status LED */
    GPIO_write(Board_STAT_LED, Board_LED_ON);

    /* Deassert the RS-422 DE & RE pins */
    GPIO_write(Board_RS422_DE, PIN_LOW);
    GPIO_write(Board_RS422_RE_N, PIN_HIGH);

    /* Init and enable interrupts */
#if 0
    GPIO_setupCallbacks(&STC1200_gpioPortMCallbacks);
    GPIO_enableInt(Board_STOP_N, GPIO_INT_RISING);
    GPIO_enableInt(Board_PLAY_N, GPIO_INT_RISING);
    GPIO_enableInt(Board_FWD_N, GPIO_INT_RISING);
    GPIO_enableInt(Board_REW_N, GPIO_INT_RISING);
#endif

    /* Create interrupt signal event */
    Error_init(&eb);
    g_eventQEI = Event_create(NULL, &eb);

    /* Create command task mailbox */
    Error_init(&eb);
    Mailbox_Params_init(&mboxParams);
    mailboxCommand = Mailbox_create(sizeof(CommandMessage), 8, &mboxParams, &eb);
    if (mailboxCommand == NULL) {
        System_abort("Mailbox create failed\nAborting...");
    }

    /* Create display task mailbox */
    Error_init(&eb);
    Mailbox_Params_init(&mboxParams);
    g_mailboxDisplay = Mailbox_create(sizeof(DisplayMessage), 2, &mboxParams, &eb);
    if (g_mailboxDisplay == NULL) {
        System_abort("Mailbox create failed\nAborting...");
    }

    /* Create task with priority 15 */
    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 2048;
    taskParams.priority  = 15;
    Task_create((Task_FuncPtr)CommandTaskFxn, &taskParams, &eb);

    System_printf("Starting STC1200 execution.\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}

//*****************************************************************************
// This function attempts to ready the unique serial number
// from the I2C
//*****************************************************************************

Void CommandTaskFxn(UArg arg0, UArg arg1)
{
    Error_Block eb;
	Task_Params taskParams;
    CommandMessage msgCmd;

    /* Read the globally unique serial number from EPROM */
    if (!ReadSerialNumber(g_sysData.ui8SerialNumber)) {
    	System_printf("Read Serial Number Failed!\n");
    	System_flush();
    }

    /*
     * Create the display task priority 15
     */

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 2048;
    taskParams.priority  = 10;
    Task_create((Task_FuncPtr)DisplayTaskFxn, &taskParams, &eb);

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 1024;
    taskParams.priority  = 12;
    Task_create((Task_FuncPtr)RemoteTaskFxn, &taskParams, &eb);

    /* Now begin the main program command task processing loop */

    while (true)
    {
    	/* Wait for a message up to 1 second */
        if (!Mailbox_pend(mailboxCommand, &msgCmd, 1000))
        {
        	/* No message, blink the LED */
    		GPIO_toggle(Board_STAT_LED);
    		continue;
        }
    }
}

//*****************************************************************************
// HWI Callback function for top left button
//*****************************************************************************

void gpioButtonStop(void)
{
	UInt32 timeLO;
	UInt32 timeHI;
	uint32_t btn;
    CommandMessage msg;
    //unsigned int key;

    /* Read the stop button press state */
    btn = GPIO_read(Board_STOP_N);

    /* Check for low transition initially */
    if ((btn & 0x04) == 0)
    {
    	/* Edge went low on button on press */
        timeLO = timeHI = Clock_getTicks();
    }
    else
    {
    	/* Edge went high on button release */
        timeHI = Clock_getTicks();

        if ((timeHI - timeLO) >= 250)
		{
		    msg.command  = SWITCHPRESS;
		    msg.ui32Data = Board_STOP_N;

			Mailbox_post(mailboxCommand, &msg, BIOS_NO_WAIT);
		}
    }

    GPIO_clearInt(Board_STOP_N);
}

//*****************************************************************************
// HWI Callback function for the top right button
//*****************************************************************************

void gpioButtonPlay(void)
{
    //unsigned int key;
    CommandMessage msg;
	uint32_t btn;

    msg.command  = SWITCHPRESS;
    msg.ui32Data = Board_PLAY_N;

    btn = GPIO_read(Board_PLAY_N);

    SysCtlDelay(3000);

    if (btn == GPIO_read(Board_PLAY_N))
    {
    	//key = Gate_enterSystem();
    	/* Clear the last command */
    	//lastCommand[0] = 0x0;
    	//Gate_leaveSystem(key);

    	/* Do not wait if there is no room for the new mail */
    	Mailbox_post(mailboxCommand, &msg, BIOS_NO_WAIT);
    }

    GPIO_clearInt(Board_PLAY_N);
}

//*****************************************************************************
// HWI Callback function for the middle left button
//*****************************************************************************

void gpioButtonFwd(void)
{
    //unsigned int key;
	uint32_t btn;
    CommandMessage msg;

    msg.command  = SWITCHPRESS;
    msg.ui32Data = Board_FWD_N;

    btn = GPIO_read(Board_FWD_N);

    SysCtlDelay(3000);

    if (btn == GPIO_read(Board_FWD_N))
    {
    	//key = Gate_enterSystem();
    	/* Clear the last command */
    	//lastCommand[0] = 0x0;
    	//Gate_leaveSystem(key);

    	/* Do not wait if there is no room for the new mail */
    	Mailbox_post(mailboxCommand, &msg, BIOS_NO_WAIT);
    }

    GPIO_clearInt(Board_FWD_N);
}

//*****************************************************************************
// HWI Callback function for the middle right button
//*****************************************************************************

void gpioButtonRew(void)
{
    //unsigned int key;
	uint32_t btn;
    CommandMessage msg;

    msg.command  = SWITCHPRESS;
    msg.ui32Data = Board_REW_N;

    btn = GPIO_read(Board_REW_N);

    SysCtlDelay(3000);

    if (btn == GPIO_read(Board_REW_N))
    {
    	//key = Gate_enterSystem();
    	/* Clear the last command */
    	//lastCommand[0] = 0x0;
    	//Gate_leaveSystem(key);

    	/* Do not wait if there is no room for the new mail */
    	Mailbox_post(mailboxCommand, &msg, BIOS_NO_WAIT);
    }

    GPIO_clearInt(Board_REW_N);
}

//*****************************************************************************
// This function attempts to ready the unique serial number
// from the I2C
//*****************************************************************************

int ReadSerialNumber(uint8_t ui8SerialNumber[16])
{
	bool			ret;
	uint8_t			txByte;
	I2C_Handle      handle;
	I2C_Params      params;
	I2C_Transaction i2cTransaction;

    /* default invalid serial number is all FF's */
    memset(ui8SerialNumber, 0xFF, sizeof(ui8SerialNumber));

	I2C_Params_init(&params);

	params.transferCallbackFxn = NULL;
	params.transferMode = I2C_MODE_BLOCKING;
	params.bitRate = I2C_100kHz;

	handle = I2C_open(Board_I2C_AT24CS01, &params);

	if (!handle) {
		System_printf("I2C did not open\n");
		System_flush();
		return 0;
	}

	/* Note the Upper bit of the word address must be set
	 * in order to read the serial number. Thus 80H would
	 * set the starting address to zero prior to reading
	 * this sixteen bytes of serial number data.
	 */

	txByte = 0x80;

	i2cTransaction.slaveAddress = Board_AT24CS01_SERIAL_ADDR;
	i2cTransaction.writeBuf     = &txByte;
	i2cTransaction.writeCount   = 1;
	i2cTransaction.readBuf      = ui8SerialNumber;
	i2cTransaction.readCount    = 16;

	ret = I2C_transfer(handle, &i2cTransaction);

	if (!ret)
	{
		System_printf("Unsuccessful I2C transfer\n");
		System_flush();
	}

	I2C_close(handle);

	return ret;
}

//*****************************************************************************
// Set default runtime values
//*****************************************************************************

void InitSysDefaults(SYSPARMS* p)
{
    /* default servo parameters */
    p->version                  = MAKEREV(FIRMWARE_VER, FIRMWARE_REV);
    p->debug                    = 0;        /* debug mode 0=off                 */

}

//*****************************************************************************
// Write system parameters from our global settings buffer to EEPROM.
//
// Returns:  0 = Sucess
//          -1 = Error writing EEPROM data
//*****************************************************************************

int SysParamsWrite(SYSPARMS* sp)
{
    int32_t rc = 0;

    sp->version = MAKEREV(FIRMWARE_VER, FIRMWARE_REV);
    sp->magic   = MAGIC;

    rc = EEPROMProgram((uint32_t *)sp, 0, sizeof(SYSPARMS));

    System_printf("Writing System Parameters %d\n", rc);
    System_flush();

    return rc;
 }

//*****************************************************************************
// Read system parameters into our global settings buffer from EEPROM.
//
// Returns:  0 = Sucess
//          -1 = Error reading flash
//
//*****************************************************************************

int SysParamsRead(SYSPARMS* sp)
{
    InitSysDefaults(sp);

    EEPROMRead((uint32_t *)sp, 0, sizeof(SYSPARMS));

    if (sp->magic != MAGIC)
    {
        System_printf("ERROR Reading System Parameters - Using Defaults...\n");
        System_flush();

        InitSysDefaults(sp);

        SysParamsWrite(sp);

        return -1;
    }

    if (sp->version != MAKEREV(FIRMWARE_VER, FIRMWARE_REV))
    {
        System_printf("WARNING New Firmware Version - Using Defaults...\n");
        System_flush();

        InitSysDefaults(sp);

        SysParamsWrite(sp);

        return -1;
    }

    return 0;
}

// End-Of-File
