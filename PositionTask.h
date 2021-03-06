/* ============================================================================
 *
 * STC-1200 Search/Timer/Comm Controller for Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2020, RTZ Professional Audio, LLC
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
#define ROLLER_TICKS_PER_REV_F      80.0f

/* This is the diameter of the tape timer roller */
#define ROLLER_CIRCUMFERENCE_F      5.0014f

/* This is the maximum signed position value we can have. Anything past
 * this is treated as a negative position value.
 */
#define MAX_ROLLER_POSITION			(0x7FFFFFFF - 1UL)
#define MIN_ROLLER_POSITION			(-MAX_ROLLER_POSITION - 1)

/*** TAPE TIME/POSITION DATA ***********************************************/

/* Tape position time as h:m:s form. These values get transmitted
 * to the 7-segment ATMega88 display processor.
 */
typedef struct _TAPETIME {
    uint8_t     hour;       /* hour (0-1)      */
    uint8_t     mins;       /* minutes (0-59)  */
    uint8_t     secs;       /* seconds  (0-59) */
    uint8_t     tens;       /* tens secs (0-9) */
    uint8_t     frame;      /* smpte frame#    */
    uint8_t     flags;	    /* display flags   */
    uint16_t    align;      /* word alignment  */
} TAPETIME;

/* TAPETIME.flags */
#define F_PLUS		0x01	/* 7-seg plus segment, negative if clear */
#define F_BLINK		0x02	/* blink all seven segment displays      */
#define F_BLANK		0x80	/* blank the entire display if set       */

/*** FUNCTION PROTOTYPES ***************************************************/

void PositionZeroReset(void);
Void PositionTaskFxn(UArg arg0, UArg arg1);

/* Tape time helper functions */
void SecondsToTapeTime(float time, TAPETIME* p);
void TapeTimeToSeconds(TAPETIME* p, float* time);

void TapeTimeToPosition(TAPETIME* tapeTime, int* tapePosition);
void PositionToTapeTime(int tapePosition, TAPETIME* tapeTime);

/*** INLINE FUNCTIONS ******************************************************/

/* Converts encoder absolute position to relative signed position */

static inline int POSITION_TO_INT(uint32_t upos)
{
	/* Check for max position wrap around if negative position */
	if (upos >= (MAX_ROLLER_POSITION / 2))
		return (int)upos - (int)MAX_ROLLER_POSITION;

	return (int)upos;
}

/* This function calculates the distance in inches from a position */

static inline float POSITION_TO_INCHES(float pos)
{
	return ((pos / ROLLER_TICKS_PER_REV_F) * ROLLER_CIRCUMFERENCE_F);
}

static inline float INCHES_TO_POSITION(float inches)
{
    return ((inches / ROLLER_CIRCUMFERENCE_F) * ROLLER_TICKS_PER_REV_F);
}

#endif /* __POSITIONTASK_H */

