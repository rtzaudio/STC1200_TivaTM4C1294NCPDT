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

#ifndef __DISPLAYTASK_H
#define __DISPLAYTASK_H

/*** CONSTANTS AND CONFIGURATION *******************************************/


/*** SYSTEM CONFIG PARAMETERS STORED IN EPROM ******************************/

typedef enum DisplayCommand{
    WAKESCREEN,
    SETSCREEN,
    NEXTSCREEN,
	PREVSCREEN,
} DisplayCommand;

typedef struct DisplayMessage{
    DisplayCommand	dispCommand;
    uint32_t		dispArg1;
    uint32_t		dispArg2;
} DisplayMessage;

/*** FUNCTION PROTOTYPES ***************************************************/

void ClearDisplay();
void DisplayWelcome();
Void DisplayTaskFxn(UArg arg0, UArg arg1);

#endif /* __DISPLAYTASK_H */
