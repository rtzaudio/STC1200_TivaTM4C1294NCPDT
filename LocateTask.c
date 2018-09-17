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

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Gate.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/knl/Mailbox.h>
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

#include "STC1200.h"
#include "Board.h"
#include "IPCTask.h"
#include "CLITask.h"

/* Local Constants */

#define BUTTON_PULSE_TIME	50
#define IPC_TIMEOUT         1000

/* External Data Items */

extern SYSDATA g_sysData;
extern Mailbox_Handle g_mailboxLocate;

/* Static Function Prototypes */

static void GPIOPulseLow(uint32_t index, uint32_t duration);

Bool SetShuttleVelocity(uint32_t velocity);
Bool GetShuttleVelocity(uint32_t* velocity);

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

	CLI_printf("SET Cue Point %d\r\n", index);
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

	CLI_printf("CLEAR Cue Point %d\r\n", index);
}

/*****************************************************************************
 * Clear all cue point memories except single cue point on the deck.
 *****************************************************************************/

void CuePointClearAll(void)
{
    size_t i;

    Semaphore_pend(g_semaCue, BIOS_WAIT_FOREVER);

    for (i=0; i < MAX_CUE_POINTS; i++)
    {
        g_sysData.cuePoint[i].ipos  = 0;
        g_sysData.cuePoint[i].flags = 0x00;
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
    uint32_t shuttle_vel;
    LocateMessage msg;

    CLI_printf("\n\nSTC-1200 Starting...\n\n");

    /* Initialize single transport cue point to zero */
    CuePointStore(SINGLE_CUE_POINT);

    while (true)
    {
    	/* Clear SEARCHING_OUT status i/o pin */
    	GPIO_write(Board_SEARCHING, PIN_HIGH);

    	/* Wait for a message up to 1 second */
        if (!Mailbox_pend(g_mailboxLocate, &msg, 250))
        {
    		//CLI_printf("%.1f\r\n", g_sysData.tapeTach);
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

		/*
		 * BEGIN AUTO-LOCATE SEARCH FUNCTION
		 */

        /* Get the shuttle velocity setting from the DTC-1200 */
        GetShuttleVelocity(&shuttle_vel);

        CLI_printf("LOCATE[%d] %d to %d\n", index,
        		g_sysData.tapePosition,
				g_sysData.cuePoint[index].ipos);

		/* Calculate the current position delta from cue point */
		delta = g_sysData.cuePoint[index].ipos - g_sysData.tapePosition;

		/* Determine which direction we need to go initially */

		if (delta > 0)
		{
			dir = DIR_FWD;
			CLI_printf("SEARCH FWD %d\n", delta);
		}
		else if (delta < 0)
		{
			dir = DIR_REW;
			CLI_printf("SEARCH REW %d\n", delta);
		}
		else
		{
			dir = DIR_ZERO;
			CLI_printf("AT ZERO ALREADY!\n");
			continue;
		}

		/* Set SEARCHING_OUT status i/o pin */
		GPIO_write(Board_SEARCHING, PIN_LOW);

		/* Clear the global search cancel flag */
	    key = Hwi_disable();
	    g_sysData.searchCancel = FALSE;
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

	    // Avoid divisions when possible. For example,
	    // revolutions = position / ROLLER_TICKS_PER_REV_F
	    // takes a lot of clock cycles. Since the ROLLER_TICKS
	    // is a fixed thing, the idea is to calculate, JUST ONCE,
	    // the inverse of that:
	    //
	    //  gInvRollerTicks = 1.0f / ROLLER_TICKS;
	    //
	    // And on execution time, use a multiplication instead:
	    //
	    //  revolutions = position * gInvRollerTicks;
	    //
	    // This takes 1 clock cycle, the ARM numeric processor
	    // has hardware for that, but not for division.

	    //float invRollerTicks = 1.0f / ROLLER_TICKS_PER_REV_F;
	    //float revolutions;

	    while (!g_sysData.searchCancel)
		{
	        /* Calculate revolutions while avoiding division */
            //revolutions = (float)g_sysData.tapePosition * invRollerTicks;
            /* Calculate distance from revolutions */
            //distance = revolutions * ROLLER_CIRCUMFERENCE_F;

	        /* Get distance in inches from zero reset point */
			distance = POSITION_TO_INCHES((float)g_sysData.tapePosition);

			/* Calculate the current position delta from cue point */
			delta = g_sysData.cuePoint[index].ipos - g_sysData.tapePosition;

			CLI_printf("%d : %.1f\n", delta, g_sysData.tapeTach);

			if (dir == DIR_FWD)
			{
				/* search forward mode completed? */
				if (delta < 0)
					break;
			}
			else if (dir == DIR_REW)
			{
				/* search rewind mode completed? */
				if (delta > 0)
					break;
			}

			Task_sleep(100);
		}

		/* Send STOP button pulse to stop transport */
		GPIOPulseLow(Board_STOP_N, BUTTON_PULSE_TIME);

		/* Set SEARCHING_OUT status i/o pin */
        GPIO_write(Board_SEARCHING, PIN_HIGH);

		CLI_printf("SEARCH END\n");
    }
}

/*****************************************************************************
 * IPC get/set operations to the DTC-1200
 *****************************************************************************/

Bool SetShuttleVelocity(uint32_t velocity)
{
    IPCMSG msg;

    msg.type     = IPC_TYPE_TRANSACTION;
    msg.opcode   = OP_SET_VELOCITY;
    msg.param1.U = velocity;
    msg.param2.U = 0;

    return IPC_Transaction(&msg, IPC_TIMEOUT);
}

Bool GetShuttleVelocity(uint32_t* velocity)
{
    IPCMSG msg;

    msg.type     = IPC_TYPE_TRANSACTION;
    msg.opcode   = OP_GET_VELOCITY;
    msg.param1.U = 0;
    msg.param2.U = 0;

    if (!IPC_Transaction(&msg, IPC_TIMEOUT))
        return FALSE;

    /* Return query results */
    *velocity = msg.param1.U;

    return TRUE;
}

/* End-Of-File */
