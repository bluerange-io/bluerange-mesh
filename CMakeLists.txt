cmake_minimum_required(VERSION 3.16.3 FATAL_ERROR)

set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

cmake_policy(SET CMP0079 NEW) # For link_library accross directories (required for cursed)
SET(visual_studio_source_list "${visual_studio_source_list}" CACHE INTERNAL "")

option(BUILD_FEATURESETS "Compile featuresets instead of simulator code." OFF)

if(WIN32)
  set(exe_suffix ".exe")
else()
  set(exe_suffix "")
endif()

macro(add_all_files_to_targets TARGETS)
  file(GLOB   LOCAL_SRC CONFIGURE_DEPENDS *.cpp)
  foreach(target ${TARGETS})
    target_sources(${target} PRIVATE ${LOCAL_SRC})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
  endforeach(target)
  SET(visual_studio_source_list ${visual_studio_source_list} ${LOCAL_SRC} CACHE INTERNAL "")
  
  file(GLOB   LOCAL_SRC CONFIGURE_DEPENDS *.c)
  foreach(target ${TARGETS})
    target_sources(${target} PRIVATE ${LOCAL_SRC})
  endforeach(target)
  SET(visual_studio_source_list ${visual_studio_source_list} ${LOCAL_SRC} CACHE INTERNAL "")
endmacro()

include(CMake/MultiTargetCommands.cmake)

if(BUILD_FEATURESETS)
  # Okay, this here is >really< ugly, but I couldn't find a better solution.
  # The underlying problem is that on windows, where Visual Studio is set as
  # the default generator, CMake always trys to call MSBuild.exe, even if the
  # toolchain specifies GCC as the compiler. Changeing the value of
  # CMAKE_GENERATOR in a CMakeLists file or the toolchain seemed to have no
  # effect, no matter in which line it is set, CMake still trys to call
  # Visual Studio executables. The only solution that worked was if the correct
  # Generator is specified on the CLI. This of course is dirty. If anybody can
  # find a better solution, please let me know.
  if(NOT CMAKE_GENERATOR STREQUAL "Unix Makefiles")
    message(FATAL_ERROR "Wrong Generator selected: ${CMAKE_GENERATOR} but \"Unix Makefiles\" was expected. Remove all files that were just created and call cmake again with -G \"Unix Makefiles\".")
  endif()
  
  set(GCC_PATH "" CACHE STRING "Path to GCC 4.9")
  if(GCC_PATH STREQUAL "")
    message(FATAL_ERROR "GCC_PATH was unspecified! Set it e.g. via -DGCC_PATH=C:/path/to/gcc-arm-embedded-4.9")
  endif()
  include(CMake/arm_none_eabi_toolchain.cmake)
  project(FruityMesh C CXX ASM)
  if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "Only GCC is supported when compiling feature sets!")
  endif()
  
  file(GLOB FEATURE_SET_MAKE_FILES CONFIGURE_DEPENDS ./config/featuresets/*.cmake)
  find_package (Python3 COMPONENTS Interpreter REQUIRED)
  set(ONLY_FEATURESET "" CACHE STRING "If set, only the given featureset will be generated. Useful e.g. in combination with FruityDeploy.")
  foreach(f ${FEATURE_SET_MAKE_FILES})
    get_filename_component(FEATURE_SET ${f} NAME_WE)
    if(NOT ONLY_FEATURESET STREQUAL "")
      if(NOT ONLY_FEATURESET STREQUAL ${FEATURE_SET})
        continue()
      endif()
    endif()
    list(APPEND FEATURE_SETS ${FEATURE_SET})
    add_executable(${FEATURE_SET})
    target_compile_definitions_multi("${FEATURE_SET}" "FEATURESET=${FEATURE_SET}")
    target_compile_definitions_multi("${FEATURE_SET}" "FEATURESET_NAME=\"${FEATURE_SET}.h\"")
    target_sources(${FEATURE_SET} PRIVATE "./config/featuresets/${FEATURE_SET}.cpp")
    unset(FRUITY_SDK)
    unset(ALLOW_MALLOC)
    set(FAIL_ON_SIZE_TOO_BIG 1)
    
    include(${f})
    
    if(NOT DEFINED FRUITY_SDK)
       message(FATAL_ERROR "Featureset ${FEATURE_SET} failed to set mandatory variable: FRUITY_SDK")
    endif()
    
    if(NOT DEFINED ALLOW_MALLOC)
      target_link_options_multi("${FEATURE_SET}" "LINKER:--wrap=malloc")
      target_link_options_multi("${FEATURE_SET}" "LINKER:--wrap=calloc")
    endif()
    
    target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-x assembler-with-cpp>)
    if (${FRUITY_SDK} STREQUAL "11")
      target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-D__HEAP_SIZE=2048>)
      target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-D__STACK_SIZE=2048>)
      target_compile_options(${FEATURE_SET} PRIVATE "-mcpu=cortex-m0")
      target_link_options(${FEATURE_SET} PRIVATE "-mcpu=cortex-m0")
      list(APPEND SDK_11_TARGETS ${FEATURE_SET})
    elseif (${FRUITY_SDK} STREQUAL "14")
      target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-D__HEAP_SIZE=4096>)
      target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-D__STACK_SIZE=4096>)
      target_compile_options(${FEATURE_SET} PRIVATE "-mcpu=cortex-m4")
      target_link_options(${FEATURE_SET} PRIVATE "-mcpu=cortex-m4")
      list(APPEND SDK_14_TARGETS ${FEATURE_SET})
    elseif (${FRUITY_SDK} STREQUAL "15")
      target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-D__HEAP_SIZE=4096>)
      target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-D__STACK_SIZE=4096>)
      target_compile_options(${FEATURE_SET} PRIVATE "-mcpu=cortex-m4")
      target_link_options(${FEATURE_SET} PRIVATE "-mcpu=cortex-m4")
      list(APPEND SDK_15_TARGETS ${FEATURE_SET})
    else(${FRUITY_SDK} STREQUAL "11")
      message(FATAL_ERROR "Unknown FRUITY_SDK version: ${FRUITY_SDK}")
    endif(${FRUITY_SDK} STREQUAL "11")
    
    target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
    target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-use-cxa-atexit>)
    target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>)
    
  
    target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall>)
    target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wextra>)
    target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Werror>)
    target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wcast-qual>)
    target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wlogical-op>)
    target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-function>)
    target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-but-set-variable>)
    target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-variable>)
    target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-vla>)
    target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-parameter>)
	
    set_target_properties(${FEATURE_SET} PROPERTIES SUFFIX ".out")
	
    add_custom_command(TARGET ${FEATURE_SET} POST_BUILD
      COMMAND ${GCC_PATH}/bin/arm-none-eabi-objcopy${exe_suffix} ARGS -O ihex ${CMAKE_CURRENT_BINARY_DIR}/${FEATURE_SET}.out ${CMAKE_CURRENT_BINARY_DIR}/${FEATURE_SET}.hex
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Creating hex file ${FEATURE_SET}"
      BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/${FEATURE_SET}.hex"
    )
    
    if (${FRUITY_SDK} STREQUAL "11")
      set(MAX_SIZE 71680) # max app size 1024 * 70
    elseif (${FRUITY_SDK} STREQUAL "14")
      set(MAX_SIZE 167936) #max app size 4096 * 41
    elseif (${FRUITY_SDK} STREQUAL "15")
      set(MAX_SIZE 167936) #max app size 4096 * 41
    else(${FRUITY_SDK} STREQUAL "11")
      message(FATAL_ERROR "Unknown FRUITY_SDK version: ${FRUITY_SDK}")
    endif(${FRUITY_SDK} STREQUAL "11")
    
    add_custom_command(TARGET ${FEATURE_SET} POST_BUILD
      COMMAND ${GCC_PATH}/bin/arm-none-eabi-size${exe_suffix} ARGS ${CMAKE_CURRENT_BINARY_DIR}/${FEATURE_SET}.out
      COMMAND ${Python3_EXECUTABLE} ARGS ${CMAKE_CURRENT_SOURCE_DIR}/CMake/CheckSize.py ${GCC_PATH}/bin/arm-none-eabi-size${exe_suffix} ${CMAKE_CURRENT_BINARY_DIR}/${FEATURE_SET}.out ${MAX_SIZE} ${FAIL_ON_SIZE_TOO_BIG}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Checking Size ${FEATURE_SET}"
      VERBATIM
    )
    
	if (${FRUITY_SDK} STREQUAL "11")
      set(SOFTDEVICE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/softdevice/s130_nrf51_2.0.1_softdevice.hex)
      set(FAMILY NRF51)
    elseif (${FRUITY_SDK} STREQUAL "14")
      set(SOFTDEVICE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/softdevice/s132_nrf52_5.1.0_softdevice.hex)
      set(FAMILY NRF52)
    elseif (${FRUITY_SDK} STREQUAL "15")
      set(SOFTDEVICE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/softdevice/s140_nrf52840_6.1.0_softdevice.hex)
      set(FAMILY NRF52)
    else(${FRUITY_SDK} STREQUAL "11")
      message(FATAL_ERROR "Unknown FRUITY_SDK version: ${FRUITY_SDK}")
    endif(${FRUITY_SDK} STREQUAL "11")
    add_custom_command(TARGET ${FEATURE_SET} POST_BUILD
      COMMAND mergehex -m ${SOFTDEVICE_PATH} ${CMAKE_CURRENT_BINARY_DIR}/${FEATURE_SET}.hex -o ${CMAKE_CURRENT_BINARY_DIR}/${FEATURE_SET}_merged.hex
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Merging with Softdevice ${FEATURE_SET}"
      VERBATIM
    )
    add_custom_target(${FEATURE_SET}_flash nrfjprog --program ${CMAKE_CURRENT_BINARY_DIR}/${FEATURE_SET}_merged.hex -f ${FAMILY} --chiperase
                                   COMMAND nrfjprog --reset -f ${FAMILY})
    add_dependencies(${FEATURE_SET}_flash ${FEATURE_SET})
	
    list(APPEND ALL_TARGETS ${FEATURE_SET})
    list(APPEND NATIVE_TARGETS ${FEATURE_SET})
    message(STATUS "Added feature set: ${FEATURE_SET}")
  endforeach(f)
  target_compile_definitions_multi("${NATIVE_TARGETS}" "SOFTDEVICE_PRESENT")
  set_property_multi("${NATIVE_TARGETS}" "CXX_STANDARD" "11")
  set_property_multi("${NATIVE_TARGETS}" "C_STANDARD" "99")
  set_property_multi("${NATIVE_TARGETS}" "CMAKE_CXX_STANDARD_REQUIRED" "ON")
  
  target_compile_options_multi("${NATIVE_TARGETS}" "-mthumb")
  target_compile_options_multi("${NATIVE_TARGETS}" "-fmessage-length=0")
  target_compile_options_multi("${NATIVE_TARGETS}" "-fsigned-char")
  target_compile_options_multi("${NATIVE_TARGETS}" "-ffunction-sections")
  target_compile_options_multi("${NATIVE_TARGETS}" "-fdata-sections")
  target_compile_options_multi("${NATIVE_TARGETS}" "-flto")
  target_compile_options_multi("${NATIVE_TARGETS}" "-fno-move-loop-invariants")
  target_compile_options_multi("${NATIVE_TARGETS}" "-fno-math-errno")
  target_compile_options_multi("${NATIVE_TARGETS}" "-fno-unroll-loops")
  target_compile_options_multi("${NATIVE_TARGETS}" "-fabi-version=0")
  target_compile_options_multi("${NATIVE_TARGETS}" "-fno-exceptions")
  target_compile_options_multi("${NATIVE_TARGETS}" "-D__need___va_list")
  target_compile_options_multi("${NATIVE_TARGETS}" "-DBLE_STACK_SUPPORT_REQD")
  target_compile_options_multi("${NATIVE_TARGETS}" "-fno-strict-aliasing")
  target_compile_options_multi("${NATIVE_TARGETS}" "-DNDEBUG")
  target_compile_options_multi("${NATIVE_TARGETS}" "-Os")
  target_compile_options_multi("${NATIVE_TARGETS}" "-g")
  
  target_link_options_multi("${NATIVE_TARGETS}" "LINKER:--gc-sections")
  target_link_options_multi("${NATIVE_TARGETS}" "--specs=nano.specs")
  target_link_options_multi("${NATIVE_TARGETS}" "-lgcc")
  target_link_options_multi("${NATIVE_TARGETS}" "-lc")
  target_link_options_multi("${NATIVE_TARGETS}" "-lnosys")
  target_link_options_multi("${NATIVE_TARGETS}" "-mthumb")
  
  add_subdirectory(src)
  add_subdirectory(sdk)
  target_link_options_multi("${NATIVE_TARGETS}" "-T${PROJECT_SOURCE_DIR}/linker/nrf5x_common.ld")
else(BUILD_FEATURESETS)
  project(CherrySim C CXX)
  if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER_ID} is not supported!")
  endif()
  add_executable(cherrySim_tester)
  add_executable(cherrySim_runner)
  list(APPEND ALL_TARGETS cherrySim_tester cherrySim_runner)
  list(APPEND SIMULATOR_TARGETS cherrySim_tester cherrySim_runner)
  
  include(CMake/AddCompilerFlags.cmake)
  
  target_compile_definitions(cherrySim_runner PRIVATE "SDK=11")
  target_compile_definitions(cherrySim_runner PRIVATE "CHERRYSIM_RUNNER_ENABLED")
  target_compile_definitions(cherrySim_runner PRIVATE "SIM_SERVER_PRESENT")
  target_compile_definitions(cherrySim_tester PRIVATE "SDK=11")
  target_compile_definitions(cherrySim_tester PRIVATE "CHERRYSIM_TESTER_ENABLED")
  target_compile_definitions(cherrySim_tester PRIVATE "SIM_SERVER_PRESENT")
  add_subdirectory(cherrysim)
  add_subdirectory(src)
  
  set_target_properties(event event_core event_extra gtest gtest_main PROPERTIES FOLDER Dependencies)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT cherrySim_runner)
  
  source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${visual_studio_source_list})
endif(BUILD_FEATURESETS)
