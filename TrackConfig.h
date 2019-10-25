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

#ifndef _TRACKCONFIG_H_
#define _TRACKCONFIG_H_

/*** CONSTANTS *************************************************************/

#define MAX_TRACKS      24

/*** FUNCTION PROTOTYPES ***************************************************/

bool Track_SetState(size_t track, uint8_t mode, uint8_t flags);
bool Track_GetState(size_t track, uint8_t* modeflags);
bool Track_SetAll(uint8_t mode, uint8_t flags);
bool Track_ClearAll(void);

#endif /* _TRACKCONFIG_H_ */
