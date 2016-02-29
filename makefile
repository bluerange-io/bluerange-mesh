#------------------------------------------------------------------------------
# Firmware build
#
# Selectable build options 
#------------------------------------------------------------------------------
TARGET_BOARD         ?= BOARD_PCA10031

#------------------------------------------------------------------------------
# Define relative paths to SDK components
#------------------------------------------------------------------------------

SDK_BASE      := $(HOME)/nrf/sdk/nrf51_sdk_latest
COMPONENTS    := $(SDK_BASE)/components
TEMPLATE_PATH := $(COMPONENTS)/toolchain/gcc
EHAL_PATH     := $(HOME)/nrf/sdk/ehal_latest
LINKER_SCRIPT := ./linker/gcc_nrf51_s130_32kb.ld
OUTPUT_NAME   := FruityMesh
JLINK	      := jlinkexe

OS := $(shell uname -s)
ifeq ($(OS),Darwin)
  JLINK 	:= jlinkexe
else
  JLINK		:= jlink
endif


#------------------------------------------------------------------------------
# Proceed cautiously beyond this point.  Little should change.
#------------------------------------------------------------------------------

export OUTPUT_NAME
export GNU_INSTALL_ROOT

MAKEFILE_NAME := $(MAKEFILE_LIST)
MAKEFILE_DIR := $(dir $(MAKEFILE_NAME) ) 

ifeq ($(OS),Windows_NT)
  include $(TEMPLATE_PATH)/Makefile.windows
else
  include $(TEMPLATE_PATH)/Makefile.posix
endif

# echo suspend
ifeq ("$(VERBOSE)","1")
  NO_ECHO := 
else
  NO_ECHO := @
endif

ifeq ($(MAKECMDGOALS),debug)
  BUILD_TYPE := debug
else
  BUILD_TYPE := release
endif

# Toolchain commands
CC       := "$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-gcc"
CPP      := "$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-g++"
OBJCOPY  := "$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-objcopy"
SIZE     := "$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-size"
MK       := mkdir
RM       := rm -rf
CP       := cp

# function for removing duplicates in a list
remduplicates = $(strip $(if $1,$(firstword $1) $(call remduplicates,$(filter-out $(firstword $1),$1))))

# source common to all targets

CPP_SOURCE_FILES += ./src/base/AdvertisingController.cpp
CPP_SOURCE_FILES += ./src/base/GAPController.cpp
CPP_SOURCE_FILES += ./src/base/GATTController.cpp
CPP_SOURCE_FILES += ./src/base/ScanController.cpp
CPP_SOURCE_FILES += ./src/Main.cpp
CPP_SOURCE_FILES += ./src/mesh/Connection.cpp
CPP_SOURCE_FILES += ./src/mesh/ConnectionManager.cpp
CPP_SOURCE_FILES += ./src/mesh/Node.cpp
CPP_SOURCE_FILES += ./src/modules/AdvertisingModule.cpp
CPP_SOURCE_FILES += ./src/modules/DFUModule.cpp
CPP_SOURCE_FILES += ./src/modules/EnrollmentModule.cpp
CPP_SOURCE_FILES += ./src/modules/Module.cpp
CPP_SOURCE_FILES += ./src/modules/ScanningModule.cpp
CPP_SOURCE_FILES += ./src/modules/StatusReporterModule.cpp
CPP_SOURCE_FILES += ./src/modules/DebugModule.cpp
CPP_SOURCE_FILES += ./src/modules/IoModule.cpp
CPP_SOURCE_FILES += ./src/test/TestBattery.cpp
CPP_SOURCE_FILES += ./src/test/Testing.cpp
CPP_SOURCE_FILES += ./src/utility/LedWrapper.cpp
CPP_SOURCE_FILES += ./src/utility/Logger.cpp
CPP_SOURCE_FILES += ./src/utility/PacketQueue.cpp
CPP_SOURCE_FILES += ./src/utility/SimpleBuffer.cpp
CPP_SOURCE_FILES += ./src/utility/SimplePushStack.cpp
CPP_SOURCE_FILES += ./src/utility/SimpleQueue.cpp
CPP_SOURCE_FILES += ./src/utility/Storage.cpp
CPP_SOURCE_FILES += ./src/utility/Terminal.cpp
CPP_SOURCE_FILES += ./src/utility/Utility.cpp

C_SOURCE_FILES += $(EHAL_PATH)/ARM/Nordic/nRF51/src/Vectors_nRF51.c
C_SOURCE_FILES += $(COMPONENTS)/libraries/timer/app_timer.c
C_SOURCE_FILES += $(COMPONENTS)/ble/ble_radio_notification/ble_radio_notification.c
C_SOURCE_FILES += ./src/nrf/simple_uart.c
C_SOURCE_FILES += $(COMPONENTS)/drivers_nrf/hal/nrf_delay.c
C_SOURCE_FILES += $(COMPONENTS)/drivers_nrf/pstorage/pstorage.c
C_SOURCE_FILES += $(COMPONENTS)/softdevice/common/softdevice_handler/softdevice_handler.c

# includes common to all targets

#fruity
INC_PATHS += -I./inc
INC_PATHS += -I./inc_c
INC_PATHS += -I./config

#arm GCC

#nordic nrf51
INC_PATHS += -I$(COMPONENTS)/ble/ble_radio_notification
INC_PATHS += -I$(COMPONENTS)/ble/ble_services/ble_dfu
INC_PATHS += -I$(COMPONENTS)/ble/common
INC_PATHS += -I$(COMPONENTS)/device
INC_PATHS += -I$(COMPONENTS)/libraries/timer
INC_PATHS += -I$(COMPONENTS)/libraries/util
INC_PATHS += -I$(COMPONENTS)/softdevice/common/softdevice_handler
INC_PATHS += -I$(COMPONENTS)/softdevice/s130/headers
INC_PATHS += -I$(COMPONENTS)/toolchain
INC_PATHS += -I$(COMPONENTS)/toolchain/arm
INC_PATHS += -I$(COMPONENTS)/toolchain/gcc
INC_PATHS += -I$(COMPONENTS)/drivers_nrf/pstorage
INC_PATHS += -I$(COMPONENTS)/drivers_nrf/hal

OBJECT_DIRECTORY = _build
LISTING_DIRECTORY = $(OBJECT_DIRECTORY)
OUTPUT_BINARY_DIRECTORY = $(OBJECT_DIRECTORY)

# Sorting removes duplicates
BUILD_DIRECTORIES := $(sort $(OBJECT_DIRECTORY) $(OUTPUT_BINARY_DIRECTORY) $(LISTING_DIRECTORY) )

ifeq ($(BUILD_TYPE),debug)
  DEBUG_FLAGS += -D DEBUG -g -O0
else
  DEBUG_FLAGS += -D NDEBUG -O3
endif

CFLAGS += -mcpu=cortex-m0
CFLAGS += -mthumb 
CFLAGS += -Og
CFLAGS += -fmessage-length=0
CFLAGS += -fsigned-char 
CFLAGS += -ffunction-sections 
CFLAGS += -fdata-sections 
CFLAGS += -flto
CFLAGS += -fno-move-loop-invariants
CFLAGS += -Wextra
CFLAGS += -g3
CFLAGS += -DBLE_STACK_SUPPORT_REQD
CFLAGS += $(DEBUG_FLAGS)
CFLAGS += -D$(TARGET_BOARD)
CFLAGS += -DNRF51
CFLAGS += -D__need___va_list
CFLAGS += -w
CFLAGS += -fabi-version=0
CFLAGS += -fno-exceptions
CFLAGS += -fno-rtti
CFLAGS += -fno-use-cxa-atexit
CFLAGS += -fno-threadsafe-statics

CFLAGS += -DENABLE_LOGGING
CFLAGS += -DDEST_BOARD_ID=0

LDFLAGS += -mcpu=cortex-m0
LDFLAGS += -mthumb
LDFLAGS += -Og
LDFLAGS += -fmessage-length=0
LDFLAGS += -fsigned-char
LDFLAGS += -ffunction-sections
LDFLAGS += -flto
LDFLAGS += -fno-move-loop-invariants
LDFLAGS += -Wextra
LDFLAGS += -g3
LDFLAGS += -T$(LINKER_SCRIPT)
LDFLAGS += -Xlinker 
LDFLAGS += --gc-sections
LDFLAGS += -Wl,-Map,"_build/FruityMesh.map"
LDFLAGS += --specs=nano.specs

LIBS += -L$(EHAL_PATH)/ARM/src
LIBS += -L$(EHAL_PATH)/ARM/Nordic/nRF51/CMSIS/Debug
LIBS += -lCMSIS

CPP_SOURCE_FILE_NAMES = $(notdir $(CPP_SOURCE_FILES))
CPP_PATHS = $(call remduplicates, $(dir $(CPP_SOURCE_FILES) ) )
CPP_OBJECTS = $(addprefix $(OBJECT_DIRECTORY)/, $(CPP_SOURCE_FILE_NAMES:.cpp=.o) )

C_SOURCE_FILE_NAMES = $(notdir $(C_SOURCE_FILES))
C_PATHS = $(call remduplicates, $(dir $(C_SOURCE_FILES) ) )
C_OBJECTS = $(addprefix $(OBJECT_DIRECTORY)/, $(C_SOURCE_FILE_NAMES:.c=.o) )

TOOLCHAIN_BASE = $(basename $(notdir $(GNU_INSTALL_ROOT)))

TIMESTAMP := $(shell date +'%s')

vpath %.cpp $(CPP_PATHS)
vpath %.c $(C_PATHS)

OBJECTS = $(CPP_OBJECTS) $(C_OBJECTS) 

all: $(BUILD_DIRECTORIES) $(OBJECTS)
	@echo Linking target: $(OUTPUT_NAME).elf
	$(NO_ECHO)$(CPP) $(LDFLAGS) $(OBJECTS) $(LIBS) $(INC_PATHS) -o $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_NAME).elf
	$(NO_ECHO)$(MAKE) -f $(MAKEFILE_NAME) -C $(MAKEFILE_DIR) -e finalize

	@echo "*****************************************************"
	@echo "build project: $(OUTPUT_NAME)"
	@echo "build type:    $(BUILD_TYPE)"
	@echo "build with:    $(TOOLCHAIN_BASE)"
	@echo "build target:  $(TARGET_BOARD)"
	@echo "build products --"
	@echo "               $(OUTPUT_NAME).elf"
	@echo "               $(OUTPUT_NAME).hex"
	@echo "*****************************************************"

debug : all
release : all

flash: all
	$(JLINK) deploy/upload_fruitymesh.jlink

# Create build directories
$(BUILD_DIRECTORIES):
	echo $(MAKEFILE_NAME)
	$(MK) $@

# Create objects from CPP SRC files
$(OBJECT_DIRECTORY)/%.o: %.cpp
	@echo Compiling file: $(notdir $<)
	$(NO_ECHO)$(CPP) -std=c++11 $(CFLAGS) $(INC_PATHS) -c $< -o $@ > $(OUTPUT_BINARY_DIRECTORY)/$*.lst

# Create objects from C SRC files
$(OBJECT_DIRECTORY)/%.o: %.c
	@echo Compiling file: $(notdir $<)
	$(NO_ECHO)$(CC) -std=gnu99 $(CFLAGS) $(INC_PATHS) -c $< -o $@ > $(OUTPUT_BINARY_DIRECTORY)/$*.lst

# Link
$(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_NAME).elf: $(BUILD_DIRECTORIES) $(OBJECTS)
	@echo Linking target: $(OUTPUT_NAME).elf
	$(NO_ECHO)$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_NAME).elf

# Create binary .bin file from the .elf file
$(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_NAME).bin: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_NAME).elf
	@echo Preparing: $(OUTPUT_NAME).bin
	$(NO_ECHO)$(OBJCOPY) -O binary $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_NAME).elf $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_NAME).bin

# Create binary .hex file from the .elf file
$(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_NAME).hex: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_NAME).elf
	@echo Preparing: $(OUTPUT_NAME).hex
	$(NO_ECHO)$(OBJCOPY) -O ihex $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_NAME).elf $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_NAME).hex

finalize: genbin genhex echosize

genbin:
	@echo Preparing: $(OUTPUT_NAME).bin
	$(NO_ECHO)$(OBJCOPY) -O binary $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_NAME).elf $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_NAME).bin

# Create binary .hex file from the .elf file
genhex: 
	@echo Preparing: $(OUTPUT_NAME).hex
	$(NO_ECHO)$(OBJCOPY) -O ihex $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_NAME).elf $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_NAME).hex

echosize:
	-@echo ""
	$(NO_ECHO)$(SIZE) $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_NAME).elf
	-@echo ""

clean:
	$(RM) $(BUILD_DIRECTORIES)

cleanobj:
	$(RM) $(BUILD_DIRECTORIES)/*.o

