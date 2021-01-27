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

#define REMOTE_MODE_UNDEFINED   0
#define REMOTE_MODE_CUE         1
#define REMOTE_MODE_STORE       2
#define REMOTE_MODE_EDIT        3

#define SCREEN_TIME             0
#define SCREEN_MENU             1
#define SCREEN_ABOUT            2

#define LAST_SCREEN             1

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

#endif /* _REMOTETASK_H_ */
