/******************************************************************************
 * File Name: led_task.c
 *
 * Description: This file contains definition of task and functions related 
 * to the LED task.
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

/* BSP header includes. */
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

/* FreeRTOS header files. */
#include <FreeRTOS.h>
#include "FreeRTOSConfig.h"
#include <task.h>
#include <limits.h>

/* AWS library includes. */
#include "iot_system_init.h"
#include "iot_logging_task.h"

/*******************************************************************************
 * Macros
 ********************************************************************************/
/* LED task configurations. */
#define LED_TASK_STACK_SIZE                 (configMINIMAL_STACK_SIZE)
#define LED_TASK_PRIORITY                   (configMAX_PRIORITIES - 3)

/*******************************************************************************
 * Global Variables
 ********************************************************************************/
/* LED task handle. */
static TaskHandle_t led_task_handle;

/*******************************************************************************
 * Function Prototypes
 ********************************************************************************/
static void led_task(void *args);

/*******************************************************************************
 * Function Name: led_task_init
 *******************************************************************************
 * Summary:
 * Initialize the LED task.
 *
 * @param[in] toggle_delay_ms defines interval between led toggle 
 * (units in milliseconds).
 *
 *******************************************************************************/
void led_task_init(const uint32 *toggle_delay_ms)
{
    BaseType_t ret_val = pdFALSE;

    configASSERT(toggle_delay_ms != NULL);

    ret_val = xTaskCreate(led_task, "LED TASK", LED_TASK_STACK_SIZE,
                          (void * const)toggle_delay_ms, LED_TASK_PRIORITY, &led_task_handle);
    if (ret_val != pdPASS)
    {
        configPRINTF(("Led init Failed !"));
        configASSERT(0);
    }
}

/*******************************************************************************
 * Function Name: led_task
 *******************************************************************************
 * Summary:
 *  Task to initialize and toggle the user LED.
 *
 * @param[in] args Task parameter defined during task creation.
 *
 *******************************************************************************/
static void led_task(void *args)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    uint32_t delay_ms = 0;

    CY_ASSERT(args != NULL);

    /* Delay period is provided by task creator. */
    delay_ms = *((uint32_t *) args);

    /* Initialize the User LED. */
    result = cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT,
                             CYHAL_GPIO_DRIVE_PULLUP,
                             CYBSP_LED_STATE_OFF);

    CY_ASSERT(result == CY_RSLT_SUCCESS);

    /* To avoid compiler warning. */
    (void) result;

    while (1)
    {
        /* Toggle the state of user LED. */
        cyhal_gpio_toggle(CYBSP_USER_LED);

        /* Provide a delay between toggles. */
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }

    /* Should never reach here. */
}

/* [] END OF FILE */
