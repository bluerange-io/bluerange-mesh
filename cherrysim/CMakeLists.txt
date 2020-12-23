include(${CMAKE_SOURCE_DIR}/CMake/FindLibEvent.cmake)
enable_testing()
include(${CMAKE_SOURCE_DIR}/CMake/google_test.cmake)
include(GoogleTest)
add_subdirectory(test)
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/vendor")
  add_subdirectory(vendor)
else()
  target_compile_definitions(cherrySim_runner PRIVATE "GITHUB_RELEASE")
  target_compile_definitions(cherrySim_tester PRIVATE "GITHUB_RELEASE")
endif(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/vendor")
add_subdirectory(aes-ccm)

file(GLOB TESTERCPP    CONFIGURE_DEPENDS   ./CherrySimTester.cpp
                                           ./test/*.cpp)
file(GLOB RUNNERCPP    ./CherrySimRunner.cpp)

file(GLOB   CHERRYSIM_SRC   CONFIGURE_DEPENDS   "./*.c"
                                                "./*.h"
                                                "./CherrySim.cpp"
                                                "./CherrySimTypes.cpp"
                                                "./CherrySimUtils.cpp"
                                                "./FruitySimPipe.cpp"
                                                "./Exceptions.cpp"
                                                "./MoveAnimation.cpp"
                                                "./FruitySimServer.cpp"
                                                "./stdfax.cpp"
                                                "./SystemTest.cpp"
                                                "./MersenneTwister.cpp"
                                                "./StackWatcher.cpp"
                                                )												
SET(visual_studio_source_list ${visual_studio_source_list} ${CHERRYSIM_SRC} ${TESTERCPP} ${RUNNERCPP} CACHE INTERNAL "")

list(APPEND LOCAL_INC             ${gtest_include_dir}
                                  # NOTE: Nordic allowed us in their forums to use their headers in our simulator as long as it
                                  # is used to simulate a Nordic Integrated Circuit.
                                  # See: https://devzone.nordicsemi.com/f/nordic-q-a/57615/legal-issue-using-nordic-sdk-code
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/ble/common"
				  "${PROJECT_SOURCE_DIR}/sdk/config_nrf52"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/ble/ble_db_discovery"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/ble/ble_radio_notification"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/ble/ble_services/ble_dfu"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/device"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/libraries/button"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/drivers_nrf/spi_master"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/drivers_nrf/twi_master"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/external/nrf_cc310/include"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/drivers_nrf/common"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/softdevice/common/softdevice_handler"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/libraries/util"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/libraries/timer"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/softdevice/s132/headers/nrf52"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/softdevice/s132/headers"
								  )

# CHERRYSIM_SRC contains all the header files, including CherrySimRunner.h and CherrySimTester.h.
# These files must be removed from the target that they don't belong to.
set(TESTER_SRC ${CHERRYSIM_SRC})
set(RUNNER_SRC ${CHERRYSIM_SRC})
list(FILTER TESTER_SRC EXCLUDE REGEX ".*CherrySimRunner.h$")
list(FILTER RUNNER_SRC EXCLUDE REGEX ".*CherrySimTester.h$")
list(APPEND TESTER_SRC ${TESTERCPP})
list(APPEND RUNNER_SRC ${RUNNERCPP})
target_sources(cherrySim_tester PRIVATE ${TESTER_SRC})
target_sources(cherrySim_runner PRIVATE ${RUNNER_SRC})

target_include_directories(cherrySim_tester SYSTEM PRIVATE ${LOCAL_INC})
target_include_directories(cherrySim_runner SYSTEM PRIVATE ${LOCAL_INC})

target_include_directories(cherrySim_tester PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_include_directories(cherrySim_runner PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_compile_definitions(cherrySim_tester PRIVATE "CHERRYSIM_TESTER_ENABLED")

if (UNIX)
  find_package(Curses REQUIRED)
  include_directories(${CURSES_INCLUDE_DIR})
  target_link_libraries(cherrySim_tester PRIVATE ${CURSES_LIBRARIES})
  target_link_libraries(cherrySim_runner PRIVATE ${CURSES_LIBRARIES})
else(UNIX)
  target_link_libraries(cherrySim_tester PRIVATE wsock32 ws2_32)
  target_link_libraries(cherrySim_runner PRIVATE wsock32 ws2_32)
endif(UNIX)

target_compile_definitions(cherrySim_tester PRIVATE "SIM_ENABLED")
target_compile_definitions(cherrySim_runner PRIVATE "SIM_ENABLED")
