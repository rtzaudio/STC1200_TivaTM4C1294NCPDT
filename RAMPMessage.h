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
#define OP_SWITCH_TRANSPORT         200     /* transport button switch press */
#define OP_SWITCH_REMOTE            201     /* all other remote switch press */

/* ============================================================================
 * DRC Notification Bit Flags (MUST MATCH VALUES IN DRC1200 HEADERS!)
 * ============================================================================ */

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
#define SW_STORE        (0x01 << 8)     // STORE button switch
#define SW_CUE          (0x02 << 8)     // CUE button switch
#define SW_SET          (0x04 << 8)     // SET button switch
#define SW_ESC          (0x08 << 8)     // ESC button switch
#define SW_PREV         (0x10 << 8)     // PREV button switch
#define SW_MENU         (0x20 << 8)     // MENU button switch
#define SW_NEXT         (0x40 << 8)     // NEXT button switch
#define SW_EDIT         (0x80 << 8)     // EDIT button switch

/*** LAMP/LED BIT MASKS ***/

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
#define L_STORE         (0x01 << 8)     // STORE button LED
#define L_CUE           (0x02 << 8)     // CUE button LED
#define L_SET           (0x04 << 8)     // SET button LED
#define L_ESC           (0x08 << 8)     // ESC button LED
#define L_PREV          (0x10 << 8)     // PREV button LED
#define L_MENU          (0x20 << 8)     // MENU button LED
#define L_NEXT          (0x40 << 8)     // NEXT button LED
#define L_EDIT          (0x80 << 8)     // EDIT button LED

#endif /* _RAMPMESSAGE_H_ */
