/* ============================================================================
 *
 * STC-1200 Search/Timer/Comm Controller for Ampex MM-1200 Tape Machines
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
#include <math.h>
#include <limits.h>

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

/* PMX42 Board Header file */

#include "STC1200.h"
#include "Board.h"
#include "CLITask.h"

/* External Data Items */

extern SYSDATA g_sysData;
extern SYSPARMS g_sysParms;
extern Event_Handle g_eventQEI;

/* Static Data Items */
static Hwi_Struct qeiHwiStruct;

/* Static Function Prototypes */

void QEI_initialize(void);
void Write7SegDisplay(UART_Handle handle, TAPETIME* p);

static Void QEIHwi(UArg arg);

/*****************************************************************************
 * Reset the QEI position to ZERO.
 *****************************************************************************/

void PositionZeroReset(void)
{
	CLI_printf("Zero Reset\n");
	QEIPositionSet(QEI_BASE_ROLLER, 0);
}

/*****************************************************************************
 * Convert absolute encoder position to tape time units.
 *****************************************************************************/

#define INV_ROLLER_TICKS_PER_REV    (1.0f / ROLLER_TICKS_PER_REV_F);

void PositionToTapeTime(int tapePosition, TAPETIME* tapeTime)
{
    /* Get the current encoder position */
    float position = fabsf((float)tapePosition);

    /* Calculate the number of revolutions from the position */
    //float revolutions = position / ROLLER_TICKS_PER_REV_F;
    float revolutions = position * INV_ROLLER_TICKS_PER_REV;

    /* Calculate the distance in inches based on the number of revolutions */
    float distance = revolutions * ROLLER_CIRCUMFERENCE_F;

    /* Get the current speed setting */
    float invspeed = GPIO_read(Board_SPEED_SELECT) ? (1.0f/30.0f) : (1.0f/15.0f);

    /* Finally, calculate the time in seconds from the distance
     * and speed while avoiding any divisions.
     */
    float seconds = distance * invspeed;

    /* Convert the total seconds value into binary H:MM:SS:T values */
    SecondsToTapeTime(seconds, tapeTime);
}

/*****************************************************************************
 * Convert tape time to absolute encoder position units.
 *****************************************************************************/

void TapeTimeToPosition(TAPETIME* tapeTime, int* tapePosition)
{
    float time;

    /* Convert tape to total seconds */
    TapeTimeToSeconds(tapeTime, &time);

    /* Get the current speed setting */
    float speed = GPIO_read(Board_SPEED_SELECT) ? 30.0f : 15.0f;

    /* Calculate the distance in inches */
    float distance = speed * time;

    float position = (distance / ROLLER_CIRCUMFERENCE_F) * ROLLER_TICKS_PER_REV_F;

    *tapePosition = (int)position;
}

/****************************************************************************
 * This function takes a 32-bit time value in total seconds and
 * calculates the hours, minutes and seconds members.
 ***************************************************************************/

#define SECS_DAY    (24L * 60L * 60L)

void SecondsToTapeTime(float time, TAPETIME* p)
{
    uint32_t dayclock = (uint32_t)time % SECS_DAY;

    float intpart;
    float fractpart = modff(time, &intpart);

    p->hour  = (uint8_t)(dayclock / 3600);
    p->mins  = (uint8_t)((dayclock % 3600) / 60);
    p->secs  = (uint8_t)(dayclock % 60);
    p->tens  = (uint8_t)(fractpart * 10.0f);
    p->frame = (uint8_t)(fractpart * 30.0f);
    p->flags = (time < 0.0f) ? 0 : F_PLUS;
}

void TapeTimeToSeconds(TAPETIME* p, float* time)
{
    float secs;

    secs  = (float)(p->hour * 3600);
    secs += (float)(p->mins % 3600) * 60.0f;
    secs += (float)(p->secs % 60);
    secs += (float)(p->tens % 10) * 0.10f;
  //secs += (float)(p->frame % 30) * (1.0f/30.0f);

    *time = secs + 0.1f;
}

//*****************************************************************************
// Position reader/display task. This function reads the tape roller
// quadrature encoder and stores the current position data. The 7-segement
// tape position display is also updated via the task so the current tape
// position is always being shown on the machine's display on the transport.
//*****************************************************************************

Void PositionTaskFxn(UArg arg0, UArg arg1)
{
	uint8_t flags;
	uint32_t rcount = 0;
	UART_Params uartParams;
	UART_Handle uartHandle;
    Error_Block eb;

	/* Create interrupt signal event */
    Error_init(&eb);
    g_eventQEI = Event_create(NULL, &eb);

	/* Initialize the quadrature encoder module */
	QEI_initialize();

	/* Initialize the UART to the ATmegaa88 */
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
	uartParams.dataLength	  = UART_LEN_8;
	uartParams.baudRate       = 250000;	//38400;
	uartParams.stopBits       = UART_STOP_ONE;
	uartParams.parityType     = UART_PAR_NONE;

	uartHandle = UART_open(Board_UART_ATMEGA88, &uartParams);

	/* This is the main tape position/counter task. Here we read the tape
	 * roller quadrature encoder to keep track of the absolute tape position.
	 * This position is relative to the last counter reset, either at power
	 * up or from the user pressing the tape counter reset button.
	 */

	g_sysData.tapePositionPrev = 0xFFFFFFFF;

    while (TRUE)
    {
    	/* Wait for any ISR events to be posted */
    	UInt events = Event_pend(g_eventQEI, Event_Id_NONE, Event_Id_00 | Event_Id_01, 10);

    	/* not used */
    	if (events & Event_Id_00)
        {

        }

    	/* Quadrature encoder error interrupt */
    	if (events & Event_Id_01)
    	{
    		System_printf("QEI Phase Error: %d\n", g_sysData.qei_error_cnt);
    		System_flush();
    	}

    	/* Read the tape roller tachometer value */
    	g_sysData.tapeTach = QEIVelocityGet(QEI_BASE_ROLLER);

    	/* Get the tape direction status from the QEI controller */
    	g_sysData.tapeDirection = QEIDirectionGet(QEI_BASE_ROLLER);

    	/* Set the tape direction output indicator pin either high or low
		 * 1 = tape forward direction, 0 = tape rewind direction
    	 */
    	if (g_sysData.tapeDirection > 0)
    		GPIO_write(Board_TAPE_DIR, PIN_HIGH);
    	else
    		GPIO_write(Board_TAPE_DIR, PIN_LOW);

    	/* Read the absolute position from the QEI controller */
    	g_sysData.tapePositionAbs = QEIPositionGet(QEI_BASE_ROLLER);

    	/* Convert absolute tape position to signed relative position */
    	g_sysData.tapePosition = POSITION_TO_INT(g_sysData.tapePositionAbs);

    	/* Here we're determining if any motion is present by looking at the previous
    	 * position and comparing to the current position. If the position has changed,
    	 * then we assume tape is moving and set the motion indicator outputs accordingly.
    	 */
    	if (g_sysData.tapePosition == g_sysData.tapePositionPrev)
    	{
			GPIO_write(Board_MOTION_FWD, PIN_HIGH);
	    	GPIO_write(Board_MOTION_REW, PIN_HIGH);
    	}
    	else
    	{
			/* Position Changed - Update the previous position */
			g_sysData.tapePositionPrev = g_sysData.tapePosition;

			/* Update the direction outputs to the old Ampex controller */
			if (g_sysData.tapeDirection > 0)
			{
				// FWD pin low - active direction
				GPIO_write(Board_MOTION_FWD, PIN_LOW);
				GPIO_write(Board_MOTION_REW, PIN_HIGH);
			}
			else
			{
				// REW pin low - active direction
				GPIO_write(Board_MOTION_REW, PIN_LOW);
				GPIO_write(Board_MOTION_FWD, PIN_HIGH);
			}

	    	//System_printf("%d\n", g_sysData.tapePosition);
	    	//System_flush();
    	}

    	/* The MM1200 timer-counter roller wheel measures closely to 1.592”
    	 * in diameter. This gives a circumference of 5.0014” (c = PI * d).
    	 * Thus, one revolution of the tachometer wheel equals 5” of
    	 * tape travel.
    	 *
    	 * The formula for time traveled is distance/speed:
    	 *
    	 *     time     = distance / speed
    	 *     speed    = distance / time
     	 *     distance = speed * time    	 
    	 *
    	 * The tape moves on the transport at either 15 or 30 inches per
    	 * second (IPS) depending on the speed switch setting.
    	 *
    	 * The time taken to cover 30 inches of tape travel is one second.
    	 *
    	 * The time taken to cover 1 inch of tape travel is 1/30 of a second.
    	 *
    	 *     time = (distance inches / 30 inches) * second
	     */

    	if (++rcount >= 10)
    	{
    		flags = rcount = 0;

			/* Set display flags to indicate proper direction sign */
			if (g_sysData.tapePosition < 0)
				flags &= ~(F_PLUS);
			else
				flags |= F_PLUS;

			/* Blink 7-seg display during searches */
			if (g_sysParms.searchBlink)
			{
                if (g_sysData.searching)
                    flags |= F_BLINK;
                else
                    flags &= ~(F_BLINK);
			}

#if 0
			/* Get the current encoder position */
			float position = fabsf((float)g_sysData.tapePosition);

			/* Calculate the number of revolutions from the position */
			//float revolutions = position / ROLLER_TICKS_PER_REV_F;
            float revolutions = position * invRollerTicks;

			/* Calculate the distance in inches based on the number of revolutions */
			float distance = revolutions * ROLLER_CIRCUMFERENCE_F;

			/* Get the current speed setting */
			float invspeed = GPIO_read(Board_SPEED_SELECT) ? (1.0f/30.0f) : (1.0f/15.0f);

			/* Calculate the time in seconds from the distance and speed
			 * while avoiding any divisions.
			 */
			uint32_t seconds = (uint32_t)(distance * invspeed);
#endif

			/* Get the tape time member values */
			PositionToTapeTime(g_sysData.tapePosition, &g_sysData.tapeTime);

			/* Convert the total seconds value into binary HH:MM:SS values */
			//btime(seconds, flags, &g_sysData.tapeTime);
			g_sysData.tapeTime.flags = flags;

			/* Refresh the 7-segment display with the new values */
			Write7SegDisplay(uartHandle, &g_sysData.tapeTime);
    	}
    }
}

/*****************************************************************************
 * QEI Interrupt Handler
 *****************************************************************************/

Void QEIHwi(UArg arg)
{
    uint32_t ulIntStat;

    /* Get and clear the current interrupt source(s) */
    ulIntStat = QEIIntStatus(QEI_BASE_ROLLER, true);
    QEIIntClear(QEI_BASE_ROLLER, ulIntStat);

    /* Determine which interrupt occurred */

    if (ulIntStat & QEI_INTERROR)       	/* phase error detected */
    {
    	g_sysData.qei_error_cnt++;
    	Event_post(g_eventQEI, Event_Id_01);
    }
    else if (ulIntStat & QEI_INTTIMER)  	/* velocity timer expired */
    {
    	Event_post(g_eventQEI, Event_Id_02);
    }
    else if (ulIntStat & QEI_INTDIR)    	/* direction change */
    {
    	Event_post(g_eventQEI, Event_Id_03);
    }
    else if (ulIntStat & QEI_INTINDEX)  	/* Index pulse detected */
    {
    	Event_post(g_eventQEI, Event_Id_04);
    }

    QEIIntEnable(QEI_BASE_ROLLER, ulIntStat);
}

//*****************************************************************************
//
//*****************************************************************************

void QEI_initialize(void)
{
	Error_Block eb;
	Hwi_Params  hwiParams;

	SysCtlPeripheralEnable(SYSCTL_PERIPH_QEI0);

	/* Enable pin PL3 for QEI0 IDX0 */
	GPIOPinConfigure(GPIO_PL3_IDX0);
	GPIOPinTypeQEI(GPIO_PORTL_BASE, GPIO_PIN_3);

	/* Enable pin PL1 for QEI0 PHA0 */
	GPIOPinConfigure(GPIO_PL1_PHA0);
	GPIOPinTypeQEI(GPIO_PORTL_BASE, GPIO_PIN_1);

	/* Enable pin PL2 for QEI0 PHB0 */
	GPIOPinConfigure(GPIO_PL2_PHB0);
	GPIOPinTypeQEI(GPIO_PORTL_BASE, GPIO_PIN_2);

	/* Configure the quadrature encoder for absolute position mode to
	 * capture edges on both signals and maintain an absolute position.
	 * The stock 1200 encoder give 40 CPR at four edges per line,
	 * there are 160 pulses per revolution.
	 */
	QEIConfigure(QEI_BASE_ROLLER,
			(QEI_CONFIG_CAPTURE_A | QEI_CONFIG_NO_RESET |
			 QEI_CONFIG_QUADRATURE | QEI_CONFIG_SWAP),
			 MAX_ROLLER_POSITION);

	/* Set initial position to zero */
	QEIPositionSet(QEI_BASE_ROLLER, 0);

	/* Configure the Velocity capture period - 1200000 is 10ms at 120MHz.
	 * This is how many 4 pulse trains we receive in half a second.
	 */
    QEIVelocityConfigure(QEI_BASE_ROLLER, QEI_VELDIV_1, 3000000);   //25ms period

	/* Enable both quadrature encoder interfaces */
	QEIEnable(QEI_BASE_ROLLER);

	/* Enable velocity mode */
	QEIVelocityEnable(QEI_BASE_ROLLER);

	/* Construct hwi object for quadrature encoder interface */
	Error_init(&eb);
    Hwi_Params_init(&hwiParams);
    Hwi_construct(&(qeiHwiStruct), INT_QEI0, QEIHwi, &hwiParams, &eb);
    if (Error_check(&eb))
        System_abort("Couldn't construct QEI hwi");

    /* Enable the QEI interrupts */
	QEIIntEnable(QEI_BASE_ROLLER, QEI_INTERROR|QEI_INTDIR|QEI_INTTIMER|QEI_INTINDEX);
}

//*****************************************************************************
//
//*****************************************************************************

void Write7SegDisplay(UART_Handle handle, TAPETIME* p)
{
	uint8_t	 b;
	uint16_t csum = 0;

    /* Write a display packet of data to the UART. The display
     * packet is composed in the following form.
	 *
     *    Byte   Description
	 *    ----   -------------------------------------
     *    [0]    Preamble 1, must be 0x89
	 *    [1]    Preamble 2, must be 0xFC
     *    [2]    The SECONDS (0-59)
	 *    [3]    The MINUTES (0-59)
     *    [4]    The HOUR digit (0 or 1)
     *    [5]    Bit flags (+sign, blink, blank)
     *    [6]    8-Bit Checksum
	 */

	/* Write the 0x89 and 0xFC preamble bytes out */

	b = 0x89;
	UART_write(handle, &b, 1);

	b = 0xFC;
	UART_write(handle, &b, 1);

	/* Write the secs, mins, hour & sign values out */

	b = p->secs;
	csum += b;
	UART_write(handle, &b, 1);

	b = p->mins;
	csum += b;
	UART_write(handle, &b, 1);

	b = p->hour;
	csum += b;
	UART_write(handle, &b, 1);

	b = p->flags;
	csum += b;
	UART_write(handle, &b, 1);

	/* Write the 8 checksum values out */
	b = csum & 0xFF;
	UART_write(handle, &b, 1);
}

/* End-Of-File */
