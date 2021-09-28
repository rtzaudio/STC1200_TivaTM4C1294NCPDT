/* ============================================================================
 *
 * DTC-1200 & STC-1200 Digital Transport Controllers for
 * Ampex MM-1200 Tape Machines
 *
 * Copyright (C) 2016-2020, RTZ Professional Audio, LLC
 * All Rights Reserved
 *
 * RTZ is registered trademark of RTZ Professional Audio, LLC
 *
 * ============================================================================
 *
 * Copyright (c) 2014, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#define DEBUG_XMDM  0

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

static int uart_putc(UART_Handle handle, uint8_t ch)
{
    return UART_write(handle, &ch, 1);
}

static void uart_flush(UART_Handle handle)
{
#if 0
    int n;
    uint8_t ch;

    while (1)
    {
        n = UART_read(handle, &ch, 1);

        if (n == UART_ERROR)
            break;
    }
#endif
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

    status = XMODEM_NO_RESPONSE;

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
#if DEBUG_XMDM
        System_printf("Got SOH!\n");
        System_flush();
#endif
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
#if DEBUG_XMDM
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
#if DEBUG_XMDM
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
#if DEBUG_XMDM
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
#if DEBUG_XMDM
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
#if DEBUG_XMDM
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
#if DEBUG_XMDM
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

            crc = 0;

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
#if DEBUG_XMDM
                System_printf("SHORT BLOCK %d!\n", i);
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
#if DEBUG_XMDM
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
#if DEBUG_XMDM
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

            if (crc != rx_crc)
            {
#if DEBUG_XMDM
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

            /* Write the block to disk */
            if ((res =  xmodem_write_block(fp, xmodem_buff, 128)) != FR_OK)
            {
#if DEBUG_XMDM
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
#if DEBUG_XMDM
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

#if DEBUG_XMDM
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
    int i;
    int c;
    int retry;
    bool lastblock;
    bool crcmode = false;
    int status = XMODEM_NO_RESPONSE;
    uint32_t bytesRead = 0;
    uint16_t crc = 0;
    uint8_t csum = 0;
    uint8_t blknum = 1;
    FRESULT res;

    for (retry=0; retry < 20; retry++)
    {
        /* Wait for a byte of data. */
        if ((c = uart_getc(handle, 3)) == UART_ERROR)
        {
            continue;
        }

        /* Check for user aborting */
        if ((c == EOT) || (c == CAN))
        {
#if DEBUG_XMDM
            System_printf("Send Canceled\n", blknum);
            System_flush();
#endif
            status = XMODEM_CANCEL;
            break;
        }

        if (c == NAK)
        {
#if DEBUG_XMDM
            System_printf("Checksum Mode Requested\n");
            System_flush();
#endif
            crcmode = false;
            status = XMODEM_SUCCESS;
            break;
        }

        /* Start of header received, start sending packet */
        if (c == 'C')
        {
#if DEBUG_XMDM
            System_printf("CRC Mode Requested\n", blknum);
            System_flush();
#endif
            crcmode = true;
            status = XMODEM_SUCCESS;
            break;
        }
    }

    if (status == XMODEM_SUCCESS)
    {
        retry = 0;
        bytesRead = 0;

        while(true)
        {
            if (++retry > 5)
            {
#if DEBUG_XMDM
                System_printf("Too Many Resends, aborting...\n", blknum);
                System_flush();
#endif

                uart_putc(handle, CAN);
                uart_putc(handle, CAN);
                uart_putc(handle, CAN);
                status = XMODEM_MAX_RETRIES;
                break;
            }

            lastblock = false;

            if (retry == 1)
            {
                memset(xmodem_buff, 0x1A, sizeof(xmodem_buff));

                if ((res = f_read(fp, xmodem_buff, 128, &bytesRead)) != FR_OK)
                {
                    status = XMODEM_FILE_READ;
                    break;
                }

                if (bytesRead < 128)
                    lastblock = true;
            }

            crc = 0;
            csum = 0;

            uart_putc(handle, SOH);
            uart_putc(handle, blknum);
            uart_putc(handle, 255 - blknum);

            for (i=0; i < 128; i++)
            {
                /* send a byte out */
                uart_putc(handle, xmodem_buff[i]);

                /* Sum the byte into the CRC */
                crc = xmodem_crc(crc, xmodem_buff[i]);

                /* Sum the checksum */
                csum += xmodem_buff[i];
            }

            if (crcmode)
            {
                /* Send CRC MSB */
                uart_putc(handle, (uint8_t)((crc >> 8) & 0xFF));
                /* Send CRC LSB */
                uart_putc(handle, (uint8_t)(crc & 0xFF));
            }
            else
            {
                /* Send checksum */
                uart_putc(handle, (uint8_t)(csum & 0xFF));
            }

            /* Wait for ACK or NAK */
            if ((c = uart_getc(handle, 3)) == UART_ERROR)
            {
                /* Missing ACK/NAK, try again */
                status = XMODEM_TIMEOUT;
                continue;
            }

            /* If NAK, then loop and retry sending the current block */
            if (c == NAK)
            {
                continue;
            }

            /* If user canceled, then exit send */
            if ((c == EOT) || (c == CAN))
            {
                status = XMODEM_CANCEL;
                break;
            }

            if (c == ACK)
            {
                if (!lastblock)
                {
                    ++blknum;
                    retry = 0;
                    continue;
                }
            }

            if (lastblock)
            {
#if DEBUG_XMDM
                System_printf("Last block sent, sending EOT\n");
                System_flush();
#endif
                /* The last block of the file was sent. Now send an EOT
                 * to indicate the transmission is complete. The
                 * receiver should ACK back to confirm the EOT.
                 */
                uart_putc(handle, EOT);

                /* Try to read back a response from EOT */
                if ((c = uart_getc(handle, 3)) == UART_ERROR)
                {
                    status = XMODEM_TIMEOUT;
                    break;
                }

                /* If ACK, the receiver has acknowledged the end
                 * of the transmission successfully.
                 */
                if (c == ACK)
                {
#if DEBUG_XMDM
                    System_printf("EOT Acknowledged\n");
                    System_flush();
#endif
                    status = XMODEM_SUCCESS;
                }
                break;
            }
        }
    }

    return status;
}

/* End-Of-File */
