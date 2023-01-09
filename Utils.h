/***************************************************************************
 *
 * DTC-1200 & STC-1200 Digital Transport Controllers for
 * Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2020, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 ***************************************************************************/

#ifndef __UTILS_H
#define __UTILS_H

#include <time.h>

//*****************************************************************************
// Function Prototypes
//*****************************************************************************

char* FS_GetErrorStr(int errnum);
void FS_GetDateStr(uint16_t fsdate, char* buf, size_t bufsize);
void FS_GetTimeStr(uint16_t fstime, char* buf, size_t bufsize);
uint32_t FS_GetFatTime(void);

bool RTC_IsRunning(void);
bool RTC_GetDateTime(RTCC_Struct* ts);
bool RTC_SetDateTime(RTCC_Struct* ts);
void RTC_GetTimeStr(RTCC_Struct* ts, char *timestr);
void RTC_GetDateStr(RTCC_Struct* ts, char *datestr);
bool RTC_IsValidTime(RTCC_Struct* ts);
bool RTC_IsValidDate(RTCC_Struct* ts);

bool ReadGUIDS(I2C_Handle handle, uint8_t ui8SerialNumber[16], uint8_t ui8MAC[6]);

void InitSysDefaults(SYSCFG* p);
int SysParamsRead(SYSCFG* sp);
int SysParamsWrite(SYSCFG* sp);

int GetMACAddrStr(char* buf, uint8_t* mac);
int GetSerialNumStr(char* buf, uint8_t* mac);

#endif /* __UTILS_H */
