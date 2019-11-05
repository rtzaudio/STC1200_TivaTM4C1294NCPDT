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

#define JOG_VEL_FAR         0       /* 0 = default shuttle velocity     */
#define JOG_VEL_MID         500     /* vel for mid distance search      */
#define JOG_VEL_NEAR        240     /* vel for near distance search     */

#define SHUTTLE_SLOW_VEL    240     /* slow velocity to use for locates */

/*** CUE POINT DATA STRUCTURE **********************************************/

/* Cue Point Memory Table Structure */
typedef struct _CUE_POINT {
    int32_t		ipos;		        /* relative tape position cue       */
    uint32_t	flags;              /* non-zero cue point active        */
} CUE_POINT;

/* Cue Point Flags */
#define CF_NONE             0x00    /* no cue point stored for location */
#define CF_ACTIVE           0x01    /* cue point available to search    */
#define CF_AUTO_PLAY        0x02    /* enter play mode after locate     */
#define CF_AUTO_REC         0x04    /* enter play + record after locate */

/* We support 10 cue points for the remote, but three extra cue memories
 * are reserved for system use. One of these holds the 'home' cue point
 * associated with the search/cue buttons on the transport deck. This is
 * a dedicated cue point for the machine operator and is associated with
 * the RTZ button on the software remote. The home cue point memory is
 * independent from the remote user cue point memories.
 */
#define USER_CUE_POINTS     10      /* locate buttons 0-9 cue points    */
#define SYS_CUE_POINTS      5       /* total system cue point memories  */
#define MAX_CUE_POINTS      (USER_CUE_POINTS + SYS_CUE_POINTS)

/* Two other cue point memories are reserved for the auto-locator
 * to define the loop start/end cue points for loop mode. These are
 * stored near the end of the cue point array memory along with the
 * home cue point memory.
 */
#define CUE_POINT_HOME      (MAX_CUE_POINTS - 1)
#define CUE_POINT_MARK_IN   (MAX_CUE_POINTS - 2)
#define CUE_POINT_MARK_OUT  (MAX_CUE_POINTS - 3)
#define CUE_POINT_PUNCH_IN  (MAX_CUE_POINTS - 4)
#define CUE_POINT_PUNCH_OUT (MAX_CUE_POINTS - 5)

/*** MESSAGE STRUCTURES ****************************************************/

#define DIR_FWD		1
#define DIR_ZERO	0
#define DIR_REW		(-1)

typedef enum LocateType {
    LOCATE_SEARCH=0,
    LOCATE_LOOP
} LocateType;

typedef struct _LocateMessage {
    LocateType	command;
    uint32_t 	param1;
    uint32_t	param2;
} LocateMessage;

/*** FUNCTION PROTOTYPES ***************************************************/

uint32_t CuePointGet(size_t index, int* ipos);
void CuePointSet(size_t index, int ipos, uint32_t cue_flags);
void CuePointClear(size_t index);
void CuePointClearAll(void);
void CuePointTimeGet(size_t index, TAPETIME* tapeTime);
Bool IsCuePointFlags(size_t index, uint32_t flags);

Bool LocateCancel(void);
Bool LocateSearch(size_t cuePointIndex, uint32_t cue_flags);
Bool LocateLoop(uint32_t cue_flags);
Bool IsLocatorSearching(void);
Bool IsLocatorAutoLoop(void);
Bool IsLocatorAutoPunch(void);
Bool IsLocating(void);

Void LocateTaskFxn(UArg arg0, UArg arg1);

#endif /* _LOCATETASK_H_ */
