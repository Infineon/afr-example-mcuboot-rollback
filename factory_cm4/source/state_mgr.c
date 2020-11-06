/******************************************************************************
 * File Name: state_mgr.c
 *
 * Description: This file contains task and functions related to state manager.
 *
 *******************************************************************************
 * (c) 2020, Cypress Semiconductor Corporation. All rights reserved.
 *******************************************************************************
 * This software, including source code, documentation and related materials
 * ("Software"), is owned by Cypress Semiconductor Corporation or one of its
 * subsidiaries ("Cypress") and is protected by and subject to worldwide patent
 * protection (United States and foreign), United States copyright laws and
 * international treaty provisions. Therefore, you may use this Software only
 * as provided in the license agreement accompanying the software package from
 * which you obtained this Software ("EULA").
 *
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software source
 * code solely for use in connection with Cypress's integrated circuit products.
 * Any reproduction, modification, translation, compilation, or representation
 * of this Software except as specified above is prohibited without the express
 * written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer of such
 * system or application assumes all risk of such use and in doing so agrees to
 * indemnify Cypress against all liability.
 *******************************************************************************/

/* Header file includes. */
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "led_task.h"

/* FreeRTOS header file. */
#include <FreeRTOS.h>
#include "FreeRTOSConfig.h"
#include <task.h>
#include <limits.h>

/* IoT header. */
#include "iot_logging_task.h"

/* Local headers. */
#include "network_cfg.h"

/*******************************************************************************
 * Macros
 ********************************************************************************/
/* State Manager task configurations. */
#define STATE_MGR_TASK_STACK_SIZE            (configMINIMAL_STACK_SIZE*4)
#define STATE_MGR_TASK_PRIORITY              (configMAX_PRIORITIES - 3)

/* Only first bit is used for the interrupt detection. */
#define USER_EVENT_DETECT_FLAG               (0x01)

/* Interrupt priority configuration. */
#define USRBTN_INTERRUPT_PRIORITY             (7u)

/* LED blink interval. */
#define LED_BLINKY_DELAY_MS                        ( 1000 )

/*******************************************************************************
 * Global Variables
 ********************************************************************************/
/* State Mgr handle. */
static TaskHandle_t state_mgr_task_handle;
static const uint32_t toggle_ms = LED_BLINKY_DELAY_MS;

/*******************************************************************************
 * Function Prototypes
 ********************************************************************************/
static void user_button_callback(void *handler_arg, cyhal_gpio_event_t event);
static void state_mgr(void *args);

/*******************************************************************************
 * Function Name: state_mgr_task_init
 *******************************************************************************
 * Summary:
 *  Creates the state manager task.
 *
 *******************************************************************************/
void state_mgr_task_init(void)
{
    bool ret_val = pdFAIL;

    /* Create the tasks. */
    ret_val = xTaskCreate(state_mgr, "STATE MGR", STATE_MGR_TASK_STACK_SIZE,
                          NULL, STATE_MGR_TASK_PRIORITY, &state_mgr_task_handle);

    if (ret_val != pdPASS)
    {
        configPRINTF(("State manager init failed !"));
        configASSERT(0);
    }
}

/*******************************************************************************
 * Function Name: user_button_callback
 ********************************************************************************
 * Summary: User button callback handler.
 *
 * @param[in] handler_arg isr handler argument (unused).
 * @param[in] cyhal_gpio_irq_event_t gpio event identifier (unused).
 *
 *******************************************************************************/
static void user_button_callback(void *handler_arg, cyhal_gpio_irq_event_t event)
{
    BaseType_t higher_priority_taskWoken = pdFALSE;

    /* To avoid compiler warnings. */
    (void) handler_arg;
    (void) event;

    /* Notify the task waiting. */
    xTaskNotifyFromISR(state_mgr_task_handle, USER_EVENT_DETECT_FLAG, eSetBits,
            &higher_priority_taskWoken);

    portYIELD_FROM_ISR(higher_priority_taskWoken);
}

/*******************************************************************************
 * Function Name: state_mgr
 *******************************************************************************
 * Summary:
 *  State manager task starts the LED task and then waits for the user button
 *  event. Network initialization will begin, when user button event is detected
 *  and then deletes itself.
 *
 * @param[in] args Task parameter defined during task creation (unused).
 *
 *******************************************************************************/
static void state_mgr(void *args)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    BaseType_t notfication_result = 0;
    uint32_t notified_val = 0;

    /* Initialize the user button. */
    result = cyhal_gpio_init(CYBSP_USER_BTN, CYHAL_GPIO_DIR_INPUT,
            CYHAL_GPIO_DRIVE_PULLUP, CYBSP_BTN_OFF);

    /* Configure GPIO interrupt. */
    cyhal_gpio_register_callback(CYBSP_USER_BTN, user_button_callback, NULL);

    /* Enable user button event. Global event is already enabled. */
    cyhal_gpio_enable_event(CYBSP_USER_BTN, CYHAL_GPIO_IRQ_FALL,
    USRBTN_INTERRUPT_PRIORITY, true);

    /* To avoid compiler warnings. */
    (void) result;
    (void) args;

    /* Kick start the LED task. */
    configPRINTF(("\r\n"));
    configPRINTF(("Starting LED task..\r\n"));

    led_task_init(&toggle_ms);

    configPRINTF(("\r\n****Waiting for user button press event****\r\n"));

    /* Wait for the event. */
    do
    {
        notfication_result = xTaskNotifyWait( pdFALSE,          /* Don't clear bits on entry. */
                                              ULONG_MAX,        /* Clear all bits on exit. */
                                              &notified_val,    /* Stores the notified value. */
                                              portMAX_DELAY);   /* Wait forever. */

        if ( (notfication_result == pdPASS) && (notified_val && USER_EVENT_DETECT_FLAG) )
        {
            configPRINTF(("Detected user button event..\r\n"));
            break;
        }

    } while (1);

    configPRINTF(("Initialize network... \r\n"));

    /* Initialize the network and start the demo. */
    network_init();

    configPRINTF(("%s() completed its job ! \r\n",__func__));

    /* Job of state manager is done. Nothing more to handle here !.
     * It can be be detached  here. No additional resources to cleanup.
     */
    vTaskDelete(NULL);
}

/* [] END OF FILE */
