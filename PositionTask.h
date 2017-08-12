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

/* This is the maxium signed position value we can have. Anything past
 * this is treated as a negative position value.
 */
#define MAX_ROLLER_POSITION			0x7FFFFFFF

/*** FUNCTION PROTOTYPES ***************************************************/

Void PositionTaskFxn(UArg arg0, UArg arg1);

#endif /* __POSITIONTASK_H */
