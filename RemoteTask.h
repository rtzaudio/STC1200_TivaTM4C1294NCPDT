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

#ifndef _REMOTETASK_H_
#define _REMOTETASK_H_

/*** CONSTANTS *************************************************************/

/* Remote Modes of Operation */
typedef enum RemoteModesType {
    REMOTE_MODE_UNDEFINED,
    REMOTE_MODE_CUE,
    REMOTE_MODE_STORE,
    REMOTE_MODE_EDIT,
    REMOTE_MODE_LAST
} RemoteModesType;

/* Display View Types */
typedef enum ViewNumberType {
    VIEW_TAPE_TIME,
    VIEW_TRACK_ASSIGN,
    VIEW_TRACK_SET_ALL,
    VIEW_INFO,
    VIEW_LAST
} ViewNumberType;

/* Display View Types */
typedef enum FieldNumberType {
    FIELD_TRACK_NUM,
    FIELD_TRACK_ARM,
    FIELD_TRACK_MODE,
    FIELD_TRACK_MONITOR,
    FIELD_LAST
} FieldNumberType;

/* Edit Time States */
typedef enum EditStateType {
    EDIT_BEGIN,
    EDIT_DIGITS,
} EditStateType;

/*** FUNCTION PROTOTYPES ***************************************************/

Bool Remote_Task_startup();
void SetLocateButtonLED(size_t index);
void SetButtonLedMask(uint32_t setMask, uint32_t clearMask);
uint32_t xlate_to_dtc_transport_switch_mask(uint32_t mask);

void Remote_PostSwitchPress(uint32_t mode, uint32_t flags);

/* RemoteDisplay.c */
void ClearDisplay(void);
void DrawScreen(uint32_t uScreenNum);

#endif /* _REMOTETASK_H_ */
