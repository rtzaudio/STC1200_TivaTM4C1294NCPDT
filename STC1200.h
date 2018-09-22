/*
 * PMX42.h : created 5/18/2015
 *
 * Copyright (C) 2015, Robert E. Starr. ALL RIGHTS RESERVED.
 *
 * THIS MATERIAL CONTAINS  CONFIDENTIAL, PROPRIETARY AND TRADE
 * SECRET INFORMATION. NO DISCLOSURE OR USE OF ANY
 * PORTIONS OF THIS MATERIAL MAY BE MADE WITHOUT THE EXPRESS
 * WRITTEN CONSENT OF THE AUTHOR.
 */

#ifndef __STC1200_H
#define __STC1200_H

#include "PositionTask.h"
#include "LocateTask.h"

//*****************************************************************************
// CONSTANTS AND CONFIGURATION
//*****************************************************************************
/* version info */
#define FIRMWARE_VER        1           /* firmware version */
#define FIRMWARE_REV        0           /* firmware revision */

#define MAGIC               0xCEB0FACE  /* magic number for EEPROM data */
#define MAKEREV(v, r)       ((v << 16) | (r & 0xFFFF))

//*****************************************************************************
// GLOBAL SHARED MEMORY & REAL-TIME DATA
//*****************************************************************************

typedef struct _SYSDATA
{
    uint8_t		ui8SerialNumber[16];		/* unique serial number       */
    uint32_t	tapePositionAbs;			/* absolute tape position     */
    int32_t 	tapePosition;				/* signed relative position   */
    int32_t 	tapePositionPrev;			/* previous tape position     */
    int32_t 	tapeDirection;				/* direction 1=fwd, 0=rew     */
    uint32_t	qei_error_cnt;				/* QEI phase error count      */
    float		tapeTach;					/* tape speed from roller     */
	bool		searchCancel;
	bool        searching;                  /* true if search in progress */
    TAPETIME	tapeTime;					/* current tape time position */
    CUE_POINT	cuePoint[MAX_CUE_POINTS+1];	/* array of cue point data    */
} SYSDATA;

//*****************************************************************************
// SYSTEM RUN-TIME CONFIG PARAMETERS STORED IN EPROM
//*****************************************************************************

typedef struct _SYSPARMS
{
    uint32_t	magic;
    uint32_t	version;
    /*** GLOBAL PARAMETERS ***/
    uint32_t	debug;                     	/* debug level */
} SYSPARMS;

//*****************************************************************************
// Task Command Message Structure
//*****************************************************************************

typedef enum CommandType {
    SWITCHPRESS,
} CommandType;

typedef struct CommandMessage {
    CommandType		command;
    uint32_t 		param;
} CommandMessage;

//*****************************************************************************
// Function Prototypes
//*****************************************************************************

int main(void);
int ReadSerialNumber(uint8_t ui8SerialNumber[16]);
void EnableClockDivOutput(uint32_t div);
Void CommandTaskFxn(UArg arg0, UArg arg1);
void InitSysDefaults(SYSPARMS* p);
int SysParamsRead(SYSPARMS* sp);
int SysParamsWrite(SYSPARMS* sp);

#endif /* __STC1200_H */
