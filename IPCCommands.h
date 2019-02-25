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

#ifndef _IPCCOMMANDS_H_
#define _IPCCOMMANDS_H_

/*** FUNCTION PROTOTYPES ***************************************************/

Bool Transport_Stop(void);
Bool Transport_Play(uint32_t flags);
Bool Transport_Fwd(uint32_t velocity, uint32_t flags);
Bool Transport_Rew(uint32_t velocity, uint32_t flags);
Bool Transport_GetMode(uint32_t* mode, uint32_t* speed);

Bool Config_SetShuttleVelocity(uint32_t velocity);
Bool Config_GetShuttleVelocity(uint32_t* velocity);

#endif /* _IPCCOMMANDS_H_ */
