/* ============================================================================
 *
 * DTC-1200 Digital Transport Controller for Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2018, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * Timer Logic contributed by Bruno Saraiva
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
 * ============================================================================ */

#include <stdint.h>
#include <stdbool.h>
#include <inc/hw_sysctl.h>
#include <inc/hw_memmap.h>
#include <inc/hw_types.h>
#include <inc/hw_ints.h>
#include <inc/hw_gpio.h>
#include <inc/hw_timer.h>
#include <inc/hw_ssi.h>
#include <inc/hw_i2c.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>

#include <driverlib/gpio.h>
#include <driverlib/flash.h>
#include <driverlib/eeprom.h>
#include <driverlib/sysctl.h>
#include <driverlib/interrupt.h>
#include <driverlib/timer.h>
#include <driverlib/i2c.h>
#include <driverlib/ssi.h>
#include <driverlib/udma.h>
#include <driverlib/adc.h>
#include <driverlib/qei.h>
#include <driverlib/pin_map.h>

#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Gate.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>

/* Project specific includes */
#include "STC1200.h"
#include "TapeTach.h"

/****************************************************************************
 * Static Data Items
 ****************************************************************************/

static uint32_t g_systemClock = 120000000;

/* Hardware Interrupt Handlers */
static Void Timer1AIntHandler(UArg arg);
static Void Timer2AIntHandler(UArg arg);

#if (TACH_TYPE_EDGE_WIDTH > 0)

static Hwi_Struct Timer1HwiStruct;
static Hwi_Struct Timer2HwiStruct;

static TACHDATA g_tach;

/****************************************************************************
 * The transport has tape tach derived from the search-to-cue timer card
 * using the quadrature encoder from the tape timer roller. The pulse stream
 * is approximately 240 Hz with tape moving at 30 IPS. We configure
 * WTIMER1A as 32-bit input edge count mode.
  ****************************************************************************/

void TapeTach_initialize(void)
{
    Error_Block eb;
    Hwi_Params  hwiParams;

	//BIOS_getCpuFreq(&freq);

	/* Map the timer interrupt handlers. We don't make sys/bios calls
	 * from these interrupt handlers and there is no need to create a
	 * context handler with stack swapping for these. These handlers
	 * just update some globals variables and need to execute as
	 * quickly and efficiently as possible.
	 */
	//Hwi_plug(INT_TIMER1A, Timer1AIntHandler);
    //Hwi_plug(INT_TIMER2A, Timer2AIntHandler);
    Error_init(&eb);
    Hwi_Params_init(&hwiParams);
    Hwi_construct(&Timer1HwiStruct, INT_TIMER1A, Timer1AIntHandler, &hwiParams, &eb);
    if (Error_check(&eb))
        System_abort("Couldn't construct TIMER1 hwi");

    Error_init(&eb);
    Hwi_Params_init(&hwiParams);
    Hwi_construct(&Timer2HwiStruct, INT_TIMER2A, Timer2AIntHandler, &hwiParams, &eb);
    if (Error_check(&eb))
        System_abort("Couldn't construct TIMER2 hwi");

    /* Enable the wide timer peripheral */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);

    /* First make sure the timer is disabled */
    TimerDisable(TIMER1_BASE, TIMER_A);
    TimerDisable(TIMER2_BASE, TIMER_A);

    /* Disable global interrupts */
    IntMasterDisable();

    /* Enable pin PL6 for TIMER1 T1CCP0 */
    GPIOPinTypeGPIOInput(GPIO_PORTL_BASE, GPIO_PIN_6);
    GPIOPinConfigure(GPIO_PL6_T1CCP0);
    GPIOPinTypeTimer(GPIO_PORTL_BASE, GPIO_PIN_6);

    /* Configure timer1 for edge mode time capture.
     */
    TimerPrescaleSet(TIMER1_BASE, TIMER_A, 100);

    TimerConfigure(TIMER1_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_CAP_TIME);
    /* Define event which generates interrupt on timer A */
    TimerControlEvent(TIMER1_BASE, TIMER_A, TIMER_EVENT_NEG_EDGE);
    /* Enable interrupt on timer A for capture event and timer B for timeout */
    TimerIntEnable(TIMER1_BASE, TIMER_CAPA_EVENT);

    /* Configure timer2 as 32-bit wide timer for timeout
     * when no edges are detected after 1/2 second.
     */
    TimerConfigure(TIMER2_BASE, TIMER_CFG_B_PERIODIC);
    /* Configure the timeout count on timer2 for half a second */
    TimerLoadSet(TIMER2_BASE, TIMER_A, g_systemClock / 2);
    /* Enable interrupt on timer A for capture event and timer B for timeout */
    //TimerIntEnable(TIMER2_BASE, TIMER_TIMA_TIMEOUT);

    /* Enable timer A & B interrupts */
    IntEnable(INT_TIMER1A);
    //IntEnable(INT_TIMER2A);

    /* Enable master interrupts */
    IntMasterEnable();

    /* Start timers A and B*/
    TimerEnable(TIMER1_BASE, TIMER_A);
    //TimerEnable(TIMER2_BASE, TIMER_A);
}

/****************************************************************************
 * WTIMER1A FALLING EDGE CAPTURE TIMER INTERRUPT HANDLER
 ****************************************************************************/

Void Timer1AIntHandler(UArg arg)
{
    uint32_t thisCount;
    uint32_t thisPeriod;
    uint32_t key;

    TimerIntClear(TIMER1_BASE, TIMER_CAPA_EVENT);

    /* ENTER - Critical Section */
    //key = Hwi_disable();

    thisCount = TimerValueGet(TIMER1_BASE, TIMER_A);
    thisPeriod = g_tach.previousCount - thisCount;
    g_tach.previousCount = thisCount;

    if (thisPeriod)       /* Shield from dividing by zero */
    {
        /* Calculates and store averaged value */
        g_tach.averageSum -= g_tach.averageCount[g_tach.averageIdx];
        g_tach.averageSum += thisPeriod;
        g_tach.averageCount[g_tach.averageIdx] = thisPeriod;
        g_tach.averageIdx++;
        g_tach.averageIdx %= TACH_AVG_QTY;

        /* Sets the status to indicate tach is alive */
        g_tach.tachAlive = true;

        /* Store RAW value, which refers to one measurement only */
        g_tach.frequencyRawHz = g_systemClock / thisPeriod; // WAS FLOAT

        /* Update the average sum */
        if (g_tach.averageSum)
            g_tach.frequencyAvgHz = g_systemClock / (g_tach.averageSum / TACH_AVG_QTY);

        /* Resets timeout timer */
        HWREG(TIMER2_BASE + TIMER_O_TAV) = g_systemClock / 2;
    }

    /* EXIT - Critical Section */
    //Hwi_restore(key);
}

/****************************************************************************
 * WTIMER1B 1/2 SECOND EDGE DETECT TIMEOUT TIMER INTERRUPT HANDLER
 ****************************************************************************/

Void Timer2AIntHandler(UArg arg)
{
    uint32_t key;

    TimerIntClear(TIMER2_BASE, TIMER_TIMA_TIMEOUT);

    key = Hwi_disable();
    {
        g_tach.tachAlive      = false;
        g_tach.frequencyAvgHz = 0.0f;
        g_tach.frequencyRawHz = 0.0f;
    }
    Hwi_restore(key);
}

/****************************************************************************
 * Read the current tape tachometer count.
 ****************************************************************************/

float TapeTach_read(void)
{
    uint32_t key;
	float avg;

	key = Hwi_disable();
	{
	    avg = (float)g_tach.frequencyAvgHz;
	    //avg = (float)g_tach.frequencyRawHz;
	}
	Hwi_restore(key);

	return avg;
}

/****************************************************************************
 * Reset the tach data.
 ****************************************************************************/

void TapeTach_reset(void)
{
    size_t i;
    uint32_t key;

    key = Hwi_disable();
    {
        for(i=0; i < TACH_AVG_QTY; i++)
            g_tach.averageCount[i] = 0;

        g_tach.previousCount  = 0;
        g_tach.averageSum     = 0;
        g_tach.averageIdx     = 0;

        g_tach.tachAlive      = false;
        g_tach.frequencyAvgHz = 0.0f;
        g_tach.frequencyRawHz = 0.0f;
    }
    Hwi_restore(key);
}

#else

/****************************************************************************
 * The transport has tape tach derived from the search-to-cue timer card
 * using the quadrature encoder from the tape timer roller. The pulse stream
 * is approximately 240 Hz with tape moving at 30 IPS. In full shuttle speed
 * the tach pulses may go up to 2.5kHz. We configure TIMER1A as 16-bit input
 * edge count mode. TIMER2A is used in 32-bit periodic mode to time the
 * period between edges.
  ****************************************************************************/

#define TACH_EDGE_COUNT	20

static uint32_t g_prevCount = 0;	//0xFFFFFFFF;
static uint32_t g_thisPeriod;
static uint32_t g_frequencyRawHz = 0;

static Hwi_Struct Timer1HwiStruct;
static Hwi_Struct Timer2HwiStruct;

void TapeTach_initialize(void)
{
    Error_Block eb;
    Hwi_Params  hwiParams;

    //g_systemClock = 120000000;	//SysCtlClockGet();

	/* Map the timer interrupt handlers. We don't make sys/bios calls
	 * from these interrupt handlers and there is no need to create a
	 * context handler with stack swapping for these. These handlers
	 * just update some globals variables and need to execute as
	 * quickly and efficiently as possible.
	 */
	//Hwi_plug(INT_TIMER1A, Timer1AIntHandler);
    //Hwi_plug(INT_TIMER2A, Timer2AIntHandler);

    Error_init(&eb);
    Hwi_Params_init(&hwiParams);
    Hwi_construct(&Timer1HwiStruct, INT_TIMER1A, Timer1AIntHandler, &hwiParams, &eb);
    if (Error_check(&eb))
        System_abort("Couldn't construct TIMER1 hwi");

    Error_init(&eb);
    Hwi_Params_init(&hwiParams);
    Hwi_construct(&Timer2HwiStruct, INT_TIMER2A, Timer2AIntHandler, &hwiParams, &eb);
    if (Error_check(&eb))
        System_abort("Couldn't construct TIMER2 hwi");

    /* Enable the timer peripheral */
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOL);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER1));
	SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);
	while(!SysCtlPeripheralReady(SYSCTL_PERIPH_TIMER2));

    /* Enable pin PL6 for TIMER1 T1CCP0 */
    GPIOPinTypeGPIOInput(GPIO_PORTL_BASE, GPIO_PIN_6);
    GPIOPinTypeTimer(GPIO_PORTL_BASE, GPIO_PIN_6);
    GPIOPinConfigure(GPIO_PL6_T1CCP0);

    /* First make sure the timers are disabled */
    TimerDisable(TIMER1_BASE, TIMER_A);
    TimerDisable(TIMER2_BASE, TIMER_A);

    /* Disable global interrupts */
    IntMasterDisable();

    /* Configure timer for edge count capture, split mode. Timer-B is
     * not used.
     */
    //TimerPrescaleSet(TIMER1_BASE, TIMER_A, PRESCALE_VALUE);
    TimerConfigure(TIMER1_BASE, TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_CAP_COUNT);
    /* Load timer with number of edges to count */
	TimerLoadSet(TIMER1_BASE, TIMER_A, TACH_EDGE_COUNT);
	/* Interrupt on count down to zero */
	TimerMatchSet(TIMER1_BASE, TIMER_A, 0);
    /* Define event which generates interrupt on timer A */
    TimerControlEvent(TIMER1_BASE, TIMER_A, TIMER_EVENT_NEG_EDGE);
	/* Enable interrupt on timer A for capture event */
	TimerIntEnable(TIMER1_BASE, TIMER_CAPA_MATCH);

	/* Configure timer2 for full width 32-bit periodic timer */
    TimerConfigure(TIMER2_BASE, TIMER_CFG_A_PERIODIC);
    /* Configure the timeout count for half a second */
    TimerLoadSet(TIMER2_BASE, TIMER_A, g_systemClock / 2);
    /* Enable interrupt on timer A for timeout */
    TimerIntEnable(TIMER2_BASE, TIMER_TIMA_TIMEOUT);

    /* Enable timer A & B interrupts */
    IntEnable(INT_TIMER1A);
    IntEnable(INT_TIMER2A);

    /* Enable master interrupts */
    IntMasterEnable();

    /* Start the timers */
	TimerEnable(TIMER1_BASE, TIMER_A);
    TimerEnable(TIMER2_BASE, TIMER_A);
}

/****************************************************************************
 * TIMER1A FALLING EDGE CAPTURE TIMER INTERRUPT HANDLER
 ****************************************************************************/
Void Timer1AIntHandler(UArg arg)
{
	uint32_t key;
    uint32_t thisCount;

    uint32_t status = TimerIntStatus(TIMER1_BASE, true);

	/* Clear the interrupt */
    TimerIntClear(TIMER1_BASE, TIMER_CAPA_MATCH);

	/* Read the current period timer count */
    thisCount = TimerValueGet(TIMER2_BASE, TIMER_A);

	/* Reset the edge count and enable the timer */
	TimerLoadSet(TIMER1_BASE, TIMER_A, TACH_EDGE_COUNT);
	TimerEnable(TIMER1_BASE, TIMER_A);

    key = Hwi_disable();

    if (thisCount > g_prevCount)
    	g_thisPeriod = thisCount - g_prevCount;
    else
    	g_thisPeriod = g_prevCount - thisCount;

    g_prevCount = thisCount;	//TimerValueGet(TIMER2_BASE, TIMER_A);

    Hwi_restore(key);

    /* Reset half second timeout timer */
    HWREG(TIMER2_BASE + TIMER_O_TAV) = g_systemClock / 2;
}

/****************************************************************************
 * TIMER2 PERIODIC TIMEOUT TIMER INTERRUPT HANDLER
 ****************************************************************************/

Void Timer2AIntHandler(UArg arg)
{
	uint32_t key;

    TimerIntClear(TIMER2_BASE, TIMER_TIMA_TIMEOUT);

    key = Hwi_disable();
    g_prevCount = 0;
    g_thisPeriod = 0;
    g_frequencyRawHz = 0;
    Hwi_restore(key);
}

/****************************************************************************
 * Read the current tape tachometer count.
 ****************************************************************************/

float TapeTach_read(void)
{
	float period;
    uint32_t key;

    key = Hwi_disable();
    period = (float)g_thisPeriod;
    Hwi_restore(key);

    if (period)
    	return 120000000.0f / period;

    return period;	//0.0f;
}

/****************************************************************************
 * Reset the tach data.
 ****************************************************************************/

void TapeTach_reset(void)
{
    uint32_t key = Hwi_disable();
    g_frequencyRawHz = 0;
    g_thisPeriod = 0;
    Hwi_restore(key);
}

#endif

/* End-Of-File */
