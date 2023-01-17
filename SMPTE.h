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
bool SMPTE_generator_start(bool reset);
bool SMPTE_generator_stop(void);
bool SMPTE_generator_set_time(uint8_t hours, uint8_t mins,
                              uint8_t secs, uint8_t frame);

#endif  /* _SMPTE_H_ */
