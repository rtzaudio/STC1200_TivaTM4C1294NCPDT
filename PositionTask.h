/* ============================================================================
 *
 * STC-1200 Search/Timer/Comm Controller for Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2018, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 * ============================================================================
 */

#ifndef __POSITIONTASK_H
#define __POSITIONTASK_H

/*** CONSTANTS AND CONFIGURATION *******************************************/

/* The Ampex tape roller quadrature encoder wheel has 40 ppr. This gives
 * either 80 or 160 edges per revolution depending on the quadrature encoder
 * configuration set by QEIConfig(). Currently we use Cha-A mode which
 * gives 80 edges per revolution. If Cha-A/B mode is used this must be
 * set to 160.
 */
#define ROLLER_TICKS_PER_REV        80

/* This is the maximum signed position value we can have. Anything past
 * this is treated as a negative position value.
 */
#define MAX_ROLLER_POSITION			(0x7FFFFFFF - 1UL)

/*** MESSAGE STRUCTURES ****************************************************/

typedef enum LocateType {
	LOCATE_CANCEL=0,
    LOCATE_POSITION,
} LocateType;

typedef struct _LocateMessage {
    LocateType	command;
    uint32_t 	param1;
    uint32_t	param2;
} LocateMessage;

/*** FUNCTION PROTOTYPES ***************************************************/

int PTOI(uint32_t apos);

void PositionReset(void);

void CuePointStore(size_t index);
void CuePointClear(size_t index);

Void PositionTaskFxn(UArg arg0, UArg arg1);

#endif /* __POSITIONTASK_H */

