/***************************************************************************
 *
 * DTC-1200 & STC-1200 Digital Transport Controllers for
 * Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2019, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 ***************************************************************************/

#ifndef _SMPTE_H_
#define _SMPTE_H_

#define SMPTE_GENERATOR     1

/*** Function Prototypes ***************************************************/

int32_t SMPTE_init(void);

int32_t SMPTE_stripe_start(void);
int32_t SMPTE_stripe_stop(void);

#endif  /* _SMPTE_H_ */
