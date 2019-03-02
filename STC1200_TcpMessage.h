/*
 * STC-1200 TCP Server Messages and Structures
 *
 * Copyright (C) 2016-2019, RTZ Professional Audio, LLC. ALL RIGHTS RESERVED.
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 */

#ifndef _STC1200_TCP_MSG_H_
#define _STC1200_TCP_MSG_H_

/*** IPC MESSAGE STRUCTURE *************************************************/

typedef struct _STC1200_TCP_MSG {
    uint16_t    type;           /* the IPC message type code   */
    uint16_t    opcode;         /* application defined op code */
    union {
        int32_t     I;
        uint32_t    U;
        float       F;
    } param1;                       /* unsigned or float param1 */
    union {
        int32_t     I;
        uint32_t    U;
        float       F;
    }  param2;                      /* unsigned or float param2 */
} STC1200_TCP_MSG;

#endif /* _STC1200_TCP_MSG_H_ */
