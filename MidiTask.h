/*
 * PMX42.h : created 5/18/2015
 *
 * Copyright (C) 2015, Robert E. Starr. ALL RIGHTS RESERVED.
 *
 * THIS MATERIAL CONTAINS  CONFIDENTIAL, PROPRIETARY AND TRADE
 * SECRET INFORMATION. NO DISCLOSURE OR USE OF ANY
 * PORTIONS OF THIS MATERIAL MAY BE MADE WITHOUT THE EXPRESS
 * WRITTEN CONSENT OF THE AUTHOR.
 */

#ifndef __MIDITASK_H
#define __MIDITASK_H

/*** MMC TRANSPORT CONTROL COMMANDS ****************************************/

/* MIDI Transport Commands */
#define MMC_STOP                        0x01
#define MMC_PLAY                        0x02
#define MMC_DEFERRED_PLAY               0x03
#define MMC_FAST_FORWARD                0x04
#define MMC_REWIND                      0x05
#define MMC_RECORD_STROBE               0x06
#define MMC_RECORD_EXIT                 0x07
#define MMC_RECORD_PAUSE                0x08
#define MMC_PAUSE                       0x09
#define MMC_EJECT                       0x0A
#define MMC_CHASE                       0x0B
#define MMC_COMMAND_ERROR_RESET         0x0C
#define MMC_RESET                       0x0D
#define MMC_LOCATE                      0x44
#define MMC_VARIABLE_PLAY               0x45
#define MMC_SEARCH                      0x46
#define MMC_SHUTTLE                     0x47
#define MMC_STEP                        0x48
#define MMC_DEFERRED_VARIABLE_PLAY      0x54
#define MMC_RECORD_STROBE_PLAY          0x55
#define MMC_WAIT          				0x7C
#define MMC_RESUME          		    0x7F

/* MIDI Response and Information Fields */
#define MRI_SELECTED_TIME_CODE        	0x01
#define MRI_SELECTED_MASTER_CODE        0x02
#define MRI_REQUESTED_OFFSET        	0x03

#define MRI_MOTION_CONTROL_TALLY        0x48
#define MRI_VELOCITY_TALLY              0x49
#define MRI_STOP_MODE                   0x4A
#define MRI_FAST_MODE                   0x4B
#define MRI_RECORD_MODE                 0x4C
#define MRI_RECORD_STATUS               0x4D
#define MRI_TRACK_RECORD_STATUS         0x4E
#define MRI_TRACK_RECORD_READY          0x4F
#define MRI_GLOBAL_MONITOR              0x50
#define MRI_RECORD_MONITOR              0x51
#define MRI_TRACK_SYNC_MONITOR          0x52
#define MRI_TRACK_INPUT_MONITOR         0x53
#define MRI_STEP_LENGTH                 0x54
#define MRI_PLAY_SPEED_REFERENCE        0x55
#define MRI_FIXED_SPEED                 0x56
#define MRI_LIFTER_DEFEAT               0x57
#define MRI_CONTROL_DISABLE             0x58
#define MRI_TRACK_MUTE                  0x63
#define MRI_FAILURE                     0x65

/*** FUNCTION PROTOTYPES ***************************************************/

Void MidiTaskFxn(UArg arg0, UArg arg1);

#endif /* __MIDITASK_H */
