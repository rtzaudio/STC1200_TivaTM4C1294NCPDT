/*
 * MidiTask.h : created 8/21/2017
 *
 * Copyright (C) 2017, Robert E. Starr. ALL RIGHTS RESERVED.
 *
 * THIS MATERIAL CONTAINS  CONFIDENTIAL, PROPRIETARY AND TRADE
 * SECRET INFORMATION. NO DISCLOSURE OR USE OF ANY
 * PORTIONS OF THIS MATERIAL MAY BE MADE WITHOUT THE EXPRESS
 * WRITTEN CONSENT OF THE AUTHOR.
 */

#ifndef _MIDITASK_H_
#define _MIDITASK_H_

/*** MIDI MACHNE CONTROL (MMC) *********************************************/

#define MIDI_MCC                0x06    /* Motion Control Command  */
#define MIDI_MCR                0x07    /* Motion Control Response */

/* MIDI Device ID 0x7F is "All Call" */
#define MIDI_DEVID_ALL_CALL     0x7F

/*** MIDI MACHINE CONTROL COMMANDS (MCC) ***********************************/

/* 0x01-0x3F : zero data bytes */
#define MCC_STOP                        	0x01
#define MCC_PLAY                        	0x02
#define MCC_DEFERRED_PLAY               	0x03
#define MCC_FAST_FORWARD                	0x04
#define MCC_REWIND                      	0x05
#define MCC_RECORD_STROBE               	0x06
#define MCC_RECORD_EXIT                 	0x07
#define MCC_RECORD_PAUSE                	0x08
#define MCC_PAUSE                       	0x09
#define MCC_EJECT                       	0x0A
#define MCC_CHASE                       	0x0B
#define MCC_COMMAND_ERROR_RESET         	0x0C
#define MCC_MMC_RESET                       0x0D
/* 0x40-0x77 : variable data, preceded by <count> byte */
#define MCC_WRITE                       	0x40
#define MCC_MASKED_WRITE                	0x41
#define MCC_READ                        	0x42
#define MCC_UPDATE                      	0x43
#define MCC_LOCATE                      	0x44
#define MCC_VARIABLE_PLAY               	0x45
#define MCC_SEARCH                      	0x46
#define MCC_SHUTTLE                     	0x47
#define MCC_STEP                        	0x48
#define MCC_ASSIGN_SYSTEM_MASTER        	0x49
#define MCC_GENERATOR_COMMAND           	0x4A
#define MCC_MIDI_TIME_CODE_COMMAND      	0x4B
#define MCC_MOVE                        	0x4C
#define MCC_ADD                         	
#define MCC_SUBTRACT                    	0x4E
#define MCC_DROP_FRAME_ADJUST           	0x4F
#define MCC_PROCEDURE                   	0x50
#define MCC_EVENT                       	0x51
#define MCC_GROUP                       	0x52
#define MCC_COMMAND_SEGMENT             	0x53
#define MCC_DEFERRED_VARIABLE_PLAY      	0x54
#define MCC_RECORD_STROBE_PLAY          	0x55
/* 0x78-0x7F : zero data bytes (handshake) */
#define MCC_WAIT                        	0x7C
#define MCC_RESUME                      	0x7F

/*** MIDI MACHINE CONTROL RESPONSES (MCR) **********************************/

/* 0x01-0x1F : 5 data bytes (standard time code fields) */
#define MCR_SELECTED_TIME_CODE        		0x01
#define MCR_SELECTED_MASTER_CODE        	0x02
#define MCR_REQUESTED_OFFSET        		0x03
#define MCR_ACTUAL_OFFSET               	0x04
#define MCR_LOCK_DEVIATION              	0x05
#define MCR_GENERATOR_TIME_CODE         	0x06
/* 0x20-0x3F : 2 data bytes (short time code fields) */

/* 0x40-0x77 : variable data, preceded by <count> byte */
#define MCR_MOTION_CONTROL_TALLY        	0x48
#define MCR_VELOCITY_TALLY              	0x49
#define MCR_STOP_MODE                   	0x4A
#define MCR_FAST_MODE                   	0x4B
#define MCR_RECORD_MODE                 	0x4C
#define MCR_RECORD_STATUS               	0x4D
#define MCR_TRACK_RECORD_STATUS         	0x4E
#define MCR_TRACK_RECORD_READY          	0x4F
#define MCR_GLOBAL_MONITOR              	0x50
#define MCR_RECORD_MONITOR              	0x51
#define MCR_TRACK_SYNC_MONITOR          	0x52
#define MCR_TRACK_INPUT_MONITOR         	0x53
#define MCR_STEP_LENGTH                 	0x54
#define MCR_PLAY_SPEED_REFERENCE        	0x55
#define MCR_FIXED_SPEED                 	0x56
#define MCR_LIFTER_DEFEAT               	0x57
#define MCR_CONTROL_DISABLE             	0x58
#define MCR_RESOLVED_PLAY_MODE          	0x59
#define MCR_CHASE_MODE                  	0x5A
#define MCR_GENERATOR_COMMAND_TALLY     	0x5B
#define MCR_GENERATOR_SETUP             	0x5C
#define MCR_GENERATOR_USERBITS          	0x5D
#define MCR_MIDI_TIME_CODE_COMMAND_TALLY	0x5E
#define MCR_MIDI_TIME_CODE_SETUP            0x5F
#define MCR_PROCEDURE_RESPONSE              0x60
#define MCR_EVENT_RESPONSE                  0x61
#define MCR_TRACK_MUTE                      0x62
#define MCR_TRACK_VITC_INSERY_ENABLE        0x63
#define MCR_RESPONSE_SEGMENT              	0x64
#define MCR_FAILURE                     	0x65
/* 0x78-0x7F : zero data bytes (handshake) */
#define MCR_WAIT                        	0x7C
#define MCR_RESUME                      	0x7F

/*** MIDI FUNCTION ERROR CODES *********************************************/

#define MIDI_ERR_TIMEOUT            (-1)
#define MIDI_ERR_RX_OVERFLOW        (-2)
#define MIDI_ERR_FRAME_BEGIN        (-3)
#define MIDI_ERR_FRAME_END          (-4)
#define MIDI_ERR_MMC_INVALID        (-5)

/*** MIDI SERVICE OBJECT ***************************************************/

#define MIDI_MAX_PACKET_SIZE        48

typedef struct _MIDI_SERVICE {
    UART_Handle uartHandle;
    uint8_t     deviceID;
} MIDI_SERVICE;

typedef struct MidiMessage {
    uint8_t     length;
    uint8_t     data[8];
} MidiMessage;

/*** FUNCTION PROTOTYPES ***************************************************/

Bool Midi_Server_init(void);
Bool Midi_Server_startup(void);

Bool MidiQueueResponse(MidiMessage* msg);

#endif /* _MIDITASK_H_ */
