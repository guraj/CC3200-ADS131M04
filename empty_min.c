/*
 * Copyright (c) 2015, Texas Instruments Incorporated
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
 *  ======== empty_min.c ========
 */
/* Standard includes */
#include <string.h>

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>

/* TI-RTOS Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>

/* Board Header files */
#include "Board.h"

// Driverlib includes: Hardware Specific Headers
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_ints.h"
#include "hw_apps_rcm.h"

// Driverlib includes: Peripheral Libraries
#include "spi.h"
#include "rom.h"
#include "rom_map.h"
#include "timer.h"
#include "uart.h"
#include "interrupt.h"

// Driverlib includes: Utilities
#include "utils.h"
#include "prcm.h"
#include "gpio_if.h"

// Interface includes
#include "pin_mux_config.h"
#include "hal.h"         // Important methods defined in hal.c
#include "ads131m0x.h"   // Important methods defined in ads131m0x.c

//*****************************************************************************
//                 DEFINITIONS FOR SPI SETTINGS
//*****************************************************************************
#define SPI_IF_BIT_RATE  100000
#define TR_BUFF_SIZE     100

//*****************************************************************************
//                 DEFINITIONS FOR PWM TIMER SETTINGS
//*****************************************************************************
#define TIMER_INTERVAL_RELOAD   19
#define DUTYCYCLE_GRANULARITY   10

//*****************************************************************************
//                 TASK SETTINGS
//*****************************************************************************
#define TASKSTACKSIZE   512

Task_Struct tsk0Struct;
UInt8 tsk0Stack[TASKSTACKSIZE];
Task_Handle task;

//*****************************************************************************
//                 GLOBAL VARIABLES
//*****************************************************************************
int count = 0;
adc_channel_data adcData;

//*****************************************************************************
//                 VECTORS (Specific for compilers)
//*****************************************************************************
#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif


//****************************************************************************
//
//! Setup the timer in PWM mode
//!
//! \param ulBase is the base address of the timer to be configured
//! \param ulTimer is the timer to be setup (TIMER_A or  TIMER_B)
//! \param ulConfig is the timer configuration setting
//! \param ucInvert is to select the inversion of the output
//!
//! This function
//!    1. The specified timer is setup to operate as PWM
//!
//! \return None.
//
//****************************************************************************

void SetupTimerPWMMode(unsigned long ulBase, unsigned long ulTimer,
                       unsigned long ulConfig, unsigned char ucInvert)
{
    //
    // Set GPT - Configured Timer in PWM mode.
    //
    MAP_TimerConfigure(ulBase,ulConfig);
    MAP_TimerPrescaleSet(ulBase,ulTimer,0);

    //
    // Inverting the timer output if required
    //
    MAP_TimerControlLevel(ulBase,ulTimer,ucInvert);

    //
    // Load value set to ~0.5 ms time period
    //
    MAP_TimerLoadSet(ulBase,ulTimer,TIMER_INTERVAL_RELOAD);

    //
    // Match value set so as to output level 0
    //
    MAP_TimerMatchSet(ulBase,ulTimer,TIMER_INTERVAL_RELOAD);

}

//****************************************************************************
//
//! Sets up the identified timers as PWM to drive the peripherals
//!
//! \param none
//!
//! This function sets up the folowing
//!    1. TIMERA2 (TIMER B) as RED of RGB light
//!    2. TIMERA3 (TIMER B) as YELLOW of RGB light
//!    3. TIMERA3 (TIMER A) as GREEN of RGB light
//!
//! \return None.
//
//****************************************************************************

void InitPWMModules()
{
    //
    // Initialization of timers to generate PWM output
    //
    MAP_PRCMPeripheralClkEnable(PRCM_TIMERA3, PRCM_RUN_MODE_CLK);

    //
    // TIMERA3 (TIMER A) as GREEN of RGB light. GPIO 11 --> PWM_7
    //
    SetupTimerPWMMode(TIMERA3_BASE, TIMER_B,
            (TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_PWM | TIMER_CFG_B_PWM), 1);

    MAP_TimerEnable(TIMERA3_BASE,TIMER_B);
}

//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************

static void
BoardInit(void)
{
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
  //
  // Set vector table base
  //
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}

//****************************************************************************
//
//! Executes the ADC task to process data after DRDY interrupt.
//!
//! \param a0 Sleep duration (in units) for the task before entering the main loop.
//! \param a1 Not used in the current implementation.
//!
//! This function performs the following operations:
//!    1. Sleeps the task for the duration specified by a0.
//!    2. Enters a continuous loop, where it waits for a DRDY interrupt.
//!    3. If the DRDY interrupt occurs:
//!       a. Clears the interrupt flag.
//!       b. Reads data from the ADC.
//!       c. If there's a CRC error in the read data, it prints a warning message.
//!    4. If the DRDY interrupt does not occur within the specified timeout, it turns on an LED.
//!
//! \return None. (Function does not exit unless externally terminated.)
//
//****************************************************************************

Void adcTask(UArg a0, UArg a1)
{
    // Initial sleep before entering main loop
    Task_sleep((UInt)a0);

    while(1) {
        // Wait for DRDY interrupt or timeout
        bool interruptOccurred = waitForDRDYinterrupt(10000);

            if (interruptOccurred) {
                // Clear interrupt flag
                set_flag_nDRDY_INTERRUPT(false);

                // Read data from ADC
                bool crcError = readData(&adcData);

                if (crcError) {
                    // Print warning for CRC error
                    System_printf("CRC error occurred.");
                    System_flush();
                } else {

                }

            } else {
                // Turn on LED if no interrupt within timeout
                GPIO_write(Board_LED0, Board_LED_ON);
            }
    }
}

/*
 *  ======== main ========
 */
int main()
{
    Task_Params tskParams;

    // Initialize board-related functions
    Board_initGeneral();
    Board_initGPIO();
    Board_initSPI();
    BoardInit();

    // Turn off the user LED
    GPIO_write(Board_LED0, Board_LED_OFF);

    // Configure pin functions for GPIO and SPI
    PinMuxConfig();

    // Initialize PWM modules
    InitPWMModules();

    // Set initial match value for timer (for PWM granularity)
    MAP_TimerMatchSet(TIMERA3_BASE,TIMER_B,DUTYCYCLE_GRANULARITY);

    // Initialize ADC with SPI enabled
    InitADC();

    // Set up the ADC task
    Task_Params_init(&tskParams);
    tskParams.stackSize = TASKSTACKSIZE;
    tskParams.stack = &tsk0Stack;
    tskParams.arg0 = 1000;
    Task_construct(&tsk0Struct, (Task_FuncPtr)adcTask, &tskParams, NULL);

    // Launch the TI-RTOS kernel
    BIOS_start();

    return (0);
}
