# We have tested everything with this version, earlier versions might also
# work but are not tested or supported
cmake_minimum_required(VERSION 3.16.3 FATAL_ERROR)

# An in-source build will cause errors during build, so we prohibit it
if("${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
  message(FATAL_ERROR "In-Source Build is prohibited. Please execute cmake from a different directory, e.g. the build directory. NOTE: Delete everything that was just created by CMake. Best to use git reset --hard to reset the repository in a state how it was cloned, otherwhise this error might persist.")
endif()

set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type")

# We organize our builds into folders that are named according to their featureset name
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# This is necessary to link libraries across directories
# This is for example required by curses that we use as a webserver in the simulator
cmake_policy(SET CMP0079 NEW)

# This is our custom implementation to display a nice directory tree in Visual Studio
# that contains all the header and source files nicely structured according to the directories
SET(visual_studio_source_list "${visual_studio_source_list}" CACHE INTERNAL "")

# If ON, this will build firmware for the target, otherwise, it builds the simulator
# TODO: Should be renamed to BUILD_TYPE (IOT-4859)
option(BUILD_FEATURESETS "Compiles the featuresets for native ARM targets if ON, otherwise compiles the x86 simulator (that contains all featuresets in one binary)." OFF)

option(CI_PIPELINE "Meant to be set by the CI Pipeline to do some code manipulation." OFF)

option(FORCE_CPPCHECK "If set and the simulator is built, the generation fails if cppcheck could not be found." OFF)

option(FM_NATIVE_RENDERER_ENABLED "If set and the simulator is built, a native renderer is compiled into the executable." OFF)

# Sanitizers are used to do a number of runtime checks that help us to spot issues.
# However the compile time increases by a lot and the executable will also execute slower.
option(ENABLE_SANITIZERS "If ON and GCC is used and the simulator is built, sanitizer flags are used during compilation." ON) # ON by default is intentional

if(WIN32)
  set(exe_suffix ".exe")
else()
  set(exe_suffix "")
endif()

# We can later use this macro in other CMakeLists files to add all the files of
# a folder to all of our currently selected targets
macro(add_all_files_to_targets TARGETS)
  # Adds all .c, .cpp or .h files to our LOCAL_SRC list
  file(GLOB   LOCAL_SRC CONFIGURE_DEPENDS *.cpp *.c *.h)
  foreach(target ${TARGETS})
    target_sources(${target} PRIVATE ${LOCAL_SRC})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
  endforeach(target)
  # Adds all files from LOCAL_SRC to our visual_studio list to later build a nice file tree
  SET(visual_studio_source_list ${visual_studio_source_list} ${LOCAL_SRC} CACHE INTERNAL "")
endmacro()

# Adds our function definitions to apply CMake functions on multiple targets
include(CMake/MultiTargetCommands.cmake)

if(BUILD_FEATURESETS)
  # Okay, this here is >really< ugly, but I couldn't find a better solution.
  # The underlying problem is that on windows, where Visual Studio is set as
  # the default generator, CMake always tries to call MSBuild.exe, even if the
  # toolchain specifies GCC as the compiler. Changing the value of
  # CMAKE_GENERATOR in a CMakeLists file or the toolchain seemed to have no
  # effect, no matter in which line it is set, CMake still tries to call
  # Visual Studio executables. The only solution that worked was if the correct
  # Generator is specified on the CLI. This of course is dirty. If anybody can
  # find a better solution, please let me know.
  if(NOT CMAKE_GENERATOR STREQUAL "Unix Makefiles" AND NOT CMAKE_GENERATOR STREQUAL "Ninja")
    message(FATAL_ERROR "Wrong Generator selected: ${CMAKE_GENERATOR} but \"Unix Makefiles\" or \"Ninja\" was expected. Remove all files that were just created and call cmake again with -G \"Unix Makefiles\".")
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
  
  # Builds a list of .cmake files from our featureset directory to check what featuresets we have
  file(GLOB FEATURE_SET_MAKE_FILES CONFIGURE_DEPENDS ./config/featuresets/*.cmake)

  find_package (Python3 COMPONENTS Interpreter REQUIRED)

  set(ONLY_FEATURESET "" CACHE STRING "If set, only the given featureset will be generated. Useful e.g. in combination with FruityDeploy.")

  set(FOUND_AT_LEAST_ONE_FEATURESET OFF)

  foreach(f ${FEATURE_SET_MAKE_FILES})
    get_filename_component(FEATURE_SET ${f} NAME_WE)

    # Check if the current featureset matches ONLY_FEATURESET (if used), else skip
    if(NOT ONLY_FEATURESET STREQUAL "")
      if(NOT ONLY_FEATURESET STREQUAL ${FEATURE_SET})
        continue()
      endif()
    endif()

    set(FOUND_AT_LEAST_ONE_FEATURESET ON)

    list(APPEND FEATURE_SETS ${FEATURE_SET})
    add_executable(${FEATURE_SET})
    target_compile_definitions_multi("${FEATURE_SET}" "FEATURESET=${FEATURE_SET}")
    target_compile_definitions_multi("${FEATURE_SET}" "FEATURESET_NAME=\"${FEATURE_SET}.h\"")
    target_sources(${FEATURE_SET} PRIVATE "./config/featuresets/${FEATURE_SET}.cpp")

    # For each feature set, we revert these variables to their default
    unset(PLATFORM)
    unset(ALLOW_MALLOC)
    set(FAIL_ON_SIZE_TOO_BIG 1)
    
    # Includes our <featureset>.cmake
    include(${f})
    
    if(NOT DEFINED PLATFORM)
       message(FATAL_ERROR "Featureset ${FEATURE_SET} failed to set mandatory variable: PLATFORM")
    endif()

    if (${PLATFORM} STREQUAL "NRF52832" OR ${PLATFORM} STREQUAL "NRF52840")
      set(CHIP NRF)
    elseif(${PLATFORM} STREQUAL "ARM")
      set(CHIP ARM)
    endif (${PLATFORM} STREQUAL "NRF52832" OR ${PLATFORM} STREQUAL "NRF52840")
    
    # We block malloc usage as all the allocations should be done statically or on the stack
    if(NOT DEFINED ALLOW_MALLOC)
      target_link_options_multi("${FEATURE_SET}" "LINKER:--wrap=malloc")
      target_link_options_multi("${FEATURE_SET}" "LINKER:--wrap=calloc")
    endif()
    
    target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-x assembler-with-cpp>)

    if (${PLATFORM} STREQUAL "NRF52832")
      target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-D__HEAP_SIZE=4096>)
      target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-D__STACK_SIZE=4096>)
      target_compile_options(${FEATURE_SET} PRIVATE "-mcpu=cortex-m4")
      target_link_options(${FEATURE_SET} PRIVATE "-mcpu=cortex-m4")
      list(APPEND SDK_14_TARGETS ${FEATURE_SET})

    elseif (${PLATFORM} STREQUAL "NRF52840")
      target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-D__HEAP_SIZE=4096>)
      target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-D__STACK_SIZE=4096>)
      target_compile_options(${FEATURE_SET} PRIVATE "-mcpu=cortex-m4")
      target_link_options(${FEATURE_SET} PRIVATE "-mcpu=cortex-m4")
      list(APPEND SDK_15_TARGETS ${FEATURE_SET})

    elseif (${PLATFORM} STREQUAL "ARM")
      target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-D__HEAP_SIZE=4096>)
      target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-D__STACK_SIZE=4096>)
      target_compile_options(${FEATURE_SET} PRIVATE "-mcpu=cortex-m4")
      target_link_options(${FEATURE_SET} PRIVATE "-mcpu=cortex-m4")
      list(APPEND ARM_TARGETS ${FEATURE_SET})

    else(${PLATFORM} STREQUAL "NRF52832")
      message(FATAL_ERROR "Unknown PLATFORM: ${PLATFORM}")
    endif(${PLATFORM} STREQUAL "NRF52832")
    
    # Some options to optimize the size of our binary
    target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
    target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-use-cxa-atexit>)
    target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>)
    
    # The following lines enable a number of compile time warnings and errors
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
    target_compile_options(${FEATURE_SET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-missing-field-initializers>)

    set_target_properties(${FEATURE_SET} PROPERTIES SUFFIX ".out")

    # Generate an output file in intel hex format (.hex) that can be flashed
    add_custom_command(TARGET ${FEATURE_SET} POST_BUILD
      COMMAND ${GCC_PATH}/bin/arm-none-eabi-objcopy${exe_suffix} ARGS -O ihex ${CMAKE_CURRENT_BINARY_DIR}/${FEATURE_SET}.out ${CMAKE_CURRENT_BINARY_DIR}/${FEATURE_SET}.hex
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Creating hex file ${FEATURE_SET}"
      BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/${FEATURE_SET}.hex"
    )
    
    # In order to support firmware updates, we limit the binary to half of the empty space in the flash that is available for the application
    if (${PLATFORM} STREQUAL "NRF52832")
      set(MAX_SIZE 167936) #max app size 4096 * 41
    # TODO: Is that still relevant? we have more space on that chip
    elseif (${PLATFORM} STREQUAL "NRF52840")
      set(MAX_SIZE 167936) #max app size 4096 * 41
    elseif (${PLATFORM} STREQUAL "ARM")
      set(MAX_SIZE 167936) #use the same value as for NRF52832 - main chip
    else(${PLATFORM} STREQUAL "NRF52832")
      message(FATAL_ERROR "Unknown PLATFORM: ${PLATFORM}")
    endif(${PLATFORM} STREQUAL "NRF52832")
    
    # Checks if the created binary exceeds the available space
    add_custom_command(TARGET ${FEATURE_SET} POST_BUILD
      COMMAND ${GCC_PATH}/bin/arm-none-eabi-size${exe_suffix} ARGS ${CMAKE_CURRENT_BINARY_DIR}/${FEATURE_SET}.out
      COMMAND ${Python3_EXECUTABLE} ARGS ${CMAKE_CURRENT_SOURCE_DIR}/CMake/CheckSize.py ${GCC_PATH}/bin/arm-none-eabi-size${exe_suffix} ${CMAKE_CURRENT_BINARY_DIR}/${FEATURE_SET}.out ${MAX_SIZE} ${FAIL_ON_SIZE_TOO_BIG}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Checking Size ${FEATURE_SET}"
      VERBATIM
    )

    # Depending on the platform, we set some compile options
    if (${CHIP} STREQUAL "NRF")
      target_compile_definitions_multi("${FEATURE_SET}" "SOFTDEVICE_PRESENT")
      target_compile_options_multi("${FEATURE_SET}" "-DBLE_STACK_SUPPORT_REQD")
    endif (${CHIP} STREQUAL "NRF")
    
    # Depending on the given platform, we select the correct BLE Stack
    if (${CHIP} STREQUAL "NRF")
      if (${PLATFORM} STREQUAL "NRF52832")
        set(SOFTDEVICE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/softdevice/s132_nrf52_5.1.0_softdevice.hex)
        set(FAMILY NRF52)
      elseif (${PLATFORM} STREQUAL "NRF52840")
        set(SOFTDEVICE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/softdevice/s140_nrf52840_6.1.0_softdevice.hex)
        set(FAMILY NRF52)
      else(${PLATFORM} STREQUAL "NRF52832")
        message(FATAL_ERROR "Unknown PLATFORM version for softdevice: ${PLATFORM}")
      endif(${PLATFORM} STREQUAL "NRF52832")

      # This step creates a merged hex file with BLE stack and application
      add_custom_command(TARGET ${FEATURE_SET} POST_BUILD
        COMMAND mergehex -m ${SOFTDEVICE_PATH} ${CMAKE_CURRENT_BINARY_DIR}/${FEATURE_SET}.hex -o ${CMAKE_CURRENT_BINARY_DIR}/${FEATURE_SET}_merged.hex
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Merging with Softdevice ${FEATURE_SET}"
        VERBATIM
      )

      # This target can be used to flash the merged binary to a device
      add_custom_target(${FEATURE_SET}_flash nrfjprog --program ${CMAKE_CURRENT_BINARY_DIR}/${FEATURE_SET}_merged.hex -f ${FAMILY} --chiperase
                                     COMMAND nrfjprog --reset -f ${FAMILY})
      add_dependencies(${FEATURE_SET}_flash ${FEATURE_SET})
    endif (${CHIP} STREQUAL "NRF")

    list(APPEND ALL_TARGETS ${FEATURE_SET})
    list(APPEND NATIVE_TARGETS ${FEATURE_SET})
    message(STATUS "Added feature set: ${FEATURE_SET}")
  endforeach(f)

  if(NOT FOUND_AT_LEAST_ONE_FEATURESET)
    message(FATAL_ERROR "No Featureset found (check if ONLY_FEATURESET contains invalid featureset)")
  endif()


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
  target_compile_options_multi("${NATIVE_TARGETS}" "-fno-strict-aliasing")
  target_compile_options_multi("${NATIVE_TARGETS}" "-DNDEBUG")
  target_compile_options_multi("${NATIVE_TARGETS}" "-Os")
  target_compile_options_multi("${NATIVE_TARGETS}" "-g")
  
  # Why do we have LINKER: here?
  target_link_options_multi("${NATIVE_TARGETS}" "LINKER:--gc-sections")
  target_link_options_multi("${NATIVE_TARGETS}" "--specs=nano.specs")
  target_link_options_multi("${NATIVE_TARGETS}" "-lgcc")
  target_link_options_multi("${NATIVE_TARGETS}" "-lc")
  target_link_options_multi("${NATIVE_TARGETS}" "-lnosys")
  target_link_options_multi("${NATIVE_TARGETS}" "-mthumb")
  
  add_subdirectory(config)
  add_subdirectory(src)
  add_subdirectory(sdk)

# Next, here are the steps for building our simulator
else(BUILD_FEATURESETS)
  project(CherrySim C CXX)
  if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER_ID} is not supported!")
  endif()
  
  # We can not build our Simulator for 64bit architectures, so we fail if this is selected
  if(NOT CMAKE_SIZEOF_VOID_P STREQUAL 4)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
      # For MSVC a fix is known...
      message(FATAL_ERROR "Only 32-bit is supported by CherrySim! Delete all the CMake generated files and execute CMake again with '-A Win32'")
    else()
      # ...for others the documentation is not so clear about the supported platforms.
      message(FATAL_ERROR "Only 32-bit is supported by CherrySim!")
    endif()
  endif()
  
  add_executable(cherrySim_tester)
  add_executable(cherrySim_runner)
  list(APPEND ALL_TARGETS cherrySim_tester cherrySim_runner)
  list(APPEND SIMULATOR_TARGETS cherrySim_tester cherrySim_runner)
  
  include(CMake/AddSimulatorCompilerFlags.cmake)
  
  target_compile_definitions(cherrySim_runner PRIVATE "SDK=11")
  target_compile_definitions(cherrySim_runner PRIVATE "CHERRYSIM_RUNNER_ENABLED")
  target_compile_definitions(cherrySim_runner PRIVATE "SIM_SERVER_PRESENT")

  target_compile_definitions(cherrySim_tester PRIVATE "SDK=11")
  target_compile_definitions(cherrySim_tester PRIVATE "CHERRYSIM_TESTER_ENABLED")
  target_compile_definitions(cherrySim_tester PRIVATE "SIM_SERVER_PRESENT")

  if(CI_PIPELINE)
    target_compile_definitions(cherrySim_runner PRIVATE "CI_PIPELINE")
    target_compile_definitions(cherrySim_tester PRIVATE "CI_PIPELINE")
  endif()
  
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config/featuresets/CMakeFragments/AddIns.cmake")
  include(config/featuresets/CMakeFragments/AddIns.cmake)
endif()

  find_program(cppcheck_exists NAMES cppcheck)
  if(cppcheck_exists)
    set(cppcheck_command "" CACHE STRING "cppcheck command")
    list(APPEND cppcheck_command
         "cppcheck"
         "--inline-suppr"
         "--enable=all"
         "-q"
         "--template=gcc"
         "--suppress=*:${CMAKE_SOURCE_DIR}/cherrysim/json.hpp"
         "--suppress=missingInclude"
         "--suppress=cstyleCast"
         "--suppress=variableScope"
         "--suppress=shadowVar"
         "--suppress=noConstructor"
         "--suppress=unmatchedSuppression"
         "--suppress=redundantAssignment"
         "--suppress=unusedVariable"
         "--suppress=unusedFunction"
         "--suppress=exceptThrowInDestructor"
         "--suppress=unreadVariable"
         "--suppress=shadowArgument"
         "--suppress=postfixOperator"
         "--suppress=shadowVariable"
         "--suppress=knownConditionTrueFalse"
         "--suppress=virtualCallInConstructor"
         "--suppress=objectIndex"
         "--suppress=zerodiv"
         "--suppress=redundantInitialization"
         "--suppress=useInitializationList"
         "--suppress=useStlAlgorithm"
	)
  if(DEFINED fruityins_SOURCE_DIR)
	  list(APPEND cppcheck_command "--suppress=*:${fruityins_SOURCE_DIR}/*")
  endif()
	if(CI_PIPELINE)
	  list(APPEND cppcheck_command "--error-exitcode=1")
	endif()
    set_target_properties(cherrySim_runner cherrySim_tester PROPERTIES CXX_CPPCHECK "${cppcheck_command}")
	message(STATUS "Found cppcheck!")
  elseif(CI_PIPELINE OR FORCE_CPPCHECK)
    message(FATAL_ERROR "CppCheck could not be found but is required.")
  else()
    message(STATUS "Could not find cppcheck! Install it for additional useful warnings!")
  endif()
  
  add_subdirectory(config)
  add_subdirectory(cherrysim)
  add_subdirectory(src)
  
  set_target_properties(event event_core event_extra gtest gtest_main PROPERTIES FOLDER Dependencies)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT cherrySim_runner)
  
  if(FM_NATIVE_RENDERER_MOCK_ENABLED)
    add_compile_definitions(FM_NATIVE_RENDERER_MOCK_ENABLED)
  endif()

  if(FM_NATIVE_RENDERER_ENABLED)
    add_compile_definitions(FM_NATIVE_RENDERER_ENABLED)
    add_subdirectory(BBERenderer)
  endif()
  
  source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX fruitymesh FILES ${visual_studio_source_list})
endif(BUILD_FEATURESETS)
