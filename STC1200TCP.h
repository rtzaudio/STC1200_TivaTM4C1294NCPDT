// =========================================================================
// STC1200TCP.h v1.01 10/21/2019
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

#ifdef _WINDOWS

// Defines for Windows equivalent types
#ifndef int8_t
#define int8_t		CHAR
#endif
#ifndef uint8_t
#define uint8_t		BYTE
#endif
#ifndef int16_t
#define int16_t		INT16
#endif
#ifndef uint16_t
#define uint16_t	UINT16
#endif
#ifndef int32_t
#define int32_t		INT32
#endif
#ifndef uint32_t
#define uint32_t	UINT32
#endif

// =========================================================================
// General purpose time structure for tape position
// =========================================================================

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
#define F_TAPETIME_PLUS		0x01	    /* 7-seg plus sign if set    */
#define F_TAPETIME_BLINK	0x02	    /* blink all seven segments  */
#define F_TAPETIME_BLANK	0x80	    /* blank the entire display  */

#endif /* _WINDOWS */

// =========================================================================
// TCP/IP Port Numbers for STC remote server
// =========================================================================

#define STC_PORT_STATE      1200        /* streaming transport state   */
#define STC_PORT_COMMAND    1201        /* transport cmd/response port */

/* Defines the maximum number of tracks supported by any machine.
 * Some machines may have less, like 16 or 8 track machines.
 */
#define STC_MAX_TRACKS		24			/* max number of audio tracks  */

/* This defines the maximum number of cue point memories. Note
 * one extra cue point is reserved in the buffer space at the end for
 * the transport deck search/cue buttons.
 */
#define STC_MAX_CUE_POINTS	64

// The home cue point memory for the cue/search buttons on the transport
#define STC_HOME_CUE_POINT	STC_MAX_CUE_POINTS

/* The STC actually has 64 cue point memories, but only 
 * ten of these are used as they are directly associated to 
 * single locator buttons for quick access.
 */
#define STC_MAX_CUES		10

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
    uint32_t    ledMaskButton;          /* DRC remote button LED mask */
    uint32_t    ledMaskTransport;       /* current transport LED mask */
    int32_t     tapePosition;           /* signed relative position   */
	uint32_t	tapeVelocity;			/* velocity of the tape       */
    uint16_t    transportMode;          /* Current transport mode     */
    int8_t      tapeDirection;          /* dir 1=fwd, 0=idle, -1=rew  */
    uint8_t     tapeSpeed;              /* tape speed (15 or 30)      */
    uint8_t     searchProgress;         /* search progress 0-100%     */
    uint8_t     searching;              /* true if search in progress */
	uint8_t		monitorFlags;			/* monitor mode flags         */
	uint8_t		trackState[STC_MAX_TRACKS];
	uint8_t		cueState[STC_MAX_CUES];
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

#define STC_MODE_MASK       0x07        /* low 3-bits transport mode  */

/* The lower three bits of STC_STATE_MSG.trackState[n] define the current
 * state of a track (input=0, 1=repro, 2=sync). The upper bit is set
 * to indicate the track is armed and ready for record.
 */
#define STC_TRACK_INPUT		0			/* track is in input mode     */
#define STC_TRACK_REPRO		1			/* track is in repro mode     */
#define STC_TRACK_SYNC		2			/* track is in sync mode      */

#define STC_TRACK_MASK      0x07        /* low 3-bits are track mode  */

/* Upper bits indicate ready/record state */
#define STC_T_RECORD		0x40		/* track is recording now     */
#define STC_T_READY			0x80		/* track is armed for record  */

/* STC_STATE_MSG.cueState[n] cue memory state bit flags. These
 * indicate the state of the 10 locator buttons associated with 
 * the first ten memory locations.
 */
#define STC_CF_NONE         0x00		/* no cue point stored addr   */
#define STC_CF_ACTIVE       0x01		/* cue point available        */
#define STC_CF_AUTO_PLAY    0x02		/* auto-play after locate     */
#define STC_CF_AUTO_REC     0x04		/* auto-play+rec after locate */

// =========================================================================
// STC Notification Bit Flags (MUST MATCH VALUES IN DRC1200 HEADERS!)
// =========================================================================

/* U7 is for transport control switches and button LEDs.
 * 5-bits for output LED's and 5-bits for pushbutton switch inputs.
 */

/* Transport Control Button LED's */
#define STC_L_REC           0x0001      /* REC button LED  */
#define STC_L_PLAY          0x0002      /* PLAY button LED */
#define STC_L_REW           0x0004      /* REW button LED  */
#define STC_L_FWD           0x0008      /* FWD button LED  */
#define STC_L_STOP          0x0010      /* STOP button LED */

/* Locator and Other Button LED's */
#define STC_L_LOC1          0x0001		/* LOC1 button LED */
#define STC_L_LOC2          0x0002		/* LOC2 button LED */
#define STC_L_LOC3          0x0004		/* LOC3 button LED */
#define STC_L_LOC4          0x0008		/* LOC4 button LED */
#define STC_L_LOC5          0x0010		/* LOC5 button LED */
#define STC_L_LOC6          0x0020		/* LOC6 button LED */
#define STC_L_LOC7          0x0040		/* LOC7 button LED */
#define STC_L_LOC8          0x0080		/* LOC8 button LED */
#define STC_L_LOC0          0x0100		/* LOC0 button LED */
#define STC_L_LOC9          0x0200		/* LOC9 button LED */
#define STC_L_MENU          0x0400		/* SET button LED  */
#define STC_L_EDIT          0x0800		/* ESC button LED  */
#define STC_L_STORE         0x1000		/* PREV button LED */
#define STC_L_ALT           0x2000		/* MENU button LED */
#define STC_L_AUTO          0x4000		/* NEXT button LED */
#define STC_L_CUE           0x8000		/* EDIT button LED */

#define STC_L_LOC_MASK      (STC_L_LOC1|STC_L_LOC2|STC_L_LOC3|STC_L_LOC4| \
                             STC_L_LOC5|STC_L_LOC6|STC_L_LOC7|STC_L_LOC8| \
                             STC_L_LOC9|STC_L_LOC0)

// =========================================================================
// STC COMMAND/RESPONSE Messages
// =========================================================================

typedef struct _STC_COMMAND_HDR {
    uint16_t    hdrlen;					/* size of this msg structure */
    uint16_t    command;				/* the command ID to execute  */
    uint16_t    param1;					/* optional paramaters field  */
    uint16_t    param2;					/* optional paramaters field  */
	uint16_t    msglen;					/* trailing msg len, 0=none   */
} STC_COMMAND_HDR;

#define STC_CMD_STOP			1
#define STC_CMD_PLAY			2       /* param0 1=record */
#define STC_CMD_REW				3
#define STC_CMD_FWD				4
#define STC_CMD_LIFTER			5
#define STC_CMD_LOCATE			6       /* param0 1=autoplay, 2=autorec    */
#define STC_CMD_LOCATE_MODE		7       /* param0 0=cue-mode, 1=store-mode */
#define STC_CMD_LOCATE_CLEAR	8       /* param0 0=index                  */

#pragma pack(pop)
