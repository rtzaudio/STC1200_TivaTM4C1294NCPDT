/* ============================================================================
 *
 * DTC-1200 & STC-1200 Digital Transport Controllers for
 * Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2020, RTZ Professional Audio, LLC
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
#include <time.h>
#include <stdbool.h>

/* Tivaware Driver files */
#include <inc/hw_memmap.h>
#include <inc/hw_gpio.h>
#include <driverlib/eeprom.h>
#include <driverlib/flash.h>
#include <driverlib/sysctl.h>
#include <driverlib/gpio.h>
#include <driverlib/pin_map.h>
#include <driverlib/sysctl.h>
#include <driverlib/fpu.h>
#include <driverlib/hibernate.h>
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
#include "Utils.h"
#include "SMPTE.h"
#include "TrackCtrl.h"

/* Enable div-clock output if non-zero */
#define DIV_CLOCK_ENABLED	0

/* Debounce time for transport locator buttons */
#define DEBOUNCE_TIME       30

/* Global STC-1200 System data */
SYSDAT g_sys;

/* SPI interface to AD9837 NCO master reference oscillator on daughter card */
AD9837_DEVICE g_ad9837;

/* Handles created dynamically */
Mailbox_Handle g_mailboxLocate  = NULL;
Mailbox_Handle g_mailboxRemote  = NULL;
Mailbox_Handle g_mailboxCommand = NULL;

/* Static Function Prototypes */
static void Init_Hardware();
static void Init_Peripherals(void);
static void Init_Application(void);
static Void MainTaskFxn(UArg arg0, UArg arg1);
static void gpioButtonResetHwi(unsigned int index);
static void gpioButtonSearchHwi(unsigned int index);
static void gpioButtonCueHwi(unsigned int index);
static void gpioButtonStopHwi(unsigned int index);
#if (DIV_CLOCK_ENABLED > 0)
static void EnableClockDivOutput(uint32_t div);
#endif

//*****************************************************************************
// Main Entry Point
//*****************************************************************************

int main(void)
{
    Error_Block eb;
	Task_Params taskParams;
    Mailbox_Params mboxParams;

    /* default GUID values */
    memset(g_sys.ui8SerialNumberSTC, 0xFF, 16);
    memset(g_sys.ui8SerialNumberDTC, 0xFF, 16);
    memset(g_sys.ui8MAC, 0xFF, 6);

    g_sys.rtcFound       = false;
    g_sys.dcsFound       = false;
    g_sys.smpteFound     = false;
    g_sys.smpteMode      = 0;
    g_sys.varispeedMode  = false;
    g_sys.standbyActive  = false;
    g_sys.standbyMonitor = false;
    g_sys.trackCount     = 0;

    /* Now call all the board initialization functions for TI-RTOS */
    Board_initGeneral();
    Board_initGPIO();
    Board_initI2C();
    Board_initSPI();
    Board_initSDSPI();
    Board_initUART();

    /* Default hardware initialization */
    Init_Hardware();

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
    MIDI_Server_init();

    /* Create task with priority 15 */

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 2048;
    taskParams.priority  = 5;
    Task_create((Task_FuncPtr)MainTaskFxn, &taskParams, &eb);

    System_printf("Starting STC1200 execution.\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}

//*****************************************************************************
// This is a hook into the NDK stack to allow delaying execution of the NDK
// stack task until after we load the MAC address from the AT24MAC serial
// EPROM part. This hook blocks on a semaphore until after we're able to call
// Board_initEMAC() in the CommandTaskFxn() below. This mechanism allows us
// to delay execution until we load the MAC from EPROM.
//*****************************************************************************

void NDKStackBeginHook(void)
{
    Semaphore_pend(g_semaNDKStartup, BIOS_WAIT_FOREVER);
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
// Default hardware initialization
//*****************************************************************************

void Init_Hardware()
{
    /* Enables Floating Point Hardware Unit */
    FPUEnable();

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
}

//*****************************************************************************
// Default Peripheral initialization
//*****************************************************************************

void Init_Peripherals(void)
{
    SDSPI_Params sdParams;
    //UART_Params uartParams;
    I2C_Params  i2cParams;

    /*
     * Open I2C-0 bus, read MAC, S/N and probe if RTC available
     */

    I2C_Params_init(&i2cParams);

    i2cParams.transferCallbackFxn = NULL;
    i2cParams.transferMode        = I2C_MODE_BLOCKING;
    i2cParams.bitRate             = I2C_100kHz;

    if ((g_sys.handleI2C0 = I2C_open(STC1200_I2C0, &i2cParams)) == NULL)
    {
        System_printf("Error: Unable to openI2C0 port\n");
        System_flush();
    }

    /* Read the globally unique serial number from the AT24MAC EPROM.
     * We also read a 6-byte MAC address from this EPROM part.
     */
    if (!ReadGUIDS(g_sys.handleI2C0, g_sys.ui8SerialNumberSTC, g_sys.ui8MAC))
    {
        System_printf("Read Serial Number Failed!\n");
        System_flush();
    }

    /* Create and initialize the MCP79410 RTC object attached to I2C0 also */
    if ((g_sys.handleRTC = MCP79410_create(g_sys.handleI2C0, NULL)) == NULL)
    {
        System_abort("MCP79410_create failed\n");
    }

    /* Determine if MCP79410 RTC chip is available or not */
    g_sys.rtcFound = MCP79410_Probe(g_sys.handleRTC);

    /* Only Rev-C or greater has MCP79410 RTC chip installed. Otherwise
     * configure CPU hibernate clock for RTC use.
     */
    if (!g_sys.rtcFound)
    {
        /* Configure hibernate module clock */
        HibernateEnableExpClk(120000000);
        /* Enable RTC mode */
        HibernateRTCEnable();
        /* Set hibernate module counter to 24-hour calendar mode */
        HibernateCounterMode(HIBERNATE_COUNTER_24HR);
    }

    /* read the initial time and date */
    RTC_GetDateTime(&g_sys.timeDate);

    /*
     * Initialize the SD drive for operation
     */

    SDSPI_Params_init(&sdParams);

    if ((g_sys.handleSD = SDSPI_open(Board_SPI_SDCARD, 0, &sdParams)) == NULL)
    {
        System_abort("Failed to open SDSPI!");
    }
}

//*****************************************************************************
// Default Device initialization
//*****************************************************************************

void Init_Application(void)
{
    int rc;
    size_t i;
    Task_Params taskParams;
    Error_Block eb;

    /* Initialize the EMAC with address loaded in Init_Peripherals() prior */
    Board_initEMAC();

    /* Now allow the NDK task, blocked by NDKStackBeginHook(), to run */
    Semaphore_post(g_semaNDKStartup);

    /* Set default system parameters */
    InitSysDefaults(&g_sys.cfgSTC);

    /* Load system configuration from EPROM */
    SysParamsRead(&g_sys.cfgSTC);

    /* Set default reference frequency */
    g_sys.ref_freq = g_sys.cfgSTC.ref_freq;

    /* Reset the capstan reference clock and set the default
     * output frequency to 9600Hz to the capstan board. The
     * AD9837 hardware is located on the SMPTE daughter card.
     */
    AD9837_init();
    AD9837_reset();

    /* Initialize SMPTE daughter card if installed */
    SMPTE_init();

    if (SMPTE_probe())
    {
        g_sys.smpteFound = true;

        SMPTE_encoder_stop();
        //SMPTE_decoder_stop();
    }

    /* Get number of tracks DCS is configured for. Note DIP
     * switch #1 must be on to enable using the track controller.
     */
    if (GPIO_read(Board_DIPSW_CFG1) == 0)
    {
        /* Startup the track manager task */
        TRACK_Manager_startup();

        /* Assume we have a working DCS for now */
        g_sys.dcsFound = true;

        /* Attempt to read track configuration from DCS */
        if (!Track_GetCount(&g_sys.trackCount))
        {
            g_sys.dcsFound = false;
        }
        else
        {
            /* Set transport tape speed */
            Track_SetTapeSpeed(g_sys.cfgSTC.tapeSpeed);

            /* Set all track states from EEPROM config */
            memcpy(g_sys.trackState, g_sys.cfgSTC.trackState, DCS_NUM_TRACKS);

            /* Make sure record active bit is clear! */
            for (i=0; i < DCS_NUM_TRACKS; i++)
                g_sys.trackState[i] &= ~(DCS_T_RECORD);

            Track_ApplyAllStates(g_sys.trackState);
        }
    }

    /* Startup the debug console task */
    CLI_init();

    /* Startup the IPC server tasks */
    IPC_Server_startup();

    /* Startup the wired remote task */
    Remote_Task_startup();

    /* Open the IPC channel on UART-B to the DTC */
    g_sys.ipcToDTC = IPCToDTC_Open();

    if (g_sys.ipcToDTC != NULL)
    {
        /* Read the DTC firmware version and serial number */
        rc = IPCToDTC_VersionGet(g_sys.ipcToDTC,
                                 &g_sys.dtcVersion,
                                 &g_sys.dtcBuild,
                                 &g_sys.ui8SerialNumberDTC[0]);

        if (rc == IPC_ERR_SUCCESS)
        {
            /* Read the DTC configuration data structure */
            rc = IPCToDTC_ConfigGet(g_sys.ipcToDTC, &g_sys.cfgDTC);

            if (rc != IPC_ERR_SUCCESS)
            {
                System_printf("Error Reading DTC Config!\n");
                System_flush();
            }
        }
        else
        {
            memset(&g_sys.cfgDTC, 0, sizeof(DTC_CONFIG_DATA));
        }
    }

    /*
     * Create the various system tasks
     */

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 1024;
    taskParams.priority  = 12;
    Task_create((Task_FuncPtr)PositionTaskFxn, &taskParams, &eb);

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.stackSize = 1024;
    taskParams.priority  = 12;
    Task_create((Task_FuncPtr)LocateTaskFxn, &taskParams, &eb);

    /* Startup the MIDI services tasks */
    MIDI_Server_startup();

    /* Startup the CLI on COM1 */
    CLI_startup();

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
}

//*****************************************************************************
// Default Device initialization
//*****************************************************************************

int ConfigSave(int level)
{
    /* Copy the current runtime track state table to the configuration
     * track state table and save to EEPROM. Next time the application
     * loads it will load whatever track states are stored in EPROM.
     */

    if (level > 0)
    {
        /* Set current track states the defaults */
        memcpy(g_sys.cfgSTC.trackState, g_sys.trackState, DCS_NUM_TRACKS);

        /* Set current tape speed as the default */
        g_sys.cfgSTC.tapeSpeed = (uint8_t)g_sys.tapeSpeed;
    }

    /* Write the parameters to EEPROM */
    return SysParamsWrite(&g_sys.cfgSTC);
}

/* Load system configuration from EPROM */

int ConfigLoad(int level)
{
    int rc;

    if ((rc = SysParamsRead(&g_sys.cfgSTC)) == 0)
    {
        if (level > 0)
        {
            /* Set current track states the defaults */
            memcpy(g_sys.trackState, g_sys.cfgSTC.trackState, DCS_NUM_TRACKS);

            /* Set current tape speed as the default */
            g_sys.tapeSpeed = g_sys.cfgSTC.tapeSpeed;
        }
    }

    return rc;
}

/* Reset current configuration to defaults */

int ConfigReset(int level)
{
    InitSysDefaults(&g_sys.cfgSTC);

    if (level > 0)
    {
        /* Set current track states the defaults */
        memcpy(g_sys.trackState, g_sys.cfgSTC.trackState, DCS_NUM_TRACKS);

        /* Set current tape speed as the default */
        g_sys.tapeSpeed = g_sys.cfgSTC.tapeSpeed;
    }

    return 0;
}

//*****************************************************************************
// This function attempts to ready the unique serial number
// from the I2C
//*****************************************************************************

Void MainTaskFxn(UArg arg0, UArg arg1)
{
    UInt32 timeout;
    uint32_t btn;
    CommandMessage msgCmd;
    RTCC_Struct timeDate;

    /* Allocate and initialize global peripherals used */
    Init_Peripherals();

    /* Initialize and startup the main application tasks */
    Init_Application();

    /* Now begin the main program command task processing loop */

    while (TRUE)
    {
        /* Blink LED fast when search in progress */
        timeout =  (g_sys.searching) ? 100 : 1000;

    	/* Wait for a message up to 1 second */
        if (!Mailbox_pend(g_mailboxCommand, &msgCmd, timeout))
        {
            /* read the time and date */
            RTC_GetDateTime(&timeDate);

            g_sys.timeDate = timeDate;

        	/* No message, blink the LED */
    		GPIO_toggle(Board_STAT_LED);
    		continue;
        }

        /*
         * Handle Transport Deck Button Press Events
         */

        /* skip if it wasn't a button command */
        if (msgCmd.command != SWITCHPRESS)
            continue;

        switch(msgCmd.param)
        {
		case Board_BTN_RESET:

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
			break;

		case Board_BTN_CUE:

			/* Store the current search home cue position */
			CuePointSet(CUE_POINT_HOME, g_sys.tapePosition, CF_ACTIVE);

            /* Debounce button delay */
            Task_sleep(DEBOUNCE_TIME);

            /* Wait for button to release, then re-enable interrupt */
            do {
                btn = GPIO_read(Board_BTN_CUE);
                Task_sleep(10);
            } while (btn);

            GPIO_enableInt(Board_BTN_CUE);
			break;

		case Board_BTN_SEARCH:

			/* Begin locate to last cue point memory. This is the
			 * memory used by the cue/search buttons on the transport.
			 */
			LocateSearch(CUE_POINT_HOME, 0);

			/* Debounce button delay */
            Task_sleep(DEBOUNCE_TIME);

            /* Wait for button to release, then re-enable interrupt */
            do {
                btn = GPIO_read(Board_BTN_SEARCH);
                Task_sleep(10);
            } while (btn);

			GPIO_enableInt(Board_BTN_SEARCH);
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
    g_sys.searchCancel = TRUE;
    Hwi_restore(key);
}

// End-Of-File
