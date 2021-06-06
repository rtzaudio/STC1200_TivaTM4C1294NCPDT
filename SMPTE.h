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

/*** Function Prototypes ***************************************************/

bool SMPTE_init(void);
bool SMPTE_probe(void);
bool SMPTE_get_revid(uint16_t* revid);
bool SMPTE_generator_start(void);
bool SMPTE_generator_stop(void);
bool SMPTE_generator_resume(void);

#endif  /* _SMPTE_H_ */
