/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Gate.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Mailbox.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Queue.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>

/* TI-RTOS Driver files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/I2C.h>

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "inc/hw_i2c.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "driverlib/i2c.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"

#include "AT24MAC.h"

/*
 * Specific I2C CMD MACROs that are not defined in I2C.h are defined here. Their
 * equivalent values are taken from the existing MACROs in I2C.h
 */
#ifndef I2C_MASTER_CMD_BURST_RECEIVE_START_NACK
#define I2C_MASTER_CMD_BURST_RECEIVE_START_NACK  I2C_MASTER_CMD_BURST_SEND_START
#endif

#ifndef I2C_MASTER_CMD_BURST_RECEIVE_STOP
#define I2C_MASTER_CMD_BURST_RECEIVE_STOP        I2C_MASTER_CMD_BURST_RECEIVE_ERROR_STOP
#endif

#ifndef I2C_MASTER_CMD_BURST_RECEIVE_CONT_NACK
#define I2C_MASTER_CMD_BURST_RECEIVE_CONT_NACK   I2C_MASTER_CMD_BURST_SEND_CONT
#endif

//*****************************************************************************
// Configure the I2C0 for master operation to the AT24MAC serial EEPROM.
//*****************************************************************************

void AT24MAC_init(AT24MAC_Object* object)
{
#if defined(TARGET_IS_TM4C129_RA0) ||                                         \
    defined(TARGET_IS_TM4C129_RA1) ||                                         \
    defined(TARGET_IS_TM4C129_RA2)
    uint32_t ui32SysClock;
#endif

    // Set the clocking to run directly from the external crystal/oscillator.
    // The SYSCTL_XTAL_ value must be changed to match the value of the
    // crystal on your board.

#if defined(TARGET_IS_TM4C129_RA0) ||                                         \
    defined(TARGET_IS_TM4C129_RA1) ||                                         \
    defined(TARGET_IS_TM4C129_RA2)
    ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
                                       SYSCTL_OSC_MAIN |
                                       SYSCTL_USE_OSC), 25000000);
#else
    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN |
                   SYSCTL_XTAL_25MHZ);
#endif

    // The I2C0 peripheral must be enabled before use.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_I2C0));

    // For this example I2C0 is used with PortB[3:2].  The actual port and
    // pins used may be different on your part, consult the data sheet for
    // more information.  GPIO port B needs to be enabled so these pins can
    // be used.

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);

    // Configure the pin muxing for I2C0 functions on port B2 and B3.
    // This step is not necessary if your part does not support pin muxing.

    GPIOPinConfigure(GPIO_PB2_I2C0SCL);
    GPIOPinConfigure(GPIO_PB3_I2C0SDA);

    // Select the I2C function for these pins.  This function will also
    // configure the GPIO pins pins for I2C operation, setting them to
    // open-drain operation with weak pull-ups.  Consult the data sheet
    // to see which functions are allocated per pin.

    GPIOPinTypeI2CSCL(GPIO_PORTB_BASE, GPIO_PIN_2);
    GPIOPinTypeI2C(GPIO_PORTB_BASE, GPIO_PIN_3);

    // Enable and initialize the I2C0 master module.  Use the system clock for
    // the I2C0 module.  The last parameter sets the I2C data transfer rate.
    // If false the data rate is set to 100kbps and if true the data rate will
    // be set to 400kbps.  For this example we will use a data rate of 100kbps.

    object->baseAddr = I2C0_BASE;

#if defined(TARGET_IS_TM4C129_RA0) ||                                         \
    defined(TARGET_IS_TM4C129_RA1) ||                                         \
    defined(TARGET_IS_TM4C129_RA2)
    I2CMasterInitExpClk(object->baseAddr, ui32SysClock, false);
#else
    I2CMasterInitExpClk(object->baseAddr, SysCtlClockGet(), false);
#endif

    SysCtlDelay(10000);

    // Enable Master Mode
    I2CMasterEnable(object->baseAddr);
}

//*****************************************************************************
// Perform an I2C transaction to the slave.
//*****************************************************************************

bool AT24MAC_transfer(AT24MAC_Object* object, AT24MAC_Transaction* transaction)
{
    size_t i;
    uint32_t errStatus;

    /* Check if anything needs to be written or read */
    if ((!transaction->writeCount) && (!transaction->readCount))
        return false;

    /*
     * Handle Transmit mode first
     */

    if (transaction->writeCount)
    {
        uint8_t* writeBuf = (uint8_t*)transaction->writeBuf;
        size_t writeCount = transaction->writeCount;

        if (writeCount == 1)
        {
            /* Specify the I2C slave write address */
            I2CMasterSlaveAddrSet(object->baseAddr, transaction->slaveAddress, false);
            /* Write data contents into data register */
            I2CMasterDataPut(object->baseAddr, *writeBuf);
            SysCtlDelay(500);
            /* Start the I2C transfer in master transmit mode */
            I2CMasterControl(object->baseAddr, I2C_MASTER_CMD_SINGLE_SEND);
            SysCtlDelay(500);
            /* Wait until transmit completes */
            while (I2CMasterBusy(object->baseAddr));
            SysCtlDelay(500);
            /* Get the status of the I2C controller */
            errStatus = I2CMasterErr(object->baseAddr);
            if (errStatus != I2C_MASTER_ERR_NONE)
            {
                System_printf("I2C Error #1\n");
                System_flush();
                return false;
            }
        }
        else
        {
            /* Specify the I2C slave write address */
            I2CMasterSlaveAddrSet(object->baseAddr, transaction->slaveAddress, false);
            /* Write first burst data */
            I2CMasterDataPut(object->baseAddr, *writeBuf++);
            /* Start the I2C transfer in master transmit mode */
            I2CMasterControl(object->baseAddr, I2C_MASTER_CMD_BURST_SEND_START);
            /* Wait until transmit completes */
            //while (I2CMasterBusy(object->baseAddr));
            /* Get the status of the I2C controller */
            //errStatus = I2CMasterErr(object->baseAddr);
            //if (errStatus != I2C_MASTER_ERR_NONE)
            //{
            //    System_printf("I2C Error #2\n");
            //    System_flush();
            //    return false;
            //}

            if (writeCount > 2)
            {
                for (i=1; i < writeCount-1; i++)
                {
                    /* Specify the I2C slave write address */
                    I2CMasterSlaveAddrSet(object->baseAddr, transaction->slaveAddress, false);
                    /* Write data contents into data register */
                    I2CMasterDataPut(object->baseAddr, *writeBuf++);
                    /* Start the I2C transfer in master transmit mode */
                    I2CMasterControl(object->baseAddr, I2C_MASTER_CMD_BURST_SEND_CONT);
                    /* Wait until transmit completes */
                    while (I2CMasterBusy(object->baseAddr));
                    /* Get the status of the I2C controller */
                    errStatus = I2CMasterErr(object->baseAddr);
                    {
                        System_printf("I2C Error #3\n");
                        System_flush();
                        return false;
                    }
                }
            }

            /* Specify the I2C slave write address */
            I2CMasterSlaveAddrSet(object->baseAddr, transaction->slaveAddress, false);
            /* Write last burst finish data */
            I2CMasterDataPut(object->baseAddr, *writeBuf++);
            /* Start the I2C transfer in master transmit mode */
            I2CMasterControl(object->baseAddr, I2C_MASTER_CMD_BURST_SEND_FINISH);
            /* Wait until transmit completes */
            while (I2CMasterBusy(object->baseAddr));
            /* Get the status of the I2C controller */
            errStatus = I2CMasterErr(object->baseAddr);
            {
                System_printf("I2C Error #4\n");
                System_flush();
                return false;
            }
        }
    }

    /*
     * Handle Receive Case if requested
     */

    if (transaction->readCount)
    {
        uint8_t* readBuf = (uint8_t*)transaction->readBuf;
        size_t readCount = transaction->readCount;

        if (readCount == 1)
        {
            /* Specify the I2C slave read address */
            I2CMasterSlaveAddrSet(object->baseAddr, transaction->slaveAddress, true);
             /* Send NACK because it's the last byte to be received
              * There is no NACK macro equivalent (0x00000001) so
              * I2C_MASTER_CMD_BURST_RECEIVE_CONT_NACK is used
              */
             I2CMasterControl(object->baseAddr, I2C_MASTER_CMD_BURST_RECEIVE_CONT_NACK);
             /* Wait until receive completes */
             while (I2CMasterBusy(object->baseAddr));
             /* Get the status of the I2C controller */
             if ((errStatus = I2CMasterErr(object->baseAddr)) != I2C_MASTER_ERR_NONE)
             {
                 System_printf("I2C Error #5n");
                 System_flush();
                 return false;
             }
             /* Read the data from the slave */
             *readBuf++ = I2CSlaveDataGet(object->baseAddr);
        }
        else
        {
            /* Specify the I2C slave read address */
            I2CMasterSlaveAddrSet(object->baseAddr, transaction->slaveAddress, true);
            /* Read the first receive byte of burst start */
            I2CMasterControl(object->baseAddr, I2C_MASTER_CMD_BURST_RECEIVE_START);
            /* Wait until receive completes */
            while (I2CMasterBusy(object->baseAddr));
            /* Get the status of the I2C controller */
            //if ((errStatus = I2CMasterErr(object->baseAddr)) != I2C_MASTER_ERR_NONE)
            //{
            //    System_printf("I2C Error #6\n");
            //    System_flush();
            //    return false;
            //}
            /* Read the data from the slave */
            *readBuf++ = I2CSlaveDataGet(object->baseAddr);

            if (readCount > 2)
            {
                for (i=1; i < readCount-1; i++)
                {
                    /* Specify the I2C slave read address */
                    I2CMasterSlaveAddrSet(object->baseAddr, transaction->slaveAddress, true);
                    /* More data to be received */
                    I2CMasterControl(object->baseAddr, I2C_MASTER_CMD_BURST_RECEIVE_CONT);
                    /* Wait until receive completes */
                    while (I2CMasterBusy(object->baseAddr));
                    /* Get the status of the I2C controller */
                    //if ((errStatus = I2CMasterErr(object->baseAddr)) != I2C_MASTER_ERR_NONE)
                    //{
                    //    System_printf("I2C Error #7\n");
                    //    System_flush();
                    //    return false;
                    //}
                    /* Read the data from the slave */
                    *readBuf++ = I2CSlaveDataGet(object->baseAddr);
                }
            }

            /* Specify the I2C slave read address */
            I2CMasterSlaveAddrSet(object->baseAddr, transaction->slaveAddress, true);
            /* Finish the receive */
            I2CMasterControl(object->baseAddr, I2C_MASTER_CMD_BURST_RECEIVE_FINISH);
            /* Wait until receive completes */
            while (I2CMasterBusy(object->baseAddr));
            /* Get the status of the I2C controller */
            //if ((errStatus = I2CMasterErr(object->baseAddr)) != I2C_MASTER_ERR_NONE)
            //{
            //    System_printf("I2C Error #8\n");
            //    System_flush();
            //    return false;
            //}
            /* Read the data from the slave */
            *readBuf++ = I2CSlaveDataGet(object->baseAddr);

            /* Specify the I2C slave read address */
            I2CMasterSlaveAddrSet(object->baseAddr, transaction->slaveAddress, true);

            /* No more data needs to be received, so follow up with a STOP bit
             * Again, there is no equivalent macro (0x00000004) so
             * I2C_MASTER_CMD_BURST_RECEIVE_STOP is used
             */
            I2CMasterControl(object->baseAddr, I2C_MASTER_CMD_BURST_RECEIVE_STOP);

            /* Get the status of the I2C controller */
            if ((errStatus = I2CMasterErr(object->baseAddr)) != I2C_MASTER_ERR_NONE)
            {
                System_printf("I2C Error #9\n");
                System_flush();
                return false;
            }
        }
    }

    return true;
}

//*****************************************************************************
// Read the 128-bit serial number and 48-bit MAC address from the EEPROM.
//*****************************************************************************

bool AT24MAC_GUID_read(AT24MAC_Object* object,
                       uint8_t ui8SerialNum[16], uint8_t ui8MAC[6])
{
    bool ret;
    uint8_t txByte;
    AT24MAC_Transaction i2cTransaction;

    /* default is all FF's  in case read fails*/
    memset(ui8SerialNum, 0xFF, 16);
    memset(ui8MAC, 0xFF, 6);

    /* Note the Upper bit of the word address must be set
     * in order to read the serial number. Thus 80H would
     * set the starting address to zero prior to reading
     * this sixteen bytes of serial number data.
     */

    txByte = 0x80;

    i2cTransaction.slaveAddress = AT24MAC_GUID128_ADDR;
    i2cTransaction.writeBuf     = &txByte;
    i2cTransaction.writeCount   = 1;
    i2cTransaction.readBuf      = ui8SerialNum;
    i2cTransaction.readCount    = 16;

    ret = AT24MAC_transfer(object, &i2cTransaction);

    if (!ret)
        return false;

    /* Now read the 6-byte 48-bit MAC at address 0x9A. The EUI-48 address
     * contains six or eight bytes. The first three bytes of the  UI read-only
     * address field are called the Organizationally Unique Identifier (OUI)
     * and the IEEE Registration Authority has assigned FCC23Dh as the Atmel OUI.
     */

    txByte = 0x9A;

    i2cTransaction.slaveAddress = AT24MAC_MAC48_ADDR;
    i2cTransaction.writeBuf     = &txByte;
    i2cTransaction.writeCount   = 1;
    i2cTransaction.readBuf      = ui8MAC;
    i2cTransaction.readCount    = 6;

    ret = AT24MAC_transfer(object, &i2cTransaction);

    return ret;
}

/* End-Of-File */
