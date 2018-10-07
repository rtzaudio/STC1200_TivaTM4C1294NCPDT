/* ============================================================================
 *
 * STC-1200 Digital Transport Controller for Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2018, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 * ============================================================================ */

#ifndef _RAMPMESSAGE_H_
#define _RAMPMESSAGE_H_

/* ============================================================================
 * Message class types for IPCMSG.type
 * ============================================================================ */

#define MSG_TYPE_DISPLAY			10      /* display buffer message packet  */
#define MSG_TYPE_SWITCH             11
#define MSG_TYPE_LED                12

/* IPC_TYPE_DISPLAY Operation Codes */
#define OP_DISPLAY_REFRESH          100

/* IPC_TYPE_SWITCH Operation Codes */
#define OP_SWITCH_PRESS             200

/* ============================================================================
 * DRC Notification Bit Flags
 * ============================================================================ */

/* U7 PORT-B (Input) Transport Push Button Switch Bits */
#define SW_REC          0x01            // REC button switch
#define SW_PLAY         0x02            // PLAY button switch
#define SW_REW          0x04            // REW button switch
#define SW_FWD          0x08            // FWD button switch
#define SW_STOP         0x10            // STOP button switch

#endif  /* _RAMPMESSAGE_H_ */
