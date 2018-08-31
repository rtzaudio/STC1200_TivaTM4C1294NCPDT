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

#ifndef __LOCATETASK_H
#define __LOCATETASK_H

/*** CUE POINT DATA STRUCTURE **********************************************/

typedef struct _CUE_POINT {
    int32_t		ipos;		/* relative tape position */
    uint32_t	flags;
} CUE_POINT;

#define MAX_CUE_POINTS		64

/*** MESSAGE STRUCTURES ****************************************************/

typedef enum LocateType {
	LOCATE_CANCEL=0,
    LOCATE_SEARCH,
} LocateType;

typedef struct _LocateMessage {
    LocateType	command;
    uint32_t 	param1;
    uint32_t	param2;
} LocateMessage;

/*** FUNCTION PROTOTYPES ***************************************************/

void CuePointStore(size_t index);
void CuePointClear(size_t index);

Void LocateTaskFxn(UArg arg0, UArg arg1);

#endif /* __LOCATETASK_H */

