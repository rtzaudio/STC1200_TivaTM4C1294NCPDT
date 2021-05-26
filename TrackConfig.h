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

#ifndef _TRACKCONFIG_H_
#define _TRACKCONFIG_H_

#include "..\DCS1200_TivaTM4C123AE6PM2\IPCDCS.h"

/*** CONSTANTS *************************************************************/

#define MAX_TRACKS      24

/*** DATA STRUCTURES *******************************************************/

typedef struct TRACK_Params {
    uint8_t dummy;
} TRACK_Params;

typedef struct TRACK_Object {
    UART_Handle         uartHandle;
    GateMutex_Struct    gate;
} TRACK_Object;

typedef TRACK_Object *TRACK_Handle;

/*** FUNCTION PROTOTYPES ***************************************************/

TRACK_Handle TRACK_construct(TRACK_Object *obj, UART_Handle uartHandle,
                             TRACK_Params *params);

TRACK_Handle TRACK_create(UART_Handle uartHandle, TRACK_Params *params);

Void TRACK_delete(TRACK_Handle handle);
Void TRACK_destruct(TRACK_Handle handle);
Void TRACK_Params_init(TRACK_Params *params);

int TRACK_Command(TRACK_Handle handle,
                  DCS_IPCMSG_HDR* request, DCS_IPCMSG_HDR* reply);

int TRACK_SetAllStates(TRACK_Handle handle);

bool Track_SetState(size_t track, uint8_t mode, uint8_t flags);
bool Track_GetState(size_t track, uint8_t* modeflags);
bool Track_SetAll(uint8_t mode, uint8_t flags);
bool Track_MaskAll(uint8_t setmask, uint8_t clearmask);
bool Track_ModeAll(uint8_t setmode);

#endif /* _TRACKCONFIG_H_ */
