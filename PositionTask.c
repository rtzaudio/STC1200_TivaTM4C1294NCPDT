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
#include "Board.h"
#include "STC1200.h"
#include "PositionTask.h"

/* External Data Items */

extern SYSDATA g_sysData;
extern Event_Handle g_eventQEI;

/* Static Data Items */
static Hwi_Struct qeiHwiStruct;

/* Static Function Prototypes */

void InitQEI(void);
void btime(const uint32_t time, uint8_t flags, TAPETIME* p);
void Write7SegDisplay(UART_Handle handle, TAPETIME* p);

static Void QEIHwi(UArg arg);

/*****************************************************************************
 * Reset the QEI position to ZERO.
 *****************************************************************************/

void PositionReset(void)
{
	QEIPositionSet(QEI_BASE_ROLLER, 0);
}

//*****************************************************************************
// Position reader/display task. This function reads the tape roller
// quadrature encoder and stores the current position data. The 7-segement
// tape position display is also updated via the task so the current tape
// position is always being shown on the machine's display on the transport.
//*****************************************************************************

Void PositionTaskFxn(UArg arg0, UArg arg1)
{
	int ipos;
	uint8_t flags = 0;
	UART_Params uartParams;
	UART_Handle uartHandle;

	/* Initialize the quadrature encoder module */
	InitQEI();

	/* Motion indicator output pins deasserted */
	GPIO_write(Board_MOTION_FWD, PIN_HIGH);
	GPIO_write(Board_MOTION_REW, PIN_HIGH);

	/* Tape direction output pin */
	GPIO_write(Board_TAPE_DIR, PIN_HIGH);

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

    while (true)
    {
    	/* Wait for any ISR events to be posted */
    	UInt events = Event_pend(g_eventQEI, Event_Id_NONE, Event_Id_00 | Event_Id_01, 25);

    	if (events & Event_Id_00)
    	{
    		System_printf("QEI Phase Error: %d\n", g_sysData.qei_error_cnt);
    		System_flush();
    	}

    	/* Read the tape direction status from the QEI controller */
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

    	/* Check for max position wrap around if negative position */
    	if (g_sysData.tapePositionAbs >= ((MAX_ROLLER_POSITION / 2) + 1))
    	{
    		flags &= ~(F_PLUS);
    		ipos = (int)g_sysData.tapePositionAbs - (int)MAX_ROLLER_POSITION;
        	g_sysData.tapePosition = ipos;
    	}
    	else
    	{
    		flags |= F_PLUS;
    		ipos = (int)g_sysData.tapePositionAbs;
        	g_sysData.tapePosition = ipos;
    	}

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

    	/* The MM1200 timer-counter roller wheel measures closely to 1.592�
    	 * in diameter. This gives a circumference of 5.0014� (c = PI * d).
    	 * Thus, one revolution of the tachometer wheel equals 5� of
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

    	/* Get the current encoder position */
    	float position = fabsf((float)ipos);

    	/* Calculate the number of revolutions from the position */
    	float revolutions = position / (float)ROLLER_TICKS_PER_REV;

    	/* Calculate the distance based on the number of revolutions */
    	float distance = revolutions * 5.0014f;

    	/* Get the current speed setting */
    	float speed = GPIO_read(Board_SPEED_SELECT) ? 30.0f : 15.0f;
    	    
    	/* Calculate the time in seconds from the distance and speed */
    	uint32_t seconds = (uint32_t)(distance / speed);

    	/* Convert the total seconds value into binary HH:MM:SS values */
    	btime(seconds, flags, &g_sysData.tapeTime);

    	/* Refresh the 7-segment display with the new values */
    	Write7SegDisplay(uartHandle, &g_sysData.tapeTime);
    }
}

/*****************************************************************************
 * QEI Interrupt Handler
 *****************************************************************************/

Void QEIHwi(UArg arg)
{
	UInt key;
    unsigned long ulIntStat;

    /* Get and clear the current interrupt source(s) */
    ulIntStat = QEIIntStatus(QEI_BASE_ROLLER, true);
    QEIIntClear(QEI_BASE_ROLLER, ulIntStat);

    /* Determine which interrupt occurred */

    if (ulIntStat & QEI_INTERROR)       	/* phase error detected */
    {
    	key = Hwi_disable();
    	g_sysData.qei_error_cnt++;
    	Hwi_restore(key);

    	Event_post(g_eventQEI, Event_Id_00);
    }
    else if (ulIntStat & QEI_INTTIMER)  	/* velocity timer expired */
    {
    	Event_post(g_eventQEI, Event_Id_01);
    }
    else if (ulIntStat & QEI_INTDIR)    	/* direction change */
    {
    	Event_post(g_eventQEI, Event_Id_02);
    }
    else if (ulIntStat & QEI_INTINDEX)  	/* Index pulse detected */
    {
    	Event_post(g_eventQEI, Event_Id_03);
    }

    QEIIntEnable(QEI_BASE_ROLLER, ulIntStat);
}

//*****************************************************************************
//
//*****************************************************************************

void InitQEI(void)
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

	/* Configure the quadrature encoder to capture edges on both signals and
	 * maintain an absolute position by resetting on index pulses. Using a
	 * 360 CPR encoder at four edges per line, there are 1440 pulses per
	 * revolution; therefore set the maximum position to 1439 since the
	 * count is zero based.
	 */

	QEIConfigure(QEI_BASE_ROLLER,
			(QEI_CONFIG_CAPTURE_A | QEI_CONFIG_NO_RESET |
			 QEI_CONFIG_QUADRATURE | QEI_CONFIG_SWAP),
			 MAX_ROLLER_POSITION);

	QEIPositionSet(QEI_BASE_ROLLER, 0);

	/* Enable both quadrature encoder interfaces */
	QEIEnable(QEI_BASE_ROLLER);

	/* Construct hwi object for quadrature encoder interface */
	Error_init(&eb);
    Hwi_Params_init(&hwiParams);
    Hwi_construct(&(qeiHwiStruct), INT_QEI0, QEIHwi, &hwiParams, &eb);
    if (Error_check(&eb)) {
        System_abort("Couldn't construct QEI hwi");
    }

    /* Enable the QEI interrupts */
	QEIIntEnable(QEI_BASE_ROLLER, QEI_INTERROR|QEI_INTDIR|QEI_INTTIMER|QEI_INTINDEX);
}

/****************************************************************************
 * This function takes a 32-bit time value in total seconds and
 * calculates the hours, minutes and seconds members.
 ***************************************************************************/

#define SECS_DAY    (24L * 60L * 60L)

void btime(const uint32_t time, uint8_t flags, TAPETIME* p)
{
    uint32_t dayclock = time % SECS_DAY;

    p->secs  = (uint8_t)(dayclock % 60);
    p->mins  = (uint8_t)((dayclock % 3600) / 60);
    p->hour  = (uint8_t)(dayclock / 3600);
    p->flags = flags;
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
