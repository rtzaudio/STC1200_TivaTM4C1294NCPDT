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
#include "TapeTach.h"
#include "STC1200.h"

#define BUTTON_PULSE_TIME	50

/* External Data Items */

extern SYSDATA g_sysData;

extern Mailbox_Handle g_mailboxLocate;

/* Static Function Prototypes */

static void GPIOPulseLow(uint32_t index, uint32_t duration);

/*****************************************************************************
 * This functions stores the current tape position to a cue point in the
 * cue point locate table. The parameter index must be the range of
 * zero to MAX_CUE_POINTS-1. Cue point 0-63 are for the remote control
 * memories. Cue point 64 is for the single memory cue point buttons
 * on the machine.
 *****************************************************************************/

void CuePointStore(size_t index)
{
    Semaphore_pend(g_semaCue, BIOS_WAIT_FOREVER);

	if (index <= MAX_CUE_POINTS)
	{
		g_sysData.cuePoint[index].ipos  = g_sysData.tapePosition;
		g_sysData.cuePoint[index].flags = 0x01;
	}

	Semaphore_post(g_semaCue);
}

/*****************************************************************************
 * This functions stores the current tape position to a cue point in the
 *****************************************************************************/

void CuePointClear(size_t index)
{
	Semaphore_pend(g_semaCue, BIOS_WAIT_FOREVER);

	if (index <= MAX_CUE_POINTS)
	{
		g_sysData.cuePoint[index].ipos  = 0;
		g_sysData.cuePoint[index].flags = 0x00;
	}

	Semaphore_post(g_semaCue);
}

//*****************************************************************************
// Pulse and I/O line LOW for the specified ms duration. The following
// gpio lines are used to control the transport directly. Index should
// be one of the following for the transport control buttons:
//
//   Board_STOP_N
//   Board_PLAY_N
//   Board_FWD_N
//   Board_REW_N
//
//*****************************************************************************

void GPIOPulseLow(uint32_t index, uint32_t duration)
{
	/* Set the i/o pin to low state */
	GPIO_write(index, PIN_LOW);

	/* Sleep for pulse duration */
	Task_sleep(duration);

	/* Return i/o pin to high state */
	GPIO_write(index, PIN_HIGH);
}

//*****************************************************************************
// Position reader/display task. This function reads the tape roller
// quadrature encoder and stores the current position data. The 7-segement
// tape position display is also updated via the task so the current tape
// position is always being shown on the machine's display on the transport.
//*****************************************************************************

Void LocateTaskFxn(UArg arg0, UArg arg1)
{
	int dir;
	int delta;
    float distance;
	uint32_t key;
	size_t index;
    LocateMessage msg;

    /* Initialize single transport cue point to zero */
    CuePointStore(MAX_CUE_POINTS);

    while (true)
    {
    	/* Clear SEARCHING_OUT status i/o pin */
    	GPIO_write(Board_SEARCHING, PIN_HIGH);

    	/* Wait for a message up to 1 second */
        if (!Mailbox_pend(g_mailboxLocate, &msg, 250))
        {
    		System_printf("%f\n", g_sysData.tapeTach);
    		System_flush();
        	continue;
        }

        if (msg.command != LOCATE_SEARCH)
        	continue;

        /* Get the cue point memory index */
        index = (size_t)msg.param1;

        if (index > MAX_CUE_POINTS)
        	continue;

        /* Make sure the cue point is active */
        if (g_sysData.cuePoint[index].flags == 0)
        	continue;
#if 0
		/*
		 * BEGIN AUTO-LOCATE SEARCH FUNCTION
		 */

        System_printf("LOCATE[%d] %d to %d\n", index,
        		g_sysData.tapePosition,
				g_sysData.cuePoint[index].ipos);

		/* Calculate the current position delta from cue point */
		delta = g_sysData.cuePoint[index].ipos - g_sysData.tapePosition;

		/* Determine which direction we need to go initially */

		if (delta > 0)
		{
			dir = DIR_FWD;
			System_printf("SEARCH FWD %d\n", delta);
			System_flush();
		}
		else if (delta < 0)
		{
			dir = DIR_REW;
			System_printf("SEARCH REW %d\n", delta);
			System_flush();
		}
		else
		{
			dir = DIR_ZERO;
			System_printf("AT RTZ!\n");
			System_flush();
			continue;
		}

		/* Set SEARCHING_OUT status i/o pin */
		GPIO_write(Board_SEARCHING, PIN_LOW);

		/* Clear the global search cancel flag */
	    key = Hwi_disable();
	    g_sysData.searchCancel = false;
	    Hwi_restore(key);

	    /* Start the transport in either FWD or REV direction
	     * based on the cue point and current location.
	     */

	    if (dir == DIR_FWD)
	    	GPIOPulseLow(Board_FWD_N, BUTTON_PULSE_TIME);
	    else
	    	GPIOPulseLow(Board_REW_N, BUTTON_PULSE_TIME);

	    /*
	     * ENTER MAIN SEARCH LOCATE LOOP
	     */

		do {

			/* Get distance in inches from zero reset point */
			distance = POSITION_TO_INCHES((float)g_sysData.tapePosition);

			/* Calculate the current position delta from cue point */
			delta = g_sysData.cuePoint[index].ipos - g_sysData.tapePosition;

			System_printf("%d : %f\n", delta, g_sysData.tapeTach);
			System_flush();

			if (dir == DIR_FWD)
			{
				if (delta < 0)
				{
					System_printf("FWD SEARCH COMPLETE\n");
					System_flush();
					break;
				}
			}
			else if (dir == DIR_REW)
			{
				if (delta > 0)
				{
					System_printf("REW SEARCH COMPLETE\n");
					System_flush();
					break;
				}
			}

			Task_sleep(250);

		} while (!g_sysData.searchCancel);

		/* Send STOP button pulse to stop transport */
		GPIOPulseLow(Board_STOP_N, BUTTON_PULSE_TIME);

		System_printf("SEARCH END\n");
		System_flush();
#endif
    }
}

/* End-Of-File */
