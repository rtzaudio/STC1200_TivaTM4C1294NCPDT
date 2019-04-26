/* ============================================================================
 *
 * DTC-1200 & STC-1200 Digital Transport Controllers for
 * Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2018, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 * ============================================================================
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
#include <ti/sysbios/knl/Queue.h>
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
#include <inc/hw_memmap.h>
#include <inc/hw_gpio.h>
#include <driverlib/eeprom.h>
#include <driverlib/sysctl.h>
#include <driverlib/gpio.h>
#include <driverlib/pin_map.h>
#include <driverlib/sysctl.h>
#include <driverlib/fpu.h>
#include <grlib/grlib.h>
#include <IPCServer.h>
#include <RAMPServer.h>
#include <RemoteTask.h>
#include <MidiTask.h>
#include "drivers/offscrmono.h"

/* STC1200 Board Header file */

#include "STC1200.h"
#include "Board.h"
#include "CLITask.h"

/* Enable div-clock output if non-zero */
#define DIV_CLOCK_ENABLED	0

/* Debounce time for transport locator buttons */
#define DEBOUNCE_TIME       30

/* Global STC-1200 System data */
SYSDATA g_sysData;
SYSPARMS g_sysParms;

/* Handles created dynamically */

Mailbox_Handle g_mailboxLocate  = NULL;
Mailbox_Handle g_mailboxRemote  = NULL;
Mailbox_Handle g_mailboxCommand = NULL;

Event_Handle g_eventQEI;

/* Static Function Prototypes */
static void gpioButtonResetHwi(unsigned int index);
static void gpioButtonSearchHwi(unsigned int index);
static void gpioButtonCueHwi(unsigned int index);

static void gpioButtonStopHwi(unsigned int index);
static void Hardware_init();

#if (DIV_CLOCK_ENABLED > 0)
static void EnableClockDivOutput(uint32_t div);
#endif

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
    Board_initSDSPI();
    Board_initUART();
    Board_initEMAC();

    /* Default hardware initialization */
    Hardware_init();

    /* Create command task mailbox */
    Error_init(&eb);
    Mailbox_Params_init(&mboxParams);
    g_mailboxCommand = Mailbox_create(sizeof(CommandMessage), 8, &mboxParams, &eb);
    if (g_mailboxCommand == NULL) {
        System_abort("Mailbox create failed\n");
    }

    /* Create locater task mailbox */
    Error_init(&eb);
    Mailbox_Params_init(&mboxParams);
    g_mailboxLocate = Mailbox_create(sizeof(LocateMessage), 8, &mboxParams, &eb);
    if (g_mailboxLocate == NULL) {
        System_abort("Mailbox create failed\n");
    }

    /* Create display task mailbox */
    Error_init(&eb);
    Mailbox_Params_init(&mboxParams);
    g_mailboxRemote = Mailbox_create(sizeof(RAMP_MSG), 16, &mboxParams, &eb);
    if (g_mailboxRemote == NULL) {
        System_abort("Mailbox create failed\n");
    }

    /* Allocate IPC server resources */
    IPC_Server_init();

    /* Allocate MIDI server resources */
    Midi_Server_init();

    /* Create task with priority 15 */

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 2048;
    taskParams.priority  = 5;
    Task_create((Task_FuncPtr)CommandTaskFxn, &taskParams, &eb);

    System_printf("Starting STC1200 execution.\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}

//*****************************************************************************
// Default hardware initialization
//*****************************************************************************

void Hardware_init()
{
    SDSPI_Handle handle;
    SDSPI_Params params;

    /* Enables Floating Point Hardware Unit */
    FPUEnable();
    /* Allows the FPU to be used inside interrupt service routines */
    //FPULazyStackingEnable();

    /* Initialize a 1 BPP off-screen OLED display buffer that we draw into */
    GrOffScreenMonoInit();

    /* Deassert the Atmega88 reset line */
    GPIO_write(Board_RESET_AVR_N, PIN_HIGH);
    /* Deassert motion status lines */
    GPIO_write(Board_TAPE_DIR, PIN_HIGH);
    GPIO_write(Board_MOTION_FWD, PIN_HIGH);
    GPIO_write(Board_MOTION_REW, PIN_HIGH);
    /* Clear SEARCHING_OUT status i/o pin */
    GPIO_write(Board_SEARCHING, PIN_HIGH);
    /* Turn on the status LED */
    GPIO_write(Board_STAT_LED, Board_LED_ON);
    /* Turn off the play and shuttle lamps */
    GPIO_write(Board_LAMP_PLAY, Board_LAMP_OFF);
    GPIO_write(Board_LAMP_FWDREW, Board_LAMP_OFF);
    /* Deassert the RS-422 DE & RE pins */
    GPIO_write(Board_RS422_DE, PIN_LOW);
    GPIO_write(Board_RS422_RE_N, PIN_HIGH);

    /* This enables the DIVSCLK output pin on PQ4
     * and generates a 1.2 Mhz clock signal on the.
     * expansion header and pin 16 of the edge
     * connector if a clock signal is required.
     */
#if (DIV_CLOCK_ENABLED > 0)
    EnableClockDivOutput(100);
#endif

    /* Initialize the SD drive for operation */
    SDSPI_Params_init(&params);
    handle = SDSPI_open(Board_SPI_SDCARD, 0, &params);
    if (handle == NULL) {
        System_abort("Failed to open SDSPI!");
    }
}

//*****************************************************************************
// This enables the DIVSCLK output pin on PQ4 and generates a clock signal
// from the main cpu clock divided by 'div' parameter. A value of 100 gives
// a clock of 1.2 Mhz.
//*****************************************************************************

#if (DIV_CLOCK_ENABLED > 0)
void EnableClockDivOutput(uint32_t div)
{
    /* Enable pin PQ4 for DIVSCLK0 DIVSCLK */
    GPIOPinConfigure(GPIO_PQ4_DIVSCLK);
    /* Configure the output pin for the clock output */
    GPIODirModeSet(GPIO_PORTQ_BASE, GPIO_PIN_4, GPIO_DIR_MODE_HW);
    GPIOPadConfigSet(GPIO_PORTQ_BASE, GPIO_PIN_4, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
    /* Enable the clock output */
    SysCtlClockOutConfig(SYSCTL_CLKOUT_EN | SYSCTL_CLKOUT_SYSCLK, div);
}
#endif

//*****************************************************************************
// This function attempts to ready the unique serial number
// from the I2C
//*****************************************************************************

int ReadSerialNumber(uint8_t ui8SerialNumber[16])
{
    bool            ret;
    uint8_t         txByte;
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
    p->version      = MAKEREV(FIRMWARE_VER, FIRMWARE_REV);
    p->build        = FIRMWARE_BUILD;
    p->debug        = 0;        /* debug mode 0=off                 */
    p->searchBlink  = TRUE;
    p->showLongTime = FALSE;
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
    sp->build   = FIRMWARE_BUILD;
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

    if (sp->build < FIRMWARE_MIN_BUILD)
    {
        System_printf("WARNING New Firmware BUILD - Resetting Defaults...\n");
        System_flush();

        InitSysDefaults(sp);

        SysParamsWrite(sp);

        return -1;
    }

    return 0;
}

//*****************************************************************************
// This function attempts to ready the unique serial number
// from the I2C
//*****************************************************************************

Void CommandTaskFxn(UArg arg0, UArg arg1)
{
    UInt32 timeout;
    uint32_t btn;
    Error_Block eb;
	Task_Params taskParams;
    CommandMessage msgCmd;

    /* Read the globally unique serial number from EPROM */
    if (!ReadSerialNumber(g_sysData.ui8SerialNumber)) {
    	System_printf("Read Serial Number Failed!\n");
    	System_flush();
    }

    /* Load system config from EPROM */
    //SysParamsRead(&g_sysParms);
    /* FIX */
    InitSysDefaults(&g_sysParms);

    /* Initialize the command line serial debug console port */
    CLI_init();

    /* Startup the IPC server threads */
    IPC_Server_startup();

    /* Initialize the remote task if CFG2 switch is ON */
    if (GPIO_read(Board_DIPSW_CFG2) == 0)
        Remote_Task_startup();

    /*
     * Create the various system tasks
     */

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 1024;
    taskParams.priority  = 10;
    Task_create((Task_FuncPtr)PositionTaskFxn, &taskParams, &eb);

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 1024;
    taskParams.priority  = 12;
    Task_create((Task_FuncPtr)LocateTaskFxn, &taskParams, &eb);

    /* Startup the MIDI services tasks */
    Midi_Server_startup();

    /* Setup the callback Hwi handler for each button */

    GPIO_setCallback(Board_BTN_RESET, gpioButtonResetHwi);
    GPIO_setCallback(Board_BTN_CUE, gpioButtonCueHwi);
    GPIO_setCallback(Board_BTN_SEARCH, gpioButtonSearchHwi);
    GPIO_setCallback(Board_STOP_DETECT_N, gpioButtonStopHwi);

    /* Enable keypad button interrupts */

    GPIO_enableInt(Board_BTN_RESET);
    GPIO_enableInt(Board_BTN_CUE);
    GPIO_enableInt(Board_BTN_SEARCH);

    GPIO_enableInt(Board_STOP_DETECT_N);

    /* Now begin the main program command task processing loop */

    while (TRUE)
    {
        /* Blink LED fast when search in progress */
        timeout =  (g_sysData.searching) ? 250 : 1000;

    	/* Wait for a message up to 1 second */
        if (!Mailbox_pend(g_mailboxCommand, &msgCmd, timeout))
        {
        	/* No message, blink the LED */
    		GPIO_toggle(Board_STAT_LED);
    		continue;
        }

        switch(msgCmd.command)
        {
		case SWITCHPRESS:

			/* Handle switch debounce */

			if (msgCmd.param == Board_BTN_RESET)
			{
				/* Zero tape timer at current tape location */
				PositionZeroReset();

				/* Debounce button delay */
				Task_sleep(DEBOUNCE_TIME);

				/* Wait for button to release, then re-enable interrupt */
				do {
			        btn = GPIO_read(Board_BTN_RESET);
			        Task_sleep(10);
			    } while (btn);

				GPIO_enableInt(Board_BTN_RESET);
			}
			else if (msgCmd.param == Board_BTN_CUE)
			{
				/* Store the current position at cue point 65 */
				CuePointSet(LAST_CUE_POINT, 0);

                /* Debounce button delay */
                Task_sleep(DEBOUNCE_TIME);

                /* Wait for button to release, then re-enable interrupt */
                do {
                    btn = GPIO_read(Board_BTN_CUE);
                    Task_sleep(10);
                } while (btn);

				GPIO_enableInt(Board_BTN_CUE);
			}
			else if (msgCmd.param == Board_BTN_SEARCH)
			{
				/* Begin locate to last cue point memory. This is the
				 * memory used by the cue/search buttons on the transport.
				 */
			    LocateSearch(LAST_CUE_POINT, 0);

			    /* Debounce button delay */
                Task_sleep(DEBOUNCE_TIME);

                /* Wait for button to release, then re-enable interrupt */
                do {
                    btn = GPIO_read(Board_BTN_SEARCH);
                    Task_sleep(10);
                } while (btn);

				GPIO_enableInt(Board_BTN_SEARCH);
			}
			break;

		default:
			break;
        }
    }
}

//*****************************************************************************
// HWI Callback function for RESET button on transport tape roller/counter.
//*****************************************************************************

void gpioButtonResetHwi(unsigned int index)
{
	uint32_t btn;
    CommandMessage msg;

    /* Read the stop button press state */
    btn = GPIO_read(Board_BTN_RESET);

    if (btn)
    {
    	GPIO_disableInt(Board_BTN_RESET);

	    msg.command = SWITCHPRESS;
	    msg.param   = Board_BTN_RESET;
		Mailbox_post(g_mailboxCommand, &msg, BIOS_NO_WAIT);
    }
}

//*****************************************************************************
// HWI Callback function for CUE button on transport tape roller/counter.
//*****************************************************************************

void gpioButtonCueHwi(unsigned int index)
{
	uint32_t btn;
    CommandMessage msg;

    /* Read the stop button press state */
    btn = GPIO_read(Board_BTN_CUE);

    if (btn)
    {
    	GPIO_disableInt(Board_BTN_CUE);

	    msg.command = SWITCHPRESS;
	    msg.param   = Board_BTN_CUE;
		Mailbox_post(g_mailboxCommand, &msg, BIOS_NO_WAIT);
    }
}

//*****************************************************************************
// HWI Callback function for SEARCH button on transport tape roller/counter.
//*****************************************************************************

void gpioButtonSearchHwi(unsigned int index)
{
	uint32_t btn;
    CommandMessage msg;

    /* Read the stop button press state */
    btn = GPIO_read(Board_BTN_SEARCH);

    if (btn)
    {
        GPIO_disableInt(Board_BTN_SEARCH);

	    msg.command = SWITCHPRESS;
	    msg.param   = Board_BTN_SEARCH;
		Mailbox_post(g_mailboxCommand, &msg, BIOS_NO_WAIT);
    }
}

//*****************************************************************************
// HWI Callback function for STOP/PLAY/FWD/REWIND buttons
//*****************************************************************************

void gpioButtonStopHwi(unsigned int index)
{
    uint32_t key = Hwi_disable();
    g_sysData.searchCancel = TRUE;
    Hwi_restore(key);
}

// End-Of-File
