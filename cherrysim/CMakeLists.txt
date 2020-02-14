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

file(GLOB TESTERCPP    ./CherrySimTester.cpp
                       ./test/*.cpp)
file(GLOB RUNNERCPP    ./CherrySimRunner.cpp)

file(GLOB   CHERRYSIM_SRC   CONFIGURE_DEPENDS   "./*.c"
                                                "./CherrySim.cpp"
                                                "./CherrySimUtils.cpp"
                                                "./FruitySimPipe.cpp"
                                                "./Exceptions.cpp"
                                                "./FruitySimServer.cpp"
                                                "./stdfax.cpp"
                                                "./SystemTest.cpp"
                                                "./MersenneTwister.cpp"
                                                "./StackWatcher.cpp"
                                                "../src/Config.cpp"
                                                "../src/Boardconfig.cpp"
                                                "../config/featuresets/*.cpp"
                                                "../config/boards/*.cpp"
                                                )												
SET(visual_studio_source_list ${visual_studio_source_list} ${CHERRYSIM_SRC} ${TESTERCPP} ${RUNNERCPP} CACHE INTERNAL "")
list(APPEND LOCAL_INC             ${CMAKE_CURRENT_LIST_DIR}
                                  ${gtest_include_dir}
                                  # NOTE: Nordic allowed us in their forums to use their headers in our simulator as long as it
                                  # is used to simulate a Nordic Integrated Circuit.
                                  # See: https://devzone.nordicsemi.com/f/nordic-q-a/57615/legal-issue-using-nordic-sdk-code
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk11/components/ble/common"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk11/components/ble/ble_db_discovery"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk11/components/ble/ble_radio_notification"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk11/components/ble/ble_services/ble_dfu"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk11/components/device"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk11/components/libraries/button"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk11/components/libraries/timer"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk11/components/libraries/util"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk11/components/softdevice/common"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk11/components/softdevice/common/softdevice_handler"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk11/components/softdevice/s130/headers"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk11/components/softdevice/s130/headers/nrf51"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk11/components/drivers_nrf/adc"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk11/components/drivers_nrf/spi_master"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk11/components/drivers_nrf/twi_master"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk11/components/drivers_nrf/clock"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk11/components/libraries/log"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk11/components/libraries/log/src"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk11/components/ble/ble_db_discovery"
                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/external/nrf_cc310/include")

target_sources(cherrySim_tester PRIVATE ${CHERRYSIM_SRC} ${TESTERCPP})
target_sources(cherrySim_runner PRIVATE ${CHERRYSIM_SRC} ${RUNNERCPP})

target_include_directories(cherrySim_tester PRIVATE ${LOCAL_INC})
target_include_directories(cherrySim_runner PRIVATE ${LOCAL_INC})

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
