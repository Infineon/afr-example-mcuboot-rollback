/******************************************************************************
* File Name:   main.c
*
* Description:
* This is the source code for CE230956- AWS IoT and FreeRTOS for PSoC 6 MCU:
* MCUboot-Based Bootloader with Rollback to Factory Image in External Flash.
* This is the main application runtime entry point for factory_app that resides
* on the external memory, which will be copied to internal flash by the
* bootloader during the rollback process.
*
* Related Document: See Readme.md
*
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

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#ifdef CY_BOOT_USE_EXTERNAL_FLASH
#include "flash_qspi.h"
#include "cy_smif_psoc6.h"
#include "cy_serial_flash_qspi.h"
#endif

#ifdef CY_USE_LWIP
#include "lwip/tcpip.h"
#endif

/* BSP & Abstraction headers. */
#include "cybsp.h"
#include "cyhal_gpio.h"
#include "cy_retarget_io.h"

/* Local includes. */
#include "state_mgr.h"

/* AWS library includes. */
#include "iot_system_init.h"
#include "iot_logging_task.h"

/*******************************************************************************
 * Macros
 ********************************************************************************/
/* Logging Task Defines. */
#define mainLOGGING_MESSAGE_QUEUE_LENGTH    ( 200 )
#define mainLOGGING_TASK_STACK_SIZE         ( configMINIMAL_STACK_SIZE * 8 )

/* Unit test defines. */
#define mainTEST_RUNNER_TASK_STACK_SIZE     ( configMINIMAL_STACK_SIZE * 16 )

/* Define them conditionally. */
#if ( ipconfigUSE_LLMNR != 0 ) || ( ipconfigUSE_NBNS != 0 )

/* The name of the devices for xApplicationDNSQueryHook. */
#define mainDEVICE_NICK_NAME                "cypress_demo" //FIXME:

#endif

/*******************************************************************************
 * Function prototypes
 ********************************************************************************/
void vApplicationDaemonTaskStartupHook(void);
static void prvMiscInitialization(void);

/*******************************************************************************
 * Global variables
 ********************************************************************************/
extern int uxTopUsedPriority;

/*-----------------------------------------------------------*/

/**
 * @brief Application runtime entry point.
 */
int main(void)
{
    /* Perform any hardware initialization that does not require the RTOS to be
     * running.  */
    prvMiscInitialization();

    /* Create tasks that are not dependent on the Wi-Fi being initialized. */
    xLoggingTaskInitialize( mainLOGGING_TASK_STACK_SIZE,
    tskIDLE_PRIORITY,
    mainLOGGING_MESSAGE_QUEUE_LENGTH);

    /* Start the scheduler.  Initialization that requires the OS to be running,
     * including the Wi-Fi initialization, is performed in the RTOS daemon task
     * startup hook. */
    vTaskStartScheduler();

    return 0;
}
/*-----------------------------------------------------------*/

/**
 * @brief Initializes the board.
 */
static void prvMiscInitialization(void)
{
    cy_rslt_t result = cybsp_init();

    if (result != CY_RSLT_SUCCESS)
    {
        printf("BSP initialization failed \r\n");
    }

    result = cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX,
            CY_RETARGET_IO_BAUDRATE);
    if (result != CY_RSLT_SUCCESS)
    {
        printf("Retarget IO initialization failed \r\n");
    }
 
    /* Start with clear screen. Printing app details to console. */
    printf("\r\n**Booting to Factory Application ");
    printf("Version: %d.%d.%d ** \r\n \r\n", APP_VERSION_MAJOR,
            APP_VERSION_MINOR, APP_VERSION_BUILD);
}

/**
 * @brief User defined Application startup hook.
 * All initialization which requires RTOS to be active will go here.
 */
void vApplicationDaemonTaskStartupHook(void)
{
    /* Need to export this var to elf so that debugger can work properly. */
    (void) uxTopUsedPriority;

    /* FIX ME: Perform any hardware initialization, that require the RTOS to be
     * running, here. */

#ifdef CY_BOOT_USE_EXTERNAL_FLASH

    __enable_irq();

    if (psoc6_qspi_init() != 0)
    {
        printf("psoc6_qspi_init() FAILED!!\r\n");
    }
#endif /* CY_BOOT_USE_EXTERNAL_FLASH */

    /* FIX ME: If your MCU is using Wi-Fi, delete surrounding compiler directives to
     * enable the unit tests and after MQTT, Bufferpool, and Secure Sockets libraries
     * have been imported into the project. If you are not using Wi-Fi, see the
     * vApplicationIPNetworkEventHook function. */
    if (SYSTEM_Init() == pdPASS)
    {
#ifdef CY_USE_LWIP
        /* Initialize lwIP stack. This needs the RTOS to be up since this function will spawn
         * the tcp_ip thread.
         */
        tcpip_init(NULL, NULL);
#endif
        /* Initialize the state manager. */
        state_mgr_task_init();
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief User defined Idle task function.
 *
 * @note Do not make any blocking operations in this function.
 */
void vApplicationIdleHook(void)
{
    /* Do nothing for now !. */
}
/*-----------------------------------------------------------*/
/**
 * @brief User defined tick hook function
 */
void vApplicationTickHook()
{
    /* Do nothing for now !. */
}

/*-----------------------------------------------------------*/

/**
 * @brief User defined assertion call. This function is plugged into configASSERT.
 * See FreeRTOSConfig.h to define configASSERT to something different.
 */
void vAssertCalled(const char * pcFile, uint32_t ulLine)
{
    /* FIX ME. If necessary, update to applicable assertion routine actions. */

    const uint32_t ulLongSleep = 1000UL;
    volatile uint32_t ulBlockVariable = 0UL;
    volatile char * pcFileName = (volatile char *) pcFile;
    volatile uint32_t ulLineNumber = ulLine;

    (void) pcFileName;
    (void) ulLineNumber;

    printf("vAssertCalled %s, %ld\n", pcFile, (long) ulLine);
    fflush(stdout);

    /* Setting ulBlockVariable to a non-zero value in the debugger will allow
     * this function to be exited. */
    taskDISABLE_INTERRUPTS();
    {
        while (ulBlockVariable == 0UL)
        {
            vTaskDelay(pdMS_TO_TICKS(ulLongSleep));
        }
    }
    taskENABLE_INTERRUPTS();
}
/*-----------------------------------------------------------*/

/**
 * @brief User defined application hook need by the FreeRTOS-Plus-TCP library.
 */
#if ( ipconfigUSE_LLMNR != 0 ) || ( ipconfigUSE_NBNS != 0 ) || ( ipconfigDHCP_REGISTER_HOSTNAME == 1 )
const char * pcApplicationHostnameHook(void)
{
    /* FIX ME: If necessary, update to applicable registration name. */

    /* This function will be called during the DHCP: the machine will be registered
     * with an IP address plus this name. */
    return clientcredentialIOT_THING_NAME;
}

#endif
/*-----------------------------------------------------------*/

/**
 * @brief:User defined application hook to process names returned by the DNS server.
 */
#if ( ipconfigUSE_LLMNR != 0 ) || ( ipconfigUSE_NBNS != 0 )
BaseType_t xApplicationDNSQueryHook( const char * pcName )
{
    /* FIX ME. If necessary, update to applicable DNS name lookup actions. */

    BaseType_t xReturn;

    /* Determine if a name lookup is for this node.  Two names are given
     * to this node: that returned by pcApplicationHostnameHook() and that set
     * by mainDEVICE_NICK_NAME. */
    if( strcmp( pcName, pcApplicationHostnameHook() ) == 0 )
    {
        xReturn = pdPASS;
    }
    else if( strcmp( pcName, mainDEVICE_NICK_NAME ) == 0 )
    {
        xReturn = pdPASS;
    }
    else
    {
        xReturn = pdFAIL;
    }

    return xReturn;
}

#endif /* if ( ipconfigUSE_LLMNR != 0 ) || ( ipconfigUSE_NBNS != 0 ) */
/*-----------------------------------------------------------*/
