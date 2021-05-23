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

//*****************************************************************************
// Function Prototypes
//*****************************************************************************

bool ReadGUIDS(I2C_Handle handle, uint8_t ui8SerialNumber[16], uint8_t ui8MAC[6]);

void InitSysDefaults(SYSPARMS* p);
int SysParamsRead(SYSPARMS* sp);
int SysParamsWrite(SYSPARMS* sp);

int GetHexStr(char* textbuf, uint8_t* databuf, int datalen);

#endif /* __UTILS_H */
