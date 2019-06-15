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

#ifndef _AT24MAC_H_
#define _AT24MAC_H_

/* AT24MAC I2C Object Structure */

typedef struct AT24MAC_Object {
    uint32_t    baseAddr;   /* base I2C port address */
} AT24MAC_Object;

typedef AT24MAC_Object* AT24MAC_Handle;

/* I2C Transaction Structure */

typedef struct AT24MAC_Transaction {
    void*   writeBuf;       /* buffer containing data to be written */
    size_t  writeCount;     /* Number of bytes to be written to the slave */
    void*   readBuf;        /* buffer to which data is to be read into */
    size_t  readCount;      /* Number of bytes to be read from the slave */
    uint8_t slaveAddress;   /* Address of the I2C slave device */
} AT24MAC_Transaction;

/* I2C Device addresses */
#define AT24MAC_EPROM_ADDR      (0xA0 >> 1)
#define AT24MAC_GUID128_ADDR    (0xB0 >> 1)
#define AT24MAC_MAC48_ADDR      (0xB1 >> 1)

/*** FUNCTION PROTOTYPES ***************************************************/

void AT24MAC_init(AT24MAC_Object* object);

bool AT24MAC_transfer(AT24MAC_Object* object,
                      AT24MAC_Transaction* transaction);

bool AT24MAC_GUID_read(AT24MAC_Object* object,
                       uint8_t ui8SerialNum[16], uint8_t ui8MAC[6]);

#endif /* _AT24MAC_H_ */
