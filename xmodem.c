/*
 * XMODEM	Simple XMODEM file transfer driver, implementing several
 *		variations of the X/MODEM protocol.  The code was written
 *		with the 10/14/88 version of Forsberg's specification in
 *		hand.  It is believed to be a correct implementation.
 *
 *		The YMODEM support code (which is used by YMODEM and the
 *		XMODEM-Batch variations of the protocol) is not always
 *		needed, so it can be disabled.
 *
 * TODO:	Test with timeouts, and see if we can automate the choice
 *		of protocol in the receiver.  As Forsberg suggests, we 
 *		can send out C's for a while, and, if that fails, switch
 *		to NAK's for the basic protocol.
 *
 * Version:	@(#)xmodem.c	1.0.1	2007/12/02
 *
 * Author:	Fred N. van Kempen, <fred.van.kempen@microwalt.nl>
 *
 *		Copyright 2007 MicroWalt Corporation.
 *		All Rights Reserved.
 *
 *		This  program  or  documentation  contains  proprietary
 *		confidential information and trade secrets of MicroWalt
 *		Corporation.  Reverse  engineering of  object  code  is
 *		prohibited.  Use of copyright  notice is  precautionary
 *		and does not imply publication.  Any  unauthorized use,
 *		reproduction  or transfer  of this program  is strictly
 *		prohibited.
 *
 *		RESTRICTED RIGHTS NOTICE
 *
 *		Use, duplication, or disclosure  by the U.S. Government
 *		is subject to restrictions as set  forth in subdivision
 *		(b)(3)(ii) of the Rights in Technical Data and Computer
 *		Software clause at 252.227-7013.
 *
 *		MicroWalt Corporation
 *		P O BOX 8
 *		1400AA, BUSSUM, NH
 *		THE NETHERLANDS
 *		PH:  +31 (35) 7503090
 *		FAX: +31 (35) 7503091
 */

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Gate.h>
#include <xdc/runtime/Memory.h>

#include <ti/sysbios/BIOS.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/SDSPI.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>
#include <ti/mw/fatfs/ff.h>

#include <file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#include <driverlib/sysctl.h>

#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/hal/Seconds.h>

#include "xmodem.h"
#include "board.h"

/* Various definitions. */
#define PKT_SIZE    128
#define PKT_SIZE_1K 1024

#define NUM_TRIES   21
#define DEBUG       1

/* ASCII codes used in the protocol. */
#define NUL         0x00
#define SOH         0x01
#define STX         0x02
#define ETX         0x03
#define EOT         0x04
#define ACK         0x06
#define NAK         0x15
#define CAN         0x18
#define SUB         0x1a    /* final packet filler value */
#define CRC         'C'

/* XMODEM Packet Buffer */
static uint8_t      xmodem_buff[PKT_SIZE_1K];
#if USE_YMODEM
char                xmodem_name[32];
uint32_t            xmodem_size;
#endif

/******************************************************************************
 * Serial Interface Functions
 ******************************************************************************/

static void uart_putc(UART_Handle handle, uint8_t ch)
{
    UART_write(handle, &ch, 1);
}

static void uart_flush(UART_Handle handle)
{
    int n;
    uint8_t ch;

    while (1)
    {
        n = UART_read(handle, &ch, 1);

        if (n == UART_ERROR)
            break;
    }
}

static int uart_getc(UART_Handle handle, int secs)
{
    int i;
    uint8_t ch;

    for (i=0; i < secs; i++)
    {
        if (UART_read(handle, &ch, 1) == 1)
        {
            return (int)ch & 0xff;
        }
    }

    return -1;
}

/******************************************************************************
 * XMODEM Helper Functions
 ******************************************************************************/

/*
 * Update the CRC16 value for the given byte.
 */

static uint16_t xmodem_crc(uint16_t crc, uint8_t c)
{
    register int i;

    crc = crc ^ ((uint16_t)c << 8);

    for (i=0; i < 8; i++)
    {
        if (crc & 0x8000)
            crc = (crc << 1) ^ 0x1021;
        else
            crc <<= 1;
    }

    return crc;
}

/*
 * Write a block of data to a file stream.
 */

static FRESULT xmodem_write_block(FIL* fp, uint8_t *buf, int32_t size)
{
    uint32_t bytesToWrite = size;
    uint32_t bytesWritten = 0;
    FRESULT res = FR_OK;

    do {

        /* Write a block of data to the destination file */
        res = f_write(fp, xmodem_buff, bytesToWrite, &bytesWritten);

        if (res != FR_OK)
            break;

        bytesToWrite -= bytesWritten;

        buf += bytesWritten;

    } while(bytesToWrite > 0);

    return res;
}
    
/****************************************************************************
 * XMODEM is a protocol that comes in many flavors with many extensions
 * and additions. We only support the CRC16 mode to ensure data validity.
 ***************************************************************************/

/*
 * Receive a file using the XMODEM protocol.
 */

int xmodem_receive(UART_Handle handle, FIL* fp)
{
    int i;
    int c;
    int try;
    int status;
    bool started = false;
    uint8_t b, d;
    uint16_t msb, lsb;
    uint16_t crc, rx_crc;
    uint8_t blknum = 1;
    FRESULT res;

    /* Send a 'C' out and look for a SOH reply */

    status = XMODEM_NO_REPLY;

    for (try=0; try < 10; try++)
    {
        /* Send out a C and try to read reply */
        uart_putc(handle, CRC);

        /* Receive a byte of data. */
        if ((c = uart_getc(handle, 5)) == UART_ERROR)
        {
            continue;
        }

        /* Check for user aborting */
        if ((c == EOT) || (c == CAN))
        {
            status = XMODEM_CANCEL;
            break;
        }

        /* Start of header received, start reading packet */
        if (c == SOH)
        {
            status = XMODEM_SUCCESS;
            break;
        }
    }

    if (status == XMODEM_SUCCESS)
    {
#if DEBUG
        System_printf("Got SOH!\n");
        System_flush();
#endif
        crc = 0;
        try = 0;

        while(true)
        {
            if (try > 5)
            {
                uart_putc(handle, CAN);
                uart_putc(handle, CAN);
                uart_putc(handle, CAN);
                status = XMODEM_MAX_RETRIES;
                break;
            }

            if (started)
            {
                /* Receive a byte of data. */
                c = uart_getc(handle, 3);

                /* Check for end of transmission */
                if (c == EOT)
                {
#if DEBUG
                    System_printf("EOT!\n");
                    System_flush();
#endif
                    /* Acknowledge the EOT */
                    uart_putc(handle, ACK);
                    status = XMODEM_SUCCESS;
                    break;
                }

                /* Check for user abort */
                if (c == CAN)
                {
#if DEBUG
                    System_printf("CAN!\n");
                    System_flush();
#endif
                    /* Acknowledge the cancel */
                    uart_putc(handle, ACK);
                    status = XMODEM_CANCEL;
                    break;
                }

                if (c != SOH)
                {
#if DEBUG
                    System_printf("MISSING SOH - GOT %2x!\n", c);
                    System_flush();
#endif
                    /* invalid compliment block number */
                    uart_flush(handle);
                    /* NAK to get sender to send again */
                    uart_putc(handle, NAK);
                    /* Loop and try to re-synchronize */
                    ++try;
                    continue;
                }
            }

            started = true;

            /* Attempt to read block number */
            if ((c = uart_getc(handle, 3)) == -1)
            {
#if DEBUG
                System_printf("BLKNUM!\n");
                System_flush();
#endif
                /* Loop and try to re-synchronize */
                ++try;
                continue;
            }

            /* Save the block number as 8-bit */
            b = (uint8_t)(c & 0xFF);

            /* Attempt to read inverse block number */
            if ((c = uart_getc(handle, 3)) == -1)
            {
#if DEBUG
                System_printf("NBLKNUM!\n");
                System_flush();
#endif
                /* Loop and try to re-synchronize */
                ++try;
                continue;
            }

            /* Get inverse block number as 8-bit */
            d = (uint8_t)(c & 0xFF);

            /* Check block numbers match */
            if (b != (255 - d))
            {
#if DEBUG
                System_printf("CBLKNUM!\n");
                System_flush();
#endif
                /* invalid compliment block number */
                uart_flush(handle);
                /* NAK to get sender to send again */
                uart_putc(handle, NAK);
                /* Loop and try to re-synchronize */
                ++try;
                continue;
            }

            for (i=0; i < 128; i++)
            {
                if ((c = uart_getc(handle, 5)) == -1)
                    break;

                /* Store data byte in our packet buffer */
                xmodem_buff[i] = (uint8_t)c;

                /* Sum the byte into the CRC */
                crc = xmodem_crc(crc, xmodem_buff[i]);
            }

            /* Did we get a whole packet? */
            if (i != 128)
            {
#if DEBUG
                System_printf("SHORT BLOCKS %d!\n", i);
                System_flush();
#endif
                /* NAK to get sender to send again */
                uart_putc(handle, NAK);
                ++try;
                continue;
            }

            /* Read CRC high byte word */
            if ((c = uart_getc(handle, 3)) == -1)
            {
#if DEBUG
                System_printf("MSB!\n");
                System_flush();
#endif
                /* invalid compliment block number */
                uart_flush(handle);
                /* NAK to get sender to send again */
                uart_putc(handle, NAK);
                ++try;
                continue;
            }

            msb = (c & 0xFF);

            /* Read CRC low byte word */
            if ((c = uart_getc(handle, 3)) == -1)
            {
#if DEBUG
                System_printf("LSB!\n");
                System_flush();
#endif
                /* invalid compliment block number */
                uart_flush(handle);
                /* NAK to get sender to send again */
                uart_putc(handle, NAK);
                ++try;
                continue;
            }

            lsb = (c & 0xFF);

            rx_crc = (msb << 8) | lsb;

#if 0
            if (crc != rx_crc)
            {
#if DEBUG
                System_printf("CRC ERR BLOCK %d!\n", blknum);
                System_flush();
#endif
                /* invalid CRC */
                uart_flush(handle);
                /* NAK to get sender to send again */
                uart_putc(handle, NAK);
                ++try;
                continue;
            }
#endif
            /* Write the block to disk */
            if ((res =  xmodem_write_block(fp, xmodem_buff, 128)) != FR_OK)
            {
#if DEBUG
                System_printf("FILE WRITE ERR %d!\n", res);
                System_flush();
#endif
                /* Cancel the transfer! */
                uart_putc(handle, CAN);
                uart_putc(handle, CAN);
                uart_putc(handle, CAN);

                status = XMODEM_FILE_WRITE;
                break;
            }
#if DEBUG
            System_printf("ACK BLOCK %d!\n", blknum);
            System_flush();
#endif
            /* Send the ACK */
            uart_putc(handle, ACK);

            /* Toggle the STAT LED every good packet */
            GPIO_toggle(Board_STAT_LED);

            /* Block received successfully, reset retry counter */
            try = 0;

            /* Increment next expected block number */
            ++blknum;
        }
    }

#if DEBUG
    System_printf("EXIT XMDM %d!\n", blknum);
    System_flush();
#endif

    return status;
}


/*
 * Send a file using the XMODEM protocol.
 */

int xmodem_send(UART_Handle handle, FIL* fp)
{
    int status = 0;

    return status;
}

/* End-Of-File */
