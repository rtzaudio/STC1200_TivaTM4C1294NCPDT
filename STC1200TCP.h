// =========================================================================
// STC1200TCP.h v1.02 01/04/2020
//
// STC-1200 Client/Server Network Packet Definitions for the software based
// version of the DRC digital remote control. 
//
// Developed by Robert E. Starr, Jr.
//
// Copyright (C) 2019, RTZ Professional Audio, LLC
//
// RTZ is a registered trademark of RTZ Professional Audio, LLC
// All Rights Reserved
// =========================================================================

#pragma once
#pragma pack(push, 8)

#include <stdint.h>

// =========================================================================
// STC Specific Constants
// =========================================================================

// The Ampex MM1200 tape roller quadrature encoder wheel has 40 ppr. This gives
// either 80 or 160 edges per revolution depending on the quadrature encoder
// configuration set by QEIConfig(). Currently we use Cha-A mode which
// gives 80 edges per revolution. If Cha-A/B mode is used this must be
// set to 160.

#define STC_ROLLER_TICKS_PER_REV        80
#define STC_ROLLER_TICKS_PER_REV_F      80.0f

 // This is the diameter of the tape timer roller
#define STC_ROLLER_CIRCUMFERENCE_F      5.0014f

// This is the maximum signed position value we can have. Anything past
// this is treated as a negative position value.

#define STC_MAX_ROLLER_POSITION			(0x7FFFFFFF - 1UL)
#define STC_MIN_ROLLER_POSITION			(-STC_MAX_ROLLER_POSITION - 1)

// =========================================================================
// General purpose time structure for tape position
// =========================================================================

#ifdef _WINDOWS
typedef struct _TAPETIME {
    uint8_t     hour;       /* hour (0-1)      */
    uint8_t     mins;       /* minutes (0-59)  */
    uint8_t     secs;       /* seconds  (0-59) */
    uint8_t     tens;       /* tens secs (0-9) */
    uint8_t     frame;      /* smpte frame#    */
    uint8_t     flags;      /* display flags   */
    uint16_t    align;      /* word alignment  */
} TAPETIME;

/* TAPETIME.flags */
#define F_TAPETIME_PLUS     0x01        /* 7-seg plus sign if set    */
#define F_TAPETIME_BLINK    0x02        /* blink all seven segments  */
#define F_TAPETIME_BLANK    0x80        /* blank the entire display  */
#endif

// =========================================================================
// TCP/IP Port Numbers for STC remote server
// =========================================================================

#define STC_PORT_STATE          1200    /* streaming transport state   */
#define STC_PORT_COMMAND        1201    /* transport cmd/response port */

/* Defines the maximum number of tracks supported by any machine.
 * Some machines may have less, like 16 or 8 track machines.
 */
#define STC_MAX_TRACKS          24      /* max number of audio tracks  */

/* We support 10 cue points for the remote, but three extra cue memories
 * are reserved for system use. One of these holds the 'home' cue point
 * associated with the search/cue buttons on the transport deck. This is
 * a dedicated cue point for the machine operator and is associated with
 * the RTZ button on the software remote. The home cue point memory is
 * independent from the remote user cue point memories.
 */
#define STC_USER_CUE_POINTS     10      /* locate buttons 0-9 cue points    */
#define STC_SYS_CUE_POINTS      5       /* total system cue point memories  */
#define STC_MAX_CUE_POINTS      (STC_USER_CUE_POINTS + STC_SYS_CUE_POINTS)

/* Two other cue point memories are reserved for the auto-locator
 * to define the loop start/end cue points for loop mode. These are
 * stored near the end of the cue point array memory along with the
 * home cue point memory.
 */
#define STC_CUE_POINT_HOME          (STC_MAX_CUE_POINTS - 1)
#define STC_CUE_POINT_MARK_IN       (STC_MAX_CUE_POINTS - 2)
#define STC_CUE_POINT_MARK_OUT      (STC_MAX_CUE_POINTS - 3)
#define STC_CUE_POINT_PUNCH_IN      (STC_MAX_CUE_POINTS - 4)
#define STC_CUE_POINT_PUNCH_OUT     (STC_MAX_CUE_POINTS - 5)

// =========================================================================
// STC state update message structure. This message streams from the STC
// to the TCP client to indicate the current transport time, the led/lamp
// states and other real time feedback information. State packets stream
// anytime there is tape motion, or any other transport state change 
// event occurs. This is a one way stream to the client and the STC-1200
// server does not attempt to receive any data back on this port stream.
// =========================================================================

typedef struct _STC_STATE_MSG {
    uint32_t    length;                 /* size of this msg structure */
    TAPETIME    tapeTime;               /* current tape time position */
    uint32_t    errorCount;             /* QEI phase error count      */
    uint32_t    ledMaskButton;          /* locater button LED mask    */
    uint32_t    ledMaskTransport;       /* transport button LED mask  */
    int32_t     tapePosition;           /* signed relative position   */
    uint32_t    tapeVelocity;           /* velocity of the tape       */
    uint16_t    transportMode;          /* Current transport mode     */
    int8_t      tapeDirection;          /* dir 1=fwd, 0=idle, -1=rew  */
    uint8_t     tapeSpeed;              /* tape speed (15 or 30)      */
    uint8_t     tapeSize;               /* tape size 1" or 2"         */
    uint8_t     searchProgress;         /* search progress 0-100%     */
    uint8_t     searching;              /* true if search in progress */
    uint8_t     monitorFlags;           /* monitor mode flags         */
    uint8_t     trackCount;
    uint8_t     reserved1;
    uint8_t     reserved2;
    uint8_t     reserved3;
    uint8_t     trackState[STC_MAX_TRACKS];
    uint8_t     cueState[STC_MAX_CUE_POINTS];
} STC_STATE_MSG;

/* The lower three bits of STC_STATE_MSG.transportMode are the
 * current transport mode. The upper bit flags are defined below.
 */
#define STC_MODE_HALT       0           /* all servo motion halted    */
#define STC_MODE_STOP       1           /* servo stop mode            */
#define STC_MODE_PLAY       2           /* servo play mode            */
#define STC_MODE_FWD        3           /* servo forward mode         */
#define STC_MODE_REW        4           /* servo rewind mode          */
#define STC_MODE_THREAD     5           /* tape thread mode in halt   */

/* Transport mode modifier bit flags */
#define STC_M_LIFTERS       0x0010      /* tape lifter engaged        */
#define STC_M_NOSLOW        0x0020      /* no auto slow shuttle mode  */
#define STC_M_LIBWIND       0x0040      /* shuttle library wind flag  */
#define STC_M_RECORD        0x0080      /* upper bit indicates record */
#define STC_M_SEARCH        0x0100      /* search active bit flag     */
#define STC_M_LOOP          0x0200      /* loop mode active bit flag  */
#define STC_M_PUNCH         0x0400      /* auto punch active bit flag */

#define STC_MODE_MASK       0x07        /* low 3-bits transport mode  */

/* The lower three bits of STC_STATE_MSG.trackState[n] define the current
 * state of a track (0=repro, 1=sync, 2=input). The upper bit is set
 * to indicate the track is armed and ready for record.
 */
#define STC_TRACK_REPRO     0           /* track is in repro mode     */
#define STC_TRACK_SYNC      1           /* track is in sync mode      */
#define STC_TRACK_INPUT     2           /* track is in input mode     */

#define STC_TRACK_MASK      0x07        /* low 3-bits are track mode  */

/* Upper bits indicate ready/record state */
#define STC_T_STANDBY       0x10        /* standby monitor active flag */
#define STC_T_MONITOR       0x20        /* standby monitor enable      */
#define STC_T_RECORD        0x40        /* track record active flag    */
#define STC_T_READY         0x80        /* track arm/ready for record  */

/* STC_STATE_MSG.cueState[n] cue memory state bit flags. These
 * indicate the state of the 10 locator buttons associated with 
 * the first ten memory locations.
 */
#define STC_CF_NONE         0x00        /* no cue point stored addr   */
#define STC_CF_ACTIVE       0x01        /* cue point available        */
#define STC_CF_AUTO_PLAY    0x02        /* auto-play after locate     */
#define STC_CF_AUTO_REC     0x04        /* auto-play+rec after locate */

// =========================================================================
// STC Notification Bit Flags (MUST MATCH VALUES IN DRC1200 HEADERS!)
// =========================================================================

/* U7 is for transport control switches and button LEDs.
 * 5-bits for output LED's and 5-bits for pushbutton switch inputs.
 */

/* Transport Control Button LED's (STC_STATE_MSG.ledMaskTransport) */
#define STC_L_REC           0x00000001      /* REC button LED  */
#define STC_L_PLAY          0x00000002      /* PLAY button LED */
#define STC_L_REW           0x00000004      /* REW button LED  */
#define STC_L_FWD           0x00000008      /* FWD button LED  */
#define STC_L_STOP          0x00000010      /* STOP button LED */
#define STC_L_LDEF          0x00000020      /* LIFT button LED */

/* Locator and Other Button LED's (STC_STATE_MSG.ledMaskButton) */
#define STC_L_LOC1          0x00000001      /* LOC1 button LED */
#define STC_L_LOC2          0x00000002      /* LOC2 button LED */
#define STC_L_LOC3          0x00000004      /* LOC3 button LED */
#define STC_L_LOC4          0x00000008      /* LOC4 button LED */
#define STC_L_LOC5          0x00000010      /* LOC5 button LED */
#define STC_L_LOC6          0x00000020      /* LOC6 button LED */
#define STC_L_LOC7          0x00000040      /* LOC7 button LED */
#define STC_L_LOC8          0x00000080      /* LOC8 button LED */
#define STC_L_LOC0          0x00000100      /* LOC0 button LED */
#define STC_L_LOC9          0x00000200      /* LOC9 button LED */
#define STC_L_MENU          0x00000400      /* SET button LED  */
#define STC_L_EDIT          0x00000800      /* ESC button LED  */
#define STC_L_STORE         0x00001000      /* PREV button LED */
#define STC_L_ALT           0x00002000      /* MENU button LED */
#define STC_L_AUTO          0x00004000      /* NEXT button LED */
#define STC_L_CUE           0x00008000      /* EDIT button LED */

/* The following bit flags are not supported by the DRC hardware remote
 * as it has less buttons than the DRCWIN application. So, we use the
 * upper 16-bits of the LED status flags for additional button flags.
 */
#define STC_L_AUTO_LOOP     0x00010000      /* auto-loop button       */
#define STC_L_MARK_IN       0x00020000      /* auto-loop mark-in btn  */
#define STC_L_MARK_OUT      0x00040000      /* auto-loop mark-out btn */

#define STC_L_AUTO_PUNCH    0x00080000      /* auto-punch button      */
#define STC_L_PUNCH_IN      0x00100000      /* auto-punch-in button   */
#define STC_L_PUNCH_OUT     0x00200000      /* auto-punch-out button  */

#define STC_L_LOC_MASK      (STC_L_LOC1|STC_L_LOC2|STC_L_LOC3|STC_L_LOC4| \
                             STC_L_LOC5|STC_L_LOC6|STC_L_LOC7|STC_L_LOC8| \
                             STC_L_LOC9|STC_L_LOC0)

// =========================================================================
// STC COMMAND/RESPONSE Messages
// =========================================================================

#define STC_ALL_TRACKS      ((uint32_t)(-1))
#define STC_ALL_CUEPOINTS   ((uint32_t)(-1))

typedef struct _STC_COMMAND_HDR {
    uint16_t    hdrlen;                 /* size of this msg structure */
    uint16_t    command;                /* the command ID to execute  */
    uint16_t    index;                  /* track or cue point index   */
    uint16_t    status;                 /* return status/error code   */
    union {
        uint32_t    U;
        int32_t     I;
        float       F;
    } param1;                           /* int, unsigned or float     */
    union {
        uint32_t    U;
        int32_t     I;
        float       F;
    }  param2;                          /* int, unsigned or float     */
    uint16_t    datalen;                /* trailing payload data len  */
} STC_COMMAND_HDR;

/*
 * Locator Command Message Types for 'STC_COMMAND_HDR.command'
 */

#define STC_CMD_STOP                1
#define STC_CMD_PLAY                2   /* param0 1=record */
#define STC_CMD_REW                 3
#define STC_CMD_FWD                 4
#define STC_CMD_LIFTER              5
#define STC_CMD_LOCATE              6   /* param1 1=autoplay, 2=autorec    */
#define STC_CMD_LOCATE_MODE_SET     8   /* param1 0=cue-mode, 1=store-mode */
#define STC_CMD_LOCATE_AUTO_LOOP    7   /* param1 1=autoplay, 2=autorec    */
#define STC_CMD_AUTO_PUNCH_SET      9
#define STC_CMD_AUTO_PUNCH_GET      10
#define STC_CMD_CUEPOINT_CLEAR      11  /* param1=index                    */
#define STC_CMD_CUEPOINT_STORE      12
#define STC_CMD_CUEPOINT_SET        13
#define STC_CMD_CUEPOINT_GET        14
#define STC_CMD_TRACK_TOGGLE_ALL    15  /* param1=mask to toggle           */
#define STC_CMD_TRACK_SET_STATE     16  /* param1=index, param2=flags      */
#define STC_CMD_TRACK_GET_STATE     17  /* param1=index, param2=flags      */
#define STC_CMD_TRACK_MASK_ALL      18  /* param1=setmask, param2=clrmask  */
#define STC_CMD_TRACK_MODE_ALL      19  /* param1=newmode, param2=0        */
#define STC_CMD_ZERO_RESET          20  /* param1=0, param2=0              */
#define STC_CMD_CANCEL              21  /* param1=0, param2=0              */
#define STC_CMD_TAPE_SPEED_SET      22  /* param1=30/15, param2=0          */
#define STC_CMD_CONFIG_SET          23  /* param1 0=load, 1=store, 2=reset */
#define STC_CMD_MONITOR             24  /* param1 0=off, 1=standby mon     */
#define STC_CMD_TRACK_GET_COUNT     25  /* number of tracks from DCS or 0  */

#pragma pack(pop)
