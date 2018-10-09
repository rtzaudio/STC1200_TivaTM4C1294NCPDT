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

#ifndef _REMOTETASK_H_
#define _REMOTETASK_H_

/*** FUNCTION PROTOTYPES ***************************************************/

Bool Remote_Task_init();

void SetLocateButtonLED(size_t index);

uint32_t xlate_to_dtc_transport_switch_mask(uint32_t mask);

#endif /* _REMOTETASK_H_ */
