#-------------------------------------------------------------------------------
# /****************************************************************************
# **
# ** Copyright (C) 2015-2019 M-Way Solutions GmbH
# ** Contact: https://www.blureange.io/licensing
# **
# ** This file is part of the Bluerange/FruityMesh implementation
# **
# ** $BR_BEGIN_LICENSE:GPL-EXCEPT$
# ** Commercial License Usage
# ** Licensees holding valid commercial Bluerange licenses may use this file in
# ** accordance with the commercial license agreement provided with the
# ** Software or, alternatively, in accordance with the terms contained in
# ** a written agreement between them and M-Way Solutions GmbH. 
# ** For licensing terms and conditions see https://www.bluerange.io/terms-conditions. For further
# ** information use the contact form at https://www.bluerange.io/contact.
# **
# ** GNU General Public License Usage
# ** Alternatively, this file may be used under the terms of the GNU
# ** General Public License version 3 as published by the Free Software
# ** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
# ** included in the packaging of this file. Please review the following
# ** information to ensure the GNU General Public License requirements will
# ** be met: https://www.gnu.org/licenses/gpl-3.0.html.
# **
# ** $BR_END_LICENSE$
# **
# ****************************************************************************/
#-------------------------------------------------------------------------------

# Makefile.local is used to configure developer specific stuff
-include Makefile.local

# *******************************************************************************************
# You can edit the following parameters for specifying the build, but it is better to copy
# Makefile.local.template to Makefile.local and set the parameters there or alternatively
# Give the parameters to make directly, e.g. make -j4 PLATFORM=NRF52832 FEATURESET=dev

# Build types are: debug, release
BUILD_TYPE       ?= release
# Platforms are: NRF51822, NRF52832, NRF52840
PLATFORM         ?= NRF52832
VERBOSE          ?= 0
# Featuresets are found in config/featuresets
FEATURESET       ?= github

# Set to 1 if make should fail once the app is too big to be updatable over the mesh
FAIL_ON_SIZE_TOO_BIG 		?= 0
# Set to disable stack unwinding code that takes up space
DISABLE_STACK_UNWINDING 	?= 1

# *******************************************************************************************

# For backward compatibility, we rename the platform if only NRF51 or NRF52 is given
ifeq ($(PLATFORM),NRF51)
override PLATFORM := NRF51822
endif

ifeq ($(PLATFORM),NRF52)
override PLATFORM := NRF52832
endif

# Determine the family as this is necessary for flashing
ifeq ($(PLATFORM),NRF51822)
FAMILY := NRF51
else
FAMILY := NRF52
endif

# Check if Featureset makefile exists and include if yes
ifneq ("$(wildcard config/featuresets/$(FEATURESET).make)","")
include config/featuresets/$(FEATURESET).make
endif

# Fail and output a warning if compiler path is unspecified
ifndef FRUITYMESH_GNU_INSTALL_ROOT
$(error You must copy Makefile.local.template to Makefile.local and define the path for FRUITYMESH_GNU_INSTALL_ROOT)
endif

PROJECT_NAME     ?= FruityMesh

OUTPUT_FILENAME = $(PROJECT_NAME)

# NRF51822 needs a different SDK than NRF52832/NRF52840 because the newer SDKs are not compatible with nRF51 anymore
# The sdks are part of the project because bugs had to be fixed
ifeq ($(PLATFORM),NRF51822)
NRF5_SDK_PATH = sdk/sdk11
SDK           = 11
PORT_CONFIG   = sdk/config_nrf51
else ifeq ($(PLATFORM),NRF52832)
NRF5_SDK_PATH = sdk/sdk14
SDK           = 14
PORT_CONFIG   = sdk/config_nrf52
else
NRF5_SDK_PATH = sdk/sdk15
SDK           = 15
PORT_CONFIG   = sdk/config_nrf52
endif

COMPONENTS     = $(NRF5_SDK_PATH)/components
TEMPLATE_PATH  = $(COMPONENTS)/toolchain/gcc

ifeq ($(SDK),15)
MODULES        = $(NRF5_SDK_PATH)/modules
INTEGRATION    = $(NRF5_SDK_PATH)/integration
DRIVERS        = $(MODULES)/nrfx/drivers
HAL            = $(MODULES)/nrfx/hal
endif

ifeq ($(OS),Windows_NT)
include $(TEMPLATE_PATH)/Makefile.windows
else
include $(TEMPLATE_PATH)/Makefile.posix
endif

ifeq ("$(VERBOSE)","1")
NO_ECHO :=
else
NO_ECHO := @
endif

# Tools
ifeq ($(OS),Windows_NT)
  MK := util/buildtools/mkdir.exe -p $@
else
  MK := mkdir -p $@
endif

RM              := rm -rf
NRFJPROG        := nrfjprog
MERGEHEX        := mergehex

# Toolchain commands
CC              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-gcc' 
CXX             := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-g++' 
AS              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-as'
AR              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-ar' -r
LD              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-ld'
NM              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-nm'
OBJDUMP         := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-objdump'
OBJCOPY         := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-objcopy'
SIZE            := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-size'
GDB             := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-gdb'

# Function for removing duplicates in a list
remduplicates = $(strip $(if $1,$(firstword $1) $(call remduplicates,$(filter-out $(firstword $1),$1))))

#source common to all targets
ifeq ($(SDK),15)
C_SOURCE_FILES += $(DRIVERS)/src/nrfx_gpiote.c
else
C_SOURCE_FILES += $(COMPONENTS)/drivers_nrf/common/nrf_drv_common.c
C_SOURCE_FILES += $(COMPONENTS)/drivers_nrf/gpiote/nrf_drv_gpiote.c
endif

C_SOURCE_FILES += $(COMPONENTS)/libraries/timer/app_timer.c
C_SOURCE_FILES += $(COMPONENTS)/ble/ble_db_discovery/ble_db_discovery.c
C_SOURCE_FILES += $(COMPONENTS)/ble/ble_radio_notification/ble_radio_notification.c
C_SOURCE_FILES += src/c/segger_rtt/RTT_Syscalls_GCC.c
C_SOURCE_FILES += src/c/segger_rtt/SEGGER_RTT.c

CPP_SOURCE_FILES += $(wildcard \
		config/boards/*.cpp \
		src/*.cpp \
		src/vendor/*.cpp \
		src/base/*.cpp \
		src/mesh/*.cpp \
		src/modules/*.cpp \
		src/utility/*.cpp \
		)

#Include the .h and .cpp for a featureset if it exists
ifneq ("$(wildcard config/featuresets/$(FEATURESET).h)","")
	CFLAGS += -DFEATURESET_NAME=\"$(FEATURESET).h\"
endif
ifneq ("$(wildcard config/featuresets/$(FEATURESET).cpp)","")
	CPP_SOURCE_FILES += config/featuresets/$(FEATURESET).cpp
endif

#Build directories
BASE_DIRECTORY ?= _build
PLATFORM_DIRECTORY ?= $(BASE_DIRECTORY)/$(BUILD_TYPE)/$(PLATFORM)/$(FEATURESET)
OBJECT_DIRECTORY ?= $(PLATFORM_DIRECTORY)
LISTING_DIRECTORY ?= $(OBJECT_DIRECTORY)
OUTPUT_BINARY_DIRECTORY ?= $(OBJECT_DIRECTORY)
DEPEND_DIRECTORY ?= $(OBJECT_DIRECTORY)

# Sorting removes duplicates
BUILD_DIRECTORIES := $(sort $(OBJECT_DIRECTORY) $(OUTPUT_BINARY_DIRECTORY) $(LISTING_DIRECTORY) )

# Debug flags
ifeq ($(BUILD_TYPE),release)
  DEBUG_FLAGS += -DNDEBUG -Os -g
else ifeq ($(BUILD_TYPE),debug)
  DEBUG_FLAGS += -DDEBUG -Og -g
endif

# Platform specific flags
include Makefile.$(PLATFORM)

# Includes common to all targets
ifeq ($(SDK),15)
INC_PATHS += -isystem $(INTEGRATION)/nrfx
INC_PATHS += -isystem $(INTEGRATION)/nrfx/legacy
INC_PATHS += -isystem $(DRIVERS)/include
INC_PATHS += -isystem $(HAL)
INC_PATHS += -isystem $(MODULES)/nrfx
INC_PATHS += -isystem $(MODULES)/nrfx/mdk
INC_PATHS += -isystem $(COMPONENTS)/libraries/delay
else
INC_PATHS += -isystem $(COMPONENTS)/drivers_nrf/common
INC_PATHS += -isystem $(COMPONENTS)/drivers_nrf/delay
INC_PATHS += -isystem $(COMPONENTS)/drivers_nrf/gpiote
INC_PATHS += -isystem $(COMPONENTS)/drivers_nrf/hal
endif
INC_PATHS += -isystem $(PORT_CONFIG)
INC_PATHS += -isystem $(COMPONENTS)/ble/common
INC_PATHS += -isystem $(COMPONENTS)/ble/ble_db_discovery
INC_PATHS += -isystem $(COMPONENTS)/ble/ble_radio_notification
INC_PATHS += -isystem $(COMPONENTS)/ble/ble_services/ble_dfu
INC_PATHS += -isystem $(COMPONENTS)/device
INC_PATHS += -isystem $(COMPONENTS)/libraries/button
INC_PATHS += -isystem $(COMPONENTS)/libraries/timer
INC_PATHS += -isystem $(COMPONENTS)/libraries/util
INC_PATHS += -isystem $(COMPONENTS)/softdevice/common
INC_PATHS += -isystem $(COMPONENTS)/softdevice/common/softdevice_handler
INC_PATHS += -isystem $(COMPONENTS)/toolchain
INC_PATHS += -isystem $(COMPONENTS)/toolchain/gcc
INC_PATHS += -isystem $(COMPONENTS)/toolchain/cmsis/include
INC_PATHS += -Isrc/c/segger_rtt
INC_PATHS += -Isrc
INC_PATHS += -Isrc/base
INC_PATHS += -Isrc/mesh
INC_PATHS += -Isrc/modules
INC_PATHS += -Isrc/test
INC_PATHS += -Isrc/utility
INC_PATHS += -Iconfig
INC_PATHS += -Iconfig/boards
INC_PATHS += -Iconfig/featuresets
INC_PATHS += -Isrc/vendor

#Conditionally disable stack unwinding
ifneq ("$(DISABLE_STACK_UNWINDING)","1")
  CFLAGS += -funwind-tables
endif

CFLAGS += -DSDK=$(SDK)

# Flags common to all targets
CFLAGS += -mcpu=$(CPU) -mthumb -fmessage-length=0 -fsigned-char
CFLAGS += -ffunction-sections -fdata-sections -flto -fno-move-loop-invariants -fno-math-errno -fno-unroll-loops
CFLAGS += -Wextra -Werror -DBLE_STACK_SUPPORT_REQD $(DEBUG_FLAGS) -DFEATURESET=$(FEATURESET)
CFLAGS += -D__need___va_list -fno-strict-aliasing

# C++ compiler flags
CXXFLAGS += $(CFLAGS)
CXXFLAGS += -Wall -Wcast-qual -Wlogical-op -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-variable -Wno-vla -Wno-unused-parameter -fabi-version=0 -fno-exceptions -fno-rtti -fno-use-cxa-atexit
CXXFLAGS += -fno-threadsafe-statics

# Linker flags
LDFLAGS += $(CFLAGS)
LDFLAGS += -Llinker/ -T$(LINKER_SCRIPT) -Xlinker --gc-sections
LDFLAGS += -Wl,-Map,"$(LISTING_DIRECTORY)/$(OUTPUT_FILENAME).map" --specs=nano.specs
# prod_asset_ins_nrf52840 uses Tensorflow which unfortunatly depends on linking to malloc, even though it promisses to not use it.
ifneq ("$(FEATURESET)","prod_asset_ins_nrf52840")
	LDFLAGS += -Xlinker --wrap=malloc -Xlinker --wrap=calloc
endif

# Assembler flags
ASMFLAGS += $(CFLAGS) -x assembler-with-cpp -D__HEAP_SIZE=$(HEAP_SIZE) -D__STACK_SIZE=$(STACK_SIZE)

LIBS += -lgcc -lc -lnosys

ifeq ("$(USE_CMAKE)","1")
  TARGET_CMD += $(addsuffix _release,$(FEATURESET))
else
  TARGET_CMD += $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex echosize
endif

 ## Building all targets
all: $(TARGET_CMD)

## Target for printing all targets (ayyyyyyy)
help:
	@echo following targets are available:
	@echo   all
	@echo 	flash
	@echo 	flash_app
	@echo 	flash_softdevice


C_SOURCE_FILE_NAMES = $(notdir $(C_SOURCE_FILES))
C_PATHS = $(call remduplicates, $(dir $(C_SOURCE_FILES) ) )
C_OBJECTS = $(addprefix $(OBJECT_DIRECTORY)/, $(C_SOURCE_FILE_NAMES:.c=.o) )

CPP_SOURCE_FILE_NAMES = $(notdir $(CPP_SOURCE_FILES))
CPP_PATHS = $(call remduplicates, $(dir $(CPP_SOURCE_FILES) ) )
CPP_OBJECTS = $(addprefix $(OBJECT_DIRECTORY)/, $(CPP_SOURCE_FILE_NAMES:.cpp=.o) )

ASM_SOURCE_FILE_NAMES = $(notdir $(ASM_SOURCE_FILES))
ASM_PATHS = $(call remduplicates, $(dir $(ASM_SOURCE_FILES) ))
ASM_OBJECTS = $(addprefix $(OBJECT_DIRECTORY)/, $(ASM_SOURCE_FILE_NAMES:.s=.o) )

vpath %.c $(C_PATHS)
vpath %.cpp $(CPP_PATHS)
vpath %.s $(ASM_PATHS)

OBJECTS = $(C_OBJECTS) $(CPP_OBJECTS) $(ASM_OBJECTS)

DEPEND = $(patsubst %.c, $(DEPEND_DIRECTORY)/%.d, $(C_SOURCE_FILE_NAMES)) $(patsubst %.cpp, $(DEPEND_DIRECTORY)/%.d, $(CPP_SOURCE_FILE_NAMES))
.PRECIOUS: $(DEPEND)

## Create objects from C source files
$(OBJECT_DIRECTORY)/%.o: %.c
	$(NO_ECHO)$(MK) $(@D)
	@echo "Compiling C   file: $(notdir $<)"
	$(NO_ECHO)$(CC) --std=gnu99 $(CFLAGS) $(INC_PATHS) -c -o $@ $<
	$(NO_ECHO)$(CC) --std=gnu99 $(CFLAGS) $(INC_PATHS) -MM -MT $(OBJECT_DIRECTORY)/$*.o $< -o $(DEPEND_DIRECTORY)/$*.d

## Create objects from C++ source files
$(OBJECT_DIRECTORY)/%.o: %.cpp
	$(NO_ECHO)$(MK) $(@D)
	@echo "Compiling CPP file: $(notdir $<)"
	$(NO_ECHO)$(CXX) --std=c++11 $(CXXFLAGS) $(INC_PATHS) -c -o $@ $<
	$(NO_ECHO)$(CXX) --std=c++11 $(CXXFLAGS) $(INC_PATHS) -MM -MT $(OBJECT_DIRECTORY)/$*.o $< -o $(DEPEND_DIRECTORY)/$*.d

## Assemble files
$(OBJECT_DIRECTORY)/%.o: %.s
	$(NO_ECHO)$(MK) $(@D)
	@echo "Compiling ASM file: $(notdir $<)"
	$(NO_ECHO)$(CC) $(ASMFLAGS) $(INC_PATHS) -c -o $@ $<

## Link
$(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out: $(OBJECTS)
	$(NO_ECHO)$(MK) $(@D)
	@echo Linking target: $(OUTPUT_FILENAME).out
	$(NO_ECHO)$(CXX) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out

## Create binary .hex file from the .out file
$(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	$(NO_ECHO)$(MK) $(@D)
	@echo Preparing: $(OUTPUT_FILENAME).hex
	$(NO_ECHO)$(OBJCOPY) -O ihex $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex

## Display binary size
echosize: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	-@echo ' --------------------------------------------------------------'
	-@echo '  Build type: $(BUILD_TYPE)'
	-@echo '  Platform:   $(PLATFORM)'
	-@echo '  Featureset: $(FEATURESET)'
	-@echo ''
	$(NO_ECHO)$(SIZE) $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	-@echo ' --------------------------------------------------------------'
## We now want to evaluate if the size is too big for updates
	$(eval SIZEINFO = $(shell $(SIZE) $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out))
	$(eval TEXT_SIZE = $(word 7, $(SIZEINFO)))
	$(eval DATA_SIZE = $(word 8, $(SIZEINFO)))
	@TOTAL_SIZE=$$(($(TEXT_SIZE)+$(DATA_SIZE)));\
	if [ $$TOTAL_SIZE -gt $(MAX_UPDATABLE_APP_SIZE) ]; then\
		echo Total size is $$TOTAL_SIZE, maximum updatable size is $(MAX_UPDATABLE_APP_SIZE);\
		if [ $(FAIL_ON_SIZE_TOO_BIG) -gt 0 ]; then\
			echo "!FATAL ERROR! Firmware size is too big for updating over the mesh. !FATAL ERROR!";\
			exit 1;\
		else\
			echo "!WARNING! Firmware will be too big for updating over the mesh.";\
			echo "!WARNING! To solve this, undef some things in your featureset.";\
		fi;\
	else\
		echo Total size is $$TOTAL_SIZE of max $(MAX_UPDATABLE_APP_SIZE);\
	fi
	-@echo ' --------------------------------------------------------------'

## Cleans build directory for all platforms
clean:
	$(NO_ECHO)$(RM) $(BASE_DIRECTORY)

cleanobj:
	$(NO_ECHO)$(RM) $(BUILD_DIRECTORIES)/*.o

## Flash Softdevice and app
flash: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex
	@echo Flashing: $< + softdevice
	$(NO_ECHO)$(MERGEHEX) -m $(SOFTDEVICE_PATH) $< -o $(OUTPUT_BINARY_DIRECTORY)/fruitymesh_merged.hex
	$(NO_ECHO)$(NRFJPROG) --program $(OUTPUT_BINARY_DIRECTORY)/fruitymesh_merged.hex -f $(FAMILY) --chiperase $(PROGFLAGS)
	$(NO_ECHO)$(NRFJPROG) --reset -f $(FAMILY) $(PROGFLAGS)

## Flash app only
flash_app: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex
	@echo Flashing: $<
	$(NO_ECHO)$(NRFJPROG) --program $< -f $(FAMILY) --sectorerase $(PROGFLAGS)
	$(NO_ECHO)$(NRFJPROG) --reset -f $(FAMILY) $(PROGFLAGS)

## Flash softdevice only
flash_softdevice:
	@echo Flashing: $(SOFTDEVICE_PATH)
	$(NO_ECHO)$(NRFJPROG) --program $(SOFTDEVICE_PATH) -f $(FAMILY) --chiperase $(PROGFLAGS)
	$(NO_ECHO)$(NRFJPROG) --reset -f $(FAMILY) $(PROGFLAGS)

BUILD_DIR     		= ../cmake_build
FEATURESETS 	 	= prod_mesh_nrf51 prod_sink_nrf51 prod_sink_nrf52 prod_mesh_nrf52 github prod_eink_nrf51 prod_asset_nrf52 dev_asset_nrf52 prod_asset_nrf51 prod_clc_mesh_nrf52 prod_vs_nrf52 dev_wm_nrf52840 dev_automated_tests_master_nrf52 dev_automated_tests_slave_nrf52 prod_clc_sink_nrf51 dev_all_nrf52 prod_asset_ins_nrf52840
RELEASE_TARGETS     = $(foreach FEATURESET,$(FEATURESETS), $(FEATURESET)_release) 
DEBUG_TARGETS       = $(foreach FEATURESET,$(FEATURESETS), $(FEATURESET)_debug)

cmake-clean:
	$(RM) $(BUILD_DIR)

cmake-clean-win:
	$(RM) $(BUILD_DIR)_win

$(RELEASE_TARGETS): %:
	mkdir -p $(BUILD_DIR)/$* && cd $(BUILD_DIR)/$* && cmake -G "Eclipse CDT4 - Unix Makefiles" -DFEATURESET=$(subst _release,,$*) -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_TOOLCHAIN_FILE=CMake/arm_none_eabi_toolchain.cmake ../../fruitymesh && make -j

$(DEBUG_TARGETS): %:
	mkdir -p $(BUILD_DIR)/$* && cd $(BUILD_DIR)/$* && cmake -G "Eclipse CDT4 - Unix Makefiles" -DFEATURESET=$(subst _debug,,$*) -DCMAKE_BUILD_TYPE=DEBUG -DCMAKE_TOOLCHAIN_FILE=CMake/arm_none_eabi_toolchain.cmake ../../fruitymesh && make -j

cherrysim_runner:
	mkdir -p $(BUILD_DIR)/cherrysim_runner && cd $(BUILD_DIR)/cherrysim_runner && cmake -G "Eclipse CDT4 - Unix Makefiles"  -DCHERRYSIM_ENABLED=CHERRYSIM_RUNNER_ENABLED -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_TOOLCHAIN_FILE=CMake/pc_toolchain.cmake ../../fruitymesh && make -j

cherrysim_runner_run:
	$(BUILD_DIR)/cherrysim_runner/cherrysim/cherrySim_runner

cherrysim_runner_win:
	mkdir -p $(BUILD_DIR)_win/cherrysim_runner && cd $(BUILD_DIR)_win/cherrysim_runner && cmake -G "Eclipse CDT4 - Unix Makefiles"  -DCHERRYSIM_ENABLED=CHERRYSIM_RUNNER_ENABLED -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_TOOLCHAIN_FILE=CMake/pc_toolchain.cmake ../../fruitymesh && make -j

cherrysim_runner_win_run:
	$(BUILD_DIR)_win/cherrysim_runner/cherrysim/cherrySim_runner

cherrysim_tester:
	mkdir -p $(BUILD_DIR)/cherrysim_tester && cd $(BUILD_DIR)/cherrysim_tester && cmake -G "Eclipse CDT4 - Unix Makefiles" -DENABLE_GT=1 -DCHERRYSIM_ENABLED=CHERRYSIM_TESTER_ENABLED -DCMAKE_BUILD_TYPE=DEBUG -DCMAKE_TOOLCHAIN_FILE=CMake/pc_toolchain.cmake ../../fruitymesh && make -j

cherrysim_tester_win:
	mkdir -p $(BUILD_DIR)_win/cherrysim_tester && cd $(BUILD_DIR)_win/cherrysim_tester && cmake -G "Eclipse CDT4 - Unix Makefiles" -DENABLE_GT=1 -DCHERRYSIM_ENABLED=CHERRYSIM_TESTER_ENABLED -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_TOOLCHAIN_FILE=CMake/pc_toolchain.cmake ../../fruitymesh && make -j

cherrysim_tester_run:
	$(BUILD_DIR)/cherrysim_tester/cherrysim/cherrySim_tester GitLab

cherrysim_tester_win_run:
	$(BUILD_DIR)_win/cherrysim_tester/cherrysim/cherrySim_tester

cmake-help:
	@echo "***********************************"
	@echo "Available cmake FruityMesh targets:"
	@for FEATURESET in ${RELEASE_TARGETS}; do \
		echo " * $$FEATURESET"; \
		done \

	@echo "***********************************"
	@echo "Available cmake CherrySim targets:"
	@echo " * cherrysim_runner"
	@echo " * cherrysim_runner_run"
	@echo " * cherrysim_tester"
	@echo " * cherrysim_tester_run"

#.NOTPARALLEL: clean (Does not work because http://stackoverflow.com/questions/16829933/how-to-use-makefile-with-notparallel-label)
.PHONY: flash flash_softdevice clean serial debug

-include $(DEPEND)
