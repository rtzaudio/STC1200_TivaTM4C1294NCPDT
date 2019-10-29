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

#ifndef _LOCATETASK_H_
#define _LOCATETASK_H_

/* Locator velocities for various distances from the locate point */

#define JOG_VEL_FAR         0       /* 0 = use DTC default shuttle velocity */
#define JOG_VEL_MID         500     /* velocity for mid distance from locate point */
#define JOG_VEL_NEAR        240     /* velocity for near distance from locate point */

#define SHUTTLE_SLOW_VEL    240     /* slow velocity to use for locates */

/*** CUE POINT DATA STRUCTURE **********************************************/

/* Cue Point Memory Table Structure */
typedef struct _CUE_POINT {
    int32_t		ipos;		/* relative tape position cue */
    uint32_t	flags;      /* non-zero cue point active  */
} CUE_POINT;

/* Cue Point Flags */
#define CF_NONE             0x00    /* no cue point stored for location */
#define CF_ACTIVE           0x01    /* cue point available to search    */
#define CF_AUTO_PLAY        0x02    /* enter play mode after locate     */
#define CF_AUTO_REC         0x04    /* enter play + record after locate */

/* This defines the array size that holds all cue point memories. Note
 * one extra cue point is reserved in the buffer space at the end for
 * the transport deck search/cue buttons.
 */
#define MAX_CUE_POINTS      64

/* We support 64 cue points for the remote, but one extra cue point
 * memory is allocated for the search/cue buttons on the transport
 * deck. This cue point is independent from the remote cue points
 * and allows tape-op's running machine to use this for their own
 * purposes if desired. By default, this cue point is set to zero
 * to serve as RTZ until new cue point is stored vie the cue button.
 */
#define LAST_CUE_POINT      MAX_CUE_POINTS

//#define MAX_TRACKS          24

/*** MESSAGE STRUCTURES ****************************************************/

#define DIR_FWD		1
#define DIR_ZERO	0
#define DIR_REW		(-1)

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

uint32_t CuePointGet(size_t index, int* ipos);
void CuePointSet(size_t index, int ipos);
void CuePointClear(size_t index);
void CuePointClearAll(void);
void CuePointTimeGet(size_t index, TAPETIME* tapeTime);

Bool LocateCancel(void);
Bool LocateSearch(size_t cuePointIndex, uint32_t cue_flags);
Bool IsLocatorSearching(void);

Void LocateTaskFxn(UArg arg0, UArg arg1);

#endif /* _LOCATETASK_H_ */
