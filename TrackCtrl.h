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

#ifndef _TRACKCTRL_H_
#define _TRACKCTRL_H_

/*** CONSTANTS *************************************************************/

#define MAX_TRACKS      24

/*** DATA STRUCTURES *******************************************************/

typedef struct TRACK_Params {
    uint8_t dummy;
} TRACK_Params;

typedef struct TRACK_Object {
    UART_Handle         uartHandle;
    GateMutex_Struct    gate;
    uint8_t             seqnum;
} TRACK_Object;

typedef TRACK_Object *TRACK_Handle;

/*** FUNCTION PROTOTYPES ***************************************************/

bool TRACK_Manager_startup(void);
bool TRACK_Manager_standby(bool enable);

TRACK_Handle TRACK_construct(TRACK_Object *obj, UART_Handle uartHandle,
                             TRACK_Params *params);

TRACK_Handle TRACK_create(UART_Handle uartHandle, TRACK_Params *params);

Void TRACK_delete(TRACK_Handle handle);
Void TRACK_destruct(TRACK_Handle handle);
Void TRACK_Params_init(TRACK_Params *params);

int TRACK_Command(TRACK_Handle handle,
                  DCS_IPCMSG_HDR* request, DCS_IPCMSG_HDR* reply);

bool Track_ApplyState(size_t track, uint8_t state);
bool Track_ApplyAllStates(uint8_t* trackStates);
bool Track_SetTapeSpeed(int speed);
bool Track_GetCount(uint32_t* count);
bool Track_SetState(size_t track, uint8_t trackState);
bool Track_GetState(size_t track, uint8_t* trackStates);
bool Track_SetAll(uint8_t mode, uint8_t flags);
bool Track_SetModeAll(uint8_t setmode);
bool Track_MaskAll(uint8_t setmask, uint8_t clearmask);
bool Track_ToggleMaskAll(uint8_t flags);
bool Track_StandbyTransfer(bool enable);

#endif /* _TRACKCTRL_H_ */
