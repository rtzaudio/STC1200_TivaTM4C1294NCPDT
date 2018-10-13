/*
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

/*
 *  ======== STC1200.c ========
 *  This file is responsible for setting up the board specific items for the
 *  STC1200 board.
 */

#include <stdint.h>
#include <stdbool.h>

#include <xdc/std.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/family/arm/m3/Hwi.h>

#include <inc/hw_ints.h>
#include <inc/hw_memmap.h>
#include <inc/hw_types.h>
#include <inc/hw_gpio.h>

#include <driverlib/flash.h>
#include <driverlib/gpio.h>
#include <driverlib/i2c.h>
#include <driverlib/pin_map.h>
#include <driverlib/pwm.h>
#include <driverlib/ssi.h>
#include <driverlib/sysctl.h>
#include <driverlib/uart.h>
#include <driverlib/udma.h>
#include <driverlib/eeprom.h>

#include "STC1200_TM4C1294NCPDT.h"

#ifndef TI_DRIVERS_UART_DMA
#define TI_DRIVERS_UART_DMA 0
#endif

#ifndef TI_EXAMPLES_PPP
#define TI_EXAMPLES_PPP 0
#else
/* prototype for NIMU init function */
extern int USBSerialPPP_NIMUInit();
#endif

/*
 *  =============================== DMA ===============================
 */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_ALIGN(dmaControlTable, 1024)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma data_alignment=1024
#elif defined(__GNUC__)
__attribute__ ((aligned (1024)))
#endif
static tDMAControlTable dmaControlTable[32];
static bool dmaInitialized = false;

/* Hwi_Struct used in the initDMA Hwi_construct call */
static Hwi_Struct dmaHwiStruct;

/*
 *  ======== dmaErrorHwi ========
 */
static Void dmaErrorHwi(UArg arg)
{
    System_printf("DMA error code: %d\n", uDMAErrorStatusGet());
    uDMAErrorStatusClear();
    System_abort("DMA error!!");
}

/*
 *  ======== STC1200_initDMA ========
 */
void STC1200_initDMA(void)
{
    Error_Block eb;
    Hwi_Params  hwiParams;

    if (!dmaInitialized) {
        Error_init(&eb);
        Hwi_Params_init(&hwiParams);
        Hwi_construct(&(dmaHwiStruct), INT_UDMAERR, dmaErrorHwi,
                      &hwiParams, &eb);
        if (Error_check(&eb)) {
            System_abort("Couldn't construct DMA error hwi");
        }

        SysCtlPeripheralEnable(SYSCTL_PERIPH_UDMA);
        uDMAEnable();
        uDMAControlBaseSet(dmaControlTable);

        dmaInitialized = true;
    }
}

/*
 *  =============================== General ===============================
 */
/*
 *  ======== STC1200_initGeneral ========
 */
void STC1200_initGeneral(void)
{ 
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOH);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOJ);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOK);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOL);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOM);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOP);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOQ);
    
    SysCtlPeripheralEnable(SYSCTL_PERIPH_QEI0);
    
    // Enable pin PL3 for QEI0 IDX0
    GPIOPinConfigure(GPIO_PL3_IDX0);
    GPIOPinTypeQEI(GPIO_PORTL_BASE, GPIO_PIN_3);

    // Enable pin PL1 for QEI0 PHA0
    GPIOPinConfigure(GPIO_PL1_PHA0);
    GPIOPinTypeQEI(GPIO_PORTL_BASE, GPIO_PIN_1);

    // Enable pin PL2 for QEI0 PHB0
    GPIOPinConfigure(GPIO_PL2_PHB0);
    GPIOPinTypeQEI(GPIO_PORTL_BASE, GPIO_PIN_2);

    // Initialize the EEPROM so we can access it later

    SysCtlPeripheralEnable(SYSCTL_PERIPH_EEPROM0);

    if (EEPROMInit() != EEPROM_INIT_OK)
        System_abort("EEPROMInit() failed!\n");

    uint32_t size = EEPROMSizeGet();
}

/*
 *  =============================== EMAC ===============================
 */
/* Place into subsections to allow the TI linker to remove items properly */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(EMAC_config, ".const:EMAC_config")
#pragma DATA_SECTION(emacHWAttrs, ".const:emacHWAttrs")
#pragma DATA_SECTION(NIMUDeviceTable, ".data:NIMUDeviceTable")
#endif

#include <ti/drivers/EMAC.h>
#include <ti/drivers/emac/EMACSnow.h>

/*
 *  Required by the Networking Stack (NDK). This array must be NULL terminated.
 *  This can be removed if NDK is not used.
 *  Double curly braces are needed to avoid GCC bug #944572
 *  https://bugs.launchpad.net/gcc-linaro/+bug/944572
 */
NIMU_DEVICE_TABLE_ENTRY NIMUDeviceTable[2] = {
    {
#if TI_EXAMPLES_PPP
        /* Use PPP driver for PPP example only */
        .init = USBSerialPPP_NIMUInit
#else
        /* Default: use Ethernet driver */
        .init = EMACSnow_NIMUInit
#endif
    },
    {NULL}
};

EMACSnow_Object emacObjects[STC1200_EMACCOUNT];

/*
 *  EMAC configuration structure
 *  Set user/company specific MAC octates. The following sets the address
 *  to ff-ff-ff-ff-ff-ff. Users need to change this to make the label on
 *  their boards.
 */
//unsigned char macAddress[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
unsigned char macAddress[6] = {0x00, 0x10, 0xfa, 0x6e, 0x38, 0x4a};

const EMACSnow_HWAttrs emacHWAttrs[STC1200_EMACCOUNT] = {
    {
        .baseAddr = EMAC0_BASE,
        .intNum = INT_EMAC0,
        .intPriority = (~0),
        .macAddress = macAddress
    }
};

const EMAC_Config EMAC_config[] = {
    {
        .fxnTablePtr = &EMACSnow_fxnTable,
        .object = &emacObjects[0],
        .hwAttrs = &emacHWAttrs[0]
    },
    {NULL, NULL, NULL}
};

/*
 *  ======== STC1200_initEMAC ========
 */
void STC1200_initEMAC(void)
{
    uint32_t ulUser0, ulUser1;

    /* Get the MAC address */
    FlashUserGet(&ulUser0, &ulUser1);

    if ((ulUser0 != 0xffffffff) && (ulUser1 != 0xffffffff)) {
        System_printf("Using MAC address in flash\n");
        /*
         *  Convert the 24/24 split MAC address from NV ram into a 32/16 split MAC
         *  address needed to program the hardware registers, then program the MAC
         *  address into the Ethernet Controller registers.
         */
        macAddress[0] = ((ulUser0 >>  0) & 0xff);
        macAddress[1] = ((ulUser0 >>  8) & 0xff);
        macAddress[2] = ((ulUser0 >> 16) & 0xff);
        macAddress[3] = ((ulUser1 >>  0) & 0xff);
        macAddress[4] = ((ulUser1 >>  8) & 0xff);
        macAddress[5] = ((ulUser1 >> 16) & 0xff);
    }
    else if (macAddress[0] == 0xff && macAddress[1] == 0xff &&
             macAddress[2] == 0xff && macAddress[3] == 0xff &&
             macAddress[4] == 0xff && macAddress[5] == 0xff) {
        System_abort("Change the macAddress variable to match your boards MAC sticker");
    }

    // Enable peripheral EPHY0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_EPHY0);
    
    // Enable pin PK6 for EPHY0 EN0LED1
    GPIOPinConfigure(GPIO_PK6_EN0LED1);
    GPIOPinTypeEthernetLED(GPIO_PORTK_BASE, GPIO_PIN_6);

    // Enable pin PK4 for EPHY0 EN0LED0
    GPIOPinConfigure(GPIO_PK4_EN0LED0);
    GPIOPinTypeEthernetLED(GPIO_PORTK_BASE, GPIO_PIN_4);

    /* Once EMAC_init is called, EMAC_config cannot be changed */
    EMAC_init();
}

/*
 *  =============================== GPIO ===============================
 */
/* Place into subsections to allow the TI linker to remove items properly */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(GPIOTiva_config, ".const:GPIOTiva_config")
#endif

#include <ti/drivers/GPIO.h>
#include <ti/drivers/gpio/GPIOTiva.h>

/*
 * Array of Pin configurations
 * NOTE: The order of the pin configurations must coincide with what was
 *       defined in DK_TM4C129X.h
 * NOTE: Pins not used for interrupts should be placed at the end of the
 *       array.  Callback entries can be omitted from callbacks array to
 *       reduce memory usage.
 */
GPIO_PinConfig gpioPinConfigs[STC1200_GPIOCOUNT] = {
	/*** INPUT PINS ***/
    /* STC1200_BTN_RESET */
    GPIOTiva_PH_0 | GPIO_CFG_INPUT | GPIO_CFG_IN_INT_RISING,
    /* STC1200_BTN_CUE */
    GPIOTiva_PH_1 | GPIO_CFG_INPUT | GPIO_CFG_IN_INT_RISING,
    /* STC1200_BTN_SEARCH */
    GPIOTiva_PH_2 | GPIO_CFG_INPUT | GPIO_CFG_IN_INT_RISING,
	/* STC1200_STOP_DETECT_N */
	GPIOTiva_PM_0 | GPIO_CFG_INPUT | GPIO_CFG_IN_INT_FALLING,
	/* STC1200_PLAY_DETECT_N */
	GPIOTiva_PM_1 | GPIO_CFG_INPUT | GPIO_CFG_IN_INT_FALLING,
	/* STC1200_DIPSW_CFG1 */
	GPIOTiva_PL_4 | GPIO_CFG_INPUT | GPIO_CFG_IN_PU,
	/* STC1200_DIPSW_CFG2 */
	GPIOTiva_PL_5 | GPIO_CFG_INPUT | GPIO_CFG_IN_PU,
	/* STC1200_SPEED_SELECT */
	GPIOTiva_PQ_0 | GPIO_CFG_INPUT,
    /*** OUTPUTS PINS ***/
    /* STC1200_EXPIO_PD4 */
    GPIOTiva_PD_4 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW,
    /* STC1200_EXPIO_PD5 */
    GPIOTiva_PD_5 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW,
    /* STC1200_EXPIO_PP2 */
    GPIOTiva_PP_2 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW,
    /* STC1200_EXPIO_PP3 */
    GPIOTiva_PP_3 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW,
    /* STC1200_EXPIO_PP4 */
    GPIOTiva_PP_4 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW,
    /* STC1200_EXPIO_PP5 */
    GPIOTiva_PP_5 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW,
    /* STC1200_EXPIO_PK2 */
    GPIOTiva_PK_2 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW,
    /* STC1200_EXPIO_PK3 */
    GPIOTiva_PK_3 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW,
    /* STC1200_RESET_AVR_N */
    GPIOTiva_PE_0 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_HIGH,
	/* STC1200_RESET_BL652 */
    GPIOTiva_PE_1 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW,
	/* STC1200_AIRLOAD*/
    GPIOTiva_PE_2 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW,
	/* STC1200_AUTORUN_N */
    GPIOTiva_PE_3 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_HIGH,
    /* STC1200_RS422_RE_N */
    GPIOTiva_PK_5 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_HIGH,
    /* STC1200_RS422_DE */
    GPIOTiva_PK_7 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW,
    /* STC1200_STOP_N */
    GPIOTiva_PM_2 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_HIGH,
    /* STC1200_PLAY_N */
    GPIOTiva_PM_3 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_HIGH,
    /* STC1200_FWD_N */
    GPIOTiva_PM_4 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_HIGH,
    /* STC1200_REW_N */
    GPIOTiva_PM_5 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_HIGH,
    /* STC1200_MOTION_REW */
    GPIOTiva_PM_6 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_HIGH,
    /* STC1200_MOTION_FWD */
    GPIOTiva_PM_7 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_HIGH,
    /* STC1200_SEARCHING */
    GPIOTiva_PQ_1 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_HIGH,
    /* STC1200_TAPE_DIR */
    GPIOTiva_PQ_2 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_HIGH,
    /* STC1200_LAMP_PLAY */
    GPIOTiva_PQ_3 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW,
    /* STC1200_LAMP_FWDREW */
    GPIOTiva_PL_0 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW,
    /* STC1200_STAT_LED */
    GPIOTiva_PF_4 | GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW,
};

/*
 * Array of callback function pointers
 * NOTE: The order of the pin configurations must coincide with what was
 *       defined in DK_TM4C129X.h
 * NOTE: Pins not used for interrupts can be omitted from callbacks array to
 *       reduce memory usage (if placed at end of gpioPinConfigs array).
 */
GPIO_CallbackFxn gpioCallbackFunctions[] = {
    NULL,  /* STC1200_BTN_RESET */
    NULL,  /* STC1200_BTN_CUE */
    NULL,  /* STC1200_BTN_SEARCH */
    NULL,  /* STC1200_STOP_DETECT_N */
    NULL,  /* STC1200_PLAY_DETECT_N */
};

/* The device-specific GPIO_config structure */
const GPIOTiva_Config GPIOTiva_config = {
    .pinConfigs = (GPIO_PinConfig *)gpioPinConfigs,
    .callbacks = (GPIO_CallbackFxn *)gpioCallbackFunctions,
    .numberOfPinConfigs = sizeof(gpioPinConfigs)/sizeof(GPIO_PinConfig),
    .numberOfCallbacks = sizeof(gpioCallbackFunctions)/sizeof(GPIO_CallbackFxn),
    .intPriority = (~0)
};

/*
 *  ======== STC1200_initGPIO ========
 */
void STC1200_initGPIO(void)
{
    // Enable pin PD4 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_4);	
    // Enable pin PD5 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_5);

    // Enable pin PE0 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTE_BASE, GPIO_PIN_0);
    // Enable pin PE1 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTE_BASE, GPIO_PIN_1);
    // Enable pin PE2 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTE_BASE, GPIO_PIN_2);    
    // Enable pin PE3 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTE_BASE, GPIO_PIN_3);

    // Enable pin PG0 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTG_BASE, GPIO_PIN_0);
    // Enable pin PG1 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTG_BASE, GPIO_PIN_1);
    
    // Enable pin PH0 for GPIOInput
    GPIOPinTypeGPIOInput(GPIO_PORTH_BASE, GPIO_PIN_0);
    // Enable pin PH1 for GPIOInput
    GPIOPinTypeGPIOInput(GPIO_PORTH_BASE, GPIO_PIN_1);
    // Enable pin PH2 for GPIOInput
    GPIOPinTypeGPIOInput(GPIO_PORTH_BASE, GPIO_PIN_2);

    // Enable pin PK2 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTK_BASE, GPIO_PIN_2);
    // Enable pin PK3 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTK_BASE, GPIO_PIN_3);
    // Enable pin PK5 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTK_BASE, GPIO_PIN_5);
    // Enable pin PK7 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTK_BASE, GPIO_PIN_7);
        
    // Enable pin PL4 for GPIOInput
    GPIOPinTypeGPIOInput(GPIO_PORTL_BASE, GPIO_PIN_4);
    // Enable pin PL5 for GPIOInput
    GPIOPinTypeGPIOInput(GPIO_PORTL_BASE, GPIO_PIN_5);    

    // Enable pin PM0 for GPIOInput
    GPIOPinTypeGPIOInput(GPIO_PORTM_BASE, GPIO_PIN_0);    
    // Enable pin PM1 for GPIOInput
    GPIOPinTypeGPIOInput(GPIO_PORTM_BASE, GPIO_PIN_1);
    // Enable pin PM2 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTM_BASE, GPIO_PIN_2);    
    // Enable pin PM3 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTM_BASE, GPIO_PIN_3);
    // Enable pin PM4 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTM_BASE, GPIO_PIN_4);
    // Enable pin PM5 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTM_BASE, GPIO_PIN_5);
    // Enable pin PM6 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTM_BASE, GPIO_PIN_6);
    // Enable pin PM7 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTM_BASE, GPIO_PIN_7);
    
    // Enable pin PP2 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTP_BASE, GPIO_PIN_2);
    // Enable pin PP3 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTP_BASE, GPIO_PIN_3);
    // Enable pin PP4 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTP_BASE, GPIO_PIN_4);
    // Enable pin PP5 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTP_BASE, GPIO_PIN_5);
    
    // Enable pin PQ0 for GPIOInput
    GPIOPinTypeGPIOInput(GPIO_PORTQ_BASE, GPIO_PIN_0);
    // Enable pin PQ1 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTQ_BASE, GPIO_PIN_1);
    // Enable pin PQ2 for GPIOOutput
    GPIOPinTypeGPIOOutput(GPIO_PORTQ_BASE, GPIO_PIN_2);

	/* Once GPIO_init is called, GPIO_config cannot be changed */
    GPIO_init();
}

/*
 *  =============================== I2C ===============================
 */
/* Place into subsections to allow the TI linker to remove items properly */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(I2C_config, ".const:I2C_config")
#pragma DATA_SECTION(i2cTivaHWAttrs, ".const:i2cTivaHWAttrs")
#endif

#include <ti/drivers/I2C.h>
#include <ti/drivers/i2c/I2CTiva.h>

I2CTiva_Object i2cTivaObjects[STC1200_I2CCOUNT];

const I2CTiva_HWAttrs i2cTivaHWAttrs[STC1200_I2CCOUNT] = {
    {
        .baseAddr = I2C0_BASE,
        .intNum = INT_I2C0,
        .intPriority = (~0)
    }
};

const I2C_Config I2C_config[] = {
    {
        .fxnTablePtr = &I2CTiva_fxnTable,
        .object = &i2cTivaObjects[0],
        .hwAttrs = &i2cTivaHWAttrs[0]
    },
    {NULL, NULL, NULL}
};

/*
 *  ======== STC1200_initI2C ========
 */
void STC1200_initI2C(void)
{

    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0);

	// Enable pin PB3 for I2C0 I2C0SDA
	GPIOPinConfigure(GPIO_PB3_I2C0SDA);
	GPIOPinTypeI2C(GPIO_PORTB_BASE, GPIO_PIN_3);

	// Enable pin PB2 for I2C0 I2C0SCL
	GPIOPinConfigure(GPIO_PB2_I2C0SCL);
	GPIOPinTypeI2CSCL(GPIO_PORTB_BASE, GPIO_PIN_2);

    I2C_init();
}

/*
 *  =============================== SDSPI ===============================
 */
/* Place into subsections to allow the TI linker to remove items properly */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(SDSPI_config, ".const:SDSPI_config")
#pragma DATA_SECTION(sdspiTivaHWattrs, ".const:sdspiTivaHWattrs")
#endif

#include <ti/drivers/SDSPI.h>
#include <ti/drivers/sdspi/SDSPITiva.h>

SDSPITiva_Object sdspiTivaObjects[STC1200_SDSPICOUNT];

const SDSPITiva_HWAttrs sdspiTivaHWattrs[STC1200_SDSPICOUNT] = {
    {
        .baseAddr = SSI1_BASE,			/* SPI base address */
        .portSCK  = GPIO_PORTB_BASE,	/* SPI SCK PORT */
        .pinSCK   = GPIO_PIN_5,			/* SCK PIN (PB5) */
        .portMISO = GPIO_PORTE_BASE,	/* SPI MISO PORT */
        .pinMISO  = GPIO_PIN_5,			/* MISO PIN (PE5) */
        .portMOSI = GPIO_PORTE_BASE,	/* SPI MOSI PORT */
        .pinMOSI  = GPIO_PIN_4,			/* MOSI PIN (PE4) */
        .portCS   = GPIO_PORTB_BASE,	/* GPIO CS PORT */
        .pinCS    = GPIO_PIN_4			/* CS PIN (PB4) */
    }
};

const SDSPI_Config SDSPI_config[] = {
    {
        .fxnTablePtr = &SDSPITiva_fxnTable,
        .object = &sdspiTivaObjects[0],
        .hwAttrs = &sdspiTivaHWattrs[0]
    },
    {NULL, NULL, NULL}
};
/*
 *  ======== STC1200_initSDSPI ========
 */
void STC1200_initSDSPI(void)
{
    /* Enable SD SSI peripherals */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI1);

    /* Configure pad settings */

    /* SCK (PB5) */
    GPIOPadConfigSet(GPIO_PORTB_BASE,
                     GPIO_PIN_5,
                     GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD);
    /* MOSI (PE4) */
    GPIOPadConfigSet(GPIO_PORTE_BASE,
                     GPIO_PIN_4,
                     GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD);
    /* MISO (PE5) */
    GPIOPadConfigSet(GPIO_PORTE_BASE,
                     GPIO_PIN_5,
                     GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD_WPU);
    /* CS (PB4) */
    GPIOPadConfigSet(GPIO_PORTB_BASE,
                     GPIO_PIN_4,
                     GPIO_STRENGTH_4MA, GPIO_PIN_TYPE_STD);

	/* SSI-1 Configure Pins */

	// Enable pin PE5 for SSI1 SSI1XDAT1
	GPIOPinConfigure(GPIO_PE5_SSI1XDAT1);
	GPIOPinTypeSSI(GPIO_PORTE_BASE, GPIO_PIN_5);
	// Enable pin PE4 for SSI1 SSI1XDAT0
	GPIOPinConfigure(GPIO_PE4_SSI1XDAT0);
	GPIOPinTypeSSI(GPIO_PORTE_BASE, GPIO_PIN_4);
	// Enable pin PB5 for SSI1 SSI1CLK
	GPIOPinConfigure(GPIO_PB5_SSI1CLK);
	GPIOPinTypeSSI(GPIO_PORTB_BASE, GPIO_PIN_5);
	// Enable pin PB4 for SSI1 SSI1FSS
	GPIOPinConfigure(GPIO_PB4_SSI1FSS);
	GPIOPinTypeSSI(GPIO_PORTB_BASE, GPIO_PIN_4);

	SDSPI_init();
}

/*
 *  =============================== SPI ===============================
 */
/* Place into subsections to allow the TI linker to remove items properly */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(SPI_config, ".const:SPI_config")
#pragma DATA_SECTION(spiTivaDMAHWAttrs, ".const:spiTivaDMAHWAttrs")
#endif

#include <ti/drivers/SPI.h>
#include <ti/drivers/spi/SPITivaDMA.h>

SPITivaDMA_Object spiTivaDMAObjects[STC1200_SPICOUNT];

#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_ALIGN(spiTivaDMAscratchBuf, 32)
#elif defined(__IAR_SYSTEMS_ICC__)
#pragma data_alignment=32
#elif defined(__GNUC__)
__attribute__ ((aligned (32)))
#endif
uint32_t spiTivaDMAscratchBuf[STC1200_SPICOUNT];


const SPITivaDMA_HWAttrs spiTivaDMAHWAttrs[STC1200_SPICOUNT] = {
    {
        .baseAddr = SSI0_BASE,
        .intNum = INT_SSI0,
        .intPriority = (~0),
        .scratchBufPtr = &spiTivaDMAscratchBuf[0],
        .defaultTxBufValue = 0,
        .rxChannelIndex = UDMA_CHANNEL_SSI0RX,
        .txChannelIndex = UDMA_CHANNEL_SSI0TX,
        .channelMappingFxn = uDMAChannelAssign,
        .rxChannelMappingFxnArg = UDMA_CH10_SSI0RX,
        .txChannelMappingFxnArg = UDMA_CH11_SSI0TX
    },
    {
        .baseAddr = SSI2_BASE,
        .intNum = INT_SSI2,
        .intPriority = (~0),
        .scratchBufPtr = &spiTivaDMAscratchBuf[1],
        .defaultTxBufValue = 0,
        .rxChannelIndex = UDMA_SEC_CHANNEL_UART2RX_12,
        .txChannelIndex = UDMA_SEC_CHANNEL_UART2TX_13,
        .channelMappingFxn = uDMAChannelAssign,
        .rxChannelMappingFxnArg = UDMA_CH12_SSI2RX,
        .txChannelMappingFxnArg = UDMA_CH13_SSI2TX
    },
    {
        .baseAddr = SSI3_BASE,
        .intNum = INT_SSI3,
        .intPriority = (~0),
        .scratchBufPtr = &spiTivaDMAscratchBuf[2],
        .defaultTxBufValue = 0,
        .rxChannelIndex = UDMA_SEC_CHANNEL_TMR2A_14,
        .txChannelIndex = UDMA_SEC_CHANNEL_TMR2B_15,
        .channelMappingFxn = uDMAChannelAssign,
        .rxChannelMappingFxnArg = UDMA_CH14_SSI3RX,
        .txChannelMappingFxnArg = UDMA_CH15_SSI3TX
    }
};

const SPI_Config SPI_config[] = {
    {
        .fxnTablePtr = &SPITivaDMA_fxnTable,
        .object = &spiTivaDMAObjects[0],
        .hwAttrs = &spiTivaDMAHWAttrs[0]
    },
    {
        .fxnTablePtr = &SPITivaDMA_fxnTable,
        .object = &spiTivaDMAObjects[1],
        .hwAttrs = &spiTivaDMAHWAttrs[1]
    },
    {
        .fxnTablePtr = &SPITivaDMA_fxnTable,
        .object = &spiTivaDMAObjects[2],
        .hwAttrs = &spiTivaDMAHWAttrs[2]
    },
    {NULL, NULL, NULL},
};

/*
 *  ======== STC1200_initSPI ========
 */
void STC1200_initSPI(void)
{
    /* Enable SSI peripherals */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI2);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI3);
     
	/* SSI-0 Configure Pins */

    // Enable pin PA4 for SSI0 SSI0XDAT0
    GPIOPinConfigure(GPIO_PA4_SSI0XDAT0);
    GPIOPinTypeSSI(GPIO_PORTA_BASE, GPIO_PIN_4);
    // Enable pin PA5 for SSI0 SSI0XDAT1
    GPIOPinConfigure(GPIO_PA5_SSI0XDAT1);
    GPIOPinTypeSSI(GPIO_PORTA_BASE, GPIO_PIN_5);
    // Enable pin PA3 for SSI0 SSI0FSS
    //GPIOPinConfigure(GPIO_PA3_SSI0FSS);
    //GPIOPinTypeSSI(GPIO_PORTA_BASE, GPIO_PIN_3);
    // Enable pin PA2 for SSI0 SSI0CLK
    GPIOPinConfigure(GPIO_PA2_SSI0CLK);
    GPIOPinTypeSSI(GPIO_PORTA_BASE, GPIO_PIN_2);	

	/* SSI-2 Configure Pins */

    // Enable pin PD3 for SSI2 SSI2CLK
    GPIOPinConfigure(GPIO_PD3_SSI2CLK);
    GPIOPinTypeSSI(GPIO_PORTD_BASE, GPIO_PIN_3);
    // Enable pin PD0 for SSI2 SSI2XDAT1
    GPIOPinConfigure(GPIO_PD0_SSI2XDAT1);
    GPIOPinTypeSSI(GPIO_PORTD_BASE, GPIO_PIN_0);
    // Enable pin PD2 for SSI2 SSI2FSS
    GPIOPinConfigure(GPIO_PD2_SSI2FSS);
    GPIOPinTypeSSI(GPIO_PORTD_BASE, GPIO_PIN_2);
    // Enable pin PD7 for SSI2 SSI2XDAT2
    // First open the lock and select the bits we want to modify in the GPIO commit register.
    HWREG(GPIO_PORTD_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
    HWREG(GPIO_PORTD_BASE + GPIO_O_CR) = 0x80;
    // Now modify the configuration of the pins that we unlocked.
    GPIOPinConfigure(GPIO_PD7_SSI2XDAT2);
    GPIOPinTypeSSI(GPIO_PORTD_BASE, GPIO_PIN_7);
    // Enable pin PD6 for SSI2 SSI2XDAT3
    GPIOPinConfigure(GPIO_PD6_SSI2XDAT3);
    GPIOPinTypeSSI(GPIO_PORTD_BASE, GPIO_PIN_6);
    // Enable pin PD1 for SSI2 SSI2XDAT0
    GPIOPinConfigure(GPIO_PD1_SSI2XDAT0);
    GPIOPinTypeSSI(GPIO_PORTD_BASE, GPIO_PIN_1);

	/* SSI-3 Configure Pins */

    // Enable pin PF3 for SSI3 SSI3CLK
    GPIOPinConfigure(GPIO_PF3_SSI3CLK);
    GPIOPinTypeSSI(GPIO_PORTF_BASE, GPIO_PIN_3);
    // Enable pin PF2 for SSI3 SSI3FSS
    GPIOPinConfigure(GPIO_PF2_SSI3FSS);
    GPIOPinTypeSSI(GPIO_PORTF_BASE, GPIO_PIN_2);
    // Enable pin PF1 for SSI3 SSI3XDAT0
    GPIOPinConfigure(GPIO_PF1_SSI3XDAT0);
    GPIOPinTypeSSI(GPIO_PORTF_BASE, GPIO_PIN_1);
    // Enable pin PF0 for SSI3 SSI3XDAT1
    GPIOPinConfigure(GPIO_PF0_SSI3XDAT1);
    GPIOPinTypeSSI(GPIO_PORTF_BASE, GPIO_PIN_0);

    STC1200_initDMA();
    SPI_init();
}
    
/*
 *  =============================== UART ===============================
 */
/* Place into subsections to allow the TI linker to remove items properly */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(UART_config, ".const:UART_config")
#pragma DATA_SECTION(uartTivaHWAttrs, ".const:uartTivaHWAttrs")
#endif

#include <ti/drivers/UART.h>
#if TI_DRIVERS_UART_DMA
#include <ti/drivers/uart/UARTTivaDMA.h>

UARTTivaDMA_Object uartTivaObjects[STC1200_UARTCOUNT];

const UARTTivaDMA_HWAttrs uartTivaHWAttrs[STC1200_UARTCOUNT] = {
    {
        .baseAddr       = UART0_BASE,
        .intNum         = INT_UART0,
        .intPriority    = (~0),
        .rxChannelIndex = UDMA_CH8_UART0RX,
        .txChannelIndex = UDMA_CH9_UART0TX,
    },
    {
        .baseAddr       = UART1_BASE,
        .intNum         = INT_UART1,
        .intPriority    = (~0),
        .rxChannelIndex = UDMA_CH8_UART1RX,
        .txChannelIndex = UDMA_CH9_UART1TX,
    },
    {
        .baseAddr       = UART2_BASE,
        .intNum         = INT_UART2,
        .intPriority    = (~0),
        .rxChannelIndex = UDMA_CH8_UART2RX,
        .txChannelIndex = UDMA_CH9_UART2TX,
    },
    {
        .baseAddr       = UART3_BASE,
        .intNum         = INT_UART3,
        .intPriority    = (~0),
        .rxChannelIndex = UDMA_CH16_UART3RX,
        .txChannelIndex = UDMA_CH16_UART3TX,
    },
    {
        .baseAddr       = UART4_BASE,
        .intNum         = INT_UART4,
        .intPriority    = (~0),
        .rxChannelIndex = UDMA_CH8_UART4RX,
        .txChannelIndex = UDMA_CH9_UART4TX,
    },
    {
        .baseAddr       = UART5_BASE,
        .intNum         = INT_UART5,
        .intPriority    = (~0),
        .rxChannelIndex = UDMA_CH6_UART5RX,
        .txChannelIndex = UDMA_CH7_UART5TX,
    },
    {
        .baseAddr       = UART6_BASE,
        .intNum         = INT_UART6,
        .intPriority    = (~0),
        .rxChannelIndex = UDMA_CH6_UART5RX,
        .txChannelIndex = UDMA_CH7_UART5TX,
    },
    {
        .baseAddr       = UART7_BASE,
        .intNum         = INT_UART7,
        .intPriority    = (~0),
        .rxChannelIndex = UDMA_CH20_UART7RX,
        .txChannelIndex = UDMA_CH21_UART7TX,
    },
};

const UART_Config UART_config[] = {
    {
        .fxnTablePtr = &UARTTivaDMA_fxnTable,
        .object = &uartTivaObjects[0],
        .hwAttrs = &uartTivaHWAttrs[0]
    },
    {
        .fxnTablePtr = &UARTTivaDMA_fxnTable,
        .object = &uartTivaObjects[1],
        .hwAttrs = &uartTivaHWAttrs[1]
    },
    {
        .fxnTablePtr = &UARTTivaDMA_fxnTable,
        .object = &uartTivaObjects[2],
        .hwAttrs = &uartTivaHWAttrs[2]
    },
    {
        .fxnTablePtr = &UARTTivaDMA_fxnTable,
        .object = &uartTivaObjects[3],
        .hwAttrs = &uartTivaHWAttrs[3]
    },
    {
        .fxnTablePtr = &UARTTivaDMA_fxnTable,
        .object = &uartTivaObjects[4],
        .hwAttrs = &uartTivaHWAttrs[4]
    },
    {
        .fxnTablePtr = &UARTTivaDMA_fxnTable,
        .object = &uartTivaObjects[5],
        .hwAttrs = &uartTivaHWAttrs[5]
    },
    {
        .fxnTablePtr = &UARTTivaDMA_fxnTable,
        .object = &uartTivaObjects[6],
        .hwAttrs = &uartTivaHWAttrs[6]
    },
    {
        .fxnTablePtr = &UARTTivaDMA_fxnTable,
        .object = &uartTivaObjects[7],
        .hwAttrs = &uartTivaHWAttrs[7]
    },
    {NULL, NULL, NULL}
};

#else

#include <ti/drivers/uart/UARTTiva.h>

UARTTiva_Object uartTivaObjects[STC1200_UARTCOUNT];
unsigned char uartTivaRingBuffer[STC1200_UARTCOUNT][32];

/* UART configuration structure */
const UARTTiva_HWAttrs uartTivaHWAttrs[STC1200_UARTCOUNT] = {
    {
        .baseAddr    = UART0_BASE,
        .intNum      = INT_UART0,
        .intPriority = (~0),
        .flowControl = UART_FLOWCONTROL_NONE,
        .ringBufPtr  = uartTivaRingBuffer[0],
        .ringBufSize = sizeof(uartTivaRingBuffer[0])
    },
    {
        .baseAddr    = UART1_BASE,
        .intNum      = INT_UART1,
        .intPriority = (~0),
        .flowControl = UART_FLOWCONTROL_NONE,
        .ringBufPtr  = uartTivaRingBuffer[1],
        .ringBufSize = sizeof(uartTivaRingBuffer[1])
    },
    {
        .baseAddr    = UART2_BASE,
        .intNum      = INT_UART2,
        .intPriority = (~0),
        .flowControl = UART_FLOWCONTROL_NONE,
        .ringBufPtr  = uartTivaRingBuffer[2],
        .ringBufSize = sizeof(uartTivaRingBuffer[2])
    },
    {
        .baseAddr    = UART3_BASE,
        .intNum      = INT_UART3,
        .intPriority = (~0),
        .flowControl = UART_FLOWCONTROL_NONE,
        .ringBufPtr  = uartTivaRingBuffer[3],
        .ringBufSize = sizeof(uartTivaRingBuffer[3])
    },
    {
        .baseAddr    = UART4_BASE,
        .intNum      = INT_UART4,
        .intPriority = (~0),
        .flowControl = UART_FLOWCONTROL_NONE,
        .ringBufPtr  = uartTivaRingBuffer[4],
        .ringBufSize = sizeof(uartTivaRingBuffer[4])
    },
    {
        .baseAddr    = UART5_BASE,
        .intNum      = INT_UART5,
        .intPriority = (~0),
        .flowControl = UART_FLOWCONTROL_NONE,
        .ringBufPtr  = uartTivaRingBuffer[5],
        .ringBufSize = sizeof(uartTivaRingBuffer[5])
    },
    {
        .baseAddr    = UART6_BASE,
        .intNum      = INT_UART6,
        .intPriority = (~0),
        .flowControl = UART_FLOWCONTROL_NONE,
        .ringBufPtr  = uartTivaRingBuffer[6],
        .ringBufSize = sizeof(uartTivaRingBuffer[6])
    },
    {
        .baseAddr    = UART7_BASE,
        .intNum      = INT_UART7,
        .intPriority = (~0),
        .flowControl = UART_FLOWCONTROL_NONE,
        .ringBufPtr  = uartTivaRingBuffer[7],
        .ringBufSize = sizeof(uartTivaRingBuffer[7])
    },
};

const UART_Config UART_config[] = {
    {
        .fxnTablePtr = &UARTTiva_fxnTable,
        .object = &uartTivaObjects[0],
        .hwAttrs = &uartTivaHWAttrs[0]
    },
    {
        .fxnTablePtr = &UARTTiva_fxnTable,
        .object = &uartTivaObjects[1],
        .hwAttrs = &uartTivaHWAttrs[1]
    },
    {
        .fxnTablePtr = &UARTTiva_fxnTable,
        .object = &uartTivaObjects[2],
        .hwAttrs = &uartTivaHWAttrs[2]
    },
    {
        .fxnTablePtr = &UARTTiva_fxnTable,
        .object = &uartTivaObjects[3],
        .hwAttrs = &uartTivaHWAttrs[3]
    },
    {
        .fxnTablePtr = &UARTTiva_fxnTable,
        .object = &uartTivaObjects[4],
        .hwAttrs = &uartTivaHWAttrs[4]
    },
    {
        .fxnTablePtr = &UARTTiva_fxnTable,
        .object = &uartTivaObjects[5],
        .hwAttrs = &uartTivaHWAttrs[5]
    },
    {
        .fxnTablePtr = &UARTTiva_fxnTable,
        .object = &uartTivaObjects[6],
        .hwAttrs = &uartTivaHWAttrs[6]
    },
    {
        .fxnTablePtr = &UARTTiva_fxnTable,
        .object = &uartTivaObjects[7],
        .hwAttrs = &uartTivaHWAttrs[7]
    },
    {NULL, NULL, NULL}
};
#endif /* TI_DRIVERS_UART_DMA */

/*
 *  ======== STC1200_initUART ========
 */
void STC1200_initUART(void)
{
	/* Enable UART Peripherals */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);    
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART2);        
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART3);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART4);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART5);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART6);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART7);

    // Enable pin PA0 for UART0 U0RX
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0);
    // Enable pin PA1 for UART0 U0TX
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_1);

    // Enable pin PB1 for UART1 U1TX
    GPIOPinConfigure(GPIO_PB1_U1TX);
    GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_1);
    // Enable pin PN1 for UART1 U1CTS
    GPIOPinConfigure(GPIO_PN1_U1CTS);
    GPIOPinTypeUART(GPIO_PORTN_BASE, GPIO_PIN_1);
    // Enable pin PN0 for UART1 U1RTS
    GPIOPinConfigure(GPIO_PN0_U1RTS);
    GPIOPinTypeUART(GPIO_PORTN_BASE, GPIO_PIN_0);
    // Enable pin PB0 for UART1 U1RX
    GPIOPinConfigure(GPIO_PB0_U1RX);
    GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_0);

    // Enable pin PA6 for UART2 U2RX
    GPIOPinConfigure(GPIO_PA6_U2RX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_6);
    // Enable pin PN2 for UART2 U2RTS
    GPIOPinConfigure(GPIO_PN2_U2RTS);
    GPIOPinTypeUART(GPIO_PORTN_BASE, GPIO_PIN_2);
    // Enable pin PN3 for UART2 U2CTS
    GPIOPinConfigure(GPIO_PN3_U2CTS);
    GPIOPinTypeUART(GPIO_PORTN_BASE, GPIO_PIN_3);
    // Enable pin PA7 for UART2 U2TX
    GPIOPinConfigure(GPIO_PA7_U2TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_7);

    // Enable pin PN4 for UART3 U3RTS
    GPIOPinConfigure(GPIO_PN4_U3RTS);
    GPIOPinTypeUART(GPIO_PORTN_BASE, GPIO_PIN_4);
    // Enable pin PJ1 for UART3 U3TX
    GPIOPinConfigure(GPIO_PJ1_U3TX);
    GPIOPinTypeUART(GPIO_PORTJ_BASE, GPIO_PIN_1);
    // Enable pin PN5 for UART3 U3CTS
    GPIOPinConfigure(GPIO_PN5_U3CTS);
    GPIOPinTypeUART(GPIO_PORTN_BASE, GPIO_PIN_5);
    // Enable pin PJ0 for UART3 U3RX
    GPIOPinConfigure(GPIO_PJ0_U3RX);
    GPIOPinTypeUART(GPIO_PORTJ_BASE, GPIO_PIN_0);

    // Enable pin PK1 for UART4 U4TX
    GPIOPinConfigure(GPIO_PK1_U4TX);
    GPIOPinTypeUART(GPIO_PORTK_BASE, GPIO_PIN_1);
    // Enable pin PK0 for UART4 U4RX
    GPIOPinConfigure(GPIO_PK0_U4RX);
    GPIOPinTypeUART(GPIO_PORTK_BASE, GPIO_PIN_0);

    // Enable pin PC6 for UART5 U5RX
    GPIOPinConfigure(GPIO_PC6_U5RX);
    GPIOPinTypeUART(GPIO_PORTC_BASE, GPIO_PIN_6);
    // Enable pin PC7 for UART5 U5TX
    GPIOPinConfigure(GPIO_PC7_U5TX);
    GPIOPinTypeUART(GPIO_PORTC_BASE, GPIO_PIN_7);

    // Enable pin PP1 for UART6 U6TX
    GPIOPinConfigure(GPIO_PP1_U6TX);
    GPIOPinTypeUART(GPIO_PORTP_BASE, GPIO_PIN_1);
    // Enable pin PP0 for UART6 U6RX
    GPIOPinConfigure(GPIO_PP0_U6RX);
    GPIOPinTypeUART(GPIO_PORTP_BASE, GPIO_PIN_0);

    // Enable pin PC4 for UART7 U7RX
    GPIOPinConfigure(GPIO_PC4_U7RX);
    GPIOPinTypeUART(GPIO_PORTC_BASE, GPIO_PIN_4);
    // Enable pin PC5 for UART7 U7TX
    GPIOPinConfigure(GPIO_PC5_U7TX);
    GPIOPinTypeUART(GPIO_PORTC_BASE, GPIO_PIN_5);
    
    /* Initialize the UART driver */
#if TI_DRIVERS_UART_DMA
    STC1200_initDMA();
#endif
    UART_init();
}

/*
 *  =============================== Watchdog ===============================
 */
/* Place into subsections to allow the TI linker to remove items properly */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(Watchdog_config, ".const:Watchdog_config")
#pragma DATA_SECTION(watchdogTivaHWAttrs, ".const:watchdogTivaHWAttrs")
#endif

#include <ti/drivers/Watchdog.h>
#include <ti/drivers/watchdog/WatchdogTiva.h>

WatchdogTiva_Object watchdogTivaObjects[STC1200_WATCHDOGCOUNT];

const WatchdogTiva_HWAttrs watchdogTivaHWAttrs[STC1200_WATCHDOGCOUNT] = {
    {
        .baseAddr = WATCHDOG0_BASE,
        .intNum = INT_WATCHDOG,
        .intPriority = (~0),
        .reloadValue = 80000000 // 1 second period at default CPU clock freq
    },
};

const Watchdog_Config Watchdog_config[] = {
    {
        .fxnTablePtr = &WatchdogTiva_fxnTable,
        .object = &watchdogTivaObjects[0],
        .hwAttrs = &watchdogTivaHWAttrs[0]
    },
    {NULL, NULL, NULL},
};

/*
 *  ======== STC1200_initWatchdog ========
 *
 * NOTE: To use the other watchdog timer with base address WATCHDOG1_BASE,
 *       an additional function call may need be made to enable PIOSC. Enabling
 *       WDOG1 does not do this. Enabling another peripheral that uses PIOSC
 *       such as ADC0 or SSI0, however, will do so. Example:
 *
 *       SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
 *       SysCtlPeripheralEnable(SYSCTL_PERIPH_WDOG1);
 *
 *       See the following forum post for more information:
 *       http://e2e.ti.com/support/microcontrollers/stellaris_arm_cortex-m3_microcontroller/f/471/p/176487/654390.aspx#654390
 */
void STC1200_initWatchdog(void)
{
    /* Enable peripherals used by Watchdog */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_WDOG0);

    /* Initialize the Watchdog driver */
    Watchdog_init();
}

/*
 *  =============================== WiFi ===============================
 */
#if TI_DRIVERS_WIFI_INCLUDED
/* Place into subsections to allow the TI linker to remove items properly */
#if defined(__TI_COMPILER_VERSION__)
#pragma DATA_SECTION(WiFi_config, ".const:WiFi_config")
#pragma DATA_SECTION(wiFiCC3100HWAttrs, ".const:wiFiCC3100HWAttrs")
#endif

#include <ti/drivers/WiFi.h>
#include <ti/drivers/wifi/WiFiCC3100.h>

WiFiCC3100_Object wiFiCC3100Objects[DK_TM4C129X_WIFICOUNT];

const WiFiCC3100_HWAttrs wiFiCC3100HWAttrs[DK_TM4C129X_WIFICOUNT] = {
    {
        .irqPort = GPIO_PORTS_BASE,
        .irqPin = GPIO_PIN_2,
        .irqIntNum = INT_GPIOS,

        .csPort = GPIO_PORTQ_BASE,
        .csPin = GPIO_PIN_7,

        .enPort = GPIO_PORTN_BASE,
        .enPin = GPIO_PIN_7
    }
};

const WiFi_Config WiFi_config[] = {
    {
        .fxnTablePtr = &WiFiCC3100_fxnTable,
        .object = &wiFiCC3100Objects[0],
        .hwAttrs = &wiFiCC3100HWAttrs[0]
    },
    {NULL,NULL, NULL},
};

/*
 *  ======== STC1200_initWiFi ========
 */
void STC1200_initWiFi(void)
{
    /* Configure EN & CS pins to disable CC3100 */
    GPIOPinTypeGPIOOutput(GPIO_PORTQ_BASE, GPIO_PIN_7);
    GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_7);
    GPIOPinWrite(GPIO_PORTQ_BASE, GPIO_PIN_7, GPIO_PIN_7);
    GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_7, 0);

    /* Configure SSI2 for CC3100 */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI2);
    GPIOPinConfigure(GPIO_PG7_SSI2CLK);
    GPIOPinConfigure(GPIO_PG5_SSI2XDAT0);
    GPIOPinConfigure(GPIO_PG4_SSI2XDAT1);
    GPIOPinTypeSSI(GPIO_PORTG_BASE, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7);

    /* Configure IRQ pin */
    GPIOPinTypeGPIOInput(GPIO_PORTS_BASE, GPIO_PIN_2);
    GPIOPadConfigSet(GPIO_PORTS_BASE, GPIO_PIN_2, GPIO_STRENGTH_2MA,
                     GPIO_PIN_TYPE_STD_WPD);
    GPIOIntTypeSet(GPIO_PORTS_BASE, GPIO_PIN_2, GPIO_RISING_EDGE);

    SPI_init();
    STC1200_initDMA();

    WiFi_init();}
#endif /* TI_DRIVERS_WIFI_INCLUDED */
