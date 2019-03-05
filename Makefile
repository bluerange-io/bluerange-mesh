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
# Give the parameters to make directly, e.g. make -j4 PLATFORM=NRF51 FEATURESET=dev51

# Build types are: debug, release
BUILD_TYPE       ?= release
# Platforms are: NRF51, NRF52, NRF52840
PLATFORM         ?= NRF52
VERBOSE          ?= 0
# Featuresets are found in config/featuresets
FEATURESET       ?= github

# Set to 1 if make should fail once the app is too big to be updatable over the mesh
FAIL_ON_SIZE_TOO_BIG 		?= 0
# Set to disable stack unwinding code that takes up space
DISABLE_STACK_UNWINDING 	?= 1

# *******************************************************************************************

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

# NRF51 and NRF52/NRF52840 need different SDKs because the latest SDK isn't compatible with nRF51 anymore
# The sdks are part of the project because bugs had to be fixed
ifeq ($(PLATFORM),NRF51)
NRF5_SDK_PATH = sdk/sdk11
else
NRF5_SDK_PATH = sdk/sdk14
endif

COMPONENTS     = $(NRF5_SDK_PATH)/components
TEMPLATE_PATH  = $(COMPONENTS)/toolchain/gcc

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
C_SOURCE_FILES += $(COMPONENTS)/drivers_nrf/common/nrf_drv_common.c
C_SOURCE_FILES += $(COMPONENTS)/drivers_nrf/gpiote/nrf_drv_gpiote.c
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

# We use generic board definitions, as we determine the board at runtime
ifneq ($(filter $(PLATFORM),NRF51),)
  BOARD ?= NRF51_BOARD
else ifneq ($(filter $(PLATFORM),NRF52),)
  BOARD ?= NRF52_BOARD
 else ifneq ($(filter $(PLATFORM),NRF52840),)
  BOARD ?= NRF52840_BOARD
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
  DEBUG_FLAGS += -D NDEBUG -Os -g
else ifeq ($(BUILD_TYPE),debug)
  DEBUG_FLAGS += -D DEBUG -Og -g
endif

# Check if Featureset exists
ifneq ("$(wildcard config/featureset.h)","")
CFLAGS += -DUSE_FEATURESET_H
endif

# Platform specific flags
include Makefile.$(PLATFORM)

# Includes common to all targets
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
INC_PATHS += -I$(COMPONENTS)/ble/common
INC_PATHS += -I$(COMPONENTS)/ble/ble_db_discovery
INC_PATHS += -I$(COMPONENTS)/ble/ble_radio_notification
INC_PATHS += -I$(COMPONENTS)/ble/ble_services/ble_dfu
INC_PATHS += -I$(COMPONENTS)/device
INC_PATHS += -I$(COMPONENTS)/drivers_nrf/common
INC_PATHS += -I$(COMPONENTS)/drivers_nrf/delay
INC_PATHS += -I$(COMPONENTS)/drivers_nrf/gpiote
INC_PATHS += -I$(COMPONENTS)/drivers_nrf/hal
INC_PATHS += -I$(COMPONENTS)/libraries/button
INC_PATHS += -I$(COMPONENTS)/libraries/timer
INC_PATHS += -I$(COMPONENTS)/libraries/util
INC_PATHS += -I$(COMPONENTS)/softdevice/common
INC_PATHS += -I$(COMPONENTS)/softdevice/common/softdevice_handler
INC_PATHS += -I$(COMPONENTS)/toolchain
INC_PATHS += -I$(COMPONENTS)/toolchain/gcc
INC_PATHS += -I$(COMPONENTS)/toolchain/cmsis/include

#Conditionally disable stack unwinding
ifneq ("$(DISABLE_STACK_UNWINDING)","1")
  CFLAGS += -funwind-tables
endif

# Flags common to all targets
CFLAGS += -mcpu=$(CPU) -mthumb -fmessage-length=0 -fsigned-char
CFLAGS += -ffunction-sections -fdata-sections -flto -fno-move-loop-invariants -fno-math-errno -fno-unroll-loops
CFLAGS += -Wextra -DBLE_STACK_SUPPORT_REQD $(DEBUG_FLAGS) -D$(BOARD) -DFEATURESET=$(FEATURESET)
CFLAGS += -D$(PLATFORM) -D__need___va_list

# C++ compiler flags
CXXFLAGS += $(CFLAGS)
CXXFLAGS += -fabi-version=0 -fno-exceptions -fno-rtti -fno-use-cxa-atexit
CXXFLAGS += -fno-threadsafe-statics

# Linker flags
LDFLAGS += $(CFLAGS)
LDFLAGS += -Llinker/ -T$(LINKER_SCRIPT) -Xlinker --gc-sections
LDFLAGS += -Wl,-Map,"$(LISTING_DIRECTORY)/$(OUTPUT_FILENAME).map" --specs=nano.specs

# Assembler flags
ASMFLAGS += $(CFLAGS) -x assembler-with-cpp -D__HEAP_SIZE=$(HEAP_SIZE) -D__STACK_SIZE=$(STACK_SIZE)

LIBS += -lgcc -lc -lnosys

## Building all targets
all: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex echosize

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
	@echo Compiling file: $(notdir $<)
	$(NO_ECHO)$(CC) --std=gnu99 $(CFLAGS) $(INC_PATHS) -c -o $@ $<
	$(NO_ECHO)$(CC) --std=gnu99 $(CFLAGS) $(INC_PATHS) -MM -MT $(OBJECT_DIRECTORY)/$*.o $< -o $(DEPEND_DIRECTORY)/$*.d

## Create objects from C++ source files
$(OBJECT_DIRECTORY)/%.o: %.cpp
	$(NO_ECHO)$(MK) $(@D)
	@echo Compiling file: $(notdir $<)
	$(NO_ECHO)$(CXX) --std=c++11 $(CXXFLAGS) $(INC_PATHS) -c -o $@ $<
	$(NO_ECHO)$(CXX) --std=c++11 $(CXXFLAGS) $(INC_PATHS) -MM -MT $(OBJECT_DIRECTORY)/$*.o $< -o $(DEPEND_DIRECTORY)/$*.d

## Assemble files
$(OBJECT_DIRECTORY)/%.o: %.s
	$(NO_ECHO)$(MK) $(@D)
	@echo Compiling file: $(notdir $<)
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
# Take the 10th word from the size command which corresponds to the total decimal file size
	$(eval TOTAL_SIZE = $(word 10, $(SIZEINFO)))
	@if [ ${TOTAL_SIZE} -gt $(MAX_UPDATABLE_APP_SIZE) ]; then\
		echo Total size is $(TOTAL_SIZE), maximum updatable size is $(MAX_UPDATABLE_APP_SIZE);\
		if [ $(FAIL_ON_SIZE_TOO_BIG) -gt 0 ]; then\
			echo "!FATAL ERROR! Firmware size is too big for updating over the mesh. !FATAL ERROR!";\
			exit 1;\
		else\
			echo "!WARNING! Firmware will be too big for updating over the mesh.";\
			echo "!WARNING! To solve this, undef some things in your featureset.";\
		fi;\
	else\
		echo Total size is $(TOTAL_SIZE) of max $(MAX_UPDATABLE_APP_SIZE);\
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
	$(NO_ECHO)$(NRFJPROG) --program $(OUTPUT_BINARY_DIRECTORY)/fruitymesh_merged.hex -f $(PLATFORM) --chiperase $(PROGFLAGS)
	$(NO_ECHO)$(NRFJPROG) --reset -f $(PLATFORM) $(PROGFLAGS)

## Flash app only
flash_app: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex
	@echo Flashing: $<
	$(NO_ECHO)$(NRFJPROG) --program $< -f $(PLATFORM) --sectorerase $(PROGFLAGS)
	$(NO_ECHO)$(NRFJPROG) --reset -f $(PLATFORM) $(PROGFLAGS)

## Flash softdevice only
flash_softdevice:
	@echo Flashing: $(SOFTDEVICE_PATH)
	$(NO_ECHO)$(NRFJPROG) --program $(SOFTDEVICE_PATH) -f $(PLATFORM) --chiperase $(PROGFLAGS)
	$(NO_ECHO)$(NRFJPROG) --reset -f $(PLATFORM) $(PROGFLAGS)

#.NOTPARALLEL: clean (Does not work because http://stackoverflow.com/questions/16829933/how-to-use-makefile-with-notparallel-label)
.PHONY: flash flash_softdevice clean serial debug

-include $(DEPEND)
