/**
 * \copyright Copyright (C) 2019 Texas Instruments Incorporated - http://www.ti.com/
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdlib.h>
#include <ti/sysbios/knl/Task.h>

// Driverlib includes
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_ints.h"
#include "spi.h"
#include "rom.h"
#include "rom_map.h"
#include "utils.h"
#include "prcm.h"
#include "uart.h"
#include "interrupt.h"
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "hw_apps_rcm.h"
#include "hw_common_reg.h"
#include "interrupt.h"
#include "rom.h"
#include "rom_map.h"
#include "timer.h"
#include "utils.h"
#include "prcm.h"
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_gpio.h"
#include "pin.h"
#include "rom.h"
#include "rom_map.h"
#include "gpio.h"
#include "prcm.h"
#include "gpio_if.h"

// Common interface includes
#include "pin_mux_config.h"
#include "hal.h"

/* BIOS Header files */
//#include <ti/sysbios/BIOS.h>
//#include <ti/sysbios/knl/Task.h>

/* TI-RTOS Header files */
#include <ti/drivers/GPIO.h>
#include "Board.h"


//****************************************************************************
//
// Internal variables
//
//****************************************************************************

// Flag to indicate if a /DRDY interrupt has occurred
static volatile bool flag_nDRDY_INTERRUPT = false;
volatile uint8_t randomNumber = 0;
#define SPI_IF_BIT_RATE  10000000



//****************************************************************************
//
// Internal function prototypes
//
//****************************************************************************
void InitGPIO(void);
void InitSPI(void);
void GPIO_DRDY_IRQHandler(unsigned int index);



//****************************************************************************
//
// External Functions (prototypes declared in hal.h)
//
//****************************************************************************


//*****************************************************************************
//
//! Initializes MCU peripherals for interfacing with the ADC.
//!
//! \fn void InitADC(void)
//!
//! \return None.
//
//*****************************************************************************
void InitADC(void)
{
    // IMPORTANT: Make sure device is powered before setting GPIOs pins to HIGH state.

    // Initialize GPIOs pins used by ADS131M0x
    InitGPIO();

    // Initialize SPI peripheral used by ADS131M0x
    InitSPI();

    // Run ADC startup function
    adcStartup();
}




//****************************************************************************
//
// Timing functions
//
//****************************************************************************



//*****************************************************************************
//
//! Provides a timing delay with 'ms' resolution.
//!
//! \fn void delay_ms(const uint32_t delay_time_ms)
//!
//! \param delay_time_ms is the number of milliseconds to delay.
//!
//! \return None.
//
//*****************************************************************************
void delay_ms(const uint32_t delay_time_ms)
{
    /* --- INSERT YOUR CODE HERE --- */

    const uint32_t cycles_per_loop = 3;
    MAP_UtilsDelay( delay_time_ms * 80000000 / (cycles_per_loop * 1000u) );
}



//*****************************************************************************
//
//! Provides a timing delay with 'us' resolution.
//!
//! \fn void delay_us(const uint32_t delay_time_us)
//!
//! \param delay_time_us is the number of microseconds to delay.
//!
//! \return None.
//
//*****************************************************************************
void delay_us(const uint32_t delay_time_us)
{
    /* --- INSERT YOUR CODE HERE --- */

    const uint32_t cycles_per_loop = 3;
    MAP_UtilsDelay( delay_time_us * 80000000 / (cycles_per_loop * 1000000u) );
}




//****************************************************************************
//
// GPIO initialization
//
//****************************************************************************



//*****************************************************************************
//
//! Configures the MCU's GPIO pins that interface with the ADC.
//!
//! \fn void InitGPIO(void)
//!
//! \return None.
//
//*****************************************************************************
void InitGPIO(void)
{
    /* Configure the GPIO for 'nSYNC_nRESET' as output and set high */
    MAP_GPIOPinWrite(GPIOA1_BASE, 1 << GPIO_PIN_4, 1 << GPIO_PIN_4);

    /* Configure the GPIO for 'nDRDY' as input with falling edge interrupt */
    GPIO_setCallback(Board_BUTTON1, GPIO_DRDY_IRQHandler);
    GPIO_enableInt(Board_BUTTON1);
}




//*****************************************************************************
//
// Interrupt handler for nDRDY GPIO
//
//*****************************************************************************



//*****************************************************************************
//
//! Interrupt handler for /DRDY falling edge interrupt.
//!
//! \fn void GPIO_DRDY_IRQHandler(void)
//!
//! \return None.
//
//*****************************************************************************
void GPIO_DRDY_IRQHandler(unsigned int index)
{
    /* --- INSERT YOUR CODE HERE --- */
    //NOTE: You many need to rename or register this interrupt function for your processor

    // Possible ways to handle this interrupt:
    // If you decide to read data here, you may want to disable other interrupts to avoid partial data reads.

    // In this example we set a flag and exit the interrupt routine. In the main program loop, your application can examine
    // all state flags and decide which state (operation) to perform next.

    /* Get the interrupt status from the GPIO and clear the status */
    uint32_t getIntStatus = MAP_GPIOIntStatus(nDRDY_PORT, true);

    /* Interrupt action: Set a flag */
    flag_nDRDY_INTERRUPT = true;

    /* Random number generation to verify interrupt handler being triggered */
    randomNumber = (uint8_t)rand() % 256;

    /* Clear interrupt */
    MAP_GPIOIntClear(nDRDY_PORT, getIntStatus);

    // NOTE: We add a short delay at the end to prevent re-entrance. Refer to E2E issue:
    // https://e2e.ti.com/support/microcontrollers/tiva_arm/f/908/p/332605/1786938#1786938
    MAP_UtilsDelay(3);
}




//****************************************************************************
//
// GPIO helper functions
//
//****************************************************************************



//*****************************************************************************
//
//! Reads that current state of the /CS GPIO pin.
//!
//! \fn bool getCS(void)
//!
//! \return boolean ('true' if /CS is high, 'false' if /CS is low).
//
//*****************************************************************************
bool getCS(void)
{
    /* --- INSERT YOUR CODE HERE --- */
    return (bool) GPIOPinRead(nCS_PORT, nCS_PIN);
}



//*****************************************************************************
//
//! Reads that current state of the nSYNC/nRESET GPIO pin.
//!
//! \fn bool getSYNC_RESET(void)
//!
//! \return boolean ('true' if nSYNC/nRESET is high, 'false' if nSYNC/nRESET is low).
//
//*****************************************************************************
bool getSYNC_RESET(void)
{
    /* --- INSERT YOUR CODE HERE --- */
    return (bool) GPIOPinRead(nSYNC_nRESET_PORT, nSYNC_nRESET_PIN);
}



//*****************************************************************************
//
//! Controls the state of the /CS GPIO pin.
//!
//! \fn void setCS(const bool state)
//!
//! \param state boolean indicating which state to set the /CS pin (0=low, 1=high)
//!
//! NOTE: The 'HIGH' and 'LOW' macros defined in hal.h can be passed to this
//! function for the 'state' parameter value.
//!
//! \return None.
//
//*****************************************************************************
void setCS(const bool state)
{
    /* --- INSERT YOUR CODE HERE --- */

    // td(CSSC) delay
    if(state) { MAP_UtilsDelay(2); }

    uint8_t value = (uint8_t) (state ? nCS_PIN : 0);
    MAP_GPIOPinWrite(nCS_PORT, nCS_PIN, value);

    // td(SCCS) delay
    if(!state) { MAP_UtilsDelay(2); }
}



//*****************************************************************************
//
//! Controls the state of the nSYNC/nRESET GPIO pin.
//!
//! \fn void setSYNC_RESET(const bool state)
//!
//! \param state boolean indicating which state to set the nSYNC/nRESET pin (0=low, 1=high)
//!
//! NOTE: The 'HIGH' and 'LOW' macros defined in hal.h can be passed to this
//! function for the 'state' parameter value.
//!
//! \return None.
//
//*****************************************************************************
void setSYNC_RESET(const bool state)
{
    /* --- INSERT YOUR CODE HERE --- */
    uint8_t value = (uint8_t) (state ? nSYNC_nRESET_PIN : 0);
    MAP_GPIOPinWrite(nSYNC_nRESET_PORT, nSYNC_nRESET_PIN, value);
}



//*****************************************************************************
//
//! Toggles the "nSYNC/nRESET" pin to trigger a synchronization
//! (LOW, delay 2 us, then HIGH).
//!
//! \fn void toggleSYNC(void)
//!
//! \return None.
//
//*****************************************************************************
void toggleSYNC(void)
{
    /* --- INSERT YOUR CODE HERE --- */
    MAP_GPIOPinWrite(nSYNC_nRESET_PORT, nSYNC_nRESET_PIN, 0);

    // nSYNC pulse width must be between 1 and 2,048 CLKIN periods
    delay_us(2);

    MAP_GPIOPinWrite(nSYNC_nRESET_PORT, nSYNC_nRESET_PIN, nSYNC_nRESET_PIN);
}



//*****************************************************************************
//
//! Toggles the "nSYNC/nRESET" pin to trigger a reset
//! (LOW, delay 2 ms, then HIGH).
//!
//! \fn void toggleRESET(void)
//!
//! \return None.
//
//*****************************************************************************
void toggleRESET(void)
{
    /* --- INSERT YOUR CODE HERE --- */
    MAP_GPIOPinWrite(nSYNC_nRESET_PORT, nSYNC_nRESET_PIN, 0);

    // Minimum /RESET pulse width (tSRLRST) equals 2,048 CLKIN periods (1 ms @ 2.048 MHz)
    delay_ms(2);

    MAP_GPIOPinWrite(nSYNC_nRESET_PORT, nSYNC_nRESET_PIN, nSYNC_nRESET_PIN);

    // tREGACQ delay before communicating with the device again
    delay_us(5);

    // NOTE: The ADS131M0x's next response word should be (0xFF20 | CHANCNT).
    // A different response may be an indication that the device did not reset.

    // Update register array
    restoreRegisterDefaults();

    // Write to MODE register to enforce mode settings
    writeSingleRegister(MODE_ADDRESS, MODE_DEFAULT);
}



//*****************************************************************************
//
//! Waits for the nDRDY interrupt or until the specified timeout occurs.
//!
//! \fn bool waitForDRDYinterrupt(const uint32_t timeout_ms)
//!
//! \param timeout_ms number of milliseconds to wait before timeout event.
//!
//! \return Returns 'true' if nDRDY interrupt occurred before the timeout.
//
//*****************************************************************************
bool waitForDRDYinterrupt(const uint32_t timeout_ms)
{
    /* --- INSERT YOUR CODE HERE ---
     * Poll the nDRDY GPIO pin until it goes low. To avoid potential infinite
     * loops, you may also want to implement a timer interrupt to occur after
     * the specified timeout period, in case the nDRDY pin is not active.
     * Return a boolean to indicate if nDRDY went low or if a timeout occurred.
     */

    // Convert ms to a # of loop iterations, OR even better use a timer here...
    uint32_t timeout = timeout_ms * 6000;   // convert to # of loop iterations

    // Reset interrupt flag
    flag_nDRDY_INTERRUPT = false;

    // Enable interrupts
    IntMasterEnable();

    // Wait for nDRDY interrupt or timeout - each iteration is about 20 ticks
    do {
        timeout--;
        Task_sleep(1000);
    } while (!flag_nDRDY_INTERRUPT && (timeout > 0));

    // Reset interrupt flag
    flag_nDRDY_INTERRUPT = false;

    // Timeout counter greater than zero indicates that an interrupt occurred
    return (timeout > 0);
}

void set_flag_nDRDY_INTERRUPT(bool value)
{
    flag_nDRDY_INTERRUPT = value;
}


//****************************************************************************
//
// SPI Communication
//
//****************************************************************************

#define SSI_BASE_ADDR       (SSI3_BASE)



//*****************************************************************************
//
//! Configures the MCU's SPI peripheral, for interfacing with the ADC.
//!
//! \fn void InitSPI(void)
//!
//! \return None.
//
//*****************************************************************************
void InitSPI(void)
{
    /* --- INSERT YOUR CODE HERE ---
     * NOTE: The ADS131M0x operates in SPI mode 1 (CPOL = 0, CPHA = 1).
     */

    //
    // Enable the clock to SSI-3 module and configure the SSI Master
    //
    MAP_PRCMPeripheralClkEnable(PRCM_GSPI,PRCM_RUN_MODE_CLK);

    //
    // Reset SPI
    //
    MAP_SPIReset(GSPI_BASE);

    //
    // Configure SPI interface
    //
    MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
                     SPI_IF_BIT_RATE,SPI_MODE_MASTER,SPI_SUB_MODE_1,
                     (SPI_SW_CTRL_CS |
                     SPI_4PIN_MODE |
                     SPI_TURBO_OFF |
                     SPI_CS_ACTIVELOW |
                     SPI_WL_8));

    //
    // Enable SPI for communication
    //
    MAP_SPIEnable(GSPI_BASE);

    //
    // Read any residual data from the SSI port.  This makes sure the receive
    // FIFOs are empty, so we don't read any unwanted junk.  This is done here
    // because the SPI SSI mode is full-duplex, which allows you to send and
    // receive at the same time.  The SSIDataGetNonBlocking function returns
    // "true" when data was returned, and "false" when no data was returned.
    // The "non-blocking" function checks if there is any data in the receive
    // FIFO and does not "hang" if there isn't.
    //
    unsigned long junk;
    while(MAP_SPIDataGetNonBlocking(GSPI_BASE, &junk));
}



//*****************************************************************************
//
//! Sends SPI byte array on MOSI pin and captures MISO data to a byte array.
//!
//! \fn void spiSendReceiveArrays(const uint8_t dataTx[], uint8_t dataRx[], const uint8_t byteLength)
//!
//! \param const uint8_t dataTx[] byte array of SPI data to send on MOSI.
//!
//! \param uint8_t dataRx[] byte array of SPI data captured on MISO.
//!
//! \param uint8_t byteLength number of bytes to send & receive.
//!
//! NOTE: Make sure 'dataTx[]' and 'dataRx[]' contain at least as many bytes of data,
//! as indicated by 'byteLength'.
//!
//! \return None.
//
//*****************************************************************************
void spiSendReceiveArrays(const uint8_t dataTx[], uint8_t dataRx[], const uint8_t byteLength)
{
    /*  --- INSERT YOUR CODE HERE ---
     *
     *  This function should send and receive multiple bytes over the SPI.
     *
     *  A typical SPI send/receive sequence may look like the following:
     *  1) Make sure SPI receive buffer is empty
     *  2) Set the /CS pin low (if controlled by GPIO)
     *  3) Send command bytes to SPI transmit buffer
     *  4) Wait for SPI receive interrupt
     *  5) Retrieve data from SPI receive buffer
     *  6) Set the /CS pin high (if controlled by GPIO)
     */

    // Require that dataTx and dataRx are not NULL pointers
    assert(dataTx && dataRx);

    // Set the nCS pin LOW
    MAP_SPICSEnable(GSPI_BASE);

    // Send all dataTx[] bytes on MOSI, and capture all MISO bytes in dataRx[]
    int i;
    for (i = 0; i < byteLength; i++)
    {
        dataRx[i] = spiSendReceiveByte(dataTx[i]);
    }

    // Set the nCS pin HIGH
    MAP_SPICSDisable(GSPI_BASE);
}



//*****************************************************************************
//
//! Sends SPI byte on MOSI pin and captures MISO return byte value.
//!
//! \fn uint8_t spiSendReceiveByte(const uint8_t dataTx)
//!
//! \param const uint8_t dataTx data byte to send on MOSI pin.
//!
//! NOTE: This function is called by spiSendReceiveArrays(). If it is called
//! directly, then the /CS pin must also be directly controlled.
//!
//! \return Captured MISO response byte.
//
//*****************************************************************************
uint8_t spiSendReceiveByte(const uint8_t dataTx)
{
    /*  --- INSERT YOUR CODE HERE ---
     *  This function should send and receive single bytes over the SPI.
     *  NOTE: This function does not control the /CS pin to allow for
     *  more programming flexibility.
     */

    // Remove any residual or old data from the receive FIFO
    unsigned long junk;
    while(MAP_SPIDataGetNonBlocking(GSPI_BASE, &junk));

    // SPI TX
    MAP_SPIDataPut(GSPI_BASE, (unsigned long)dataTx);

    // SPI RX
    unsigned long dataRx;
    MAP_SPIDataGet(GSPI_BASE, &dataRx);

    return (uint8_t)dataRx;
}
