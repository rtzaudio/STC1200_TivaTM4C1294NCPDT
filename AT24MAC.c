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
                   SYSCTL_XTAL_16MHZ);
#endif

    // The I2C0 peripheral must be enabled before use.

    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0);

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

    object->ui32Base = I2C0_BASE;

#if defined(TARGET_IS_TM4C129_RA0) ||                                         \
    defined(TARGET_IS_TM4C129_RA1) ||                                         \
    defined(TARGET_IS_TM4C129_RA2)
    I2CMasterInitExpClk(object->ui32Base, ui32SysClock, false);
#else
    I2CMasterInitExpClk(object->ui32Base, SysCtlClockGet(), false);
#endif
}

//*****************************************************************************
// Perform an I2C transaction to the slave.
//*****************************************************************************

int AT24MAC_transfer(AT24MAC_Object* object, AT24MAC_Transaction* transaction)
{
#if 0
    size_t ui32Index;

    for(ui32Index=0; ui32Index < pTransaction->data_size; ui32Index++)
    {
        // Place the data to be sent in the data register
        I2CMasterDataPut(object->ui32Base, pui32DataTx[ui32Index]);

        // Initiate send of data from the master.  Since the loopback
        // mode is enabled, the master and slave units are connected
        // allowing us to receive the same data that we sent out.
        I2CMasterControl(object->ui32Base, I2C_MASTER_CMD_SINGLE_SEND);

        // Wait until the slave has received and acknowledged the data.
        while(!(I2CSlaveStatus(object->ui32Base) & I2C_SLAVE_ACT_RREQ))
        {
        }

        // Read the data from the slave.
        pui32DataRx[ui32Index] = I2CSlaveDataGet(object->ui32Base);

        // Wait until master module is done transferring.
        while(I2CMasterBusy(object->ui32Base))
        {
        }

        // Display the data that the slave has received.
        //UARTprintf("Received: '%c'\n", pui32DataRx[ui32Index]);
    }
#endif
    return 0;
}

//*****************************************************************************
// Configure the I2C0 master and slave and connect them using loopback mode.
//*****************************************************************************

bool AT24MAC_GUID_read(AT24MAC_Object* object,
                       uint8_t ui8SerialNum[16], uint8_t ui8MAC[6])
{
    bool ret;
    uint8_t txByte;
    AT24MAC_Transaction i2cTransaction;

    /* default is all FF's  in case read fails*/
    memset(ui8SerialNum, 0xFF, sizeof(ui8SerialNum));
    memset(ui8MAC, 0xFF, sizeof(ui8MAC));

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
