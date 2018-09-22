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

Bool Transport_Stop(void);
Bool Transport_Play(void);
Bool Transport_Fwd(uint32_t velocity);
Bool Transport_Rew(uint32_t velocity);

Bool Config_SetShuttleVelocity(uint32_t velocity);
Bool Config_GetShuttleVelocity(uint32_t* velocity);

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

	CLI_printf("CUE SET[%u] %d\n", index, g_sysData.cuePoint[index].ipos);
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

	CLI_printf("CUE CLR[%u] %d\n", index, g_sysData.cuePoint[index].ipos);
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
// Cue up a locate request to the locator. The cue point index is specified
// in param1. Cue point 65 is single point for cue/search buttons on machine.
//*****************************************************************************

Bool LocateSearch(size_t cuePointIndex)
{
    LocateMessage msgLocate;

    if (cuePointIndex > LAST_CUE_POINT)
        return FALSE;

    msgLocate.command = LOCATE_SEARCH;
    msgLocate.param1  = (uint32_t)cuePointIndex;
    msgLocate.param2  = 0;

    return Mailbox_post(g_mailboxLocate, &msgLocate, 1000);
}

Bool LocateCancel(void)
{
    LocateMessage msgLocate;

    msgLocate.command = LOCATE_CANCEL;
    msgLocate.param1  = 0;
    msgLocate.param2  = 0;

    return Mailbox_post(g_mailboxLocate, &msgLocate, 1000);
}

//*****************************************************************************
// Position reader/display task. This function reads the tape roller
// quadrature encoder and stores the current position data. The 7-segement
// tape position display is also updated via the task so the current tape
// position is always being shown on the machine's display on the transport.
//*****************************************************************************

typedef enum SearchState {
    SEARCH_START_STATE,
    SEARCH_SPEED_RANGE,
    SEARCH_BEGIN_NEAR,
    SEARCH_BEGIN_FAR,
    SEARCH_BEGIN_MID,
    SEARCH_BRAKE_PHASE,
    SEARCH_JOG_PEND,
    SEARCH_JOG_STATE,
    SEARCH_ZERO_CROSS,
    SEARCH_COMPLETE,
} SearchState;

Void LocateTaskFxn(UArg arg0, UArg arg1)
{
    int dir;
	int idist;
	int adist;
	uint32_t key;
	uint32_t shuttle_vel;
	size_t cue_index;
	size_t itemp;
    bool cancel = FALSE;
    LocateMessage msg;

    CLI_printf("\n\nSTC-1200 Starting...\n\n");

    /* Clear SEARCHING_OUT status i/o pin */
    GPIO_write(Board_SEARCHING, PIN_HIGH);

    /* Initialize single transport cue point to zero */
    CuePointStore(LAST_CUE_POINT);

    while(TRUE)
    {
        /* Wait for a locate request */
        Mailbox_pend(g_mailboxLocate, &msg, BIOS_WAIT_FOREVER);

        if (msg.command != LOCATE_SEARCH)
        	continue;

        /* Get the cue point memory index. The cue point table is arranged
         * with 64 memory positions from 0-63. Memory location 64 is reserved
         * for the single point cue/search buttons on the machine timer/roller.
         */

        cue_index = (size_t)msg.param1;

        if (cue_index > MAX_CUE_POINTS)
        	continue;

        /* Is the cue point is active? */
        if (g_sysData.cuePoint[cue_index].flags == 0)
        	continue;

        /* Set SEARCHING_OUT status i/o pin */
        GPIO_write(Board_SEARCHING, PIN_LOW);

        /* Set transport to STOP mode */
        Transport_Stop();

        /* Clear the global search cancel flag */
        key = Hwi_disable();
        g_sysData.searching = TRUE;
        g_sysData.searchCancel = FALSE;
        Hwi_restore(key);

        /**************************************/
        /* BEGIN MAIN AUTO-LOCATE SEARCH LOOP */
        /**************************************/

        SearchState state = SEARCH_START_STATE;

        cancel = FALSE;

	    while (!cancel)
	    {
	        float time;
            float velocity;
	        //float distance;

			/* Calculate the current absolute position(distance) from cue point */
			idist = g_sysData.cuePoint[cue_index].ipos - g_sysData.tapePosition;

			/* absolute distance unsigned */
			adist = abs(idist);

			/* v = d/t */
			velocity = g_sysData.tapeTach;

			if (velocity < 1.0f)
			    velocity = 1;

			/* t = d/v */
			time = (float)adist / velocity;

			/* d = v * t */
			//distance = velocity * time;

            CLI_printf("%u, %u, %f\n", (uint32_t)velocity, adist, time);

			/* Finite State Machine */

			switch(state)
			{
			case SEARCH_START_STATE:
		        /* Determine which direction we need to go initially */
			    CLI_printf("LOCATE[%u] %d => ", cue_index, idist);

			    if (idist > 0)
		        {
		            dir = DIR_FWD;
		            CLI_printf("FWD\n");
		        }
		        else if (idist < 0)
		        {
		            dir = DIR_REW;
		            CLI_printf("REW\n");
		        }
		        else
		        {
		            dir = DIR_ZERO;
                    cancel = TRUE;
		            CLI_printf("AT ZERO!!\n");
		            break;
		        }
		        state = SEARCH_SPEED_RANGE;
		        break;

			case SEARCH_SPEED_RANGE:
			    /* Determine the shuttle speed range based on distance out */
                if (adist > 12500)
                {
                    shuttle_vel = 500;
                    state = SEARCH_BEGIN_FAR;
                    CLI_printf(" FAR\n");
                }
                else if (adist < 6250)
                {
                    shuttle_vel = 100;
                    state = SEARCH_BEGIN_NEAR;
                    CLI_printf(" NEAR\n");
                }
                else
                {
                    shuttle_vel = 300;
                    state = SEARCH_BEGIN_MID;
                    CLI_printf(" MID\n");
                }

		        /* Start the transport in either FWD or REV direction
		         * based on the cue point and current location.
		         */
		        if (dir == DIR_FWD)
		            Transport_Fwd(shuttle_vel);
		        else
		            Transport_Rew(shuttle_vel);
			    break;

            case SEARCH_BEGIN_FAR:
                if (time < 135.0f)
			    {
                    state = SEARCH_JOG_PEND;

	                if (g_sysData.tapeTach > 35.0f)
	                    state = SEARCH_BRAKE_PHASE;
			    }
			    break;

            case SEARCH_BEGIN_MID:
                if (time < 90.0f)
                {
                    state = SEARCH_JOG_PEND;

                    if (g_sysData.tapeTach > 35.0f)
                        state = SEARCH_BRAKE_PHASE;
                }
                break;

            case SEARCH_BEGIN_NEAR:
                if (time < 45.0f)
                {
                    state = SEARCH_JOG_PEND;
                }
                break;

            case SEARCH_BRAKE_PHASE:
                CLI_printf("BRAKE PHASE: %d\n", idist);
                Transport_Stop();
                //if (dir == DIR_FWD)
                //    Transport_Rew(shuttle_vel);
                //else
                //    Transport_Fwd(shuttle_vel);
                state = SEARCH_JOG_PEND;
                /* Fall through to next state.. */

			case SEARCH_JOG_PEND:
			    /* Wait for velocity to drop to 25 or below */
			    if (g_sysData.tapeTach <= 25.0f)
			    {
			        CLI_printf("JOG STATE\n");

	                /* Begin jog at low speed! */
	                if (dir == DIR_FWD)
	                    Transport_Fwd(120);
	                else
	                    Transport_Rew(120);

			        state = SEARCH_ZERO_CROSS;
			    }
			    break;

			case SEARCH_JOG_STATE:
			    /* Switch direction! */
                if (dir == DIR_FWD)
                    Transport_Fwd(120);
                else
                    Transport_Rew(120);
                state = SEARCH_ZERO_CROSS;
                break;

			case SEARCH_ZERO_CROSS:
			    /* Last state, we check zero cross always */
			    break;

			default:
			    break;
			}

			/* ZERO CROSS - check taking direction into account */

			if (dir == DIR_FWD)
			{
				if (idist < 0)
				{
				    /* FORWARD overshoot, change direction */
                    Transport_Rew(300);
                    Task_sleep(1500);
					break;
				}
			}
			else
			{
				if (idist > 0)
				{
				    /* REWIND overshoot, change direction */
				    Transport_Fwd(300);
                    Task_sleep(1500);
					break;
				}
			}

			/* Check for a new locate command. It's possible the user may have requested
			 * a new locate cue point while a locate command was already in progress.
			 * If so, we cancel the current locate request and reset to start searching
			 * at the new cue point requested.
			 */

	        if (Mailbox_pend(g_mailboxLocate, &msg, 10))
	        {
	            /* Abort search loop if cancel requested */
                if (msg.command == LOCATE_CANCEL)
                    break;

                /* New search requested? */
	            if (msg.command == LOCATE_SEARCH)
	            {
	                itemp = (size_t)msg.param1;

	                if (itemp > MAX_CUE_POINTS)
	                    continue;

	                /* Is the new cue point is active? */
	                if (g_sysData.cuePoint[itemp].flags == 0)
	                    continue;

	                /* Switch to new cue point index */
	                cue_index = itemp;

	                /* Reset initial search state machine */
	                state = SEARCH_START_STATE;
	            }
	        }

	        /* Check for search cancel */
            if (g_sysData.searchCancel)
                cancel = TRUE;
	    }

	    /* Send STOP button pulse to stop transport */
	    //GPIOPulseLow(Board_STOP_N, BUTTON_PULSE_TIME);
	    Transport_Stop();

	    /* Set SEARCHING_OUT status i/o pin */
	    GPIO_write(Board_SEARCHING, PIN_HIGH);

	    /* Clear the search in progress flag */
	    key = Hwi_disable();
	    g_sysData.searching = FALSE;
	    g_sysData.searchCancel = FALSE;
	    Hwi_restore(key);

        CLI_printf("SEARCH END\n");
    }
}

/*****************************************************************************
 * HELPER FUNCTIONS
 *****************************************************************************/

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

/*****************************************************************************
 * DTC-1200 TRANSPORT COMMANDS
 *****************************************************************************/

//#define OP_MODE_FWD_LIB             303
//#define OP_MODE_REW_LIB             305

Bool Transport_Stop(void)
{
    IPCMSG msg;

    msg.type     = IPC_TYPE_TRANSPORT;
    msg.opcode   = OP_MODE_STOP;
    msg.param1.U = 0;
    msg.param2.U = 0;

    return IPC_Transaction(&msg, IPC_TIMEOUT);
}

Bool Transport_Play(void)
{
    IPCMSG msg;

    msg.type     = IPC_TYPE_TRANSPORT;
    msg.opcode   = OP_MODE_PLAY;
    msg.param1.U = 0;
    msg.param2.U = 0;

    return IPC_Transaction(&msg, IPC_TIMEOUT);
}

Bool Transport_Fwd(uint32_t velocity)
{
    IPCMSG msg;

    msg.type     = IPC_TYPE_TRANSPORT;
    msg.opcode   = OP_MODE_FWD;
    msg.param1.U = velocity;
    msg.param2.U = 0;

    return IPC_Transaction(&msg, IPC_TIMEOUT);
}

Bool Transport_Rew(uint32_t velocity)
{
    IPCMSG msg;

    msg.type     = IPC_TYPE_TRANSPORT;
    msg.opcode   = OP_MODE_REW;
    msg.param1.U = velocity;
    msg.param2.U = 0;

    return IPC_Transaction(&msg, IPC_TIMEOUT);
}

/*****************************************************************************
 * DTC-1200 CONFIGURATION PARAMETERS
 *****************************************************************************/

Bool Config_SetShuttleVelocity(uint32_t velocity)
{
    IPCMSG msg;

    msg.type     = IPC_TYPE_CONFIG;
    msg.opcode   = OP_SET_SHUTTLE_VELOCITY;
    msg.param1.U = velocity;
    msg.param2.U = 0;

    return IPC_Transaction(&msg, IPC_TIMEOUT);
}

Bool Config_GetShuttleVelocity(uint32_t* velocity)
{
    IPCMSG msg;

    msg.type     = IPC_TYPE_CONFIG;
    msg.opcode   = OP_GET_SHUTTLE_VELOCITY;
    msg.param1.U = 0;
    msg.param2.U = 0;

    if (!IPC_Transaction(&msg, IPC_TIMEOUT))
        return FALSE;

    /* Return query results */
    *velocity = msg.param1.U;

    return TRUE;
}

/* End-Of-File */
