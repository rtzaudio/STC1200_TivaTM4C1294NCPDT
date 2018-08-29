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

/* Converts encoder absolute position to relative signed position */

static inline int POSITION_TO_INT(uint32_t apos)
{
	int ipos;

	/* Check for max position wrap around if negative position */

	if (apos >= ((MAX_ROLLER_POSITION / 2) + 0))
		ipos = (int)apos - (int)MAX_ROLLER_POSITION;
	else
		ipos = (int)apos;

	return ipos;
}

/*** TAPE TIME/POSITION DATA ***********************************************/

typedef struct _TAPETIME {
    uint8_t hour;   /* hour    */
    uint8_t mins;   /* minutes */
    uint8_t secs;   /* seconds */
    uint8_t flags;	/* flags   */
} TAPETIME;

/* TAPETIME.flags */
#define F_PLUS		0x01	/* 7-seg plus segment, negative if clear */
#define F_BLINK		0x02	/* blink all seven segment displays      */
#define F_BLANK		0x80	/* blank the entire display if set       */

/*** FUNCTION PROTOTYPES ***************************************************/

int PTOI(uint32_t apos);

void PositionReset(void);

Void PositionTaskFxn(UArg arg0, UArg arg1);

#endif /* __POSITIONTASK_H */

