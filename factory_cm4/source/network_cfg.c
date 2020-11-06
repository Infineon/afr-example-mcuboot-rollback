/******************************************************************************
 * File Name: network_cfg.c
 *
 * Description: This file contains functions related to enabling the Wi-Fi and
 * establishing network connection.
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

/* FreeRTOS header files. */
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

/* AWS library includes. */
#include "iot_system_init.h"
#include "iot_wifi.h"
#include "iot_network_manager_private.h"
#include "aws_clientcredential.h"
#include "aws_dev_mode_key_provisioning.h"

/* Local include. */
#include "network_cfg.h"

/* Demo includes. */
#include "aws_demo.h"

/*******************************************************************************
 * Macros
 ********************************************************************************/
/* The task delay for allowing the lower priority logging task to print out Wi-Fi
 * failure status before blocking indefinitely. */
#define LOGGING_WIFI_STATUS_DELAY           pdMS_TO_TICKS( 1000 )

/* IP size in bytes. */
#define IPCFG_SIZE_IN_BYTES                 (4)

/* Network connection retry count. */
#define NETWORK_CONN_MAX_RETRY              (5)
/*******************************************************************************
 * Function prototypes
 ********************************************************************************/
static void wifi_connect(void);

/*******************************************************************************
 * Function Name: network_init
 *******************************************************************************
 * Summary:
 * Initialize and connect to WiFi and starts the demo task.
 *
 *******************************************************************************/
void network_init(void)
{
    /* Connect to the Wi-Fi before running the tests. */
    wifi_connect();

    /* Provision the device with AWS certificate and private key. */
    vDevModeKeyProvisioning();

    /* Start the demo task. Demo is configure to run MQTT. */
    DEMO_RUNNER_RunDemos();
}
/*-----------------------------------------------------------*/

/*******************************************************************************
 * Function Name: wifi_connect
 *******************************************************************************
 * Summary:
 *  Turn on WiFi and connect to AP using the configured parameters.
 *
 *******************************************************************************/
static void wifi_connect(void)
{
    WIFINetworkParams_t network_params;
    WIFIReturnCode_t wifi_status;
    uint8_t temp_ip[IPCFG_SIZE_IN_BYTES] = { 0 };
    uint32_t nw_retry_count = 1 ;

    wifi_status = WIFI_On();

    if (wifi_status == eWiFiSuccess)
    {
        configPRINTF(( "Wi-Fi module initialized. Connecting to AP...\r\n" ));
    }
    else
    {
        configPRINTF(( "Asserting: Wi-Fi module failed to initialize.\r\n" ));

        /* Delay to allow the lower priority logging task to print the above status. */
        vTaskDelay( LOGGING_WIFI_STATUS_DELAY);

        configASSERT(0);
    }

    /* Setup parameters. */
    network_params.pcSSID = clientcredentialWIFI_SSID;
    network_params.ucSSIDLength = sizeof( clientcredentialWIFI_SSID) - 1;
    network_params.pcPassword = clientcredentialWIFI_PASSWORD;
    network_params.ucPasswordLength = sizeof( clientcredentialWIFI_PASSWORD) - 1;
    network_params.xSecurity = clientcredentialWIFI_SECURITY;
    network_params.cChannel = 0;

    while ( nw_retry_count < NETWORK_CONN_MAX_RETRY)
    {
        configPRINTF(("Wi-Fi connecting to AP %s.\r\n", network_params.pcSSID));
        wifi_status = WIFI_ConnectAP(&(network_params));

        if (wifi_status == eWiFiSuccess)
        {
            configPRINTF(("Wi-Fi Connected to AP successfully. \r\n"));

            wifi_status = WIFI_GetIP(temp_ip);
            if (eWiFiSuccess == wifi_status)
            {
                configPRINTF(("IP Address acquired %d.%d.%d.%d\r\n", temp_ip[0], temp_ip[1], temp_ip[2], temp_ip[3]));
            }

            vTaskDelay( LOGGING_WIFI_STATUS_DELAY);
            break;
        }
        else
        {
            nw_retry_count++;

            /* Connection failed. */
            configPRINTF(("Wi-Fi failed to connect to AP %s. (Connection attempt %d)\r\n", network_params.pcSSID, nw_retry_count));

            /* Add a small delay to allow RTOS to finish printing. */
            vTaskDelay( LOGGING_WIFI_STATUS_DELAY);

            if(nw_retry_count >= NETWORK_CONN_MAX_RETRY)
            {
                configPRINTF(("Asserting: Wi-Fi connection retry count(%d) exceeded the max limit\r\n",nw_retry_count));
                vTaskDelay( LOGGING_WIFI_STATUS_DELAY);
                configASSERT(0);
            }
        }
    }

    configPRINTF(("Wi-Fi configuration successful. \r\n"));
    vTaskDelay( LOGGING_WIFI_STATUS_DELAY);
}
/*-----------------------------------------------------------*/
