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
/** ============================================================================
 *  @file       STC1200.h
 *
 *  @brief      STC1200 Board Specific APIs
 *
 *  The STC1200 header file should be included in an application as follows:
 *  @code
 *  #include <STC1200.h>
 *  @endcode
 *
 *  ============================================================================
 */

#ifndef __STC1200_TM4C1294NCPDT_H
#define __STC1200_TM4C1294NCPDT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ti/drivers/GPIO.h>

/* STC hardware version (1=A, 2=B, etc) */
#define STC1200_HARDWARE_REV    2

/* LEDs on STC1200 are active high. */
#define STC1200_LED_OFF		    ( 0)
#define STC1200_LED_ON		    (~0)

/* Lamps on STC1200 are active high. */
#define STC1200_LAMP_OFF	    ( 0)
#define STC1200_LAMP_ON		    (~0)

#define PIN_LOW				    ( 0)
#define PIN_HIGH			    (~0)

/* Board specific I2C addresses */
#define AT24MAC_EPROM_ADDR      (0xA0 >> 1)
#define AT24MAC_EPROM_EXT_ADDR  (0xB0 >> 1)

/*** Hardware Constants ****************************************************/

/* QEI-0 is tape roller encoder */
#define QEI_BASE_ROLLER		    QEI0_BASE

/*!
 *  @def    STC1200_EMACName
 *  @brief  Enum of EMAC names on the STC1200 dev board
 */
typedef enum STC1200_EMACName {
    STC1200_EMAC0 = 0,

    STC1200_EMACCOUNT
} STC1200_EMACName;

/*!
 *  @def    STC1200_GPIOName
 *  @brief  Enum of LED names on the STC1200 dev board
 */
typedef enum STC1200_GPIOName {
    STC1200_BTN_RESET = 0,
    STC1200_BTN_CUE,
    STC1200_BTN_SEARCH,
    STC1200_STOP_DETECT_N,
    STC1200_PLAY_DETECT_N,
    STC1200_SMPTE_INT_N,
    STC1200_DIPSW_CFG1,
    STC1200_DIPSW_CFG2,
    STC1200_SPEED_SELECT,
    STC1200_EXPIO_PD4,
    STC1200_EXPIO_PD5,
    STC1200_EXPIO_PP2,
    STC1200_EXPIO_PP3,
    STC1200_EXPIO_PP4,
    STC1200_EXPIO_PP5,
    STC1200_EXPIO_PK2,
    STC1200_EXPIO_PK3,
	STC1200_RESET_AVR_N,
	STC1200_RESET_BL652,
	STC1200_AIRLOAD,
	STC1200_AUTORUN_N,
    STC1200_RS422_RE_N,
    STC1200_RS422_DE,
    STC1200_STOP_N,
    STC1200_PLAY_N,
    STC1200_FWD_N,
    STC1200_REW_N,
    STC1200_MOTION_REW,
    STC1200_MOTION_FWD,
    STC1200_SEARCHING,
    STC1200_TAPE_DIR,
    STC1200_LAMP_PLAY,
    STC1200_LAMP_FWDREW,
    STC1200_STAT_LED,
    STC1200_EXPIO_PF2_SSI3FS,
    STC1200_EXPIO_PA3_SSI0FS,
    STC1200_GPIOCOUNT
} STC1200_GPIOName;

/*!
 *  @def    STC1200_I2CName
 *  @brief  Enum of I2C names on the STC1200 dev board
 */
typedef enum STC1200_I2CName {
    STC1200_I2C0 = 0,

    STC1200_I2CCOUNT
} STC1200_I2CName;

/*!
 *  @def    STC1200_SDSPIName
 *  @brief  Enum of SDSPI names on the STC1200 dev board
 */
typedef enum STC1200_SDSPIName {
    STC1200_SDSPI1 = 0,		/* SD Card Socket */

    STC1200_SDSPICOUNT
} STC1200_SDSPIName;

/*!
 *  @def    STC1200_SPIName
 *  @brief  Enum of SPI names on the STC1200 dev board
 */
typedef enum STC1200_SPIName {
    STC1200_SPI0 = 0,		/* Expansion I/O connector   */
    STC1200_SPI2,			/* S25FL127 Serial NOR FLASH */
    STC1200_SPI3,			/* Expansion I/O connector   */

    STC1200_SPICOUNT
} STC1200_SPIName;

/*!
 *  @def    STC1200_UARTName
 *  @brief  Enum of UARTs on the STC1200 dev board
 */
typedef enum STC1200_UARTName {
    STC1200_UART0 = 0,
    STC1200_UART1,
    STC1200_UART2,
    STC1200_UART3,
    STC1200_UART4,
    STC1200_UART5,
    STC1200_UART6,
    STC1200_UART7,

    STC1200_UARTCOUNT
} STC1200_UARTName;

/*
 *  @def    STC1200_WatchdogName
 *  @brief  Enum of Watchdogs on the STC1200 dev board
 */
typedef enum STC1200_WatchdogName {
    STC1200_WATCHDOG0 = 0,

    STC1200_WATCHDOGCOUNT
} STC1200_WatchdogName;

/*!
 *  @def    STC1200_WiFiName
 *  @brief  Enum of WiFi names on the STC1200 dev board
 */
typedef enum STC1200_WiFiName {
    STC1200_WIFI = 0,

    STC1200_WIFICOUNT
} STC1200_WiFiName;

/*!
 *  @brief  Initialize the general board specific settings
 *
 *  This function initializes the general board specific settings. This include
 *     - Enable clock sources for peripherals
 */
extern void STC1200_initGeneral(void);

/*!
 *  @brief Initialize board specific EMAC settings
 *
 *  This function initializes the board specific EMAC settings and
 *  then calls the EMAC_init API to initialize the EMAC module.
 *
 *  The EMAC address is programmed as part of this call.
 *
 */
extern void STC1200_initEMAC(void);

/*!
 *  @brief  Initialize board specific GPIO settings
 *
 *  This function initializes the board specific GPIO settings and
 *  then calls the GPIO_init API to initialize the GPIO module.
 *
 *  The GPIOs controlled by the GPIO module are determined by the GPIO_config
 *  variable.
 */
extern void STC1200_initGPIO(void);

/*!
 *  @brief  Initialize board specific I2C settings
 *
 *  This function initializes the board specific I2C settings and then calls
 *  the I2C_init API to initialize the I2C module.
 *
 *  The I2C peripherals controlled by the I2C module are determined by the
 *  I2C_config variable.
 */
extern void STC1200_initI2C(void);

/*!
 *  @brief  Initialize board specific SDSPI settings
 *
 *  This function initializes the board specific SDSPI settings and then calls
 *  the SDSPI_init API to initialize the SDSPI module.
 *
 *  The SDSPI peripherals controlled by the SDSPI module are determined by the
 *  SDSPI_config variable.
 */
extern void STC1200_initSDSPI(void);

/*!
 *  @brief  Initialize board specific SPI settings
 *
 *  This function initializes the board specific SPI settings and then calls
 *  the SPI_init API to initialize the SPI module.
 *
 *  The SPI peripherals controlled by the SPI module are determined by the
 *  SPI_config variable.
 */
extern void STC1200_initSPI(void);

/*!
 *  @brief  Initialize board specific UART settings
 *
 *  This function initializes the board specific UART settings and then calls
 *  the UART_init API to initialize the UART module.
 *
 *  The UART peripherals controlled by the UART module are determined by the
 *  UART_config variable.
 */
extern void STC1200_initUART(void);

/*!
 *  @brief  Initialize board specific Watchdog settings
 *
 *  This function initializes the board specific Watchdog settings and then
 *  calls the Watchdog_init API to initialize the Watchdog module.
 *
 *  The Watchdog peripherals controlled by the Watchdog module are determined
 *  by the Watchdog_config variable.
 */
extern void STC1200_initWatchdog(void);

/*!
 *  @brief  Initialize board specific WiFi settings
 *
 *  This function initializes the board specific WiFi settings and then calls
 *  the WiFi_init API to initialize the WiFi module.
 *
 *  The hardware resources controlled by the WiFi module are determined by the
 *  WiFi_config variable.
 */
extern void STC1200_initWiFi(void);

#ifdef __cplusplus
}
#endif

#endif /* __STC1200_TM4C1294NCPDT_H */
