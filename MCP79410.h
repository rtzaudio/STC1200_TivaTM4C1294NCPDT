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

#ifndef __MCP79410_H__
#define __MCP79410_H__

#include <stdint.h>
#include <stdbool.h>

#include <ti/drivers/I2C.h>
#include <ti/sysbios/gates/GateMutex.h>

/************************* RTCC Memory map ****************************/

#define  EEPROM_WRITE   (0xAE >> 1) //  DEVICE ADDR for EEPROM (writes)
#define  EEPROM_READ    (0xAF >> 1) //  DEVICE ADDR for EEPROM (reads)
#define  RTCC_WRITE     (0xDE >> 1) //  DEVICE ADDR for RTCC MCHP  (writes)
#define  RTCC_READ      (0xDF >> 1) //  DEVICE ADDR for RTCC MCHP  (reads)

#define  SRAM_PTR       0x20        //  pointer of the SRAM area (RTCC)
#define  EEPROM_SR      0xFF        //  STATUS REGISTER in the  EEPROM

#define  SEC            0x00        //  address of SECONDS      register
#define  MIN            0x01        //  address of MINUTES      register
#define  HOUR           0x02        //  address of HOURS        register
#define  DAY            0x03        //  address of DAY OF WK    register
#define  STAT           0x03        //  address of STATUS       register
#define  DATE           0x04        //  address of DATE         register
#define  MNTH           0x05        //  address of MONTH        register
#define  YEAR           0x06        //  address of YEAR         register
#define  CTRL           0x07        //  address of CONTROL      register
#define  CAL            0x08        //  address of CALIB        register
#define  ULID           0x09        //  address of UNLOCK ID    register

#define  ALM0SEC        0x0A        //  address of ALARM0 SEC   register
#define  ALM0MIN        0x0B        //  address of ALARM0 MIN   register
#define  ALM0HR         0x0C        //  address of ALARM0 HOUR  register
#define  ALM0WDAY       0x0D        //  address of ALARM0 CONTR register
#define  ALM0DATE       0x0E        //  address of ALARM0 DATE  register
#define  ALM0MTH        0x0F        //  address of ALARM0 MONTH register

#define  ALM1SEC        0x11        //  address of ALARM1 SEC   register
#define  ALM1MIN        0x12        //  address of ALARM1 MIN   register
#define  ALM1HR         0x13        //  address of ALARM1 HOUR  register
#define  ALM1WDAY       0x14        //  address of ALARM1 CONTR register
#define  ALM1DATE       0x15        //  address of ALARM1 DATE  register
#define  ALM1MTH        0x16        //  address of ALARM1 MONTH register

#define  PWRDNMIN       0x18        //  address of T_SAVER MIN(VDD->BAT)
#define  PWRDNHOUR      0x19        //  address of T_SAVER HR (VDD->BAT)
#define  PWRDNDATE      0x1A        //  address of T_SAVER DAT(VDD->BAT)
#define  PWRDNMTH       0x1B        //  address of T_SAVER MTH(VDD->BAT)

#define  PWRUPMIN       0x1C        //  address of T_SAVER MIN(BAT->VDD)
#define  PWRUPHOUR      0x1D        //  address of T_SAVER HR (BAT->VDD)
#define  PWRUPDATE      0x1E        //  address of T_SAVER DAT(BAT->VDD)
#define  PWRUPMTH       0x1F        //  address of T_SAVER MTH(BAT->VDD)

/************************GLOBAL CONSTANTS RTCC - INITIALIZATION****************/

#define  PM             0x20        //  post-meridian bit (HOUR)
#define  HOUR_FORMAT    0x40        //  Hour format
#define  OUT_PIN        0x80        //  = b7 (CTRL)
#define  SQWEN          0x40        //  SQWE = b6 (CTRL)
#define  ALM_NO         0x00        //  no alarm activated        (CTRL)
#define  ALM_0          0x10        //  ALARM0 is       activated (CTRL)
#define  ALM_1          0x20        //  ALARM1 is       activated (CTRL)
#define  ALM_01         0x30        //  both alarms are activated (CTRL)
#define  MFP_01H        0x00        //  MFP = SQVAW(01 HERZ)      (CTRL)
#define  MFP_04K        0x01        //  MFP = SQVAW(04 KHZ)       (CTRL)
#define  MFP_08K        0x02        //  MFP = SQVAW(08 KHZ)       (CTRL)
#define  MFP_32K        0x03        //  MFP = SQVAW(32 KHZ)       (CTRL)
#define  MFP_64H        0x04        //  MFP = SQVAW(64 HERZ)      (CTRL)
#define  ALMx_POL       0x80        //  polarity of MFP on alarm  (ALMxCTL)
#define  ALMxC_SEC      0x00        //  ALARM compare on SEC      (ALMxCTL)
#define  ALMxC_MIN      0x10        //  ALARM compare on MIN      (ALMxCTL)
#define  ALMxC_HR       0x20        //  ALARM compare on HOUR     (ALMxCTL)
#define  ALMxC_DAY      0x30        //  ALARM compare on DAY      (ALMxCTL)
#define  ALMxC_DAT      0x40        //  ALARM compare on DATE     (ALMxCTL)
#define  ALMxC_ALL      0x70        //  ALARM compare on all param(ALMxCTL)
#define  ALMx_IF        0x08        //  MASK of the ALARM_IF      (ALMxCTL)

#define  OSCRUN         0x20        //  state of the oscillator(running or not)
#define  PWRFAIL        0x10
#define  VBATEN         0x08        //  enable battery for back-up
#define  VBAT_DIS       0x37        //  disable battery back-up

#define  START_32KHZ    0x80        //  start crystal: ST = b7 (SEC)
#define  LP             0x20        //  mask for the leap year bit(MONTH REG)
#define  HOUR_12        0x40        //  12 hours format   (HOUR)

#define  LPYR           0x20


/********************************************************************************/
#define ALM1MSK2        0x40
#define ALM1MSK1        0x20
#define ALM1MSK0        0x10

#define ALM0MSK2        0x40
#define ALM0MSK1        0x20
#define ALM0MSK0        0x10

/*********************************************************************************/

typedef struct _RTCC_Struct
{
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t weekday;
    uint8_t date;
    uint8_t month;
    uint8_t year;
} RTCC_Struct;

typedef enum Alarm {ZERO = 0, ONE} Alarm_t;

typedef enum AlarmStatus {NOT_SET = 0, SET} AlarmStatus_t;

typedef enum PMAM {AMT = 0, PMT} PMAM_t;

typedef enum Format {H24 = 0, H12} Format_t;

typedef enum Match {SECONDS_MATCH = 0, MINUTES_MATCH, HOURS_MATCH, WEEKDAY_MATCH, DATE_MATCH, FULL_DATE_MATCH } Match_t;

typedef enum MFP_MODE {GPO = 0, ALARM_INTERRUPT, SQUARE_WAVE} MFP_t;

typedef enum MFP_POL {LOWPOL = 0, HIGHPOL} Polarity_t;

#define TRUE    1
#define FALSE   0

/***************************Function definitions********************************************/

/*!
 *  @brief MCP79410 Parameters
 *
 *  This is a place-holder structure now since there are no parameters
 *  for the create/construct calls.
 *
 *  @sa         MCP79410_Params_init()
 */
typedef struct MCP79410_Params {
    uint8_t dummy;
} MCP79410_Params;

/*!
 *  @brief MCP79410 Transaction Structure
 *
 *  This structure is used to describe the data to read/write.
 */
typedef struct MCP79410_Transaction {
    uint8_t *data;       /*!< data pointer to read or write       */
    uint32_t data_size;  /*!< size of data to read or write       */
    uint32_t byte;       /*!< byte offset in memory to read/write */
} MCP79410_Transaction;

/*!
 *  @brief MCP79410 Object
 *
 *  The application should never directly access the fields in the structure.
 */
typedef struct MCP79410_Object {
    I2C_Handle          i2cHandle;
    GateMutex_Struct    gate;
} MCP79410_Object;

/*!
 *  @brief MCP79410 Handle
 *
 *  Used to identify a AD45DB device in the APIs
 */
typedef MCP79410_Object *MCP79410_Handle;

/*!
 *  @brief  Function to initialize a given MCP79410 object
 *
 *  Function to initialize a given MCP79410 object specified by the
 *  particular I2C handle and GPIO CS index values.
 *
 *  @param  obj           Pointer to a MCP79410_Object structure. It does not
 *                        need to be initialized.
 *
 *  @param  params        Pointer to an parameter block, if NULL it will use
 *                        default values. All the fields in this structure are
 *                        RO (read-only).
 *
 *  @return A MCP79410_Handle on success or a NULL on an error.
 *
   @sa     MCP79410_destruct()
 */
MCP79410_Handle MCP79410_construct(MCP79410_Object *obj, I2C_Handle i2cHandle, MCP79410_Params *params);

/*!
 *  @brief  Function to initialize a given MCP79410 device
 *
 *  Function to create a MCP79410 object specified by the
 *  particular I2C handle and GPIO CS index values.
 *
 *  @param  i2cHandle     I2C handle that the MCP79410 is attached to
 *
 *  @param  params        Pointer to an parameter block, if NULL it will use
 *                        default values. All the fields in this structure are
 *                        RO (read-only).
 *
 *  @return A MCP79410_Handle on success or a NULL on an error.
 *
   @sa     MCP79410_delete()
 */
MCP79410_Handle MCP79410_create(I2C_Handle i2cHandle, MCP79410_Params *params);

/*!
 *  @brief  Function to delete a MCP79410 instance
 *
 *  @pre    MCP79410_create() had to be called first.
 *
 *  @param  handle      A MCP79410_Handle returned from MCP79410_create
 *
 *  @sa     MCP79410_create()
 */
void MCP79410_delete(MCP79410_Handle handle);

Void MCP79410_Params_init(MCP79410_Params *params);
void MCP79410_Initialize(MCP79410_Handle handle, RTCC_Struct *time, Format_t format);
void MCP79410_EnableOscillator(MCP79410_Handle handle);
void MCP79410_DisableOscillator(MCP79410_Handle handle);
uint8_t MCP79410_IsRunning(MCP79410_Handle handle);
bool MCP79410_Probe(MCP79410_Handle handle);

void MCP79410_GetTime(MCP79410_Handle handle, RTCC_Struct *current_time);
void MCP79410_SetTime(MCP79410_Handle handle, RTCC_Struct *time);
void MCP79410_SetHourFormat(MCP79410_Handle handle, Format_t format);
void MCP79410_SetPMAM(MCP79410_Handle handle, PMAM_t meridian);

void MCP79410_EnableAlarm(MCP79410_Handle handle, Alarm_t alarm);
void MCP79410_DisableAlarm(MCP79410_Handle handle, Alarm_t alarm);
AlarmStatus_t MCP79410_GetAlarmStatus(MCP79410_Handle handle, Alarm_t alarm);
void MCP79410_ClearInterruptFlag(MCP79410_Handle handle, Alarm_t alarm);
void MCP79410_SetAlarmTime(MCP79410_Handle handle, RTCC_Struct *time, Alarm_t alarm);
void MCP79410_SetAlarmMFPPolarity(MCP79410_Handle handle, Polarity_t MFP_pol,Alarm_t alarm);
void MCP79410_SetAlarmMatch(MCP79410_Handle handle, Match_t match,Alarm_t alarm);
void MCP79410_SetMFP_Functionality(MCP79410_Handle handle, MFP_t mode);
void MCP79410_SetMFP_GPOStatus(MCP79410_Handle handle, Polarity_t status);

uint8_t MCP79410_CheckPowerFailure(MCP79410_Handle handle);
uint8_t MCP79410_IsVbatEnabled(MCP79410_Handle handle);
void MCP79410_EnableVbat(MCP79410_Handle handle);
void MCP79410_DisableVbat(MCP79410_Handle handle);
void MCP79410_GetPowerUpTime(MCP79410_Handle handle, RTCC_Struct *powerup_time);
void MCP79410_GetPowerDownTime(MCP79410_Handle handle, RTCC_Struct *powerdown_time);

#endif
