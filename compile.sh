#!/bin/bash

if [ "$(uname)" == "Darwin" ]; then
	PLATFORM="mac"
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then
	PLATFORM="linux"
fi

if [ ! -f src/nrf/Vectors_nRF51.c ]; then
	cp ../../sdk/ehal_2015_06_01/ARM/Nordic/nRF51/src/Vectors_nRF51.c src/nrf/
	cp ../../sdk/nrf_sdk_9_0/components/libraries/timer/app_timer.c src/nrf/
	cp ../../sdk/nrf_sdk_9_0/components/ble/ble_radio_notification/ble_radio_notification.c src/nrf/
	cp ../../sdk/nrf_sdk_9_0/components/drivers_nrf/hal/nrf_delay.c src/nrf/
	cp ../../sdk/nrf_sdk_9_0/components/drivers_nrf/pstorage/pstorage.c src/nrf/
	cp ../../sdk/nrf_sdk_9_0/components/softdevice/common/softdevice_handler/softdevice_handler.c src/nrf/
fi

if [ ! -d Debug ]; then
	mkdir Debug
fi

echo "Cleaning..."

rm -f `find . -name "*.o" -or -name "*.d"` FruityMesh.elf Debug/FruityMesh.hex FruityMesh.map

if [ "$1" == "clean" ]; then
	exit 0
fi

IFS=$'\n'

for FILE in `find . -name "*.cpp" -not -path "./src_examples/*" -or -name "*.c" -not -path "./linker/*"`; do
	if [ "${FILE:${#FILE}-3}" == "cpp" ]; then
		COMPILER="g++"
		STD="c++11"
	else
		COMPILER="gcc"
		STD="gnu99"
	fi
	
	echo "Compiling ${FILE}..."

	${HOME}/nrf/sdk/gcc_arm_embedded_4_9_${PLATFORM}/bin/arm-none-eabi-${COMPILER} \
	-mcpu=cortex-m0 \
	-mthumb \
	-Og \
	-fmessage-length=0 \
	-fsigned-char \
	-ffunction-sections \
	-fdata-sections \
	-flto \
	-fno-move-loop-invariants \
	-g3 \
	-DBLE_STACK_SUPPORT_REQD \
	-DDEBUG \
	-DBOARD_PCA10031 \
	-DNRF51 \
	-D__need___va_list \
	-w \
	-I${HOME}/nrf/projects/fruitymesh/config \
	-I${HOME}/nrf/projects/fruitymesh/inc \
	-I${HOME}/nrf/projects/fruitymesh/inc_c \
	-I${HOME}/nrf/sdk/arm_cmsis_4_3/CMSIS/Driver/Include \
	-I${HOME}/nrf/sdk/arm_cmsis_4_3/CMSIS/Include \
	-I${HOME}/nrf/sdk/arm_cmsis_4_3/CMSIS/RTOS/RTX/INC \
	-I${HOME}/nrf/sdk/arm_cmsis_4_3/CMSIS/RTOS/RTX/SRC \
	-I${HOME}/nrf/sdk/arm_cmsis_4_3/CMSIS/RTOS/RTX/UserCodeTemplates \
	-I${HOME}/nrf/sdk/arm_cmsis_4_3/CMSIS/RTOS/Template \
	-I${HOME}/nrf/sdk/arm_cmsis_4_3/CMSIS/SVD \
	-I${HOME}/nrf/sdk/arm_cmsis_4_3/Device/ARM/ARMCM0/Include \
	-I${HOME}/nrf/sdk/arm_cmsis_4_3/Device/ARM/ARMCM0plus/Include \
	-I${HOME}/nrf/sdk/arm_cmsis_4_3/Device/ARM/ARMCM3/Include \
	-I${HOME}/nrf/sdk/arm_cmsis_4_3/Device/ARM/ARMCM4/Include \
	-I${HOME}/nrf/sdk/arm_cmsis_4_3/Device/ARM/ARMCM7/Include \
	-I${HOME}/nrf/sdk/arm_cmsis_4_3/Device/ARM/ARMSC000/Include \
	-I${HOME}/nrf/sdk/arm_cmsis_4_3/Device/ARM/ARMSC300/Include \
	-I${HOME}/nrf/sdk/arm_cmsis_4_3/Device/_Template_Flash \
	-I${HOME}/nrf/sdk/arm_cmsis_4_3/Device/_Template_Vendor/Vendor/Device/Include \
	-I${HOME}/nrf/sdk/ehal_2015_06_01/ARM/Nordic/nRF51/CMSIS/include \
	-I${HOME}/nrf/sdk/ehal_2015_06_01/ARM/Nordic/nRF51/dfu_nrf51/src \
	-I${HOME}/nrf/sdk/ehal_2015_06_01/ARM/Nordic/nRF51/dfu_nrf51/src/config_ble \
	-I${HOME}/nrf/sdk/ehal_2015_06_01/ARM/Nordic/nRF51/dfu_nrf51/src/config_ser \
	-I${HOME}/nrf/sdk/ehal_2015_06_01/ARM/Nordic/nRF51/dfu_sdk7/src \
	-I${HOME}/nrf/sdk/ehal_2015_06_01/ARM/Nordic/nRF51/EHAL/include \
	-I${HOME}/nrf/sdk/ehal_2015_06_01/ARM/Nordic/nRF51/exemples/Blinky_ble/src \
	-I${HOME}/nrf/sdk/ehal_2015_06_01/ARM/Nordic/nRF51/exemples/LMXDisplay_ble/src \
	-I${HOME}/nrf/sdk/ehal_2015_06_01/ARM/Nordic/nRF51/exemples/Uart_Ble/src \
	-I${HOME}/nrf/sdk/ehal_2015_06_01/ARM/NXP/include \
	-I${HOME}/nrf/sdk/ehal_2015_06_01/ARM/NXP/LPC11xx/CMSIS/include \
	-I${HOME}/nrf/sdk/ehal_2015_06_01/ARM/NXP/LPC11xx/EHAL/include \
	-I${HOME}/nrf/sdk/ehal_2015_06_01/ARM/NXP/LPC17xx/CMSIS/Include \
	-I${HOME}/nrf/sdk/ehal_2015_06_01/ARM/NXP/LPC17xx/EHAL/include \
	-I${HOME}/nrf/sdk/ehal_2015_06_01/ARM/TI/CC3200/CMSIS/include \
	-I${HOME}/nrf/sdk/ehal_2015_06_01/ARM/TI/CC3200/exemples/Blinky/src \
	-I${HOME}/nrf/sdk/ehal_2015_06_01/include \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ant/ant_channel_config \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ant/ant_profiles/ant_hrm \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ant/ant_profiles/ant_hrm/pages \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ant/ant_profiles/ant_hrm/pages/logger \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ant/ant_profiles/ant_hrm/utils \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ant/ant_pulse_simulator \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ant/ant_stack_config \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ant/ant_stack_config/config \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ant/ant_state_indicator \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_advertising \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_db_discovery \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_debug_assert_handler \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_dtm \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_error_log \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_racp \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_radio_notification \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_ancs_c \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_ans_c \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_bas \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_bas_c \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_bps \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_cscs \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_cts_c \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_dfu \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_dis \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_gls \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_hids \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_hrs \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_hrs_c \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_hts \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_ias \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_ias_c \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_lls \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_nus \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_rscs \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_rscs_c \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/ble_services/ble_tps \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/common \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/device_manager \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/ble/device_manager/config \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/device \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/ant_fs \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/bootloader_dfu \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/bootloader_dfu/ble_transport \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/bootloader_dfu/experimental \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/bootloader_dfu/hci_transport \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/button \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/console \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/crc16 \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/fifo \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/gpiote \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/hci \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/hci/config \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/ic_info \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/mem_manager \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/pwm \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/scheduler \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/sensorsim \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/sha256 \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/simple_timer \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/timer \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/trace \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/libraries/util \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/properitary_rf/esb \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/properitary_rf/gzll \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/properitary_rf/gzll/config \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/serialization/application/codecs/common \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/serialization/application/codecs/s130/serializers \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/serialization/application/hal \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/serialization/application/transport \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/serialization/common \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/serialization/common/struct_ser/s130 \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/serialization/common/transport \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/serialization/common/transport/debug \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/serialization/common/transport/ser_phy \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/serialization/common/transport/ser_phy/config \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/serialization/connectivity \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/serialization/connectivity/codecs/common \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/serialization/connectivity/codecs/s130/middleware \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/serialization/connectivity/codecs/s130/serializers \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/serialization/connectivity/hal \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/softdevice/common/softdevice_handler \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/softdevice/s130/headers \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/toolchain \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/toolchain/arm \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/toolchain/gcc \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/external/rtx/include \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/external/rtx/source \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/drivers_nrf/pstorage \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/drivers_nrf/pstorage/config \
	-I${HOME}/nrf/sdk/nrf_sdk_9_0/components/drivers_nrf/hal/ \
	-std=${STD} \
	-fabi-version=0 \
	-fno-exceptions \
	-fno-rtti \
	-fno-use-cxa-atexit \
	-fno-threadsafe-statics \
	-MMD \
	-MP \
	-MF"${FILE}.d" \
	-MT"${FILE}.o" \
	-c \
	-o "${FILE}.o" \
	${FILE}

	if [ $? -ne 0 ]; then
		echo "Error while compiling file '${FILE}' - aborting..."
		exit 1
	fi
done

echo "Linking..."

${HOME}/nrf/sdk/gcc_arm_embedded_4_9_${PLATFORM}/bin/arm-none-eabi-g++ \
-mcpu=cortex-m0 \
-mthumb \
-Og \
-fmessage-length=0 \
-fsigned-char \
-ffunction-sections \
-fdata-sections \
-flto \
-fno-move-loop-invariants \
-Wextra \
-g3 \
-T linker/gcc_nrf51_s130_32kb.ld \
-Xlinker \
--gc-sections \
-L${HOME}/nrf/sdk/ehal_latest/ARM/Nordic/nRF51/CMSIS/Debug \
-Wl,-Map,"FruityMesh.map" \
--specs=nano.specs \
-L${HOME}/nrf/sdk/ehal_latest/ARM/src \
-o "FruityMesh.elf" \
`find . -name "*.o" -not -path "./linker/*"`  \
-lCMSIS

if [ $? -ne 0 ]; then
	echo "Error while linking ELF binary - aborting..."
	exit 1
fi

echo "Generating HEX file..."

${HOME}/nrf/sdk/gcc_arm_embedded_4_9_${PLATFORM}/bin/arm-none-eabi-objcopy \
-O ihex \
"FruityMesh.elf" \
"Debug/FruityMesh.hex"

if [ $? -ne 0 ]; then
	echo "Error while generating HEX file - aborting..."
	exit 1
fi

echo "Finished without errors"
