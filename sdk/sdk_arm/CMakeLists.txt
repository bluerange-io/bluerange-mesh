target_include_directories_system_multi("${ARM_TARGETS}" "../config_arm_template")
target_link_options_multi("${ARM_TARGETS}" "-T${PROJECT_SOURCE_DIR}/linker/gcc_arm.ld")
target_link_options_multi("${ARM_TARGETS}" "-T${PROJECT_SOURCE_DIR}/linker/nrf5x_common.ld")
target_compile_definitions_multi("${ARM_TARGETS}" "ARM_TEMPLATE")