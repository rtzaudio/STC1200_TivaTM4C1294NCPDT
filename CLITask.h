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

#ifndef __CLITASK_H
#define __CLITASK_H

/*** CONSTANTS AND CONFIGURATION *******************************************/


/*** FUNCTION PROTOTYPES ***************************************************/

int CLI_init(void);
void CLI_printf(const char *fmt, ...);

Void CLITaskFxn(UArg arg0, UArg arg1);

#endif /* __REMOTETASK_H */
