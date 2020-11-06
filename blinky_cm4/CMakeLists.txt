#-------------------------------------------------------------------------------
# To build, run the following commands in current directory:
# cmake -DVENDOR=cypress -DBOARD=CY8CPROTO_062_4343W -DCOMPILER=arm-gcc -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=../../../../tools/cmake/toolchains/arm-gcc.cmake
# cmake --build build
#-------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.13)

# Configure compiler assembler for code outside amazon-freertos source tree.
enable_language(C)
enable_language(ASM)

set(afr_app_name "blinky_cm4")

project(${afr_app_name})

set(AFR_PATH "${CMAKE_SOURCE_DIR}/../../../..")
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
set(OTA_SUPPORT "1")

#-------------------------------------------------------------------------------
# Convert from "_" to "-" in the target board name in order to be compatible 
# with GNU Make based approach.
#-------------------------------------------------------------------------------
string(REPLACE "_" "-" APP_BOARD_NAME "${BOARD}")

#-------------------------------------------------------------------------------
# Get the application directory path.
#-------------------------------------------------------------------------------
get_filename_component(CY_APP_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
set(AFR_BOARD "${VENDOR}.${BOARD}" CACHE INTERNAL "")
set(AFR_TOOLCHAIN "${COMPILER}" CACHE INTERNAL "")

#-------------------------------------------------------------------------------
# Configure include for freertos config files, aws config files, and cypress
# psoc6_core header files before add_subdirectory.
#-------------------------------------------------------------------------------
set(BOARD_DEMO_DIR "${AFR_PATH}/vendors/cypress/boards/${BOARD}/aws_demos")
set(BOARD_CY_CODE_DIR "${BOARD_DEMO_DIR}/application_code/cy_code")

set(CY_APP_IOT_CONFIG_DIR "${CMAKE_SOURCE_DIR}/../common/include")
set(CY_APP_CONFIG_DIR "${CMAKE_SOURCE_DIR}/../common/config_files")

#-------------------------------------------------------------------------------
# Takes custom design files if it's defined. Uncomment the following lines to 
# take custom BSP design files. Make sure that the path to custom BSP design
# files are provided in CY_APP_DESIGN_MODUS_DIR.
#-------------------------------------------------------------------------------
# option(CUSTOM_DESIGN_MODUS "Use custom design configuration.")
# add_definitions( -DCUSTOM_DESIGN_MODUS )
# set(CY_APP_DESIGN_MODUS_DIR "${CY_APP_DIR}/COMPONENT_CUSTOM_DESIGN_MODUS/TARGET_${APP_BOARD_NAME}/GeneratedSource" )

#-------------------------------------------------------------------------------
# External FLASH Defaults to ON
#
# FLASH layout is handled in vendors/cypress/<board>/CMakeLists.txt
# as it is common between all apps using external FLASH on a specific board.
set(ENV{OTA_USE_EXTERNAL_FLASH}     "1")

# Add external memory description, OTA uses external memory for secondary slot.
if ($ENV{OTA_USE_EXTERNAL_FLASH} STREQUAL "1")
    add_definitions(-DCY_FLASH_MAP_EXT_DESC)
endif()

# With OTA enabled, OTA_USE_EXTERNAL_FLASH must be set to "1".
if ("${OTA_SUPPORT}" STREQUAL "1")

    if (NOT $ENV{OTA_USE_EXTERNAL_FLASH} STREQUAL "1")
        message(FATAL_ERROR "OTA_USE_EXTERNAL_FLASH must be set to 1")
    endif()

endif()

#-------------------------------------------------------------------------------
# CY_INCLUDE_DIRS must be set when building in LIB_MODE.
#-------------------------------------------------------------------------------
set(additional_include_dirs "${BOARD_CY_CODE_DIR}"
                            "${AFR_PATH}/libraries/c_sdk/standard/mqtt/include"
                            "${CMAKE_SOURCE_DIR}/../common/config_files"
                            "${CMAKE_SOURCE_DIR}/../common/include"
                            "${CMAKE_SOURCE_DIR}/source")

if (DEFINED CUSTOM_DESIGN_MODUS)
    list(APPEND additional_include_dirs "${CY_APP_DESIGN_MODUS_DIR}")
else()
    list(APPEND additional_include_dirs "${BOARD_CY_CODE_DIR}/GeneratedSource")
endif()

if (EXISTS "${BOARD_CY_CODE_DIR}/SDIO_HOST")
    list(APPEND additional_include_dirs "${BOARD_CY_CODE_DIR}/SDIO_HOST")
endif()

set(CY_INCLUDE_DIRS ${additional_include_dirs} CACHE INTERNAL "")

#-------------------------------------------------------------------------------
# Set IMG_TYPE as BOOT or UPGRADE to change led blink frequency.
#
# You can over-ride from command line when running cmake
# ex: "-DIMG_TYPE=UPGRADE_IMG"
#-------------------------------------------------------------------------------
# Set IMG_TYPE to UPGRADE, if not defined already.
if("${IMG_TYPE}" STREQUAL "BOOT")
    message("IMG_TYPE set to BOOT")
    add_definitions( -DBOOT_IMG=1 )
else()
    message("IMG_TYPE set to UPGRADE")
    add_definitions( -DUPGRADE_IMG=1 )
endif()

# Set application version. Defaults to V1.0.0.
add_definitions( -DAPP_VERSION_MAJOR=1 )
add_definitions( -DAPP_VERSION_MINOR=0 )
add_definitions( -DAPP_VERSION_BUILD=0 )

#-------------------------------------------------------------------------------
# Removing modules that are not needed by the example.
#-------------------------------------------------------------------------------
set(CY_APP_DISABLE_AFR_MODULES
    "defender"
    "greengrass"
    "shadow"
    )

#-------------------------------------------------------------------------------
# Build amazon-freertos as library.
#-------------------------------------------------------------------------------
set(LIB_MODE 1)
add_subdirectory(${AFR_PATH} "${AFR_PATH}/Lib")

#-------------------------------------------------------------------------------
# Include .cmake files that contain definitions of functions and variables.
#-------------------------------------------------------------------------------
include("${AFR_PATH}/vendors/cypress/MTB/psoc6/cmake/cy_defines.cmake")
include("${AFR_PATH}/vendors/cypress/MTB/psoc6/cmake/cy_create_exe_target.cmake")

#-------------------------------------------------------------------------------
# Add board specific files (taken from amazon-freertos/vendors/cypress/boards
# /${BOARD}/aws_demos/application_code/cy_code). Customize as necessary or 
# replace with another cypress board. The code in ${BSP_DIR}/GeneratedSource is
# generated by our configurators when saving ${BSP_DIR}/design.modus.
#-------------------------------------------------------------------------------
cy_find_files(exe_glob_files DIRECTORY "${BOARD_CY_CODE_DIR}")
if (DEFINED CUSTOM_DESIGN_MODUS)
    foreach(item ${exe_glob_files})
        if(${item} MATCHES "GeneratedSource")
            list(REMOVE_ITEM exe_glob_files ${item})
        endif(${item} MATCHES "GeneratedSource")
    endforeach()
endif()
cy_get_src(exe_source_files ITEMS "${exe_glob_files}")

# add executable target source files
add_executable(${afr_app_name} "${CMAKE_SOURCE_DIR}/main.c"
                "${CMAKE_SOURCE_DIR}/source/led.c"
                "${CMAKE_SOURCE_DIR}/../common/ext_flash_map.c"
                "${exe_source_files}"
                )

target_include_directories(${afr_app_name} PUBLIC "${CY_INCLUDE_DIRS}")

#-------------------------------------------------------------------------------
# Additional compiler definitions.
#-------------------------------------------------------------------------------
target_compile_definitions(${afr_app_name} PUBLIC
    "-DCY_RETARGET_IO_CONVERT_LF_TO_CRLF"
    )

#-------------------------------------------------------------------------------
# Add linker script and map file generation.
#-------------------------------------------------------------------------------
cy_add_linker_options(EXE_APP_NAME ${afr_app_name})

#-------------------------------------------------------------------------------
# Set up dependent linked libraries.
#-------------------------------------------------------------------------------
target_link_libraries(${afr_app_name}  PUBLIC
    AFR::common
    AFR::utils
    AFR::wifi
    AFR::wifi::mcu_port
    psoc6_core # Pulls in defines, includes, source code for psoc6 devices.
    AFR::demo_base
)

#-------------------------------------------------------------------------------
# Configure OTA specific compile definitions.
#-------------------------------------------------------------------------------
cy_config_ota_exe_target(EXE_APP_NAME ${afr_app_name})

#-------------------------------------------------------------------------------
# Configure signing script for generating signed hex and corresponding bin
# files to upload to AWS.
#-------------------------------------------------------------------------------
cy_sign_boot_image(EXE_APP_NAME ${afr_app_name})

#END OF FILE.
