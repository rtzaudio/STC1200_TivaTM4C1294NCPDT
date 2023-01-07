// ==========================================================================
// STC1200TCP.h     v1.04 01/07/2023
//
// STC-1200 Client/Server Network Packet Definitions for the software based
// version of the DRC digital remote control. 
//
// Developed by Robert E. Starr, Jr.
//
// Copyright (C) 2019-2023, RTZ Professional Audio, LLC
//
// RTZ is a registered trademark of RTZ Professional Audio, LLC
// All Rights Reserved
// ==========================================================================

#pragma once
#pragma pack(push, 8)

#include <stdint.h>

// ===========================================================================
// DTC-1200 CONFIG PARAMETERS - MUST MATCH SYSPARMS STRUCT IN DTC1200.h
// ===========================================================================

#ifndef _DTC_CONFIG_DATA_DEFINED_
#define _DTC_CONFIG_DATA_DEFINED_

typedef struct _DTC_CONFIG_DATA {
    uint32_t    magic;
    uint32_t    version;
    uint32_t    build;
    /*** GLOBAL PARAMETERS ***/
    int32_t     debug;                      /* debug level */
    int32_t     pinch_settle_time;          /* delay before engaging play mode   */
    int32_t     lifter_settle_time;         /* lifter settling time in ms        */
    int32_t     brake_settle_time;          /* break settling time after STOP    */
    int32_t     play_settle_time;           /* play after shuttle settling time  */
    int32_t     rechold_settle_time;        /* record pulse length time          */
    int32_t     record_pulse_time;          /* record pulse length time          */
    int32_t     vel_detect_threshold;       /* vel detect threshold (10)         */
    uint32_t    debounce;                   /* debounce transport buttons time   */
    uint32_t    sysflags;                   /* global system bit flags           */
    /*** SOFTWARE GAIN PARAMETERS ***/
    float       reel_radius_gain;           /* reeling radius play gain factor   */
    float       reel_offset_gain;           /* reeling radius offset gain factor */
    float       tension_sensor_gain;        /* tension sensor gain divisor       */
    float       tension_sensor_midscale1;   /* ADC mid-scale for 1" tape         */
    float       tension_sensor_midscale2;   /* ADC mid-scale for 2" tape         */
    /*** THREAD TAPE PARAMETERS ***/
    int32_t     thread_supply_tension;      /* supply tension level (0-DAC_MAX)  */
    int32_t     thread_takeup_tension;      /* takeup tension level (0-DAC_MAX)  */
    /*** STOP SERVO PARAMETERS ***/
    int32_t     stop_supply_tension;        /* supply tension level (0-DAC_MAX)  */
    int32_t     stop_takeup_tension;        /* takeup tension level (0-DAC_MAX)  */
    int32_t     stop_brake_torque;          /* stop brake torque in shuttle mode */
    /*** SHUTTLE SERVO PARAMETERS ***/
    int32_t     shuttle_supply_tension;     /* play supply tension (0-DAC_MAX)   */
    int32_t     shuttle_takeup_tension;     /* play takeup tension               */
    int32_t     shuttle_velocity;           /* target speed for shuttle mode     */
    int32_t     shuttle_lib_velocity;       /* library wind mode velocity        */
    int32_t     shuttle_autoslow_velocity;  /* velocity to reduce speed to       */
    int32_t     autoslow_at_offset;         /* auto-slow trigger at offset       */
    int32_t     autoslow_at_velocity;       /* auto-slow trigger at velocity     */
    float       shuttle_fwd_holdback_gain;  /* velocity tension gain factor      */
    float       shuttle_rew_holdback_gain;  /* velocity tension gain factor      */
    /* reel servo PID values */
    float       shuttle_servo_pgain;        /* P-gain */
    float       shuttle_servo_igain;        /* I-gain */
    float       shuttle_servo_dgain;        /* D-gain */
    /*** PLAY SERVO PARAMETERS ***/
    /* play high speed boost parameters */
    int32_t     play_hi_supply_tension;     /* play supply tension (0-DAC_MAX) */
    int32_t     play_hi_takeup_tension;     /* play takeup tension (0-DAC_MAX) */
    int32_t     play_hi_boost_end;
    float       play_hi_boost_pgain;        /* P-gain */
    float       play_hi_boost_igain;        /* I-gain */
    /* play low speed boost parameters */
    int32_t     play_lo_supply_tension;     /* play supply tension (0-DAC_MAX) */
    int32_t     play_lo_takeup_tension;     /* play takeup tension (0-DAC_MAX) */
    int32_t     play_lo_boost_end;
    float       play_lo_boost_pgain;        /* P-gain */
    float       play_lo_boost_igain;        /* I-gain */
} DTC_CONFIG_DATA;

/* System Bit Flags for DTC1200_CONFIG.sysflags */
#define DTC_SF_LIFTER_AT_STOP       0x0001  /* leave lifter engaged at stop */
#define DTC_SF_BRAKES_AT_STOP       0x0002  /* leave brakes engaged at stop */
#define DTC_SF_BRAKES_STOP_PLAY     0x0004  /* use brakes to stop play mode */
#define DTC_SF_ENGAGE_PINCH_ROLLER  0x0008  /* engage pinch roller at play  */
#define DTC_SF_STOP_AT_TAPE_END     0x0010  /* stop @tape end leader detect */

#endif /* _DTC_CONFIG_DATA_DEFINED_ */

// ===========================================================================
// STC-1200 CONFIG PARAMETERS - MUST MATCH SYSPARMS STRUCT IN STC1200.h
// ===========================================================================

#ifndef _STC_CONFIG_DATA_DEFINED_
#define _STC_CONFIG_DATA_DEFINED_

typedef struct _STC_CONFIG_DATA
{
    uint32_t    magic;
    uint32_t    version;
    uint32_t    build;
    uint32_t    length;
    /** Remote Parameters **/
    bool        showLongTime;
    /** Locator Parameters **/
    bool        searchBlink;            /* blink 7-seg during search */
    /** Locator velocities for various distances from the locate point **/
    uint32_t    jog_vel_far;            /* 0 = use DTC default shuttle velocity */
    uint32_t    jog_vel_mid;            /* vel for mid distance from locate point */
    uint32_t    jog_vel_near;           /* vel for near distance from locate point */
    /* NCO reference freq */
    float       ref_freq;               /* default reference freq */
    /* shadow copy of track config */
    uint8_t     trackState[24];
    uint8_t     tapeSpeed;              /* 15=low speed, 30=high speed */
    /* SMPTE board config */
    uint16_t    smpteFPS;               /* frames per sec config */
    /* MIDI config */
    uint8_t     midiDevID;              /* midi device ID */
    uint8_t     reserved;
} STC_CONFIG_DATA;

#define STC_REF_FREQ        9600.0f
#define STC_REF_FREQ_MIN    1000.0f
#define STC_REF_FREQ_MAX    18000.0f

#endif /* _STC_CONFIG_DATA_DEFINED_ */

// ==========================================================================
// General Purpose Tape time and RTC time/date structures
// ==========================================================================

#ifdef _WINDOWS
/* Tape time structure for tape position in time */
typedef struct _TAPETIME {
    uint8_t     hour;                   /* hour (0-1)      */
    uint8_t     mins;                   /* minutes (0-59)  */
    uint8_t     secs;                   /* seconds  (0-59) */
    uint8_t     tens;                   /* tens secs (0-9) */
    uint8_t     frame;                  /* smpte frame#    */
    uint8_t     flags;                  /* display flags   */
    uint16_t    align;                  /* word alignment  */
} TAPETIME;

/* TAPETIME.flags */
#define F_TAPETIME_PLUS     0x01        /* 7-seg plus sign if set    */
#define F_TAPETIME_BLINK    0x02        /* blink all seven segments  */
#define F_TAPETIME_BLANK    0x80        /* blank the entire display  */
#endif

/* RTC time/date structure for the STC real time clock */
typedef struct _DATETIME
{
    uint8_t     sec;                    /* seconds 0-59      */
    uint8_t     min;                    /* minutes 0-59      */
    uint8_t     hour;                   /* 24-hour 0-23      */
    uint8_t     weekday;                /* weekday 1-7       */
    uint8_t     date;                   /* day of month 0-30 */
    uint8_t     month;                  /* month 0-11        */
    uint8_t     year;                   /* year 0-128 (+2000)*/
} DATETIME;

// ==========================================================================
// TCP/IP Port Numbers for STC remote server
// ==========================================================================

#define STC_PORT_STATE          1200    /* streaming transport state   */
#define STC_PORT_COMMAND        1201    /* transport cmd/response port */

/* Defines the maximum number of tracks supported by any machine.
 * Some machines may have less, like 16 or 8 track machines.
 */
#define STC_MAX_TRACKS          24      /* max number of audio tracks  */

/* We support 10 cue points for the remote, but five extra cue memories
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

/* STC state update message structure. This message streams from the STC
 * to the TCP client to indicate the current transport time, the led/lamp
 * states and other real time feedback information. State packets stream
 * anytime there is tape motion, or any other transport state change 
 * event occurs. This is a one way stream to the client and the STC-1200
 * server does not attempt to receive any data back on this port stream.
 */

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
    uint8_t     trackCount;             /* number of tracks supported */
    uint8_t     hardwareFlags;          /* optional hardware status   */
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

#define STC_TRACK_MASK      0x03        /* low 2-bits are track state */

/* Upper bits of trackState indicate ready/record state */
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

/* STC_STATE_MSG.hardwareFlags status bit flags. These flags
 * indicate the status of optional hardware systems supported.
 */
#define STC_HF_NONE         0x00        /* no cue point stored addr   */
#define STC_HF_RTC          0x01        /* external RTC clock found   */
#define STC_HF_DCS          0x02        /* DCS channel switcher       */
#define STC_HF_SMPTE        0x04        /* SMPTE daughter card        */
#define STC_HF_NCO          0x08        /* external NCO ref clock     */

// ==========================================================================
// STC Notification Bit Flags (MUST MATCH VALUES IN DRC1200 HEADERS!)
// ==========================================================================

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

// ==========================================================================
// STC Specific Constants
// ==========================================================================

/* The Ampex MM1200 tape roller quadrature encoder wheel has 40 ppr. This gives
 * either 80 or 160 edges per revolution depending on the quadrature encoder
 * configuration set by QEIConfig(). Currently we use Cha-A mode which
 * gives 80 edges per revolution. If Cha-A/B mode is used this must be
 * set to 160.
 */

#define STC_ROLLER_TICKS_PER_REV        80
#define STC_ROLLER_TICKS_PER_REV_F      80.0f

 /* This is the diameter of the tape timer roller */
#define STC_ROLLER_CIRCUMFERENCE_F      5.0014f

/* This is the maximum signed position value we can have.Anything past
 * this is treated as a negative position value.
 */

#define STC_MAX_ROLLER_POSITION			(0x7FFFFFFF - 1UL)
#define STC_MIN_ROLLER_POSITION			(-STC_MAX_ROLLER_POSITION - 1)

// ==========================================================================
// STC COMMAND/RESPONSE MESSAGES
// ==========================================================================

#define STC_ALL_TRACKS      ((uint32_t)(-1))
#define STC_ALL_CUEPOINTS   ((uint32_t)(-1))

typedef struct _STC_COMMAND_HDR {
    uint16_t    length;                 /* size of full msg structure */
    uint16_t    command;                /* the command ID to execute  */
    uint16_t    index;                  /* track or cue point index   */
    uint16_t    status;                 /* return status/error code   */
} STC_COMMAND_HDR;

/*** COMMON ARGUMENTS FOR MOST COMMANDS ************************************/

typedef struct _STC_COMMAND_ARG {
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
    uint16_t        bitflags;           /* optional flags mask word   */
} STC_COMMAND_ARG;

 // ==========================================================================
 // MESSAGE COMMAND TYPES FOR 'STC_COMMAND_HDR.command'
 // ==========================================================================

#define STC_CMD_VERSION_GET             0   /* return the current STC version   */
#define STC_CMD_STOP                    1
#define STC_CMD_PLAY                    2   /* param0 1=record                  */
#define STC_CMD_REW                     3
#define STC_CMD_FWD                     4
#define STC_CMD_LIFTER                  5
#define STC_CMD_LOCATE                  6   /* param1 1=autoplay, 2=autorec     */
#define STC_CMD_LOCATE_AUTO_LOOP        7   /* param1 1=autoplay, 2=autorec     */
#define STC_CMD_LOCATE_MODE_SET         8   /* param1 0=cue-mode, 1=store-mode  */
#define STC_CMD_AUTO_PUNCH_SET          9
#define STC_CMD_AUTO_PUNCH_GET          10
#define STC_CMD_CUEPOINT_CLEAR          11  /* param1=index                     */
#define STC_CMD_CUEPOINT_STORE          12
#define STC_CMD_CUEPOINT_SET            13
#define STC_CMD_CUEPOINT_GET            14
#define STC_CMD_TRACK_TOGGLE_ALL        15  /* param1=mask to toggle            */
#define STC_CMD_TRACK_SET_STATE         16  /* param1=index, param2=flags       */
#define STC_CMD_TRACK_GET_STATE         17  /* param1=index, param2=flags       */
#define STC_CMD_TRACK_MASK_ALL          18  /* param1=setmask, param2=clrmask   */
#define STC_CMD_TRACK_MODE_ALL          19  /* param1=newmode, param2=0         */
#define STC_CMD_ZERO_RESET              20  /* param1=0, param2=0               */
#define STC_CMD_CANCEL                  21  /* param1=0, param2=0               */
#define STC_CMD_TAPE_SPEED_SET          22  /* param1=30/15, param2=0           */
#define STC_CMD_CONFIG_EPROM            23  /* param1 0=load, 1=store, 2=reset  */
#define STC_CMD_MONITOR                 24  /* param1 0=off, 1=standby, 2=mon   */
#define STC_CMD_TRACK_GET_COUNT         25
#define STC_CMD_MACHINE_CONFIG          26  /* param1 0=load, 1=store, 2=reset  */
#define STC_CMD_MACHINE_CONFIG_GET      27
#define STC_CMD_MACHINE_CONFIG_SET      28
#define STC_CMD_SMPTE_MASTER_CTRL       29
#define STC_CMD_RTC_TIMEDATE_GET        30
#define STC_CMD_RTC_TIMEDATE_SET        31

/*** STC_CMD_STOP ***********************************************************/

typedef struct _STC_COMMAND_VERSION_GET {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;
} STC_COMMAND_VERSION_GET;

/*** STC_CMD_STOP ***********************************************************/

typedef struct _STC_COMMAND_STOP {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;
} STC_COMMAND_STOP;

/*** STC_CMD_PLAY ***********************************************************/

typedef struct _STC_COMMAND_PLAY {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;
} STC_COMMAND_PLAY;

/*** STC_CMD_REW ************************************************************/

typedef struct _STC_COMMAND_REW {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;
} STC_COMMAND_REW; 

/*** STC_CMD_FWD ************************************************************/

typedef struct _STC_COMMAND_FWD {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;
} STC_COMMAND_FWD;

/*** STC_CMD_LIFTER *********************************************************/

typedef struct _STC_COMMAND_LIFTER {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;
} STC_COMMAND_LIFTER;

/*** STC_CMD_LOCATE *********************************************************/

typedef struct _STC_COMMAND_LOCATE {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;        /* param1 1=autoplay, 2=autorec    */
} STC_COMMAND_LOCATE;

/*** STC_CMD_LOCATE_MODE_SET ************************************************/

typedef struct _STC_COMMAND_LOCATE_MODE_SET {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;        /* param1 1=autoplay, 2=autorec    */
} STC_COMMAND_LOCATE_MODE_SET;

/*** STC_CMD_LOCATE_MODE_SET ************************************************/

typedef struct _STC_COMMAND_LOCATE_AUTO_LOOP {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;        /* param1 1=autoplay, 2=autorec    */
} STC_COMMAND_LOCATE_AUTO_LOOP;

/*** STC_CMD_AUTO_PUNCH_SET *************************************************/

typedef struct _STC_COMMAND_AUTO_PUNCH_SET {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;
} STC_COMMAND_AUTO_PUNCH_SET;

/*** STC_CMD_AUTO_PUNCH_GET *************************************************/

typedef struct _STC_COMMAND_AUTO_PUNCH_GET {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;
} STC_COMMAND_AUTO_PUNCH_GET;

/*** STC_CMD_CUEPOINT_CLEAR *************************************************/

typedef struct _STC_COMMAND_CUEPOINT_CLEAR {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;
} STC_COMMAND_CUEPOINT_CLEAR;

/*** STC_CMD_CUEPOINT_STORE *************************************************/

typedef struct _STC_COMMAND_CUEPOINT_STORE {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;
} STC_COMMAND_CUEPOINT_STORE;

/*** STC_CMD_CUEPOINT_SET ***************************************************/

typedef struct _STC_COMMAND_CUEPOINT_SET {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;
} STC_COMMAND_CUEPOINT_SET;

/*** STC_CMD_CUEPOINT_GET ***************************************************/

typedef struct _STC_COMMAND_CUEPOINT_GET {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;
} STC_COMMAND_CUEPOINT_GET;

/*** STC_CMD_TRACK_TOGGLE_ALL ***********************************************/

typedef struct _STC_COMMAND_TRACK_TOGGLE_ALL {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;
} STC_COMMAND_TRACK_TOGGLE_ALL;

/*** STC_CMD_TRACK_SET_STATE ************************************************/

typedef struct _STC_COMMAND_TRACK_SET_STATE {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;
} STC_COMMAND_TRACK_SET_STATE;

/*** STC_CMD_TRACK_GET_STATE ************************************************/

typedef struct _STC_COMMAND_TRACK_GET_STATE {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;
} STC_COMMAND_TRACK_GET_STATE;

/*** STC_CMD_TRACK_MASK_ALL *************************************************/

typedef struct _STC_COMMAND_TRACK_MASK_ALL {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;        /* param1=setmask, param2=clrmask  */
} STC_COMMAND_TRACK_MASK_ALL;

/*** STC_CMD_TRACK_MODE_ALL *************************************************/

typedef struct _STC_COMMAND_TRACK_MODE_ALL {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;        /* param1=newmode, param2=0        */
} STC_COMMAND_TRACK_MODE_ALL;

/*** STC_CMD_ZERO_RESET *****************************************************/

typedef struct _STC_COMMAND_ZERO_RESET {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;        /* param1=0, param2=0               */
} STC_COMMAND_ZERO_RESET;

/*** STC_CMD_CANCEL *********************************************************/

typedef struct _STC_COMMAND_CANCEL {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;        /* param1=0, param2=0               */
} STC_COMMAND_CANCEL;

/*** STC_CMD_TAPE_SPEED_SET *************************************************/

typedef struct _STC_COMMAND_TAPE_SPEED_SET {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;        /* param1=0, param2=0               */
} STC_COMMAND_TAPE_SPEED_SET;

/*** STC_CMD_TRACK_CONFIG_EPROM *********************************************/

typedef struct _STC_COMMAND_CONFIG_EPROM {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;        /* parm1: 0=load, 1=store, 2=reset  */
} STC_COMMAND_CONFIG_EPROM;

/*** STC_CMD_TAPE_MONITOR ***************************************************/

typedef struct _STC_COMMAND_MONITOR {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;        /* param1 0=off, 1=standby, 2=mon   */
} STC_COMMAND_MONITOR;

/*** STC_CMD_TRACK_GET_COUNT ************************************************/

typedef struct _STC_COMMAND_TRACK_GET_COUNT {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;        /* param1=0, param2=0              */
} STC_COMMAND_TRACK_GET_COUNT;

/*** STC_CMD_MACHINE_CONFIG *************************************************/

typedef struct _STC_COMMAND_MACHINE_CONFIG {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;        /* param1 0=load,1=store,2=defaults*/
} STC_COMMAND_MACHINE_CONFIG;

/*** STC_CMD_MACHINE_CONFIG_GET *********************************************/

typedef struct _STC_COMMAND_MACHINE_CONFIG_GET {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;        /* param1=0, param2=0              */
    STC_CONFIG_DATA     stc;        /* STC config parameters struct    */
    DTC_CONFIG_DATA     dtc;        /* DTC config parameters struct    */
} STC_COMMAND_MACHINE_CONFIG_GET;

/*** STC_CMD_MACHINE_CONFIG_SET *********************************************/

typedef struct _STC_COMMAND_MACHINE_CONFIG_SET {
    STC_COMMAND_HDR     hdr;
    STC_COMMAND_ARG     arg;        /* param1=0, param2=0              */
    STC_CONFIG_DATA     stc;        /* STC config parameters struct    */
    DTC_CONFIG_DATA     dtc;        /* DTC config parameters struct    */
} STC_COMMAND_MACHINE_CONFIG_SET;

/*** STC_CMD_SMPTE_MASTER_CTRL **********************************************/

typedef struct _STC_COMMAND_SMPTE_MASTER_CTRL {
    STC_COMMAND_HDR     hdr;
    uint32_t            ctrl;
    uint32_t            mode;
} STC_COMMAND_SMPTE_MASTER_CTRL;

/*** STC_CMD_RTC_TIMEDATE_GET ***********************************************/

typedef struct _STC_COMMAND_RTC_TIMEDATE_GET {
    STC_COMMAND_HDR     hdr;
    DATETIME            datetime;
} STC_COMMAND_RTC_TIMEDATE_GET;

/*** STC_CMD_RTC_TIMEDATE_SET ***********************************************/

typedef struct _STC_COMMAND_RTC_TIMEDATE_SET {
    STC_COMMAND_HDR     hdr;
    DATETIME            datetime;
} STC_COMMAND_RTC_TIMEDATE_SET;

#pragma pack(pop)

/* End-Of-File */
