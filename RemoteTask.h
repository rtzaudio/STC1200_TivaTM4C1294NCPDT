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

/* Display Screen Types */
typedef enum ScreenNumberType {
    SCREEN_TIME,
    SCREEN_MENU,
    SCREEN_ABOUT,
    SCREEN_TRACK_ASSIGN,
    SCREEN_LAST
} ScreenNumberType;

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
void DrawAbout(void);
void DrawMenu(void);
void DrawTapeTime(void);
void DrawTrackAssign(void);

#endif /* _REMOTETASK_H_ */
