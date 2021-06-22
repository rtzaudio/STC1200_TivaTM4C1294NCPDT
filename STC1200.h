/***************************************************************************
 *
 * DTC-1200 & STC-1200 Digital Transport Controllers for
 * Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2020, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 ***************************************************************************/

#ifndef __STC1200_H
#define __STC1200_H

#include "PositionTask.h"
#include "LocateTask.h"
#include "TrackConfig.h"
#include "MCP79410.h"
#include "AD9837.h"

//*****************************************************************************
// CONSTANTS AND CONFIGURATION
//*****************************************************************************

/* VERSION INFO - The min build specifies the minimum build required
 * that does NOT force a default reset of all the config parameters
 * at run time. For instance, if the config loads build 5 and the minimum
 * is set to 3, then it will reset config for anything less than build 3.
 * Likewise, versions 3 or higher would load and use the config values from
 * eprom as normal. This provides a means to force run time config defaults
 * to be reset or not.
 */
#define FIRMWARE_VER        1           /* firmware version */
#define FIRMWARE_REV        18          /* firmware revision */
#define FIRMWARE_BUILD      1           /* firmware build number */
#define FIRMWARE_MIN_BUILD  1           /* min build req'd to force reset */

#if (FIRMWARE_MIN_BUILD > FIRMWARE_BUILD)
#error "STC build option FIRMWARE_MIN_BUILD set incorrectly"
#endif

#define MAGIC               0xCEB0FACE  /* magic number for EEPROM data */
#define MAKEREV(v, r)       ((v << 16) | (r & 0xFFFF))

//*****************************************************************************
// SYSTEM RUN-TIME CONFIG PARAMETERS STORED IN EPROM
//*****************************************************************************

#define REF_FREQ            9600.0f
#define REF_FREQ_MIN        1000.0f
#define REF_FREQ_MAX        18000.0f

typedef struct _SYSCFG
{
    uint32_t    magic;
    uint32_t    version;
    uint32_t    build;
    uint32_t    length;              /* debug level */
    /** Remote Parameters **/
    bool        showLongTime;
    /** Locator Parameters **/
    bool        searchBlink;        /* blink 7-seg during search */
    /** Locator velocities for various distances from the locate point **/
    uint32_t    jog_vel_far;        /* 0 = use DTC default shuttle velocity */
    uint32_t    jog_vel_mid;        /* vel for mid distance from locate point */
    uint32_t    jog_vel_near;       /* vel for near distance from locate point */
    /* NCO reference freq */
    float       ref_freq;           /* default reference freq */
    /* shadow copy of track config */
    uint8_t     trackState[MAX_TRACKS];
    uint8_t     tapeSpeed;          /* 15=low speed, 30=high speed */
    /* SMPTE board config */
    uint16_t    smpteFPS;           /* frames per sec config */
    /* MIDI config */
    uint8_t     midiDevID;          /* midi device ID */
    uint8_t     reserved;
} SYSCFG;

//*****************************************************************************
// GLOBAL SHARED MEMORY & REAL-TIME DATA
//*****************************************************************************

#define MAX_DIGITS_BUF      8

typedef struct _SYSDAT
{
    uint8_t		    ui8SerialNumber[16];		/* 128-bit serial number      */
    uint8_t         ui8MAC[6];                  /* 48-bit MAC from EPROM      */
    char            ipAddr[32];                 /* IP address from DHCP       */
    /* Items below are updated from DTC notifications */
    uint32_t        ledMaskTransport;           /* current transport LED mask */
    uint32_t        transportMode;              /* Current transport mode     */
    /* These items are internal to STC */
    uint32_t        tapeSpeed;                  /* tape speed (15 or 30)      */
    int32_t         tapeDirection;              /* direction 1=fwd, 0=rew     */
    uint32_t        tapePositionAbs;            /* absolute tape position     */
    int32_t 	    tapePosition;				/* signed relative position   */
    int32_t 	    tapePositionPrev;			/* previous tape position     */
    int32_t         searchProgress;             /* progress to cue (0-100%)   */
    uint32_t	    qei_error_cnt;				/* QEI phase error count      */
    float		    tapeTach;					/* tape speed from roller     */
	bool		    searchCancel;               /* true if search canceling   */
	bool            searching;                  /* true if search in progress */
    bool            autoLoop;                   /* true if loop mode running  */
    bool            autoPunch;                  /* auto punch mode active     */
    /* Remote control edit data */
    uint32_t        ledMaskRemote;              /* DRC remote button LED mask */
    int32_t         remoteMode;                 /* current remote mode        */
    int32_t         remoteModeLast;             /* last mode before edit      */
    int32_t         remoteModePrev;             /* previous remote mode       */
    int32_t         editState;                  /* current edit time state    */
    TAPETIME        editTime;                   /* edit tape time conversion  */
    int32_t         digitCount;
    char            digitBuf[MAX_DIGITS_BUF+1]; /* input digits buffer        */
    bool            autoMode;                   /* auto mode active flag      */
    bool            shiftRecButton;             /* true if shifted REC button */
    bool            shiftAltButton;             /* true if shifted ALT button */
    /* Locate and Position data */
    bool            varispeedMode;              /* jog wheel varispeed active */
    float           ref_freq;                   /* master ref freq 9600 Hz    */
    TAPETIME        tapeTime;                   /* current tape time position */
    size_t          cueIndex;                   /* current cue table index    */
    CUE_POINT	    cuePoint[MAX_CUE_POINTS];	/* array of cue point data    */
    uint8_t         trackState[MAX_TRACKS];
    uint32_t        trackCount;                 /* number of tracks in machine*/
    /* Application Global Handles */
    Event_Handle    handleEventQEI;
    SDSPI_Handle    handleSD;
    I2C_Handle      handleI2C0;
    MCP79410_Handle handleRTC;
    bool            rtcFound;                   /* true if MCP79410 RTC found */
    UART_Handle     handleUartDCS;
    TRACK_Handle    handleDCS;
    bool            dcsFound;                   /* true if DCS-1200 found     */
    bool            smpteFound;                 /* true if SMPTE card found   */
} SYSDAT;

//*****************************************************************************
// Task Command Message Structure
//*****************************************************************************

typedef enum CommandType {
    SWITCHPRESS,
} CommandType;

typedef struct CommandMessage {
    CommandType		command;
    uint32_t 		param;
} CommandMessage;

//*****************************************************************************
// Function Prototypes
//*****************************************************************************

/* Global STC-1200 System data */
extern SYSDAT g_sys;
extern SYSCFG g_cfg;

/* Function Prototypes */
int main(void);

int ConfigSave(int level);
int ConfigLoad(int level);
int ConfigReset(int level);

#endif /* __STC1200_H */
