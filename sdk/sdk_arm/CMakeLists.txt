set_property_multi("${ARM_TARGETS}" "FM_MAX_APP_SIZE" 254000) # Calculated with a chunk size of 128 for updates

target_include_directories_system_multi("${ARM_TARGETS}" "../config_arm_template")
target_link_options_multi("${ARM_TARGETS}" "-T${PROJECT_SOURCE_DIR}/linker/gcc_arm.ld")
target_link_options_multi("${ARM_TARGETS}" "-T${PROJECT_SOURCE_DIR}/linker/nrf5x_common.ld")
target_compile_definitions_multi("${ARM_TARGETS}" "ARM_TEMPLATE")

target_compile_options_multi("${ARM_TARGETS}"  "-mcpu=cortex-m4")
target_link_options_multi("${ARM_TARGETS}" "-mcpu=cortex-m4")