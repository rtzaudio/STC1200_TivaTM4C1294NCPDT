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

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "inc/hw_gpio.h"
#include "inc/hw_hibernate.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/hibernate.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "driverlib/systick.h"
#include "utils/ustdlib.h"

//*****************************************************************************
// Lookup table to convert numerical value of a month into text.
//*****************************************************************************

static char *g_ppcMonth[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

//*****************************************************************************
// Flag that informs that date and time have to be set.
//*****************************************************************************

volatile bool g_bSetDate;

//*****************************************************************************
// Variables that keep track of the date and time.
//*****************************************************************************

uint32_t g_ui32MonthIdx, g_ui32DayIdx, g_ui32YearIdx;
uint32_t g_ui32HourIdx, g_ui32MinIdx;

//*****************************************************************************
// This function reads the current date and time from the calendar logic of the
// hibernate module.  Return status indicates the validity of the data read.
// If the received data is valid, the 24-hour time format is converted to
// 12-hour format.
//*****************************************************************************

bool DateTimeGet(struct tm *sTime)
{
    // Get the latest time.
    HibernateCalendarGet(sTime);

    // Is valid data read?
    if(((sTime->tm_sec < 0) || (sTime->tm_sec > 59)) ||
       ((sTime->tm_min < 0) || (sTime->tm_min > 59)) ||
       ((sTime->tm_hour < 0) || (sTime->tm_hour > 23)) ||
       ((sTime->tm_mday < 1) || (sTime->tm_mday > 31)) ||
       ((sTime->tm_mon < 0) || (sTime->tm_mon > 11)) ||
       ((sTime->tm_year < 100) || (sTime->tm_year > 199)))
    {
        // No - Let the application know
        return false;
    }

    // Return that new data is available so that it can be displayed.
    return true;
}

//*****************************************************************************
// This function formats valid new date and time to be displayed on the home
// screen in the format "MMM DD, YYYY  HH : MM : SS AM/PM".  Example of this
// format is Aug 01, 2013  08:15:30 AM.  It also indicates if valid new data
// is available or not.  If date and time is invalid, this function sets the
// date and time to default value.
//*****************************************************************************

bool DateTimeDisplayGet(char *pcBuf, uint32_t ui32BufSize)
{
    static uint32_t ui32SecondsPrev = 0xFF;
    struct tm sTime;
    uint32_t ui32Len;

    // Get the latest date and time and check the validity.
    if (DateTimeGet(&sTime) == false)
    {
        // Invalid - Force set the date and time to default values and return
        // false to indicate no information to display.
        g_bSetDate = true;
        return false;
    }

    // If date and time is valid, check if seconds have updated from previous
    // visit.

    if(ui32SecondsPrev == sTime.tm_sec)
    {
        // No - Return false to indicate no information to display.
        return false;
    }

    // If valid new date and time is available, update a local variable to keep
    // track of seconds to determine new data for next visit.

    ui32SecondsPrev = sTime.tm_sec;

    // Format the date and time into a user readable format.
    ui32Len = usnprintf(pcBuf, ui32BufSize, "%s %02u, 20%02u  ",
                        g_ppcMonth[sTime.tm_mon], sTime.tm_mday,
                        sTime.tm_year - 100);

    usnprintf(&pcBuf[ui32Len], ui32BufSize - ui32Len, "%02u : %02u : %02u",
              sTime.tm_hour, sTime.tm_min, sTime.tm_sec);

    // Return true to indicate new information to display.
    return true;
}

//*****************************************************************************
// This function writes the requested date and time to the calendar logic of
// hibernation module.
//*****************************************************************************

void DateTimeSet(void)
{
    struct tm sTime;

    // Get the latest date and time.  This is done here so that unchanged
    // parts of date and time can be written back as is.
    HibernateCalendarGet(&sTime);

    // Set the date and time values that are to be updated.
    sTime.tm_hour = g_ui32HourIdx;
    sTime.tm_min  = g_ui32MinIdx;
    sTime.tm_mon  = g_ui32MonthIdx;
    sTime.tm_mday = g_ui32DayIdx;
    sTime.tm_year = 100 + g_ui32YearIdx;

    // Update the calendar logic of hibernation module with the requested data.
    HibernateCalendarSet(&sTime);
}

//*****************************************************************************
// This function sets the time to the default system time.
//*****************************************************************************

void DateTimeDefaultSet(void)
{
    g_ui32MonthIdx = 7;
    g_ui32DayIdx   = 29;
    g_ui32YearIdx  = 13;
    g_ui32HourIdx  = 8;
    g_ui32MinIdx   = 30;
}

//*****************************************************************************
// This function updates individual buffers with valid date and time to be
// displayed on the date screen so that the date and time can be updated.
//*****************************************************************************

bool DateTimeUpdateGet(void)
{
    struct tm sTime;

    // Get the latest date and time and check the validity.
    if(DateTimeGet(&sTime) == false)
    {
        // Invalid - Return here with false as no information to update.
        // So, use default values.

        DateTimeDefaultSet();
        return false;
    }

    // If date and time is valid, copy the date and time values into respective
    // indexes.

    g_ui32MonthIdx = sTime.tm_mon;
    g_ui32DayIdx = sTime.tm_mday;
    g_ui32YearIdx = sTime.tm_year - 100;
    g_ui32HourIdx = sTime.tm_hour;
    g_ui32MinIdx = sTime.tm_min;

    // Return true to indicate new information has been updated.
    return true;
}

//*****************************************************************************
// This function returns the number of days in a month including for a
// leap year.
//*****************************************************************************

uint32_t GetDaysInMonth(uint32_t ui32Year, uint32_t ui32Mon)
{
    // Return the number of days based on the month.
    if (ui32Mon == 1)
    {
        // For February return the number of days based on the year being a
        // leap year or not.
        if ((ui32Year % 4) == 0)
        {
            // If leap year return 29.
            return 29;
        }
        else
        {
            // If not leap year return 28.
            return 28;
        }
    }
    else if((ui32Mon == 3) || (ui32Mon == 5) || (ui32Mon == 8) ||
            (ui32Mon == 10))
    {
        // For April, June, September and November return 30.
        return 30;
    }

    // For all the other months return 31.
    return 31;
}

//*****************************************************************************
// This function returns the date and time value that is written to the
// calendar match register.  5 seconds are added to the current time.  Any
// side-effects due to this addition are handled here.
//*****************************************************************************

void GetCalendarMatchValue(struct tm* sTime)
{
    uint32_t ui32MonthDays;

    // Get the current date and time and add 5 secs to it.

    HibernateCalendarGet(sTime);
    sTime->tm_sec += 5;

    // Check if seconds is out of bounds.  If so subtract seconds by 60 and
    // increment minutes.

    if(sTime->tm_sec > 59)
    {
        sTime->tm_sec -= 60;
        sTime->tm_min++;
    }

    // Check if minutes is out of bounds.  If so subtract minutes by 60 and
    // increment hours.

    if(sTime->tm_min > 59)
    {
        sTime->tm_min -= 60;
        sTime->tm_hour++;
    }

    // Check if hours is out of bounds.  If so subtract minutes by 24 and
    // increment days.

    if(sTime->tm_hour > 23)
    {
        sTime->tm_hour -= 24;
        sTime->tm_mday++;
    }

    // Since different months have varying number of days, get the number of
    // days for the current month and year.

    ui32MonthDays = GetDaysInMonth(sTime->tm_year, sTime->tm_mon);

    // Check if days is out of bounds for the current month and year.  If so
    // subtract days by the number of days in the current month and increment
    // months.

    if(sTime->tm_mday > ui32MonthDays)
    {
        sTime->tm_mday -= ui32MonthDays;
        sTime->tm_mon++;
    }

    // Check if months is out of bounds.  If so subtract months by 11 and
    // increment years.

    if(sTime->tm_mon > 11)
    {
        sTime->tm_mon -= 11;
        sTime->tm_year++;
    }

    // Check if years is out of bounds.  If so subtract years by 100.

    if(sTime->tm_year > 99)
    {
        sTime->tm_year -= 100;
    }
}

/* End-Of-File */
