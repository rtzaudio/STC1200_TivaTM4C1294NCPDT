// =========================================================================
// STC1200TCP.h
// Copyright (C) 2019, RTZ Professional Audio, LLC
// All Rights Reserved
// =========================================================================

#pragma once
#pragma pack(push, 8)

#ifdef _WINDOWS

// Defines for Windows equivalent types
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

// =========================================================================
// STC state update message structure. This message streams from the STC
// to the TCP client to indicate the current transport time, the led/lamp
// states and other real time feedback information.
// =========================================================================

typedef struct _STC_STATE_MSG {
    uint32_t    length;                 /* size of this msg structure */
    TAPETIME    tapeTime;               /* current tape time position */
    uint32_t    errorCount;             /* QEI phase error count      */
    uint32_t    ledMaskButton;          /* DRC remote button LED mask */
    uint32_t    ledMaskTransport;       /* current transport LED mask */
    uint32_t    transportMode;          /* Current transport mode     */
    int32_t     tapeSpeed;              /* tape speed (15 or 30)      */
    int32_t     tapeDirection;          /* direction 1=fwd, 0=rew     */
    int32_t     tapePosition;           /* signed relative position   */
    int32_t     searchProgress;         /* search progress 0-100%     */
    int32_t     searching;              /* true if search in progress */
} STC_STATE_MSG;

/* The lower three bits of STC_STATE_MSG.transportMode are
 * the current transport mode. The upper bits are flags below.
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

#define STC_MODE_MASK       0x07        /* lower 3-bits indicate mode */

// =========================================================================
// STC Notification Bit Flags (MUST MATCH VALUES IN DRC1200 HEADERS!)
// =========================================================================

/* U7 is for transport control switches and button LEDs.
 * 5-bits for output LED's and 5-bits for pushbutton switch inputs.
 */

/* U7 PORT-A (Output) Transport Control Button LED's */
#define STC_L_REC           0x01            // REC button LED
#define STC_L_PLAY          0x02            // PLAY button LED
#define STC_L_REW           0x04            // REW button LED
#define STC_L_FWD           0x08            // FWD button LED
#define STC_L_STOP          0x10            // STOP button LED

/* U11 PORT-A (Output) LED Bits */
#define STC_L_LOC1          (0x01 << 0)     // LOC1 button LED
#define STC_L_LOC2          (0x02 << 0)     // LOC2 button LED
#define STC_L_LOC3          (0x04 << 0)     // LOC3 button LED
#define STC_L_LOC4          (0x08 << 0)     // LOC4 button LED
#define STC_L_LOC5          (0x10 << 0)     // LOC5 button LED
#define STC_L_LOC6          (0x20 << 0)     // LOC6 button LED
#define STC_L_LOC7          (0x40 << 0)     // LOC7 button LED
#define STC_L_LOC8          (0x80 << 0)     // LOC8 button LED

/* U11 PORT-B (Output) LED Bits */
#define STC_L_LOC0          (0x01 << 8)     // LOC0 button LED
#define STC_L_LOC9          (0x02 << 8)     // LOC9 button LED
#define STC_L_MENU          (0x04 << 8)     // SET button LED
#define STC_L_EDIT          (0x08 << 8)     // ESC button LED
#define STC_L_STORE         (0x10 << 8)     // PREV button LED
#define STC_L_ALT           (0x20 << 8)     // MENU button LED
#define STC_L_AUTO          (0x40 << 8)     // NEXT button LED
#define STC_L_CUE           (0x80 << 8)     // EDIT button LED

#define STC_L_LOC_MASK      (STC_L_LOC1|STC_L_LOC2|STC_L_LOC3|STC_L_LOC4| \
                             STC_L_LOC5|STC_L_LOC6|STC_L_LOC7|STC_L_LOC8| \
                             STC_L_LOC9|STC_L_LOC0)

// =========================================================================
// STC COMMAND/RESPONSE Messages
// =========================================================================

typedef struct _STC_COMMAND_HDR {
    uint16_t    hdrlen;				/* size of this msg structure */
    uint16_t    command;			/* the command ID to execute  */
    uint16_t    param0;				/* optional paramaters field  */
    uint16_t    param1;				/* optional paramaters field  */
	uint16_t    msglen;				/* trailing msg len, 0=none   */
} STC_COMMAND_HDR;

#define STC_CMD_STOP			1
#define STC_CMD_PLAY			2       /* param0 1=record */
#define STC_CMD_REW				3
#define STC_CMD_FWD				4
#define STC_CMD_LIFTER			5
#define STC_CMD_LOCATE			6       /* param0 1=autoplay, 2=autorec    */
#define STC_CMD_LOCATE_MODE		7       /* param0 0=cue-mode, 1=store-mode */

#pragma pack(pop)
