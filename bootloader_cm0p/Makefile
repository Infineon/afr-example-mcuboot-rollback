################################################################################
# \file Makefile
# \version 1.0
#
# \brief
# Top-level application make file.
#
################################################################################
# \copyright
# Copyright 2018-2020 Cypress Semiconductor Corporation
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
################################################################################

################################################################################
# MCUboot Specific Configuration
###############################################################################

# Set this to 1, if you would like to add external memory programming
# configurations to bootloader.
EN_XMEM_PROG ?= 0

# Default configured to use EXTERNAL FLASH for secondary slot.
OTA_USE_EXTERNAL_FLASH:=1

################################################################################
# Basic Configuration
################################################################################

# Target board/hardware (BSP).
# To change the target, use the Library manager ('make modlibs' from command line).
# If TARGET is manually edited, ensure TARGET_<BSP>.lib with a valid URL exists
# in the application, and run 'make getlibs' to fetch BSP contents.
TARGET=CY8CPROTO-062-4343W
# Underscore needed for directory
TARGET_UNDERSCORE=$(subst -,_,$(TARGET))

# locate AFR root.
CY_AFR_ROOT=../../../..

# Build artifact location
CY_BUILD_RELATIVE_LOCATION=$(CY_AFR_ROOT)/build
CY_BUILD_LOCATION=$(abspath $(CY_BUILD_RELATIVE_LOCATION))

# Name of application (used to derive name of final linked file).
APPNAME=bootloader_cm0p

# Name of toolchain to use. Options include:
#
# GCC_ARM -- GCC 7.2.1, provided with ModusToolbox IDE
# ARM     -- ARM Compiler (must be installed separately)
# IAR     -- IAR Compiler (must be installed separately)
#
# See also: CY_COMPILER_PATH below
TOOLCHAIN=GCC_ARM

# CPU to target; CM4 is the default CPU when this variable is not present.
CORE=CM0P

# Default build configuration. Options include:
#
# Debug -- build with minimal optimizations, focus on debugging.
# Release -- build with full optimizations
# Custom -- build with custom configuration, set the optimization flag in CFLAGS

CONFIG=Debug

# If set to "true" or "1", display full command-lines when building.
VERBOSE=

################################################################################
# Advanced Configuration
################################################################################

# Enable optional code that is ordinarily disabled by default.
#
# Available components depend on the specific targeted hardware and firmware
# in use. In general, if you have
#
#    COMPONENTS=foo bar
#
# ... then code in directories named COMPONENT_foo and COMPONENT_bar will be
# added to the build
#

# Do not define PSOC6HAL component as HAL is not supported for CM0+
COMPONENTS=CUSTOM_DESIGN_MODUS

# Like COMPONENTS, but disable optional code that was enabled by default.
DISABLE_COMPONENTS=BSP_DESIGN_MODUS

# By default the build system automatically looks in the Makefile's directory
# tree for source code and builds it. The SOURCES variable can be used to
# manually add source code to the build process from a location not searched
# by default, or otherwise not found by the build system.
SOURCES=

# Like SOURCES, but for include directories. Value should be paths to
# directories (without a leading -I).
INCLUDES=

# Include common make config.
include ../common/make_support/shared_config.mk

# Include the application make file 
include ./app.mk

# The following defines describe the flash map used by MCUBoot
DEFINES+=CY_BOOT_BOOTLOADER_SIZE=$(BOOTLOADER_APP_FLASH_SIZE) \
         CY_BOOT_PRIMARY_1_SIZE=$(MCUBOOT_SLOT_SIZE) \
         CY_BOOT_SECONDARY_1_SIZE=$(MCUBOOT_SLOT_SIZE) \
         CY_BOOT_SCRATCH_SIZE=$(MCUBOOT_SCRATCH_SIZE)\
         MCUBOOT_MAX_IMG_SECTORS=$(MAX_IMG_SECTORS)\
         MCUBOOT_IMAGE_NUMBER=1

# Add additional defines to the build process (without a leading -D).
DEFINES+=PSOC_064_512K \
         MBEDTLS_CONFIG_FILE='"mcuboot_crypto_config.h"' 

ifeq ($(EN_XMEM_PROG), 1)
DEFINES+=CY_ENABLE_EXMEM_PROGRAM
endif

ifeq ($(OTA_USE_EXTERNAL_FLASH), 1)
DEFINES+=CY_BOOT_USE_EXTERNAL_FLASH    # Use external flash.
DEFINES+=CY_FLASH_MAP_EXT_DESC         # Add external flash map description to defines list.
endif

ifeq ($(USE_CRYPTO_HW), 1)
DEFINES+=CY_CRYPTO_HAL_DISABLE MBEDTLS_USER_CONFIG_FILE='"mcuboot_crypto_acc_config.h"'
else
CY_IGNORE+=libs/cy-mbedtls-acceleration
endif

# Select softfp or hardfp floating point. Default is softfp.
VFP_SELECT=hardfp

# Additional / custom C compiler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
CFLAGS=

# Additional / custom C++ compiler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
CXXFLAGS=

# Additional / custom assembler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
ASFLAGS=

# Additional / custom linker flags.
LDFLAGS=

# Additional / custom libraries to link in to the application.
LDLIBS=

# Custom pre-build commands to run.
# MCUboot uses submodules and "make getlibs" currently does not support
# fetching submodules.
# The PREBUILD command fetches the submodules only if the content of the 
# "mcuboot/ext/mbedtls" directory is empty. 
PREBUILD=\
if [ -n "$$(ls -A $(MBEDTLS_PATH) 2>/dev/null)" ]; then\
	echo "Git submodules for mcuboot exist. Skipping this step...";\
	else\
	echo "Initializing Git Submodules for MCUboot";\
	cd libs/mcuboot;git submodule update --init --recursive;\
fi;\
if [ $(EN_XMEM_PROG) -eq 1 ]; then\
$(CY_QSPI_CONFIGURATOR_DIR)/qspi-configurator-cli --config $(wildcard ./COMPONENT_CUSTOM_DESIGN_MODUS/TARGET_$(TARGET)/*.cyqspi);\
fi

# Custom post-build commands to run.
POSTBUILD=

# Path to the linker script to use (if empty, use the default linker script).
ifeq ($(TOOLCHAIN), GCC_ARM)
LINKER_SCRIPT=$(wildcard ./linker_script/TARGET_$(TARGET)/TOOLCHAIN_$(TOOLCHAIN)/*.ld)
LDFLAGS+=-Wl,--defsym=CM0P_FLASH_SIZE=$(BOOTLOADER_APP_FLASH_SIZE),--defsym=CM0P_RAM_SIZE=$(BOOTLOADER_APP_RAM_SIZE)
else
$(error Only GCC_ARM is supported at this moment)
endif


################################################################################
# Paths
################################################################################

# Relative path to the project directory (default is the Makefile's directory).
#
# This controls where automatic source code discovery looks for code.
CY_APP_PATH=

# Relative path to the "base" library. It provides the core makefile build
# infrastructure.
CY_BASELIB_PATH=$(CY_AFR_ROOT)/vendors/cypress/MTB/psoc6/psoc6make

# Absolute path to the compiler's "bin" directory.
#
# The default depends on the selected TOOLCHAIN (GCC_ARM uses the ModusToolbox
# IDE provided compiler by default).
CY_COMPILER_PATH=


# Locate ModusToolbox IDE helper tools folders in default installation
# locations for Windows, Linux, and macOS.
CY_WIN_HOME=$(subst \,/,$(USERPROFILE))
CY_TOOLS_PATHS ?= $(wildcard \
    $(CY_WIN_HOME)/ModusToolbox/tools_2.1 \
    $(HOME)/ModusToolbox/tools_2.1 \
    /Applications/ModusToolbox/tools_2.1)

# If you install ModusToolbox IDE in a custom location, add the path to its
# "tools_X.Y" folder (where X and Y are the version number of the tools
# folder).
CY_TOOLS_PATHS+=

# Default to the newest installed tools folder, or the users override (if it's
# found).
CY_TOOLS_DIR=$(lastword $(sort $(wildcard $(CY_TOOLS_PATHS))))

ifeq ($(CY_TOOLS_DIR),)
$(error Unable to find any of the available CY_TOOLS_PATHS -- $(CY_TOOLS_PATHS))
endif

$(info Tools Directory: $(CY_TOOLS_DIR))

include $(CY_TOOLS_DIR)/make/start.mk
