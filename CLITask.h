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

#ifndef __CLITASK_H
#define __CLITASK_H

/*** CONSTANTS AND CONFIGURATION *******************************************/


/*** FUNCTION PROTOTYPES ***************************************************/

int CLI_init(void);
void CLI_printf(const char *fmt, ...);

Void CLITaskFxn(UArg arg0, UArg arg1);

#endif /* __REMOTETASK_H */
