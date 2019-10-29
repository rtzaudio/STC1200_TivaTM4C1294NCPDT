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

#include "STC1200.h"
#include "Board.h"
#include "IPCServer.h"
#include "IPCCommands.h"
#include "RemoteTask.h"
#include "CLITask.h"

/*** Local Constants ***/

#define IPC_TIMEOUT     1000
#define TTY_DEBUG_MSGS  0

/* Locator States */
typedef enum _LocateState {
    LOCATE_START_STATE,
    LOCATE_BEGIN_SHUTTLE,
    LOCATE_SHUTTLE_FAR,
    LOCATE_SHUTTLE_MID,
    LOCATE_SHUTTLE_NEAR,
    LOCATE_BRAKE_STATE,
    LOCATE_BRAKE_VELOCITY,
    LOCATE_PAST_ZERO,
    LOCATE_ZERO_DIR,
    LOCATE_ZERO_CROSS,
    LOCATE_COMPLETE,
} LocateState;

/*** External Data Items ***/

extern SYSDATA g_sysData;
extern Event_Handle g_eventQEI;
extern Mailbox_Handle g_mailboxLocate;

/*** Static Function Prototypes ***/

Bool IsTransportHaltMode(void);

/*****************************************************************************
 * This function stores the current tape position to a cue point memory
 * location specified by index.
 *****************************************************************************/

void CuePointClear(size_t index)
{
	Semaphore_pend(g_semaCue, BIOS_WAIT_FOREVER);

	if (index <= MAX_CUE_POINTS)
	{
	    uint32_t key = Hwi_disable();

		g_sysData.cuePoint[index].ipos  = 0;
		g_sysData.cuePoint[index].flags = CF_NONE;

		Hwi_restore(key);
	}

	Semaphore_post(g_semaCue);
}

/*****************************************************************************
 * Clear all cue point memories except the single cue point memory for
 * the single button locate point on the deck.
 *****************************************************************************/

void CuePointClearAll(void)
{
    size_t i;

    Semaphore_pend(g_semaCue, BIOS_WAIT_FOREVER);

    for (i=0; i < MAX_CUE_POINTS; i++)
    {
        uint32_t key = Hwi_disable();

        g_sysData.cuePoint[i].ipos  = 0;
        g_sysData.cuePoint[i].flags = CF_NONE;

        Hwi_restore(key);
    }

    Semaphore_post(g_semaCue);
}

/*****************************************************************************
 * This functions stores the current tape position to a cue point in the
 * cue point memory table. The parameter index must be the range of
 * zero to MAX_CUE_POINTS. Cue point 0-63 are for the remote control
 * memories. Cue point 64 is for the single memory cue point buttons
 * on the machine.
 *****************************************************************************/

void CuePointSet(size_t index, int ipos)
{
    Semaphore_pend(g_semaCue, BIOS_WAIT_FOREVER);

    if (!ipos)
        ipos = g_sysData.tapePosition;

    if (index <= MAX_CUE_POINTS)
    {
        uint32_t key = Hwi_disable();

        g_sysData.cuePoint[index].ipos  = ipos;
        g_sysData.cuePoint[index].flags = CF_ACTIVE;

        Hwi_restore(key);
    }

    Semaphore_post(g_semaCue);
}

/*****************************************************************************
 *
 *****************************************************************************/

uint32_t CuePointGet(size_t index, int* ipos)
{
    uint32_t flags = 0;

    Semaphore_pend(g_semaCue, BIOS_WAIT_FOREVER);

    if (index <= MAX_CUE_POINTS)
    {
        uint32_t key = Hwi_disable();

        if (ipos)
            *ipos = g_sysData.cuePoint[index].ipos;

        flags = g_sysData.cuePoint[index].flags;

        Hwi_restore(key);
    }

    Semaphore_post(g_semaCue);

    return flags;
}

/*****************************************************************************
 * Return the tape time from the cue point absolute position.
 *****************************************************************************/

void CuePointTimeGet(size_t index, TAPETIME* tapeTime)
{
    memset(tapeTime, 0, sizeof(TAPETIME));

    if (index <= MAX_CUE_POINTS)
    {
        int cuePosition = g_sysData.cuePoint[index].ipos;
        PositionToTapeTime(cuePosition, tapeTime);
        tapeTime->flags = (cuePosition < 0) ? 0 : F_PLUS;
    }
}

//*****************************************************************************
// Cue up a locate request to the locator. The cue point index is specified
// in param1. Cue point 65 is single point for cue/search buttons on machine.
//*****************************************************************************

Bool LocateSearch(size_t cuePointIndex, uint32_t cue_flags)
{
    LocateMessage msgLocate;

    if (cuePointIndex > LAST_CUE_POINT)
        return FALSE;

    /* Make sure the memory location has a cue point stored */
    if (!(g_sysData.cuePoint[cuePointIndex].flags & CF_ACTIVE))
        return FALSE;

    msgLocate.command = LOCATE_SEARCH;
    msgLocate.param1  = (uint32_t)cuePointIndex;
    msgLocate.param2  = cue_flags;

    return Mailbox_post(g_mailboxLocate, &msgLocate, 1000);
}

Bool LocateCancel(void)
{
    uint32_t key = Hwi_disable();
    g_sysData.searchCancel = TRUE;
    Hwi_restore(key);
    return TRUE;
}

Bool IsLocatorSearching(void)
{
    return g_sysData.searching;
}

Bool IsTransportHaltMode(void)
{
    uint32_t mode = (g_sysData.transportMode & MODE_MASK);

    /* Abort if transport halted, must be tape out? */
    if ((mode == MODE_HALT) || (mode == MODE_THREAD))
        return TRUE;

    return FALSE;
}

//*****************************************************************************
// Position reader/display task. This function reads the tape roller
// quadrature encoder and stores the current position data. The 7-segement
// tape position display is also updated via the task so the current tape
// position is always being shown on the machine's display on the transport.
//*****************************************************************************

Void LocateTaskFxn(UArg arg0, UArg arg1)
{
    Bool     cancel;
    int32_t  dir;
    int32_t  cue_from;
	int32_t  cue_dist;
	int32_t  abs_dist;
    size_t   cue_index;
    uint32_t cue_flags;
	uint32_t shuttle_vel;
    uint32_t key;
    LocateState state;
    LocateMessage msg;

    /* Get current transport mode & speed from DTC */
    Transport_GetMode(&g_sysData.transportMode, &g_sysData.tapeSpeed);

#if (TTY_DEBUG_MSGS > 0)
    CLI_printf("\n\nLocator task starting (mode %x)\n\n", g_sysData.transportMode);
#endif

    /* Clear SEARCHING_OUT status i/o pin */
    GPIO_write(Board_SEARCHING, PIN_HIGH);

    /* Initialize single transport cue point to zero */
    CuePointSet(LAST_CUE_POINT, 0);

    /* Initialize LOC-1 to zero */
    CuePointSet(g_sysData.cueIndex, 0);

    while(TRUE)
    {
        /* Send TCP state change notification */
        Event_post(g_eventTransport, Event_Id_00);

        /* Wait for a locate request */
        Mailbox_pend(g_mailboxLocate, &msg, BIOS_WAIT_FOREVER);

        /* Only look for search requests initially */
        if (msg.command != LOCATE_SEARCH)
        {
#if (TTY_DEBUG_MSGS > 0)
            CLI_printf("IGNORING LOCATE COMMAND %u\n", msg.command);
#endif
            continue;
        }

        /* Discard locate request if in halt mode */
        if (IsTransportHaltMode())
            continue;

        /* Get the cue point memory index. The cue point table is arranged
         * with 64 memory positions from 0-63. Memory location 64 is reserved
         * for the single point cue/search buttons on the machine timer/roller.
         */
        cue_index = (size_t)msg.param1;
        cue_flags = msg.param2;

        if (cue_index > MAX_CUE_POINTS)
            continue;

        /* Is the cue point is active? */
        if ((g_sysData.cuePoint[cue_index].flags & CF_ACTIVE) == 0)
        {
#if (TTY_DEBUG_MSGS > 0)
            CLI_printf("\nLOCATE CUE %u NOT ACTIVE - IGNORING!\n", cue_index);
#endif
            continue;
        }

        /* Set SEARCHING_OUT status i/o pin */
        GPIO_write(Board_SEARCHING, PIN_LOW);

        /* Cue memory is active, turn on the button LED */
        if (cue_index < LAST_CUE_POINT)
            SetLocateButtonLED(cue_index);

        /* Set transport to STOP mode initially */
        Transport_Stop();

        /* Save cue from point distance */
        cue_from = g_sysData.cuePoint[cue_index].ipos - g_sysData.tapePosition;

        if (!cue_from)
            cue_from = 1;

        /* Clear the global search cancel flag */
        key = Hwi_disable();
        g_sysData.searching = TRUE;
        g_sysData.searchCancel = FALSE;
        g_sysData.searchProgress = 0;
        Hwi_restore(key);

        /* Send TCP state change notification */
        //Event_post(g_eventTransport, Event_Id_00);

        /**************************************/
        /* BEGIN MAIN AUTO-LOCATE SEARCH LOOP */
        /**************************************/

        state = LOCATE_START_STATE;

        cancel = FALSE;

	    while (!cancel)
	    {
	        float time;
            float velocity;

            /* Abort if transport halted, must be tape out? */
            if (IsTransportHaltMode())
                break;

			/* Get the signed absolute position(distance) cue_from the cue point */
            cue_dist = g_sysData.cuePoint[cue_index].ipos - g_sysData.tapePosition;

            abs_dist = abs(cue_dist);

			/* v = d/t */
			if ((velocity = g_sysData.tapeTach) < 1.0f)
			    velocity = 1.0f;

			/* d = v * t */
			/* t = d/v */
			time = (float)abs_dist / velocity;

			/* Calculate the search progress as percentage */

            float progress = fabs(((float)cue_dist / cue_from) * 100.0f);

            if (progress > 100.0f)
                progress = 100.0f;

            if (progress < 0.0f)
                progress = 0.0f;

            g_sysData.searchProgress = 100 - (int32_t)progress;

#if (TTY_DEBUG_MSGS > 0)
            //if (state >= LOCATE_SHUTTLE_FAR)
            //    CLI_printf("d=%d, t=%u, v=%u\n", cue_dist, (uint32_t)time, (uint32_t)velocity);
#endif
			/*
			 * SEARCH FINITE STATE MACHINE
			 */

            /* Send TCP state change notification */
            Event_post(g_eventTransport, Event_Id_00);

			switch(state)
			{
			case LOCATE_START_STATE:

			    /* Determine which direction we need to go initially */
#if (TTY_DEBUG_MSGS > 0)
			    CLI_printf("\nBEGIN LOCATE[%u] ", cue_index);
#endif
			    if (g_sysData.cuePoint[cue_index].ipos > g_sysData.tapePosition)
		        {
#if (TTY_DEBUG_MSGS > 0)
                    CLI_printf("FWD > ");
#endif
		            dir = DIR_FWD;
		        }
		        else if (g_sysData.cuePoint[cue_index].ipos < g_sysData.tapePosition)
		        {
#if (TTY_DEBUG_MSGS > 0)
                    CLI_printf("REW < ");
#endif
		            dir = DIR_REW;
		        }
		        else
		        {
#if (TTY_DEBUG_MSGS > 0)
                    CLI_printf("AT ZERO!!\n");
#endif
                    cancel = TRUE;
		            dir = DIR_ZERO;
		            break;
		        }

			    /* Determine the shuttle speed range based on distance out */

			    if (abs_dist > 9000)
                {
#if (TTY_DEBUG_MSGS > 0)
                    CLI_printf("FAR");
#endif
                    shuttle_vel = JOG_VEL_FAR;
                    state = LOCATE_SHUTTLE_FAR;
                }
                else if (abs_dist > 3000)
                {
#if (TTY_DEBUG_MSGS > 0)
                    CLI_printf("MID");
#endif
                    shuttle_vel = JOG_VEL_MID;
                    state = LOCATE_SHUTTLE_MID;
                }
                else
                {
#if (TTY_DEBUG_MSGS > 0)
                    CLI_printf("NEAR");
#endif
                    shuttle_vel = JOG_VEL_NEAR;
                    state = LOCATE_SHUTTLE_NEAR;
                }

                //CLI_printf(" d=%d, t=%d\n", cue_dist, (int32_t)time);

                /* Start the transport in either FWD or REV direction
		         * based on the cue point and current location. We set
		         * the M_NOSLOW so the DTC auto-slow function will be
		         * disabled for the shuttle command requested.
		         */
                if (dir == DIR_FWD)
		            Transport_Fwd(shuttle_vel, M_NOSLOW);
		        else
		            Transport_Rew(shuttle_vel, M_NOSLOW);
			    break;

            case LOCATE_SHUTTLE_FAR:

                if (time < 110.0f)
			    {
                    state = LOCATE_BRAKE_VELOCITY;

	                if (velocity > 35.0f)
	                {
#if (TTY_DEBUG_MSGS > 0)
                        CLI_printf("SHUTTLE FAR BRAKE STATE d=%d\n", cue_dist);
#endif
	                    state = LOCATE_BRAKE_STATE;
	                }
			    }
			    break;

            case LOCATE_SHUTTLE_MID:

                if (time < 70.0f)
                {
                    state = LOCATE_BRAKE_VELOCITY;

                    if (velocity > 35.0f)
                    {
#if (TTY_DEBUG_MSGS > 0)
                        CLI_printf("SHUTTLE MID BRAKE STATE d=%d\n", cue_dist);
#endif
                        state = LOCATE_BRAKE_STATE;
                    }
                }
                break;

            case LOCATE_SHUTTLE_NEAR:

                if (time < 30.0f)
                {
#if (TTY_DEBUG_MSGS > 0)
                    CLI_printf("SHUTTLE NEAR BRAKE STATE d=%d\n", cue_dist);
#endif
                    state = LOCATE_BRAKE_VELOCITY;
                }
                break;

            case LOCATE_BRAKE_STATE:

                //CLI_printf("CHECK BRAKE STATE: d=%d, t=%d, v=%u\n", abs_dist, (int32_t)time, (uint32_t)velocity);

                if (velocity > 30.0f)
                {
#if (TTY_DEBUG_MSGS > 0)
                    CLI_printf("BEGIN DYNAMIC BRAKE: v=%u\n", (uint32_t)velocity);
#endif
                    Transport_Stop();

                    state = LOCATE_BRAKE_VELOCITY;
                }
                else
                {
#if (TTY_DEBUG_MSGS > 0)
                    CLI_printf("BEGIN COAST BRAKE: v=%u\n", (uint32_t)velocity);
#endif
                    /* Begin low speed shuttle */
                    if (dir == DIR_FWD)
                        Transport_Fwd(SHUTTLE_SLOW_VEL, M_NOSLOW);
                    else
                        Transport_Rew(SHUTTLE_SLOW_VEL, M_NOSLOW);

                    state = LOCATE_ZERO_CROSS;
                }

                /* Fall through to next velocity check state */

			case LOCATE_BRAKE_VELOCITY:

			    /* Wait for velocity to drop to 15 or below */
                //CLI_printf("d=%d, f=%d, t=%u, v=%u\n", cue_dist, cue_from, (uint32_t)time, (uint32_t)velocity);

			    if (velocity > 40.0f)
			        break;
#if (TTY_DEBUG_MSGS > 0)
			    CLI_printf("BEGIN SLOW SHUTTLE d=%d, t=%d, v=%u\n", cue_dist, (int32_t)time, (uint32_t)velocity);
#endif
                /* Begin low speed shuttle */
                if (dir == DIR_FWD)
                    Transport_Fwd(SHUTTLE_SLOW_VEL, M_NOSLOW);
                else
                    Transport_Rew(SHUTTLE_SLOW_VEL, M_NOSLOW);

                /* Next look for zero cross */
                state = LOCATE_ZERO_CROSS;

                /* Fall through... */

            case LOCATE_ZERO_CROSS:

                /* ZERO CROSS - check taking direction into account */

                //CLI_printf("d=%d, f=%d, t=%u, v=%u\n", cue_dist, cue_from, (uint32_t)time, (uint32_t)velocity);

                /* Check for overshoot! */
                if (dir == DIR_FWD)
                {
                    if (cue_from < cue_dist)
                    {
#if (TTY_DEBUG_MSGS > 0)
                        CLI_printf("OVERSHOOT FWD: %d, %d\n", cue_dist, cue_from);
#endif
                        g_sysData.searchProgress = 100;
                        Transport_Stop();
                        cancel = TRUE;
                        break;
                    }
                }
                else
                {
                    if (cue_from > cue_dist)
                    {
#if (TTY_DEBUG_MSGS > 0)
                        CLI_printf("OVERSHOOT REW: %d, %d\n", cue_dist, cue_from);
#endif
                        g_sysData.searchProgress = 100;
                        Transport_Stop();
                        cancel = TRUE;
                        break;
                    }
                }

                if (time < 20.0f)
                {
                    g_sysData.searchProgress = 100;
                    Transport_Stop();
                    cancel = TRUE;
#if (TTY_DEBUG_MSGS > 0)
                    //CLI_printf("ZERO CROSSED cue=%d, abs=%d\n", cue_dist, abs_dist);
#endif
                }
                break;

            default:
			    /* invalid state! */
			    cancel = TRUE;
			    break;
			}

	        /* Send TCP state change notification */
	        Event_post(g_eventTransport, Event_Id_00);

			/* Check for a new locate command. It's possible the user may have requested
			 * a new locate cue point while a locate command was already in progress.
			 * If so, we cancel the current locate request and reset to start searching
			 * at the new cue point requested.
			 */

			if (cancel)
                break;

	        if (Mailbox_pend(g_mailboxLocate, &msg, 2))
	        {
	            /* Abort search loop if cancel requested */
                if (msg.command == LOCATE_CANCEL)
                {
                    cancel = TRUE;
#if (TTY_DEBUG_MSGS > 0)
        CLI_printf("*** USER SEARCH CANCEL ***\n");
#endif
                    break;
                }
                else if (msg.command == LOCATE_SEARCH)
	            {
                    /* New search requested! */

                    cue_index = (size_t)msg.param1;
                    cue_flags = msg.param2;

                    if (cue_index > MAX_CUE_POINTS)
                        break;

                    /* Is the cue point is active? */
                    if (g_sysData.cuePoint[cue_index].flags == 0)
                        break;

                    /* Clear the global search cancel flag */
                    key = Hwi_disable();
                    g_sysData.searching = TRUE;
                    g_sysData.searchCancel = FALSE;
                    g_sysData.searchProgress = 0;
                    Hwi_restore(key);

                    /* Set SEARCHING_OUT status i/o pin */
                    GPIO_write(Board_SEARCHING, PIN_LOW);

                    /* Cue memory is active, turn on the button LED */
                    if (cue_index < LAST_CUE_POINT)
                        SetLocateButtonLED(cue_index);

                    /* Set transport to STOP mode initially */
                    Transport_Stop();

                    /* Save cue from distance */
                    cue_from = g_sysData.cuePoint[cue_index].ipos - g_sysData.tapePosition;

                    if (!cue_from)
                        cue_from = 1;

                    state = LOCATE_START_STATE;
	            }
	        }

	        /* Abort if transport halted, must be tape out? */
            if (IsTransportHaltMode())
                break;

	        /* Exit if user search cancel */
            if (cancel || g_sysData.searchCancel)
                break;
	    }

#if (TTY_DEBUG_MSGS > 0)
	    CLI_printf("\n** SEARCH END **\n");
#endif
        /* Set SEARCHING_OUT status i/o pin */
        GPIO_write(Board_SEARCHING, PIN_HIGH);

        /* Clear the search in progress flag */
        key = Hwi_disable();
        g_sysData.searching = FALSE;
        g_sysData.searchCancel = FALSE;
        Hwi_restore(key);

        /* Send TCP state change notification */
        Event_post(g_eventTransport, Event_Id_00);

        /* Send STOP button pulse to stop transport */
	    if (!g_sysData.searchCancel)
	    {
	        Transport_Stop();

	        if (cue_flags & CF_AUTO_PLAY)
	        {
                if (cue_flags & CF_AUTO_REC)
                    Transport_Play(M_RECORD);
                else
                    Transport_Play(0);
	        }
	    }
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

#if 0
#define BUTTON_PULSE    50

void GPIOPulseLow(uint32_t index, uint32_t duration)
{
    /* Set the i/o pin to low state */
    GPIO_write(index, PIN_LOW);
    /* Sleep for pulse duration */
    Task_sleep(duration);
    /* Return i/o pin to high state */
    GPIO_write(index, PIN_HIGH);
}
#endif

/* End-Of-File */
