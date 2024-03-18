/***************************************************************************
 *
 * DTC-1200 & STC-1200 Digital Transport Controllers for
 * Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2021, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 ***************************************************************************/

#ifndef _SMPTE_H_
#define _SMPTE_H_

/* SMPTE CONTROLLER SPI SLAVE REGISTERS
 *
 * All registers are 16-bits with the upper word containing the command
 * and flag bits. The lower 8-bits contains any associated data byte.
 *
 *   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *   | R | A | A | A | C | C | C | C | B | B | B | B | B | B | B | B |
 *   | W | 6 | 5 | 4 | 3 | 2 | 1 | 0 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |   |       |   |           |   |                           |
 *     |   +---+---+   +-----+-----+   +-------------+-------------+
 *     |       |             |                       |
 *    R/W     RSVD          REG                  DATA/FLAGS
 *
 * The SMPTE_REG_DATA command register returns an additional 32-bits
 * time code data containing the HH:MM:SS:FF as follows.
 *
 *   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *   | D | D | D | D | D | D | D | D | D | D | D | D | D | D | D | D |
 *   |15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |                           |   |                           |
 *     +-------------+-------------+   +-------------+-------------+
 *                   |                               |
 *                  SECS                           FRAME
 *
 *   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *   | D | D | D | D | D | D | D | D | D | D | D | D | D | D | D | D |
 *   |31 |30 |29 |28 |27 |26 |25 |24 |23 |22 |21 |20 |19 |18 |17 |16 |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |                           |   |                           |
 *     +-------------+-------------+   +-------------+-------------+
 *                   |                               |
 *                 HOUR                             MINS
 */

#include "..\STC_SMPTE_TivaTM4C123AE6PM\STC_SMPTE_SPI.h"

/*** DATA STRUCTURES *******************************************************/

typedef struct SMPTE_Params {
    SPI_Handle          spiHandle;
    uint32_t            gpioCS;         /* Chip select in Board.h */
} SMPTE_Params;

typedef struct SMPTE_Object {
    SPI_Handle          spiHandle;
    uint32_t            gpioCS;         /* Chip select in Board.h */
    GateMutex_Struct    gate;
} SMPTE_Object;

typedef SMPTE_Object *SMPTE_Handle;

/*** Function Prototypes ***************************************************/

SMPTE_Handle SMPTE_construct(SMPTE_Object *obj, SMPTE_Params *params);
SMPTE_Handle SMPTE_create(SMPTE_Params *params);

Void SMPTE_Params_init(SMPTE_Params *params);
Void SMPTE_delete(SMPTE_Handle handle);
Void SMPTE_destruct(SMPTE_Handle handle);

bool SMPTE_init(void);
bool SMPTE_probe(void);
bool SMPTE_get_revid(uint16_t* revid);

bool SMPTE_decoder_start(void);
bool SMPTE_decoder_stop(void);

bool SMPTE_encoder_start(bool reset);
bool SMPTE_encoder_stop(void);
bool SMPTE_encoder_set_time(uint8_t hours, uint8_t mins,
                            uint8_t secs, uint8_t frame);

#endif  /* _SMPTE_H_ */
