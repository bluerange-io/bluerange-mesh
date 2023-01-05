# This CMakeLists file is used to build against nRF SDK 17.1
# The user must set the following in his featureset:
# TARGET_SDK = NRF_SDK_17_1_0
# TARGET_CHIPSET = NRF52832_XXAA / NRF52840_XXAA
# TARGET_STACK = NRF_S132_V7_2_0 / NRF_S140_V7_2_0

foreach(target ${FEATURE_SETS})
  get_property(TARGET_STACK TARGET "${target}" PROPERTY FM_TARGET_STACK)
  get_property(TARGET_CHIPSET TARGET "${target}" PROPERTY FM_TARGET_CHIPSET)
  get_property(TARGET_SDK TARGET "${target}" PROPERTY FM_TARGET_SDK)
  get_property(FAIL_ON_SIZE_TOO_BIG TARGET "${target}" PROPERTY FM_FAIL_ON_SIZE_TOO_BIG)

  # Skip all targets that are not SDK 17.1 targets
  if (NOT "${TARGET_SDK}" STREQUAL "NRF_SDK_17_1_0")
    continue()
  endif()

  # Add the correct options for the selected Bluetooth Stack
  if("${TARGET_STACK}" STREQUAL "NRF_S132_V7_3_0")
    target_compile_definitions_multi("${target}" "S132")

    target_include_directories_system_multi("${target}" "./components/softdevice/s132/headers")
    target_include_directories_system_multi("${target}" "./components/softdevice/s132/headers/nrf52")
  elseif("${TARGET_STACK}" STREQUAL "NRF_S140_V7_3_0")
    target_compile_definitions_multi("${target}" "S140")

    target_include_directories_system_multi("${target}" "./components/softdevice/s140/headers")
    target_include_directories_system_multi("${target}" "./components/softdevice/s140/headers/nrf52")
  else()
    message(FATAL_ERROR "Unsupported stack, but you can add it if you like :-)")
  endif()

  # Add the correct options for the selected Chipset
  if("${TARGET_CHIPSET}" STREQUAL "NRF52832_XXAA")
    target_compile_definitions_multi("${target}" "NRF52832_XXAA")
    target_compile_definitions_multi("${target}" "NRF52832")

    target_sources_multi("${target}" "./modules/nrfx/mdk/gcc_startup_nrf52.S")
    target_sources_multi("${target}" "./modules/nrfx/mdk/system_nrf52.c")
  elseif("${TARGET_CHIPSET}" STREQUAL "NRF52833_XXAA")
    target_compile_definitions_multi("${target}" "NRF52833_XXAA")
    target_compile_definitions_multi("${target}" "NRF52833")

    target_sources_multi("${target}" "./modules/nrfx/mdk/gcc_startup_nrf52833.S")
    target_sources_multi("${target}" "./modules/nrfx/mdk/system_nrf52833.c")
  elseif("${TARGET_CHIPSET}" STREQUAL "NRF52840_XXAA")
    target_compile_definitions_multi("${target}" "NRF52840_XXAA")
    target_compile_definitions_multi("${target}" "NRF52840")

    target_sources_multi("${target}" "./modules/nrfx/mdk/gcc_startup_nrf52840.S")
    target_sources_multi("${target}" "./modules/nrfx/mdk/system_nrf52840.c")
  else()
    message(FATAL_ERROR "Unsupported chipset, but you can add it if you like :-)")
  endif()

  # Select the correct linker script which is based on Stack and Chipset
  if("${TARGET_CHIPSET}" STREQUAL "NRF52832_XXAA" AND ${TARGET_STACK} STREQUAL "NRF_S132_V7_3_0")
    target_link_options_multi("${target}" "-T${PROJECT_SOURCE_DIR}/linker/gcc_nrf52832_xxaa_s132_v7.ld")
    set_property(TARGET "${target}" PROPERTY FM_MAX_APP_SIZE 163840) # 80 pages in total for App + Free Space, 40 * 4096
  elseif("${TARGET_CHIPSET}" STREQUAL "NRF52833_XXAA" AND ${TARGET_STACK} STREQUAL "NRF_S140_V7_3_0")
    target_link_options_multi("${target}" "-T${PROJECT_SOURCE_DIR}/linker/gcc_nrf52833_xxaa_s140_v7.ld")
    set_property(TARGET "${target}" PROPERTY FM_MAX_APP_SIZE 159744) # 80 pages in total for App + Free Space, 39 * 4096
  elseif("${TARGET_CHIPSET}" STREQUAL "NRF52840_XXAA" AND ${TARGET_STACK} STREQUAL "NRF_S140_V7_3_0")
    target_link_options_multi("${target}" "-T${PROJECT_SOURCE_DIR}/linker/gcc_nrf52840_xxaa_s140_v7.ld")
    set_property(TARGET "${target}" PROPERTY FM_MAX_APP_SIZE 245000) # Calculated with an update chunk size of 128
  else()
    message(FATAL_ERROR "Unsupported combination, but you can add it if you like :-)")
  endif()

  # Add (mostly) generic compile definitions and options
  target_compile_definitions_multi("${target}" "SDK=17")
  target_compile_definitions_multi("${target}" "NRF52_SERIES")
  target_compile_options_multi("${target}" "-mfloat-abi=hard")
  target_compile_options_multi("${target}" "-mfpu=fpv4-sp-d16")
  target_compile_options_multi("${target}" "-mcpu=cortex-m4")

  # We rely on the SoftDevice Stack being present
  target_compile_definitions_multi("${target}" "SOFTDEVICE_PRESENT")
  target_compile_definitions_multi("${target}" "BLE_STACK_SUPPORT_REQD")

  # Add (mostly) generic linking options
  target_link_options_multi("${target}" "-mcpu=cortex-m4")
  target_link_options_multi("${target}" "-mfloat-abi=hard")
  target_link_options_multi("${target}" "-mfpu=fpv4-sp-d16")
  target_link_options_multi("${target}" "-T${PROJECT_SOURCE_DIR}/linker/nrf5x_common.ld")

  # Add the source code files from the SDK
  target_sources_multi("${target}" "./modules/nrfx/drivers/src/nrfx_saadc.c")
  target_sources_multi("${target}" "./modules/nrfx/drivers/src/nrfx_twi.c")
  target_sources_multi("${target}" "./modules/nrfx/drivers/src/nrfx_spi.c")
  target_sources_multi("${target}" "./modules/nrfx/drivers/src/prs/nrfx_prs.c")
  target_sources_multi("${target}" "./integration/nrfx/legacy/nrf_drv_spi.c")
  target_sources_multi("${target}" "./integration/nrfx/legacy/nrf_drv_twi.c")
  target_sources_multi("${target}" "./modules/nrfx/drivers/src/nrfx_gpiote.c")
  target_sources_multi("${target}" "./modules/nrfx/drivers/src/nrfx_uarte.c")
  target_sources_multi("${target}" "./components/libraries/util/app_util_platform.c")
  target_sources_multi("${target}" "./components/libraries/hardfault/nrf52/handler/hardfault_handler_gcc.c")
  target_sources_multi("${target}" "./components/libraries/experimental_section_vars/nrf_section_iter.c")
  target_sources_multi("${target}" "./components/libraries/scheduler/app_scheduler.c")
  target_sources_multi("${target}" "./components/libraries/timer/app_timer.c")
  target_sources_multi("${target}" "./components/libraries/serial/nrf_serial.c")
  target_sources_multi("${target}" "./components/libraries/balloc/nrf_balloc.c")
  target_sources_multi("${target}" "./components/libraries/memobj/nrf_memobj.c")
  target_sources_multi("${target}" "./components/libraries/queue/nrf_queue.c")
  target_sources_multi("${target}" "./components/libraries/atomic/nrf_atomic.c")
  target_sources_multi("${target}" "./components/libraries/atomic/nrf_atomic.c")
  target_sources_multi("${target}" "./components/ble/ble_db_discovery/ble_db_discovery.c")
  target_sources_multi("${target}" "./components/ble/nrf_ble_gq/nrf_ble_gq.c")
  target_sources_multi("${target}" "./components/ble/ble_radio_notification/ble_radio_notification.c")
  target_sources_multi("${target}" "./components/softdevice/common/nrf_sdh.c")
  target_sources_multi("${target}" "./components/softdevice/common/nrf_sdh_ble.c")
  target_sources_multi("${target}" "./components/softdevice/common/nrf_sdh_soc.c")

  # Add the include directories from FruityMesh
  target_include_directories_system_multi("${target}" "../config_nrf52")

  # Add include directories from the SDK
  target_include_directories_system_multi("${target}" "./components/ble/common")
  target_include_directories_system_multi("${target}" "./components/ble/ble_db_discovery")
  target_include_directories_system_multi("${target}" "./components/ble/ble_radio_notification")
  target_include_directories_system_multi("${target}" "./components/ble/ble_services/ble_dfu")
  target_include_directories_system_multi("${target}" "./components/ble/nrf_ble_gq")
  target_include_directories_system_multi("${target}" "./components/device")
  target_include_directories_system_multi("${target}" "./components/libraries/button")
  target_include_directories_system_multi("${target}" "./components/libraries/crypto")
  target_include_directories_system_multi("${target}" "./components/libraries/crypto/backend/nrf_hw")
  target_include_directories_system_multi("${target}" "./components/libraries/crypto/backend/cc310")
  target_include_directories_system_multi("${target}" "./components/libraries/crypto/backend/optiga")
  target_include_directories_system_multi("${target}" "./components/libraries/timer")
  target_include_directories_system_multi("${target}" "./components/libraries/util")
  target_include_directories_system_multi("${target}" "./components/softdevice/common")
  target_include_directories_system_multi("${target}" "./components/softdevice/common/softdevice_handler")
  target_include_directories_system_multi("${target}" "./components/toolchain")
  target_include_directories_system_multi("${target}" "./components/toolchain/gcc")
  target_include_directories_system_multi("${target}" "./components/toolchain/cmsis/include")
  target_include_directories_system_multi("${target}" "./components/libraries/log")
  target_include_directories_system_multi("${target}" "./components/libraries/log/src")
  target_include_directories_system_multi("${target}" "./components/libraries/experimental_section_vars")
  target_include_directories_system_multi("${target}" "./components/libraries/strerror")
  target_include_directories_system_multi("${target}" "./components/libraries/scheduler")
  target_include_directories_system_multi("${target}" "./components/libraries/memobj")
  target_include_directories_system_multi("${target}" "./components/libraries/balloc")
  target_include_directories_system_multi("${target}" "./components/libraries/delay")
  target_include_directories_system_multi("${target}" "./components/libraries/serial")
  target_include_directories_system_multi("${target}" "./components/libraries/queue")
  target_include_directories_system_multi("${target}" "./components/libraries/mutex")
  target_include_directories_system_multi("${target}" "./components/libraries/atomic")
  target_include_directories_system_multi("${target}" "./integration/nrfx")
  target_include_directories_system_multi("${target}" "./integration/nrfx/legacy")
  target_include_directories_system_multi("${target}" "./modules/nrfx/drivers/include")
  target_include_directories_system_multi("${target}" "./modules/nrfx/hal")
  target_include_directories_system_multi("${target}" "./modules/nrfx")
  target_include_directories_system_multi("${target}" "./modules/nrfx/mdk")
  target_include_directories_system_multi("${target}" "./external/nrf_cc310/include")
endforeach()
