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

// General purpose time structure for tape position

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
// STC state update message structure. This message streams from the STC
// to the TCP client to indicate the current transport time, the led/lamp
// states and other real time feedback information.
// =========================================================================

typedef struct _STC_STATE_MSG {
    uint32_t    length;                 /* size of this msg structure */
    TAPETIME    tapeTime;               /* current tape time position */
    uint32_t    ledMaskButton;          /* DRC remote button LED mask */
    uint32_t    ledMaskTransport;       /* current transport LED mask */
    uint32_t    transportMode;          /* Current transport mode     */
    uint32_t    tapeSpeed;              /* tape speed (15 or 30)      */
    int32_t     tapeDirection;          /* direction 1=fwd, 0=rew     */
    int32_t     tapePosition;           /* signed relative position   */
    int32_t     searchProgress;         /* search progress 0-100%     */
} STC_STATE_MSG;

/* The lower three bits of STC_STATE_MSG.transportMode are
 * the current transport mode. The upper bits are flags below.
 */
#define STC_MODE_HALT       0           /* all servo motion halted    */
#define STC_MODE_STOP       1           /* servo stop mode            */
#define STC_MODE_PLAY       2           /* servo play mode            */
#define STC_MODE_FWD        3           /* servo forward mode         */
#define STC_MODE_REW        4           /* servo rewind mode          */

 /* Transport mode modifier bit flags */
#define STC_M_NOSLOW        0x0020      /* no auto slow shuttle mode  */
#define STC_M_LIBWIND       0x0040      /* shuttle library wind flag  */
#define STC_M_RECORD        0x0080      /* upper bit indicates record */
#define STC_M_SEARCH        0x0100      /* search active bit flag     */

#define STC_MODE_MASK       0x07

// =========================================================================
// DRC Notification Bit Flags (MUST MATCH VALUES IN DRC1200 HEADERS!)
// =========================================================================

#if 0
/*
 * U7 is for transport control switches and button LEDs.
 * 5-bits for output LED's and 5-bits for pushbutton switch inputs.
 */

/* U7 PORT-A (Output) Transport Control Button LED's */
#define L_REC           0x01            // REC button LED
#define L_PLAY          0x02            // PLAY button LED
#define L_REW           0x04            // REW button LED
#define L_FWD           0x08            // FWD button LED
#define L_STOP          0x10            // STOP button LED

/* U7 PORT-B (Input) Transport Push Button Switch Bits */
#define SW_REC          0x01            // REC button switch
#define SW_PLAY         0x02            // PLAY button switch
#define SW_REW          0x04            // REW button switch
#define SW_FWD          0x08            // FWD button switch
#define SW_STOP         0x10            // STOP button switch

/* U11 PORT-A (Output) LED Bits */
#define L_LOC1          (0x01 << 0)     // LOC1 button LED
#define L_LOC2          (0x02 << 0)     // LOC2 button LED
#define L_LOC3          (0x04 << 0)     // LOC3 button LED
#define L_LOC4          (0x08 << 0)     // LOC4 button LED
#define L_LOC5          (0x10 << 0)     // LOC5 button LED
#define L_LOC6          (0x20 << 0)     // LOC6 button LED
#define L_LOC7          (0x40 << 0)     // LOC7 button LED
#define L_LOC8          (0x80 << 0)     // LOC8 button LED

/* U11 PORT-B (Output) LED Bits */
#define L_LOC0          (0x01 << 8)     // LOC0 button LED
#define L_LOC9          (0x02 << 8)     // LOC9 button LED
#define L_MENU          (0x04 << 8)     // SET button LED
#define L_EDIT          (0x08 << 8)     // ESC button LED
#define L_STORE         (0x10 << 8)     // PREV button LED
#define L_ALT           (0x20 << 8)     // MENU button LED
#define L_AUTO          (0x40 << 8)     // NEXT button LED
#define L_CUE           (0x80 << 8)     // EDIT button LED

#define L_LOC_MASK      (L_LOC1|L_LOC2|L_LOC3| L_LOC4| L_LOC5| \
                         L_LOC6|L_LOC7| L_LOC8|L_LOC9|L_LOC0)
/*
 * U10 is two 8-bit INPUT ports for reading button switches.
 */

/* U10 PORT-A (Input) Pushbutton Switch Bits */
#define SW_LOC1         (0x01 << 0)     // LOC1 button switch
#define SW_LOC2         (0x02 << 0)     // LOC2 button switch
#define SW_LOC3         (0x04 << 0)     // LOC3 button switch
#define SW_LOC4         (0x08 << 0)     // LOC4 button switch
#define SW_LOC5         (0x10 << 0)     // LOC5 button switch
#define SW_LOC6         (0x20 << 0)     // LOC6 button switch
#define SW_LOC7         (0x40 << 0)     // LOC7 button switch
#define SW_LOC8         (0x80 << 0)     // LOC8 button switch

/* U10 PORT-B (Input) Pushbutton Switch Bits */
#define SW_LOC0         (0x01 << 8)     // LOC0 button switch
#define SW_LOC9         (0x02 << 8)     // LOC9 button switch
#define SW_MENU         (0x04 << 8)     // MENU button switch
#define SW_EDIT         (0x08 << 8)     // EDIT button switch
#define SW_STORE        (0x10 << 8)     // STORE button switch
#define SW_ALT          (0x20 << 8)     // ALT button switch
#define SW_AUTO         (0x40 << 8)     // AUTO button switch
#define SW_CUE          (0x80 << 8)     // CUE button switch

#define SW_LOC_MASK     (SW_LOC1|SW_LOC2|SW_LOC3|SW_LOC4|SW_LOC5| \
                         SW_LOC6|SW_LOC7|SW_LOC8|SW_LOC9|SW_LOC0)
#endif

#pragma pack(pop)
