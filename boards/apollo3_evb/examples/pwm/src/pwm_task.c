//*****************************************************************************
//
//! @file led_task.c
//!
//! @brief Task to handle LED operation.
//!
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2020, Ambiq Micro
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// Third party software included in this distribution is subject to the
// additional license terms as defined in the /docs/licenses directory.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// This is part of revision 2.4.2 of the AmbiqSuite Development Package.
//
//*****************************************************************************

//*****************************************************************************
//
// Global includes for this project.
//
//*****************************************************************************
//*****************************************************************************
//
// Standard AmbiqSuite includes.
//
//*****************************************************************************
#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_util.h"

//*****************************************************************************
//
// FreeRTOS include files.
//
//*****************************************************************************
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

//*****************************************************************************
//
// LED task handle.
//
//*****************************************************************************
TaskHandle_t pwm_task_handle;
//*****************************************************************************
//
// Handle for LED-related events.
//
//*****************************************************************************
EventGroupHandle_t xLedEventHandle;

//*****************************************************************************
//
// Macros
//
//*****************************************************************************
#define USE_GPIO    25
#define USE_TIMER    5
#define TIMER_SEG AM_HAL_CTIMER_TIMERA
#define TIMER_INT AM_HAL_CTIMER_INT_TIMERA5C0

#define BC_CLKSRC   "HFRC"
#define PWM_CLK     AM_HAL_CTIMER_HFRC_12MHZ

#define PERIOD  24
uint32_t g_ui32Index = 0;


void pwm_ctimer_handler(void)
{
    //uint32_t ui32OnTime;


    //ui32OnTime = g_pui8Brightness[g_ui32Index];

    if ( !(g_ui32Index & 1) )
    {
        // This is a CMPR0 interrupt.
        // The CMPR2 interrupt will come later, but while we're here go ahead
        // and update the CMPR0 period.
        //
        //am_hal_ctimer_period_set(USE_TIMER, TIMER_SEG,
        //                         PERIOD, ui32OnTime);
    }
    else
    {
        //
        // This is a CMPR2 interrupt.  Update the CMPR2 period.
        //
        //am_hal_ctimer_aux_period_set(USE_TIMER, TIMER_SEG,
        //                             PERIOD, ui32OnTime);
    }

    //
    // Set up the LED duty cycle for the next pulse.
    //
    g_ui32Index = (g_ui32Index + 1) % PERIOD;

}

static void pwm_ctimer_init(void)
{


    //
    // Configure the output pin.
    //
    am_hal_ctimer_output_config(USE_TIMER,
                                TIMER_SEG,
                                USE_GPIO,
                                AM_HAL_CTIMER_OUTPUT_NORMAL,
                                AM_HAL_GPIO_PIN_DRIVESTRENGTH_12MA);

    //
    // Configure a timer to drive the LED.
    //
    am_hal_ctimer_config_single(USE_TIMER,               // ui32TimerNumber
                                TIMER_SEG,           // ui32TimerSegment
                                (AM_HAL_CTIMER_FN_PWM_REPEAT    |   // ui32ConfigVal
                                 PWM_CLK                        |
                                 AM_HAL_CTIMER_INT_ENABLE) );

    //
    // Set up initial timer periods.
    //
    am_hal_ctimer_period_set(USE_TIMER,
                             TIMER_SEG, (PERIOD-1), PERIOD/2);
    am_hal_ctimer_aux_period_set(USE_TIMER,
                                 TIMER_SEG, (PERIOD-1), PERIOD/2);

	am_hal_ctimer_int_register(TIMER_INT,pwm_ctimer_handler);

    //
    // Enable interrupts for the Timer we are using on this board.
    //
    am_hal_ctimer_int_enable(TIMER_INT);
    NVIC_EnableIRQ(CTIMER_IRQn);

    //
    // Start the timer.
    //
    am_hal_ctimer_start(USE_TIMER, TIMER_SEG);



}


//*****************************************************************************
//
// Interrupt handler for the GPIO pins.
//
//*****************************************************************************
void
am_gpio_isr(void)
{
    //
    // Read and clear the GPIO interrupt status.
    //
#if defined(AM_PART_APOLLO3P)
    AM_HAL_GPIO_MASKCREATE(GpioIntStatusMask);

    am_hal_gpio_interrupt_status_get(false, pGpioIntStatusMask);
    am_hal_gpio_interrupt_clear(pGpioIntStatusMask);
    am_hal_gpio_interrupt_service(pGpioIntStatusMask);
#elif defined(AM_PART_APOLLO3)
    uint64_t ui64Status;

    am_hal_gpio_interrupt_status_get(false, &ui64Status);
    am_hal_gpio_interrupt_clear(ui64Status);
    am_hal_gpio_interrupt_service(ui64Status);
#else
    #error Unknown device.
#endif
}

//*****************************************************************************
//
// Interrupt handler for the Buttons
//
//*****************************************************************************
void
button_handler(uint32_t buttonId)
{
    BaseType_t xHigherPriorityTaskWoken, xResult;
    //
    // Send an event to the main LED task
    //
    xHigherPriorityTaskWoken = pdFALSE;

    xResult = xEventGroupSetBitsFromISR(xLedEventHandle, (1 << buttonId),
                                        &xHigherPriorityTaskWoken);

    //
    // If the LED task is higher-priority than the context we're currently
    // running from, we should yield now and run the radio task.
    //
    if (xResult != pdFAIL)
    {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void
button0_handler(void)
{
    uint32_t count;
    uint32_t val;

    //
    // Debounce for 20 ms.
    // We're triggered for rising edge - so we expect a consistent HIGH here
    //
    for (count = 0; count < 10; count++)
    {
        am_hal_gpio_state_read(AM_BSP_GPIO_BUTTON0,  AM_HAL_GPIO_INPUT_READ, &val);
        if (!val)
        {
            return; // State not high...must be result of debounce
        }
        am_util_delay_ms(2);
    }

    button_handler(0);
}

void
button1_handler(void)
{
    uint32_t count;
    uint32_t val;

    //
    // Debounce for 20 ms.
    // We're triggered for rising edge - so we expect a consistent HIGH here
    //
    for (count = 0; count < 10; count++)
    {
        am_hal_gpio_state_read(AM_BSP_GPIO_BUTTON1,  AM_HAL_GPIO_INPUT_READ, &val);
        if (!val)
        {
            return; // State not high...must be result of debounce
        }
        am_util_delay_ms(2);
    }

    button_handler(1);
}

void
button2_handler(void)
{
    uint32_t count;
    uint32_t val;

    //
    // Debounce for 20 ms.
    // We're triggered for rising edge - so we expect a consistent HIGH here
    //
    for (count = 0; count < 10; count++)
    {
        am_hal_gpio_state_read(AM_BSP_GPIO_BUTTON2,  AM_HAL_GPIO_INPUT_READ, &val);
        if (!val)
        {
            return; // State not high...must be result of debounce
        }
        am_util_delay_ms(2);
    }

    button_handler(2);
}

//*****************************************************************************
//
// Perform initial setup for the LED task.
//
//*****************************************************************************
void
PwmTaskSetup(void)
{
    am_util_debug_printf("PWMTask: setup\r\n");

    //
    // Create an event handle for our wake-up events.
    //
    xLedEventHandle = xEventGroupCreate();

    //
    // Make sure we actually allocated space for the events we need.
    //
    while (xLedEventHandle == NULL);

	pwm_ctimer_init();

    // Initialize the LEDs
    am_devices_led_array_init(am_bsp_psLEDs, AM_BSP_NUM_LEDS);
    am_devices_led_off(am_bsp_psLEDs, 0);
    am_devices_led_off(am_bsp_psLEDs, 1);
    am_devices_led_off(am_bsp_psLEDs, 2);
    am_devices_led_off(am_bsp_psLEDs, 3);
    am_devices_led_off(am_bsp_psLEDs, 4);
    NVIC_SetPriority(GPIO_IRQn, NVIC_configMAX_SYSCALL_INTERRUPT_PRIORITY);
    //
    // Register interrupt handler for button presses
    //
    am_hal_gpio_interrupt_register(AM_BSP_GPIO_BUTTON0, button0_handler);
    am_hal_gpio_interrupt_register(AM_BSP_GPIO_BUTTON1, button1_handler);
    am_hal_gpio_interrupt_register(AM_BSP_GPIO_BUTTON2, button2_handler);

    am_hal_gpio_pinconfig(AM_BSP_GPIO_BUTTON0, g_AM_BSP_GPIO_BUTTON0);
    am_hal_gpio_pinconfig(AM_BSP_GPIO_BUTTON1, g_AM_BSP_GPIO_BUTTON1);
    am_hal_gpio_pinconfig(AM_BSP_GPIO_BUTTON2, g_AM_BSP_GPIO_BUTTON2);

    //
    // Clear the GPIO Interrupt (write to clear).
    //
    AM_HAL_GPIO_MASKCREATE(GpioIntMask0);
    AM_HAL_GPIO_MASKCREATE(GpioIntMask1);
    AM_HAL_GPIO_MASKCREATE(GpioIntMask2);
    am_hal_gpio_interrupt_clear(AM_HAL_GPIO_MASKBIT(pGpioIntMask0, AM_BSP_GPIO_BUTTON0));
    am_hal_gpio_interrupt_clear(AM_HAL_GPIO_MASKBIT(pGpioIntMask1, AM_BSP_GPIO_BUTTON1));
    am_hal_gpio_interrupt_clear(AM_HAL_GPIO_MASKBIT(pGpioIntMask2, AM_BSP_GPIO_BUTTON2));

    //
    // Enable the GPIO/button interrupt.
    //
    am_hal_gpio_interrupt_enable(AM_HAL_GPIO_MASKBIT(pGpioIntMask0, AM_BSP_GPIO_BUTTON0));
    am_hal_gpio_interrupt_enable(AM_HAL_GPIO_MASKBIT(pGpioIntMask1, AM_BSP_GPIO_BUTTON1));
    am_hal_gpio_interrupt_enable(AM_HAL_GPIO_MASKBIT(pGpioIntMask2, AM_BSP_GPIO_BUTTON2));
    NVIC_EnableIRQ(GPIO_IRQn);

    //
    // Enable interrupts to the core.
    //
    am_hal_interrupt_master_enable();
}

//*****************************************************************************
//
// Short Description.
//
//*****************************************************************************
void
PwmTask(void *pvParameters)
{
    uint32_t bitSet;

    while (1)
    {
        //
        // Wait for an event to be posted to the LED Event Handle.
        //
        bitSet = xEventGroupWaitBits(xLedEventHandle, 0x7, pdTRUE,
                            pdFALSE, portMAX_DELAY);
        if (bitSet != 0)
        {
            // Button Press Event received
            // Toggle respective LED(s)
            if (bitSet & (1 << 0))
            {
                am_devices_led_toggle(am_bsp_psLEDs, 0);
            }
            if (bitSet & (1 << 1))
            {
                am_devices_led_toggle(am_bsp_psLEDs, 1);
            }
            if (bitSet & (1 << 2))
            {
                am_devices_led_toggle(am_bsp_psLEDs, 2);
            }
        }
    }
}

