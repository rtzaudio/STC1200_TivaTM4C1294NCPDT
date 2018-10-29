/***************************************************************************
 *
 * DTC-1200 & STC-1200 Digital Transport Controllers for
 * Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2018, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 ***************************************************************************/

#ifndef _REMOTETASK_H_
#define _REMOTETASK_H_

/*** FUNCTION PROTOTYPES ***************************************************/

Bool Remote_Task_startup();

void SetLocateButtonLED(size_t index);

void SetButtonLedMask(uint32_t setMask, uint32_t clearMask);

uint32_t xlate_to_dtc_transport_switch_mask(uint32_t mask);

#endif /* _REMOTETASK_H_ */
