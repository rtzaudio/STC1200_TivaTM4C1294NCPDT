/***************************************************************************
 *
 * DTC-1200 & STC-1200 Digital Transport Controllers for
 * Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2018, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 ***************************************************************************/

#ifndef __STC1200_H
#define __STC1200_H

#include "PositionTask.h"
#include "LocateTask.h"

//*****************************************************************************
// CONSTANTS AND CONFIGURATION
//*****************************************************************************
/* version info */
#define FIRMWARE_VER        1           /* firmware version */
#define FIRMWARE_REV        1           /* firmware revision */

#define MAGIC               0xCEB0FACE  /* magic number for EEPROM data */
#define MAKEREV(v, r)       ((v << 16) | (r & 0xFFFF))

//*****************************************************************************
// GLOBAL SHARED MEMORY & REAL-TIME DATA
//*****************************************************************************

typedef struct _SYSDATA
{
    uint8_t		ui8SerialNumber[16];		/* unique serial number       */
    uint32_t    ledMaskButton;              /* DRC remote button LED mask */
    /* Items below are updated from DTC notifications */
    uint32_t    ledMaskTransport;           /* current transport LED mask */
    uint32_t    transportMode;              /* Current transport mode     */
    /* These items  are internal to STC */
    uint32_t    tapeSpeed;                  /* tape speed (15 or 30)      */
    int32_t     tapeDirection;              /* direction 1=fwd, 0=rew     */
    uint32_t    tapePositionAbs;            /* absolute tape position     */
    int32_t 	tapePosition;				/* signed relative position   */
    int32_t 	tapePositionPrev;			/* previous tape position     */
    int32_t     searchProgress;             /* progress to cue (0-100%)   */
    uint32_t	qei_error_cnt;				/* QEI phase error count      */
    float		tapeTach;					/* tape speed from roller     */
	bool		searchCancel;
	bool        searching;                  /* true if search in progress */
    TAPETIME	tapeTime;					/* current tape time position */
    size_t      currentCueIndex;            /* currend cue table index    */
    size_t      currentCueBank;             /* current cue bank (1-8)     */
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
    bool        searchBlink;                /* blink 7-seg during search */
    bool        showLongTime;
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
