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

#define TTY_DEBUG_MSGS  0

#define IPC_TIMEOUT     1000

/* Locator States */
typedef enum _LocateState {
    STATE_START_STATE,
    STATE_BEGIN_SHUTTLE,
    STATE_SEARCH_FAR,
    STATE_SEARCH_MID,
    STATE_SEARCH_NEAR,
    STATE_BRAKE_STATE,
    STATE_BRAKE_VELOCITY,
    STATE_PAST_ZERO,
    STATE_ZERO_DIR,
    STATE_ZERO_CROSS,
    STATE_BEGIN_LOOP,
    STATE_MARK_OUT,
    STATE_LOOP,
    STATE_COMPLETE,
} LocateState;

/*** External Data Items ***/

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

	if (index < MAX_CUE_POINTS)
	{
	    uint32_t key = Hwi_disable();

		g_sys.cuePoint[index].ipos  = 0;
		g_sys.cuePoint[index].flags = CF_NONE;

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

    for (i=0; i < USER_CUE_POINTS; i++)
    {
        uint32_t key = Hwi_disable();

        g_sys.cuePoint[i].ipos  = 0;
        g_sys.cuePoint[i].flags = CF_NONE;

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

void CuePointSet(size_t index, int ipos, uint32_t cue_flags)
{
    Semaphore_pend(g_semaCue, BIOS_WAIT_FOREVER);

    if (!ipos)
        ipos = g_sys.tapePosition;

    if (index < MAX_CUE_POINTS)
    {
        uint32_t key = Hwi_disable();

        g_sys.cuePoint[index].ipos  = ipos;
        g_sys.cuePoint[index].flags = cue_flags;

        Hwi_restore(key);
    }

    Semaphore_post(g_semaCue);
}

/*****************************************************************************
 * This functions sets or clears a bit mask for a cue point in the
 * cue point memory table. The parameter index must be the range of
 * zero to MAX_CUE_POINTS. Cue point 0-63 are for the remote control
 * memories. Cue point 64 is for the single memory cue point buttons
 * on the machine.
 *****************************************************************************/

void CuePointMask(size_t index, uint32_t setmask, uint32_t clearmask)
{
    Semaphore_pend(g_semaCue, BIOS_WAIT_FOREVER);

    if (index < MAX_CUE_POINTS)
    {
        uint32_t key = Hwi_disable();

        /* get current cue bit mask */
        uint32_t cue_flags = g_sys.cuePoint[index].flags;

        /* set any mask bits specified */
        cue_flags |= setmask;

        /* clear any mask bits specified */
        cue_flags &= ~(clearmask);

        /* save the updated bit mask */
        g_sys.cuePoint[index].flags = cue_flags;

        Hwi_restore(key);
    }

    Semaphore_post(g_semaCue);
}

/*****************************************************************************
 *
 *****************************************************************************/

void CuePointGet(size_t index, int* ipos, uint32_t* flags)
{
    Semaphore_pend(g_semaCue, BIOS_WAIT_FOREVER);

    if (index < MAX_CUE_POINTS)
    {
        uint32_t key = Hwi_disable();

        if (ipos)
            *ipos = g_sys.cuePoint[index].ipos;

        if (flags)
            *flags = g_sys.cuePoint[index].flags;

        Hwi_restore(key);
    }

    Semaphore_post(g_semaCue);
}

/*****************************************************************************
 * Return the tape time from the cue point absolute position.
 *****************************************************************************/

void CuePointTimeGet(size_t index, TAPETIME* tapeTime)
{
    memset(tapeTime, 0, sizeof(TAPETIME));

    if (index < MAX_CUE_POINTS)
    {
        int cuePosition = g_sys.cuePoint[index].ipos;

        PositionToTapeTime(cuePosition, tapeTime);

        tapeTime->flags = (cuePosition < 0) ? 0 : F_PLUS;
    }
}

/*****************************************************************************
 * Test cue point status flags
 *****************************************************************************/

Bool IsCuePointFlags(size_t index, uint32_t flags)
{
    Bool status = FALSE;

    if (index < MAX_CUE_POINTS)
    {
        uint32_t key = Hwi_disable();

        status = g_sys.cuePoint[index].flags & flags;

        Hwi_restore(key);
    }

    return status;
}

//*****************************************************************************
// Cue up a locate request to the locator. The cue point index is specified
// in param1. Cue point 65 is single point for cue/search buttons on machine.
//*****************************************************************************

Bool LocateSearch(size_t cuePointIndex, uint32_t cue_flags)
{
    LocateMessage msgLocate;

    if (cuePointIndex >= MAX_CUE_POINTS)
        return FALSE;

    /* Make sure the memory location has a cue point stored */
    if (!(g_sys.cuePoint[cuePointIndex].flags & CF_ACTIVE))
        return FALSE;

    msgLocate.command = LOCATE_SEARCH;
    msgLocate.param1  = (uint32_t)cuePointIndex;
    msgLocate.param2  = cue_flags;

    return Mailbox_post(g_mailboxLocate, &msgLocate, 1000);
}

Bool LocateLoop(uint32_t cue_flags)
{
    LocateMessage msgLocate;

    /* Make sure both the mark in/out points have been set */
    if (!IsCuePointFlags(CUE_POINT_MARK_IN, CF_ACTIVE))
        return FALSE;

    if (!IsCuePointFlags(CUE_POINT_MARK_OUT, CF_ACTIVE))
        return FALSE;

    msgLocate.command = LOCATE_LOOP;
    msgLocate.param1  = CUE_POINT_MARK_IN;
    msgLocate.param2  = cue_flags;

    return Mailbox_post(g_mailboxLocate, &msgLocate, 1000);
}

//*****************************************************************************
// Cancel a locate request in process or check the status to see if the
// locator is searching and/or looping.
//*****************************************************************************

Bool LocateCancel(void)
{
    uint32_t key = Hwi_disable();
    g_sys.searchCancel = TRUE;
    Hwi_restore(key);
    return TRUE;
}

Bool IsLocating(void)
{
    if (g_sys.searching)
        return TRUE;

    if (g_sys.autoLoop)
        return TRUE;

    return FALSE;
}

Bool IsLocatorSearching(void)
{
    return g_sys.searching;
}

Bool IsLocatorAutoLoop(void)
{
    return g_sys.autoLoop;
}

Bool IsLocatorAutoPunch(void)
{
    return g_sys.autoPunch;
}

//*****************************************************************************
// Test to see if transport is out of tape or in thread mode.
//*****************************************************************************

Bool IsTransportHaltMode(void)
{
    uint32_t mode = (g_sys.transportMode & MODE_MASK);

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
    Bool     done;
    Bool     looping;
    int32_t  dir;
    int32_t  cue_from;
    int32_t  out_dist;
	int32_t  cue_dist;
	int32_t  abs_dist;
    size_t   cue_index;
    uint32_t cue_flags;
	uint32_t shuttle_vel;
    uint32_t key;
    LocateState state;
    LocateMessage msg;

    /* Get current transport mode & speed from DTC */
    Transport_GetMode(&g_sys.transportMode, &g_sys.tapeSpeed);

    /* Clear SEARCHING_OUT status i/o pin */
    GPIO_write(Board_SEARCHING, PIN_HIGH);

    /* Set home cue point memory to zero */
    CuePointSet(CUE_POINT_HOME, 0, CF_ACTIVE);

    /* Initialize LOC-0 to zero */
    CuePointSet(g_sys.cueIndex, 0, CF_ACTIVE);

    g_sys.searchCancel = false;             /* true if search canceling   */
    g_sys.searching    = false;             /* true if search in progress */
    g_sys.autoLoop     = false;             /* true if loop mode running  */
    g_sys.autoPunch    = false;

    /*
     * ENTER THE MAIN LOCATOR SEARCH LOOP!
     */

    while(TRUE)
    {
        /* Send TCP state change notification */
        Event_post(g_eventTransport, Event_Id_00);

        /* Wait for a locate request */
        Mailbox_pend(g_mailboxLocate, &msg, BIOS_WAIT_FOREVER);

        /* Discard locate request if in halt mode */
        if (IsTransportHaltMode())
            continue;

        looping = FALSE;

        /* Only look for search requests initially */
        if (msg.command == LOCATE_LOOP)
        {
            cue_index = CUE_POINT_MARK_IN;
            cue_flags = msg.param2;
            looping = TRUE;
#if (TTY_DEBUG_MSGS > 0)
            CLI_printf("LOOP COMMAND!\n");
#endif
        }
        else if (msg.command == LOCATE_SEARCH)
        {
            /* Get the cue point memory index. The cue point table is arranged
             * with 64 memory positions from 0-63. Memory location 64 is reserved
             * for the single point cue/search buttons on the machine timer/roller.
             */
            cue_index = (size_t)msg.param1;
            cue_flags = msg.param2;
#if (TTY_DEBUG_MSGS > 0)
            CLI_printf("SEARCH COMMAND!\n");
#endif
        }
        else
        {
#if (TTY_DEBUG_MSGS > 0)
            CLI_printf("BAD SEARCH COMMAND!\n");
#endif
            /* Unknown command */
            continue;
        }

        if (cue_index >= MAX_CUE_POINTS)
        {
#if (TTY_DEBUG_MSGS > 0)
            CLI_printf("INVALID CUE INDEX %u\n", cue_index);
#endif
            continue;
        }

        /* Is the cue point is active? */
        if ((g_sys.cuePoint[cue_index].flags & CF_ACTIVE) == 0)
        {
#if (TTY_DEBUG_MSGS > 0)
            CLI_printf("LOCATE CUE %u NOT ACTIVE - IGNORING!\n", cue_index);
#endif
            continue;
        }

        /* Set SEARCHING_OUT status i/o pin */
        GPIO_write(Board_SEARCHING, PIN_LOW);

        /* If user cue memory, turn on the button LED */
        if (cue_index < USER_CUE_POINTS)
            SetLocateButtonLED(cue_index);

        /* Set transport to STOP mode initially */
        Transport_Stop();

        /* Save cue from point distance */
        cue_from = g_sys.cuePoint[cue_index].ipos - g_sys.tapePosition;

        if (!cue_from)
            cue_from = 1;

        /* Clear the global search cancel flag */
        key = Hwi_disable();
        g_sys.searching = TRUE;
        g_sys.autoLoop  = looping;
        g_sys.searchProgress = 0;
        Hwi_restore(key);

        /* Send TCP state change notification */
        //Event_post(g_eventTransport, Event_Id_00);

        /**************************************/
        /* BEGIN MAIN AUTO-LOCATE SEARCH LOOP */
        /**************************************/

        state = STATE_START_STATE;

        done = FALSE;

	    do
	    {
	        float time, mark;
            float velocity;

            /* Abort if transport halted, must be tape out? */
            if (IsTransportHaltMode())
            {
#if (TTY_DEBUG_MSGS > 0)
                CLI_printf("*** TRANSPORT HALT STATE 1 ***\n");
#endif
                break;
            }

			/* Get the signed and absolute position(distance) cue_from the cue point */
            cue_dist = g_sys.cuePoint[cue_index].ipos - g_sys.tapePosition;

            abs_dist = abs(cue_dist);

			/* v = d/t */
			if ((velocity = g_sys.tapeTach) < 1.0f)
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

            g_sys.searchProgress = 100 - (int32_t)progress;

#if (TTY_DEBUG_MSGS > 0)
            //if (state >= STATE_SEARCH_FAR)
            //    CLI_printf("d=%d, t=%u, v=%u\n", cue_dist, (uint32_t)time, (uint32_t)velocity);
#endif
			/*
			 * SEARCH FINITE STATE MACHINE
			 */

            /* Send TCP state change notification */
            //Event_post(g_eventTransport, Event_Id_00);

			switch(state)
			{
			case STATE_START_STATE:

			    /* Reset the search cancel flag! This gets set by a
			     * button interrupt handler in the program main.
			     */
		        key = Hwi_disable();
			    g_sys.searchCancel = false;
		        Hwi_restore(key);

                cancel = false;

			    /* Determine which direction we need to go initially */
#if (TTY_DEBUG_MSGS > 0)
			    CLI_printf("BEGIN LOCATE[%u] ", cue_index);
#endif
			    if (g_sys.cuePoint[cue_index].ipos > g_sys.tapePosition)
		        {
#if (TTY_DEBUG_MSGS > 0)
                    CLI_printf("FWD > ");
#endif
		            dir = DIR_FWD;
		        }
		        else if (g_sys.cuePoint[cue_index].ipos < g_sys.tapePosition)
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
                    done = TRUE;
		            dir = DIR_ZERO;
		            break;
		        }

			    /* Determine the shuttle speed range based on distance out */

			    if (abs_dist > 9000)
                {
#if (TTY_DEBUG_MSGS > 0)
                    CLI_printf("FAR\n");
#endif
                    shuttle_vel = JOG_VEL_FAR;
                    state = STATE_SEARCH_FAR;
                }
                else if (abs_dist > 3000)
                {
#if (TTY_DEBUG_MSGS > 0)
                    CLI_printf("MID\n");
#endif
                    shuttle_vel = JOG_VEL_MID;
                    state = STATE_SEARCH_MID;
                }
                else
                {
#if (TTY_DEBUG_MSGS > 0)
                    CLI_printf("NEAR\n");
#endif
                    shuttle_vel = JOG_VEL_NEAR;
                    state = STATE_SEARCH_NEAR;
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

            case STATE_SEARCH_FAR:

                if (time < 130.0f)  /* RES120722 changed to 130, was 110 */
			    {
                    state = STATE_BRAKE_VELOCITY;

	                if (velocity > 35.0f)
	                {
#if (TTY_DEBUG_MSGS > 0)
                        CLI_printf("SHUTTLE FAR BRAKE STATE d=%d\n", cue_dist);
#endif
	                    state = STATE_BRAKE_STATE;
	                }
			    }
			    break;

            case STATE_SEARCH_MID:

                if (time < 70.0f)
                {
                    state = STATE_BRAKE_VELOCITY;

                    if (velocity > 35.0f)
                    {
#if (TTY_DEBUG_MSGS > 0)
                        CLI_printf("SHUTTLE MID BRAKE STATE d=%d\n", cue_dist);
#endif
                        state = STATE_BRAKE_STATE;
                    }
                }
                break;

            case STATE_SEARCH_NEAR:

                if (time < 30.0f)
                {
#if (TTY_DEBUG_MSGS > 0)
                    CLI_printf("SHUTTLE NEAR BRAKE STATE d=%d\n", cue_dist);
#endif
                    state = STATE_BRAKE_VELOCITY;
                }
                break;

            case STATE_BRAKE_STATE:

                //CLI_printf("CHECK BRAKE STATE: d=%d, t=%d, v=%u\n", abs_dist, (int32_t)time, (uint32_t)velocity);

                if (velocity > 30.0f)
                {
#if (TTY_DEBUG_MSGS > 0)
                    CLI_printf("BEGIN DYNAMIC BRAKE: v=%u\n", (uint32_t)velocity);
#endif
                    Transport_Stop();

                    state = STATE_BRAKE_VELOCITY;
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

                    state = STATE_ZERO_CROSS;
                }

                /* Fall through to next velocity check state */

			case STATE_BRAKE_VELOCITY:

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
                state = STATE_ZERO_CROSS;

                /* Fall through... */

            case STATE_ZERO_CROSS:

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
                        g_sys.searchProgress = 100;

                        Transport_Stop();

                        if (looping)
                        {
                            state = STATE_BEGIN_LOOP;
                            break;
                        }
                        done = TRUE;
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
                        g_sys.searchProgress = 100;

                        Transport_Stop();

                        if (looping)
                        {
                            state = STATE_BEGIN_LOOP;
                            break;
                        }
                        done = TRUE;
                        break;
                    }
                }

                if (time < 15.0f)
                {
                    g_sys.searchProgress = 100;
                    Transport_Stop();
#if (TTY_DEBUG_MSGS > 0)
                    CLI_printf("ZERO CROSSED cue=%d, abs=%d\n", cue_dist, abs_dist);
#endif
                    if (looping)
                    {
#if (TTY_DEBUG_MSGS > 0)
                        CLI_printf("*** AUTO-LOOPING ***\n");
#endif
                        state = STATE_BEGIN_LOOP;
                        break;
                    }

                    done = TRUE;
                }
                break;

            case STATE_BEGIN_LOOP:

                g_sys.searching = FALSE;

                if (cue_flags & CF_AUTO_REC)
                    Transport_Play(M_RECORD);
                else
                    Transport_Play(0);

#if (TTY_DEBUG_MSGS > 0)
                CLI_printf("BEGIN LOOP MODE\n");
#endif
                /* Next look for zero cross */
                state = STATE_MARK_OUT;
                break;

            case STATE_MARK_OUT:

                /* Look for tape position to reach/pass mark-out point */

                out_dist = g_sys.cuePoint[CUE_POINT_MARK_OUT].ipos - g_sys.tapePosition;

                /* d = v * t */
                /* t = d/v */
                mark = fabs((float)out_dist) / velocity;

                //CLI_printf("MARK-OUT pos=%d, out=%f\n", g_sysData.tapePosition, mark);

                if (mark < 10.0f)
                {
#if (TTY_DEBUG_MSGS > 0)
                    CLI_printf("MARK-OUT REACHED pos=%d, out=%f\n", g_sys.tapePosition, mark);
#endif
                    state = STATE_LOOP;
                    break;
                }
                break;

            case STATE_LOOP:
                /* Queue up the locate request again and loop again */
                cue_flags |=  CF_AUTO_PLAY;
                state = STATE_START_STATE;
                continue;

            default:
#if (TTY_DEBUG_MSGS > 0)
                CLI_printf("*** INVALID STATE %d ***\n", state);
#endif
			    /* invalid state! */
			    done = TRUE;
			    break;
			}

	        /* Send TCP state change notification */
	        //Event_post(g_eventTransport, Event_Id_00);

            if (done || g_sys.searchCancel)
            {
#if (TTY_DEBUG_MSGS > 0)
                if (done)
                    CLI_printf("*** SEARCH DONE STATE 1 ***\n");

                if (g_sys.searchCancel)
                    CLI_printf("*** SEARCH CANCELED STATE 1 ***\n");
#endif
                break;
            }

			/* Check for a new locate command. It's possible the user may have requested
			 * a new locate cue point while a locate command was already in progress.
			 * If so, we cancel the current locate request and reset to start searching
			 * at the new cue point requested.
			 */
#if 0
	        if (Mailbox_getNumPendingMsgs(g_mailboxLocate) > 0)
	        {
	            done = cancel = TRUE;
	            looping = FALSE;
	            break;
	        }
#else
	        if (Mailbox_pend(g_mailboxLocate, &msg, 1))
	        {
                if (msg.command == LOCATE_SEARCH)
	            {
#if (TTY_DEBUG_MSGS > 0)
                    CLI_printf("*** NEW SEARCH REQUESTED ***\n");
#endif
                    cancel = looping = FALSE;

                    /* New search requested! */

                    cue_index = (size_t)msg.param1;
                    cue_flags = msg.param2;

                    if (cue_index >= MAX_CUE_POINTS)
                    {
#if (TTY_DEBUG_MSGS > 0)
                        CLI_printf("*** INVALID CUE INDEX %d ***\n", cue_index);
#endif
                        break;
                    }

                    /* Is the cue point is active? */
                    if (g_sys.cuePoint[cue_index].flags == 0)
                    {
#if (TTY_DEBUG_MSGS > 0)
                        CLI_printf("*** CUE POINT %d IS NOT ACTIVE! ***\n", cue_index);
#endif
                        break;
                    }

                    /* Clear the global search cancel flag */
                    key = Hwi_disable();
                    g_sys.searching = TRUE;
                    g_sys.autoLoop  = FALSE;
                    //g_sysData.searchCancel = FALSE;
                    g_sys.searchProgress = 0;
                    Hwi_restore(key);

                    /* Set SEARCHING_OUT status i/o pin */
                    GPIO_write(Board_SEARCHING, PIN_LOW);

                    /* If user cue memory, turn on the button LED */
                    if (cue_index < USER_CUE_POINTS)
                        SetLocateButtonLED(cue_index);

                    /* Set transport to STOP mode initially */
                    Transport_Stop();

                    /* Save cue from distance */
                    cue_from = g_sys.cuePoint[cue_index].ipos - g_sys.tapePosition;

                    if (!cue_from)
                        cue_from = 1;

                    state = STATE_START_STATE;
	            }
	        }
#endif
	        /* Abort if transport halted, must be tape out? */
            if (IsTransportHaltMode())
            {
#if (TTY_DEBUG_MSGS > 0)
                CLI_printf("*** TRANSPORT HALT STATE 2 ***\n");
#endif
                done = cancel = TRUE;
                break;
            }

            /* Check for cancel search request */
            cancel = g_sys.searchCancel;

	        /* Exit if user search cancel */
            if (done || cancel)
            {
#if (TTY_DEBUG_MSGS > 0)
                if (cancel)
                    CLI_printf("*** USER SEARCH CANCEL ***\n");
                if (done)
                    CLI_printf("*** SEARCH DONE ***\n");
#endif
                done = TRUE;
                break;
            }

	    } while (!done);    /* END OF CUE POINT SEARCH LOOP */

#if (TTY_DEBUG_MSGS > 0)
	    CLI_printf("** SEARCHING COMPLETE **\n");
#endif
        /* Set SEARCHING_OUT status i/o pin */
        GPIO_write(Board_SEARCHING, PIN_HIGH);

        /* Clear the search in progress flag */
        key = Hwi_disable();
        g_sys.searching = FALSE;
        g_sys.autoLoop  = FALSE;
        //g_sysData.searchCancel = FALSE;
        Hwi_restore(key);

        /* Send STOP button pulse to stop transport. If the
         * user canceled the search, then don't stop or
         * auto-play and just exit the loop allowing the
         * machine to run in it's current mode. If the
         * locate completed, we stop and enter auto play
         * if the auto play/rec cue flag specified.
         */

	    if (!cancel)
	    {
	        Transport_Stop();

            if (!looping)
            {
                if (cue_flags & CF_AUTO_PLAY)
                {
                    if (cue_flags & CF_AUTO_REC)
                        Transport_Play(M_RECORD);
                    else
                        Transport_Play(0);
                }
            }
	    }

	    g_sys.searchCancel = FALSE;

#if (TTY_DEBUG_MSGS > 0)
	    CLI_prompt();
#endif

    } /* MAIN OUTER LOOP, BACK TO QUEUE WAIT FOR NEXT LOCATE REQUEST */
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
