# Include local configuration changes in Makefile.local
-include Makefile.local

PROJECT_NAME ?= FruityMesh
BUILD_TYPE   ?= debug
BOARD        ?= PCA10031
VERBOSE      ?= 0

SERIAL_DEVICE ?= /dev/ttyACM0  # FIXME Use proper matching for OSX compatiblity

OUTPUT_FILENAME = $(PROJECT_NAME)

SDK_PATH      ?= $(HOME)/nrf/sdk/nrf5_sdk_latest
COMPONENTS     = $(SDK_PATH)/components
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
MK              := mkdir
RM              := rm -rf
NRFJPROG        := nrfjprog

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

#function for removing duplicates in a list
remduplicates = $(strip $(if $1,$(firstword $1) $(call remduplicates,$(filter-out $(firstword $1),$1))))

#source common to all targets
C_SOURCE_FILES += $(COMPONENTS)/ble/ble_radio_notification/ble_radio_notification.c
C_SOURCE_FILES +=	$(COMPONENTS)/drivers_nrf/pstorage/pstorage.c
C_SOURCE_FILES += $(COMPONENTS)/libraries/button/app_button.c
C_SOURCE_FILES +=	$(COMPONENTS)/libraries/timer/app_timer.c
C_SOURCE_FILES +=	$(COMPONENTS)/softdevice/common/softdevice_handler/softdevice_handler.c
C_SOURCE_FILES +=	src/nrf/simple_uart.c
C_SOURCE_FILES +=	src/nrf/crc16.c

CPP_SOURCE_FILES += $(wildcard \
		src/*.cpp \
		src/base/*.cpp \
		src/mesh/*.cpp \
		src/modules/*.cpp \
		src/test/*.cpp \
		src/utility/*.cpp \
		)

# Includes common to all targets
INC_PATHS += -Iinc
INC_PATHS += -Iinc_c
INC_PATHS += -Iconfig
INC_PATHS += -I$(COMPONENTS)/ble/common
INC_PATHS += -I$(COMPONENTS)/ble/ble_radio_notification
INC_PATHS += -I$(COMPONENTS)/ble/ble_services/ble_dfu
INC_PATHS += -I$(COMPONENTS)/device
INC_PATHS += -I$(COMPONENTS)/drivers_nrf/common
INC_PATHS += -I$(COMPONENTS)/drivers_nrf/config
INC_PATHS += -I$(COMPONENTS)/drivers_nrf/delay
INC_PATHS += -I$(COMPONENTS)/drivers_nrf/gpiote
INC_PATHS += -I$(COMPONENTS)/drivers_nrf/hal
INC_PATHS += -I$(COMPONENTS)/drivers_nrf/pstorage
INC_PATHS += -I$(COMPONENTS)/libraries/button
INC_PATHS += -I$(COMPONENTS)/libraries/timer
INC_PATHS += -I$(COMPONENTS)/libraries/util
INC_PATHS += -I$(COMPONENTS)/softdevice/common/softdevice_handler
INC_PATHS += -I$(COMPONENTS)/toolchain
INC_PATHS += -I$(COMPONENTS)/toolchain/gcc
INC_PATHS += -I$(COMPONENTS)/toolchain/CMSIS/Include
INC_PATHS += -I$(SDK_PATH)/examples/bsp

OBJECT_DIRECTORY ?= _build
LISTING_DIRECTORY ?= $(OBJECT_DIRECTORY)
OUTPUT_BINARY_DIRECTORY ?= $(OBJECT_DIRECTORY)

# Sorting removes duplicates
BUILD_DIRECTORIES := $(sort $(OBJECT_DIRECTORY) $(OUTPUT_BINARY_DIRECTORY) $(LISTING_DIRECTORY) )

# Debug flags
ifeq ($(BUILD_TYPE),debug)
  DEBUG_FLAGS += -D DEBUG -Os -g
else
  DEBUG_FLAGS += -D NDEBUG -Os
endif

# Board specific flags
ifneq ($(filter $(BOARD),PCA10031 ARS100748),)
	PLATFORM ?= NRF51
else ifneq ($(filter $(BOARD),PCA10036 PCA10040),)
	PLATFORM ?= NRF52
endif

# Platform specific flags
include Makefile.$(PLATFORM)

# Flags common to all targets
CFLAGS += -mcpu=$(CPU) -mthumb -fmessage-length=0 -fsigned-char
CFLAGS += -ffunction-sections -fdata-sections -flto -fno-move-loop-invariants
CFLAGS += -Wextra -DBLE_STACK_SUPPORT_REQD $(DEBUG_FLAGS) -D$(BOARD)
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
all: $(BUILD_DIRECTORIES) $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex echosize

## Target for printing all targets (ayyyyyyy)
help:
	@echo following targets are available:
	@echo   all
	@echo 	flash
	@echo 	flash_softdevice
	@echo   serial


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

## Create build directories
$(BUILD_DIRECTORIES):
	@echo Making $@
	$(NO_ECHO)$(MK) $@

## Create objects from C source files
$(OBJECT_DIRECTORY)/%.o: %.c
	@echo Compiling file: $(notdir $<)
	$(NO_ECHO)$(CC) --std=gnu99 $(CFLAGS) $(INC_PATHS) -c -o $@ $<

## Create objects from C++ source files
$(OBJECT_DIRECTORY)/%.o: %.cpp
	@echo Compiling file: $(notdir $<)
	$(NO_ECHO)$(CXX) --std=c++11 $(CXXFLAGS) $(INC_PATHS) -c -o $@ $<

## Assemble files
$(OBJECT_DIRECTORY)/%.o: %.s
	@echo Compiling file: $(notdir $<)
	$(NO_ECHO)$(CC) $(ASMFLAGS) $(INC_PATHS) -c -o $@ $<

## Link
$(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out: $(OBJECTS)
	@echo Linking target: $(OUTPUT_FILENAME).out
	$(NO_ECHO)$(CXX) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out

## Create binary .hex file from the .out file
$(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	@echo Preparing: $(OUTPUT_FILENAME).hex
	$(NO_ECHO)$(OBJCOPY) -O ihex $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex

## Display binary size
echosize: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	-@echo ' --------------------------------------------------------------'
	-@echo '  Board:      $(BOARD)'
	-@echo '  Platform:   $(PLATFORM)'
	-@echo '  Build type: $(BUILD_TYPE)'
	-@echo ''
	$(NO_ECHO)$(SIZE) $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	-@echo ' --------------------------------------------------------------'

## Cleanup
clean:
	$(NO_ECHO)$(RM) $(BUILD_DIRECTORIES)

cleanobj:
	$(NO_ECHO)$(RM) $(BUILD_DIRECTORIES)/*.o

## Flash device
flash: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex
	@echo Flashing: $<
	$(NO_ECHO)$(NRFJPROG) --program $< -f $(PLATFORM) --sectorerase
	$(NO_ECHO)$(NRFJPROG) --reset -f $(PLATFORM)

## Flash softdevice
flash_softdevice:
	@echo Flashing: $(SOFTDEVICE_PATH)
	$(NO_ECHO)$(NRFJPROG) --program $(SOFTDEVICE_PATH) -f $(PLATFORM) --chiperase
	$(NO_ECHO)$(NRFJPROG) --reset -f $(PLATFORM)

serial:
	# FIXME use proper tools for multiple platforms
	$(NO_ECHO)screen $(SERIAL_DEVICE) 38400

.NOTPARALLEL: clean
.PHONY: flash flash_softdevice clean serial debug
