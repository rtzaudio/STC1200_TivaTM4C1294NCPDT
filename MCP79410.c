/****************************************************************************
 * Copyright (C) 2015 Sensorian
 *
 * Modified for use with TI-RTOS by RTZ Microsystems, LLC
 *
 * This file is part of Sensorian.
 *
 *   Sensorian is free software: you can redistribute it and/or modify it
 *   under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Sensorian is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with Sensorian.
 *   If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/

/* TI-RTOS Kernel Header files */
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

/* TI-RTOS Kernel Header files */
#include <xdc/std.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Diags.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Log.h>
#include <xdc/runtime/Memory.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/knl/Task.h>

#include <ti/sysbios/knl/Task.h>

/* TI-RTOS Driver Header files */
#include <ti/drivers/SPI.h>
#include <ti/drivers/GPIO.h>

#include "MCP79410.h"

#define PIN_LOW  (0)

/* Default MCP79410 parameters structure */
const MCP79410_Params MCP79410_defaultParams = {
    0,   /* dummy */
};

/* Static helper function prototypes */
static Void MCP79410_destruct(MCP79410_Handle handle);

static uint8_t MCP79410_dec2bcd(uint8_t num);
static uint8_t MCP79410_bcd2dec(uint8_t num);

static bool MCP79410_Write(MCP79410_Handle handle, uint8_t rtcc_reg, uint8_t data);
static uint8_t MCP79410_Read(MCP79410_Handle handle, uint8_t rtcc_reg);

/*
 *  ======== MCP79410_construct ========
 */
MCP79410_Handle MCP79410_construct(MCP79410_Object *obj, I2C_Handle i2cHandle, MCP79410_Params *params)
{
    /* Initialize the object's fields */
    obj->i2cHandle = i2cHandle;

    GateMutex_construct(&(obj->gate), NULL);

    return ((MCP79410_Handle)obj);
}

/*
 *  ======== MCP79410_create ========
 */
MCP79410_Handle MCP79410_create(I2C_Handle i2cHandle, MCP79410_Params *params)
{
    MCP79410_Handle handle;
    Error_Block eb;

    Error_init(&eb);

    handle = Memory_alloc(NULL, sizeof(MCP79410_Object), NULL, &eb);

    if (handle == NULL) {
        return (NULL);
    }

    handle = MCP79410_construct(handle, i2cHandle, params);

    return (handle);
}

/*
 *  ======== MCP79410_delete ========
 */
Void MCP79410_delete(MCP79410_Handle handle)
{
    MCP79410_destruct(handle);

    Memory_free(NULL, handle, sizeof(MCP79410_Object));
}

/*
 *  ======== MCP79410_destruct ========
 */
Void MCP79410_destruct(MCP79410_Handle handle)
{
    Assert_isTrue((handle != NULL), NULL);

    GateMutex_destruct(&(handle->gate));
}

/*
 *  ======== AT45DB_Params_init ========
 */
Void MCP79410_Params_init(MCP79410_Params *params)
{
    Assert_isTrue(params != NULL, NULL);

    *params = MCP79410_defaultParams;
}

void MCP79410_Initialize(MCP79410_Handle handle, RTCC_Struct *time, Format_t format)
{
    MCP79410_SetHourFormat(handle, format);             // Set hour format to military time standard
    MCP79410_EnableVbat(handle);                        // Enable battery backup
    MCP79410_SetTime(handle, time);
    MCP79410_EnableOscillator(handle);                  // Start clock by enabling oscillator
}

void MCP79410_EnableOscillator(MCP79410_Handle handle)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    uint8_t ST_bit = MCP79410_Read(handle, DAY);        // Read day + OSCON bit
    ST_bit = ST_bit | START_32KHZ;
    MCP79410_Write(handle, SEC,ST_bit);                 // START bit is located in the Sec register

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
}

void MCP79410_DisableOscillator(MCP79410_Handle handle)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    uint8_t ST_bit = MCP79410_Read(handle, DAY);        // Read day + OSCON bit
    ST_bit = ST_bit & ~START_32KHZ;
    MCP79410_Write(handle, SEC, ST_bit);                // START bit is located in the Sec regist

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
}

uint8_t MCP79410_IsRunning(MCP79410_Handle handle)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    uint8_t mask = MCP79410_Read(handle, DAY);

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
    
    return ((mask & OSCRUN) == OSCRUN) ? TRUE : FALSE;
}

void MCP79410_GetTime(MCP79410_Handle handle, RTCC_Struct *current_time)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    current_time->sec     = MCP79410_bcd2dec(MCP79410_Read(handle, SEC) & (~START_32KHZ));
    current_time->min     = MCP79410_bcd2dec(MCP79410_Read(handle, MIN));
    
    uint8_t hour_t = MCP79410_Read(handle, HOUR);

    // hour is in 24 hour format
    hour_t = ((hour_t & HOUR_12) == HOUR_12)? (hour_t & 0x1F) : (hour_t & 0x3F);
    
    current_time->hour    = MCP79410_bcd2dec(hour_t);
    current_time->weekday = MCP79410_bcd2dec(MCP79410_Read(handle, DAY) & ~(OSCRUN|PWRFAIL|VBATEN));
    current_time->date    = MCP79410_bcd2dec(MCP79410_Read(handle, DATE));
    current_time->month   = MCP79410_bcd2dec(MCP79410_Read(handle, MNTH) & ~(LPYR));
    current_time->year    = MCP79410_bcd2dec(MCP79410_Read(handle, YEAR));

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
}

void MCP79410_SetTime(MCP79410_Handle handle, RTCC_Struct *time)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    uint8_t sec     = MCP79410_Read(handle, SEC);       //Seconds
    uint8_t min     = 0;                                //Minutes
    uint8_t hour    = MCP79410_Read(handle, HOUR);      //Hours
    uint8_t weekday = MCP79410_Read(handle, DAY);       //Weekday
    uint8_t date    = 0;                                //Date
    uint8_t month   = MCP79410_Read(handle, MNTH);      //Month
    uint8_t year    = 0;                                //Year
        
    // Seconds register
    if ((sec & START_32KHZ) == START_32KHZ)
        sec = MCP79410_dec2bcd(time->sec) | START_32KHZ;
    else
        sec = MCP79410_dec2bcd(time->sec);
    
    // Minutes
    min = MCP79410_dec2bcd(time->min);
    
    // Hour register
    if ((hour & HOUR_12) == HOUR_12)
        hour = MCP79410_dec2bcd(time->hour) | HOUR_12;
    else
        hour = MCP79410_dec2bcd(time->hour);
    
    if ((hour & PM) == PM)
        hour = hour | PM;
    
    // Mask 3 upper bits
    weekday &= 0x38;

    // Weekday
    weekday |=  MCP79410_dec2bcd(time->weekday);
    
    // Date
    date =  MCP79410_dec2bcd(time->date);
    
    // Month
    if ((month & LPYR) == LPYR)
        month = MCP79410_dec2bcd(time->month) | LPYR;
    else
        month = MCP79410_dec2bcd(time->month);

    // Year
    year = MCP79410_dec2bcd(time->year);
    
    MCP79410_Write(handle, SEC, sec);
    MCP79410_Write(handle, MIN, min);
    MCP79410_Write(handle, HOUR, hour);
    MCP79410_Write(handle, DAY, weekday);
    MCP79410_Write(handle, DATE, date);
    MCP79410_Write(handle, MNTH, month);
    MCP79410_Write(handle, YEAR, year);

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
}

void MCP79410_SetHourFormat(MCP79410_Handle handle, Format_t format)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    MCP79410_DisableOscillator(handle);                 //Diable clock
    uint8_t Format_bit = MCP79410_Read(handle, HOUR);   //Read hour format bit
    if (format == H24)
        Format_bit &= ~HOUR_FORMAT;                     //Set format to H12 (military)
    else
        Format_bit |= HOUR_FORMAT;                      //Set format to H12
    MCP79410_Write(handle, HOUR,Format_bit);            //START bit is located in the Sec register
    MCP79410_EnableOscillator(handle);                  //Enable clock

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
}

void MCP79410_SetPMAM(MCP79410_Handle handle, PMAM_t meridian)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    MCP79410_DisableOscillator(handle);                 //Diable clock
    uint8_t PMAM_bit = MCP79410_Read(handle, HOUR);     //Read meridian bit
    if (meridian == AMT)
        PMAM_bit &= ~PM;                                //Set AM
    else
        PMAM_bit |= PM;                                 //Set PM
    MCP79410_Write(handle, HOUR, PMAM_bit);             //Update PM/AM meridian bit
    MCP79410_EnableOscillator(handle);                  //Enable clock

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
}

void MCP79410_EnableAlarm(MCP79410_Handle handle, Alarm_t alarm)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    uint8_t ctrl_bits = MCP79410_Read(handle, CTRL);

    if (alarm == ZERO)
    {   
        ctrl_bits |= ALM_0;
        MCP79410_Write(handle, CTRL, ctrl_bits);
    }
    else
    {
        ctrl_bits |= ALM_1;
        MCP79410_Write(handle, CTRL, ctrl_bits);
    }

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
}

void MCP79410_DisableAlarm(MCP79410_Handle handle, Alarm_t alarm)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    uint8_t ctrl_bits = MCP79410_Read(handle, CTRL);

    if (alarm == ZERO)
    {   
        ctrl_bits &= ~ALM_0;
        MCP79410_Write(handle, CTRL, ctrl_bits);
    }
    else
    {
        ctrl_bits &= ~ALM_1;
        MCP79410_Write(handle, CTRL, ctrl_bits);
    }

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
}

AlarmStatus_t MCP79410_GetAlarmStatus(MCP79410_Handle handle, Alarm_t alarm)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    AlarmStatus_t status;
    uint8_t temp;

    if (alarm == ZERO)
        temp = MCP79410_Read(handle, ALM0WDAY);     //Read WKDAY register for ALRAM 0
    else
        temp = MCP79410_Read(handle, ALM1WDAY);     //Read WKDAY register for ALRAM 1

    status = (AlarmStatus_t)((temp & ALMx_IF) == ALMx_IF) ? SET : NOT_SET;

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);

    return status;
}

void MCP79410_ClearInterruptFlag(MCP79410_Handle handle, Alarm_t alarm)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    uint8_t temp;

    if (alarm == ZERO)
    {
        temp = MCP79410_Read(handle, ALM0WDAY);     //Read WKDAY register for ALRAM 0
        temp &= (~ALMx_IF);                         //Clear 4-th bit
        MCP79410_Write(handle, ALM0WDAY, temp);     //Enable backup battery mode
    }
    else
    {
        temp = MCP79410_Read(handle, ALM1WDAY);     //Read WKDAY register for ALRAM 1
        temp &= (~ALMx_IF);                         //Clear 4-th bit
        MCP79410_Write(handle, ALM1WDAY, temp);     //Enable backup battery mode
    }

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
}

void MCP79410_SetAlarmTime(MCP79410_Handle handle, RTCC_Struct *time, Alarm_t alarm)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    uint8_t sec     = MCP79410_dec2bcd(time->sec);
    uint8_t min     = MCP79410_dec2bcd(time->min);
    uint8_t hour    = MCP79410_dec2bcd(time->hour);
    uint8_t weekday = MCP79410_dec2bcd(time->weekday);
    uint8_t date    = MCP79410_dec2bcd(time->date);
    uint8_t month   = MCP79410_dec2bcd(time->month);
        
    if (alarm == ZERO)
    {   
        MCP79410_Write(handle, ALM0SEC, sec|START_32KHZ);
        MCP79410_Write(handle, ALM0MIN, min);
        MCP79410_Write(handle, ALM0HR, hour);
        MCP79410_Write(handle, ALM0WDAY, weekday);
        MCP79410_Write(handle, ALM0DATE, date);
        MCP79410_Write(handle, ALM0MTH, month);
    }
    else
    {
        MCP79410_Write(handle, ALM1SEC, sec|START_32KHZ);
        MCP79410_Write(handle, ALM1MIN, min);
        MCP79410_Write(handle, ALM1HR, hour);
        MCP79410_Write(handle, ALM1WDAY, weekday);
        MCP79410_Write(handle, ALM1DATE, date);
        MCP79410_Write(handle, ALM1MTH, month);
    }

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
}
 
void MCP79410_SetAlarmMFPPolarity(MCP79410_Handle handle, Polarity_t MFP_pol, Alarm_t alarm)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    uint8_t Polarity_bit = 0;
    
    if (alarm == ZERO)
        Polarity_bit = MCP79410_Read(handle, ALM0WDAY);     //Read hour format bit
    else
        Polarity_bit = MCP79410_Read(handle, ALM1WDAY);     //Read hour format bit
    
    if (MFP_pol == PIN_LOW)
        Polarity_bit &= ~ALMx_POL;          //Set MFP LOW
    else
        Polarity_bit |= ALMx_POL;           //Set MFP HIGH
    
    if (alarm == ZERO)
        MCP79410_Write(handle, ALM0WDAY, Polarity_bit);     //Update polarity bit for Alarm 0
    else
        MCP79410_Write(handle, ALM1WDAY, Polarity_bit);     //Update polarity bit for Alarm 1

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
}
 
void MCP79410_SetAlarmMatch(MCP79410_Handle handle, Match_t match,Alarm_t alarm)
{   
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    uint8_t AlarmRegister = 0;

    if (alarm == ZERO)
        AlarmRegister = ALM0WDAY;
    else
        AlarmRegister = ALM1WDAY;
    
    uint8_t match_bits = MCP79410_Read(handle, AlarmRegister);

    switch(match)
    {
        case SECONDS_MATCH :
            match_bits &= ~(ALM0MSK2|ALM0MSK1|ALM0MSK0);
            MCP79410_Write(handle, AlarmRegister, match_bits);   //Minutes match
            break;

        case MINUTES_MATCH :
            match_bits |= ALM0MSK0;
            MCP79410_Write(handle, AlarmRegister, match_bits);   //Minutes match
            break;

        case HOURS_MATCH :
            match_bits |= ALM0MSK1;
            MCP79410_Write(handle, AlarmRegister, match_bits);   //Hours match
            break;

        case WEEKDAY_MATCH :
            match_bits |= ALM0MSK1|ALM0MSK0;
            MCP79410_Write(handle, AlarmRegister, match_bits);   //Day of week match
            break;

        case DATE_MATCH :
            match_bits |= ALM0MSK2;
            MCP79410_Write(handle, AlarmRegister, match_bits);   //Date match
            break;

        case FULL_DATE_MATCH :
            match_bits |= ALM0MSK2|ALM0MSK1|ALM0MSK0;
            MCP79410_Write(handle, AlarmRegister, match_bits);   //Sec, Minutes Hours, Date match
            break;

        default :
            match_bits |= ALM0MSK0;
            MCP79410_Write(handle, AlarmRegister, match_bits);   //Minutes match
            break;
    }   

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
}

void MCP79410_SetMFP_Functionality(MCP79410_Handle handle, MFP_t mode)
{   
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    uint8_t MFP_bits = MCP79410_Read(handle, CTRL);
    
    switch(mode)
    {
        case GPO :              //For GPO clear SQWEN, ALM0EN, ALM1EN
            MFP_bits &= ~(SQWEN|ALM_0|ALM_1);
            MCP79410_Write(handle, CTRL, MFP_bits);
            break;

        case ALARM_INTERRUPT :  //For ALARM Interrupts clear SQWEN and set either ALM0EN or ALM1EN
            MFP_bits &= SQWEN;
            MFP_bits |= ALM_0;
            MCP79410_Write(handle, CTRL, MFP_bits);
            break;

        case SQUARE_WAVE :      //For SQUARE WAVE set SQWEN 
            MFP_bits &= ~(ALM_0|ALM_1);
            MFP_bits |= SQWEN;
            MCP79410_Write(handle, CTRL, MFP_bits);
            break;

        default:                //ALARM Interrupts 
            MFP_bits &= SQWEN;
            MFP_bits |= ALM_0;
            MCP79410_Write(handle, CTRL, MFP_bits);
            break;  
    }

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
}

void MCP79410_SetMFP_GPOStatus(MCP79410_Handle handle, Polarity_t status)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    //General Purpose Output mode only available when (SQWEN = 0, ALM0EN = 0, and ALM1EN = 0):
    uint8_t gpo_bit = MCP79410_Read(handle, CTRL);

    if (status == PIN_LOW)
    {
        gpo_bit = OUT_PIN;           // MFP signal level is logic low
        MCP79410_Write(handle, CTRL, gpo_bit);
    }
    else
    {                               // MFP signal level is logic high
        gpo_bit |= OUT_PIN;
        MCP79410_Write(handle, CTRL, gpo_bit);
    }

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
}

uint8_t MCP79410_CheckPowerFailure(MCP79410_Handle handle)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    uint8_t PowerFailure_bit = MCP79410_Read(handle, DAY);      //Read meridian bit
    uint8_t PowerFail;
    PowerFail = ((PowerFailure_bit & PWRFAIL)  == PWRFAIL) ? TRUE : FALSE;
    PowerFailure_bit &= ~PWRFAIL;                               // Clear Power failure bit
    MCP79410_Write(handle, DAY, PowerFailure_bit);              //Update PM/AM meridian bit

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);

    return PowerFail;
}

uint8_t MCP79410_IsVbatEnabled(MCP79410_Handle handle)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    uint8_t temp;
    temp = MCP79410_Read(handle, DAY);      //The 3rd bit of the RTCC_RTCC day register controls VBATEN

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
    
    return ((temp & VBATEN) == VBATEN) ? TRUE : FALSE;
}
 
void MCP79410_EnableVbat(MCP79410_Handle handle)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    uint8_t temp;
    temp = MCP79410_Read(handle, DAY);      //The 3rd bit of the RTCC_RTCC day register controls VBATEN
    temp = (temp | VBATEN);                 //Set 3rd bit to enable backup battery mode
    MCP79410_Write(handle, DAY, temp);      //Enable backup battery mode

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
}

void MCP79410_DisableVbat(MCP79410_Handle handle)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    uint8_t temp;
    temp = MCP79410_Read(handle, DAY);      //The 3rd bit of the RTCC_RTCC day register controls VBATEN
    temp = (temp & VBAT_DIS);               //Clear 3rd bit to disable backup battery mode
    MCP79410_Write(handle, DAY, temp);      //Enable backup battery mode

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
}

void MCP79410_GetPowerUpTime(MCP79410_Handle handle, RTCC_Struct *powerup_time)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    powerup_time->min   = MCP79410_bcd2dec(MCP79410_Read(handle, PWRUPMIN));
    powerup_time->hour  = MCP79410_bcd2dec(MCP79410_Read(handle, PWRUPHOUR));
    powerup_time->date  = MCP79410_bcd2dec(MCP79410_Read(handle, PWRUPDATE));
    powerup_time->month = MCP79410_bcd2dec(MCP79410_Read(handle, PWRUPMTH));

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
}

void MCP79410_GetPowerDownTime(MCP79410_Handle handle, RTCC_Struct *powerdown_time)
{
    IArg key;
    key = GateMutex_enter(GateMutex_handle(&(handle->gate)));

    powerdown_time->min   = MCP79410_bcd2dec(MCP79410_Read(handle, PWRDNMIN));
    powerdown_time->hour  = MCP79410_bcd2dec(MCP79410_Read(handle, PWRDNHOUR));
    powerdown_time->date  = MCP79410_bcd2dec(MCP79410_Read(handle, PWRDNDATE));
    powerdown_time->month = MCP79410_bcd2dec(MCP79410_Read(handle, PWRDNMTH));

    GateMutex_leave(GateMutex_handle(&(handle->gate)), key);
}

/*
 *  ======== Static Helper Functions ========
 */

uint8_t MCP79410_dec2bcd(uint8_t num)
{
  return ((num/10 * 16) + (num % 10));
}

uint8_t MCP79410_bcd2dec(uint8_t num)
{
  return ((num/16 * 10) + (num % 16));
}

bool MCP79410_Write(MCP79410_Handle handle, uint8_t rtcc_reg, uint8_t data)
{
    I2C_Transaction i2cTransaction;
    uint8_t txBuffer[2];
    uint8_t rxBuffer[2];

    txBuffer[0] = rtcc_reg;
    txBuffer[1] = data;

    /* Initialize master SPI transaction structure */
    i2cTransaction.slaveAddress = RTCC_WRITE;
    i2cTransaction.writeCount   = 2;
    i2cTransaction.writeBuf     = txBuffer;
    i2cTransaction.readCount    = 0;
    i2cTransaction.readBuf      = rxBuffer;

    /* Initiate SPI transfer */
    if (!I2C_transfer(handle->i2cHandle, &i2cTransaction))
    {
        System_printf("Unsuccessful I2C transfer\n");
        System_flush();
        return false;
    }

    return true;
}  

uint8_t MCP79410_Read(MCP79410_Handle handle, uint8_t rtcc_reg)
{
    I2C_Transaction i2cTransaction;
    uint8_t txBuffer[2];
    uint8_t rxBuffer[2];

    txBuffer[0] = rtcc_reg;
    rxBuffer[0] = 0;

    /* Write a dummy byte with address */
    i2cTransaction.slaveAddress = RTCC_READ;
    i2cTransaction.writeBuf     = &txBuffer;
    i2cTransaction.writeCount   = 1;
    i2cTransaction.readBuf      = &rxBuffer;
    i2cTransaction.readCount    = 1;

    if (!I2C_transfer(handle->i2cHandle, &i2cTransaction))
    {
        System_printf("Unsuccessful I2C transfer\n");
        System_flush();
    }

    return rxBuffer[0];
}

bool MCP79410_Probe(MCP79410_Handle handle)
{
    I2C_Transaction i2cTransaction;
    uint8_t txBuffer[2];
    uint8_t rxBuffer[2];

    /* Try to read the day of the week */
    txBuffer[0] = DAY;
    rxBuffer[0] = 0;

    /* Write a dummy byte with address */
    i2cTransaction.slaveAddress = RTCC_READ;
    i2cTransaction.writeBuf     = &txBuffer;
    i2cTransaction.writeCount   = 1;
    i2cTransaction.readBuf      = &rxBuffer;
    i2cTransaction.readCount    = 1;

    if (!I2C_transfer(handle->i2cHandle, &i2cTransaction))
        return FALSE;

    return TRUE;    //((mask & OSCRUN) == OSCRUN) ? TRUE : FALSE;
}

