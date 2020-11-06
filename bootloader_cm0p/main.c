/******************************************************************************
* File Name:   main.c
*
* Description:
* This is the source code for CE230956- AWS IoT and FreeRTOS for PSoC 6 MCU:
* MCUboot-Based Bootloader with Rollback to Factory Image in External Flash.
* This file implements the MCUboot bootloader with rollback abilities.
*
* Related Document: See README.md
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

/* Standard headers. */
#include <stdio.h>

/* Driver header files. */
#include "cy_pdl.h"
#include "cycfg.h"
#include "cy_result.h"
#include "cy_retarget_io_pdl.h"

#include "cycfg_clocks.h"
#include "cycfg_peripherals.h"
#include "cycfg_pins.h"

/* MCUboot header files. */
#include "bootutil/image.h"
#include "bootutil/bootutil.h"
#include "bootutil/sign_key.h"
#include "bootutil/bootutil_log.h"

/*  Flash access headers. */
#include "flash_map_backend/flash_map_backend.h"
#include "cy_smif_psoc6.h"
#include "sysflash.h"

/*******************************************************************************
* Macros
********************************************************************************/
/* Delay for which CM0+ waits before enabling CM4 so that the messages printed
 * by CM0+ do not go unnoticed by the user since these messages may be
 * overwritten by CM4. 
 */
#define CM4_BOOT_DELAY_MS       (100UL)

/* Slave Select line to which the external memory is connected.
 * Acceptable values are:
 * 0 - SMIF disabled (no external memory)
 * 1, 2, 3, or 4 - slave select line to which the memory module is connected.
 */
#define QSPI_SLAVE_SELECT_LINE  (1UL)

/* Button status: GPIO will read LOW, if pressed. */
#define USER_BTN_PRESSED        (0)

/* User button interrupt configurations.  */
static cy_stc_sysint_t user_btn_isr_cfg =
{
    .intrSrc = NvicMux6_IRQn,
    .cm0pSrc = ioss_interrupts_gpio_0_IRQn,
    .intrPriority = 1,
};

/*******************************************************************************
* Global  variables
********************************************************************************/
static volatile bool is_user_button_pressed = false ;

/*******************************************************************************
* Function Prototypes
********************************************************************************/
static cy_rslt_t transfer_factory_image(void);
static void do_boot(struct boot_rsp *rsp, char *msg);
static void rollback_to_factory_image(void);
static void user_button_callback(void);
static void deinit_hw(void);

/******************************************************************************
 * Function Name: user_button_callback
 ******************************************************************************
 * Summary:
 * This callback function will be executed on user button press event detection.
 * Sets event flag and clears the respective interrupt flag.
 *
 ******************************************************************************/
static void user_button_callback(void)
{
    /* Set the event flag. */
    is_user_button_pressed = true;

    /* Clear the interrupt flag. */
    Cy_GPIO_ClearInterrupt(USER_BTN_PORT, USER_BTN_PIN);
}

/******************************************************************************
 * Function Name: deinit_hw
 ******************************************************************************
 * Summary:
 * This function performs the necessary hardware de-initialization.
 ******************************************************************************/
static void deinit_hw(void)
{
    cy_retarget_io_pdl_deinit();
    Cy_GPIO_Port_Deinit(CYBSP_UART_RX_PORT);
    Cy_GPIO_Port_Deinit(CYBSP_UART_TX_PORT);
    qspi_deinit(QSPI_SLAVE_SELECT_LINE);
}

/******************************************************************************
 * Function Name: transfer_factory_image
 ******************************************************************************
 * Summary:
 *  This function does a simple sanity check on factory app and transfers it 
 *  from external memory to primary slot, if found valid. 
 *  Asserts on critical errors.
 *
 * Parameters:
 *  none
 *
 * Return
 * status of operation cy_rslt_t
 *
 ******************************************************************************/
static cy_rslt_t transfer_factory_image(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    struct flash_area fap_extf;
    const struct flash_area *fap_primary = NULL;
    uint8_t ram_buf[CY_FLASH_SIZEOF_ROW] = {0};
    uint32_t index = 0 , bytes_to_copy = 0 , prim_slot_off = 0, fact_img_off = 0 ;
    uint32_t image_magic = 0 ;

    /* Factory app is stored in external flash. Flash map doesn't have any
     * flash_area entry for the factory app. To be compatible with MCUboot
     * smif wrappers, populate a dummy flash_area structure with necessary
     * details.
     * Note: For read operation, only "fa_device_id" is sufficient. 
     * Just populating it.
     */
    fap_extf.fa_device_id = FLASH_DEVICE_EXTERNAL_FLASH(CY_BOOT_EXTERNAL_DEVICE_INDEX);

    /* Open primary slot. */
    result = flash_area_open(FLASH_AREA_IMAGE_PRIMARY(0), &fap_primary);

    if(result != CY_RSLT_SUCCESS)
    {
        BOOT_LOG_ERR("Failed to open primary slot !");

        /* Critical error: asserting. */
        CY_ASSERT(0);
    }
    else
    {
        result = psoc6_smif_read((const struct flash_area *)&fap_extf,
                CY_SMIF_BASE_MEM_OFFSET, (void *)&image_magic, sizeof(image_magic));
    }

    if(result != CY_RSLT_SUCCESS)
    {
        BOOT_LOG_ERR("Failed to read 'factory app' magic from external memory\r\n");

        /* Critical error: asserting. */
        CY_ASSERT(0);
    }
    else if(image_magic != IMAGE_MAGIC)
    {
        BOOT_LOG_ERR("Invalid image magic 0x%08x !\r\n", (int)image_magic);

        /* Critical error: asserting. */
        CY_ASSERT(0);
    }
    else
    {
        BOOT_LOG_INF("Valid image magic found");
        BOOT_LOG_INF("Erasing primary slot. Please wait for a while...\r\n");

        /* Erase primary slot completely. */
        result = flash_area_erase(fap_primary, 0, fap_primary->fa_size);
    }

    if (result != CY_RSLT_SUCCESS)
    {
        BOOT_LOG_ERR("Failed to erase Primary Slot !");

        /* Critical error: asserting. */
        CY_ASSERT(0);
    }
    else
    {
        /* Initialize the parameters required for copy operation.
         * Partition size of internal flash and that of external flash
         * need not be same always. External flash might have bigger size
         * reserved for factory application  than the primary slot size.
         * However, image can't be larger than that of primary partition size.
         * So, always copy "fap_primary->fa_size" bytes from external flash to
         * primary slot during Rollback.
         */
        bytes_to_copy = fap_primary->fa_size;
        fact_img_off = CY_SMIF_BASE_MEM_OFFSET;
        prim_slot_off = 0;

        BOOT_LOG_INF("Transferring 'factory app' to 'primary slot'");
        BOOT_LOG_INF("Please wait for a while...\r\n");

        /* Copy factory app to primary slot.
         * Read from external memory and then write to primary slot in
         * chunks of "CY_FLASH_SIZEOF_ROW" bytes. status of the transfer will be
         * returned to caller.
         */
        CY_ASSERT((bytes_to_copy % CY_FLASH_SIZEOF_ROW) == 0);
        while (index < bytes_to_copy)
        {
            /* Read from QSPI. */
            result = psoc6_smif_read((const struct flash_area *) &fap_extf,
                    fact_img_off, ram_buf, CY_FLASH_SIZEOF_ROW);
            if (result != CY_RSLT_SUCCESS)
            {
                BOOT_LOG_ERR("failed to read factory app @ offset 0x%8x",
                        (int )fact_img_off);
                break;
            }

            /* Write to Internal flash. */
            result = flash_area_write(fap_primary, prim_slot_off, ram_buf, CY_FLASH_SIZEOF_ROW);

            if (result != CY_RSLT_SUCCESS)
            {
                BOOT_LOG_ERR("failed to write primary slot @ offset 0x%8x",
                        (int )prim_slot_off);
                break;
            }

            fact_img_off += CY_FLASH_SIZEOF_ROW;
            prim_slot_off += CY_FLASH_SIZEOF_ROW;
            index += CY_FLASH_SIZEOF_ROW;
        }
    }

    /* Cleanup the resources acquired. */
    flash_area_close(fap_primary);

    if (result == CY_RSLT_SUCCESS)
    {
        /* Copy operation is successful. */
        BOOT_LOG_INF("factory app copied to primary slot successfully");
    }

    return result;
}

/******************************************************************************
 * Function Name: do_boot
 ******************************************************************************
 * Summary:
 *  This function extracts the image address and enables CM4 to let it boot
 *  from that address.
 *
 * Parameters:
 *  rsp - Pointer to a structure holding the address to boot from.
 *  msg - String used for intuitive indications to user.
 *
 ******************************************************************************/
static void do_boot(struct boot_rsp *rsp, char *msg)
{
    uint32_t app_addr = (rsp->br_image_off + rsp->br_hdr->ih_hdr_size);

    CY_ASSERT(msg != NULL);

    BOOT_LOG_INF("Starting %s on CM4. Please wait...", msg);

    cy_retarget_io_wait_tx_complete(CYBSP_UART_HW, CM4_BOOT_DELAY_MS);

    deinit_hw();

    Cy_SysEnableCM4(app_addr);

    while (true)
    {
        Cy_SysPm_CpuEnterDeepSleep(CY_SYSPM_WAIT_FOR_INTERRUPT);
    }
}

/******************************************************************************
 * Function Name: rollback_to_factory_image
 ******************************************************************************
 * Summary:
 *  This function validates the primary slot and start CM4 boot, if a valid
 *  image is found in primary slot. Never returns on successful boot of factory app
 *  and asserts on failure.
 *
 ******************************************************************************/
static void rollback_to_factory_image(void)
{
    struct boot_rsp rsp;

    if(transfer_factory_image() != CY_RSLT_SUCCESS)
     {
         BOOT_LOG_ERR("factory app transfer failed !");
         CY_ASSERT(0);
     }

     /* Image successfully copied to primary slot at this point. 
      * Now, verify the copied image and boot to it. 
      * All pending updates are cleared on POR and no more updates pending
      * at this point. 
      */
     if (boot_go(&rsp) == 0)
     {
         BOOT_LOG_INF("factory app validated successfully");

        /* Run boot process, never return. */
         do_boot(&rsp, "Factory app");
     }

     /* Assert on failure to rollback. */
     BOOT_LOG_ERR("factory app validation failed");
     BOOT_LOG_ERR("Can't Rollback, asserting!!");

     CY_ASSERT(0);
}

/******************************************************************************
 * Function Name: main
 ******************************************************************************
 * Summary:
 *  System entrance point. This function initializes system resources & 
 *  peripherals, retarget IO and user button. Boots to application,
 *  if a valid image is present. Performs the rollback, if requested by the user.
 *
 ******************************************************************************/
int main(void)
{
    struct boot_rsp rsp;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    /* Initialize system resources and peripherals. */
    init_cycfg_all();

    /* Initialize retarget-io to redirect the printf output. */
    result = cy_retarget_io_pdl_init(CY_RETARGET_IO_BAUDRATE);
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    /* Enable interrupts. */
    __enable_irq();

    /* Initialize QSPI NOR flash using SFDP. */
    result = qspi_init_sfdp(QSPI_SLAVE_SELECT_LINE);
    if( result == CY_RSLT_SUCCESS)
    {
        BOOT_LOG_INF("External Memory initialization using SFDP mode.");
    }
    else
    {
        BOOT_LOG_ERR("External Memory initialization using SFDP Failed 0x%08x", (int)result);

        /* Critical error: asserting. */
        CY_ASSERT(0);
    }

    /* Perform upgrade if pending and check primary slot is valid or not. */
    if (boot_go(&rsp) == 0)
    {
        BOOT_LOG_INF("Application validated successfully !");

        /* We have a valid image in primary slot. Check if user wants
         * to initiate rollback. Rollback can be initiated only if user button
         * is held pressed during this stage. otherwise device will jump to
         * the application directly.
         */

        if(Cy_GPIO_Read(USER_BTN_PORT, USER_BTN_PIN) == USER_BTN_PRESSED)
        {
            BOOT_LOG_INF("Detected user button event");
            BOOT_LOG_INF("Rollback initiated at startup \r\n") ;

            /* Never return from here. */
            rollback_to_factory_image();
        }

        /* User button event not detected, boot to application. */
        do_boot(&rsp, "Application");
    }
    else
    {
        /* No update is pending in secondary slot. Primary slot is not valid.
         * Wait for user input for further actions.
         */

        /* Configure GPIO interrupt vector for Port 0. */
        Cy_SysInt_Init(&user_btn_isr_cfg, user_button_callback);
        NVIC_EnableIRQ((IRQn_Type)user_btn_isr_cfg.intrSrc);

        /* Console message and inform user that an action is expected. */
        BOOT_LOG_INF("No Upgrade available !");
        BOOT_LOG_INF("No valid image found in primary slot !");
        BOOT_LOG_INF("Press and release user button to initiate Rollback \r\n");
        
        do
        {
            /* Put MCU in WFI and wait for use events.  */
            __WFI();

            if(is_user_button_pressed == true)
            {
                /* We don't need this interrupt further. Disabling it. */
                NVIC_DisableIRQ((IRQn_Type)user_btn_isr_cfg.intrSrc);

                BOOT_LOG_INF("Detected user button event ");
                BOOT_LOG_INF("Initiating the Rollback...\r\n") ;

                break;
            }
        } while(1);

        /* Reset the button status. */
        is_user_button_pressed = false;

        /* This function never returns. */
        rollback_to_factory_image();
    }

    return 0;
}
    
/* [] END OF FILE */
