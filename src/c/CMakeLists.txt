if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/drivers")
  add_subdirectory(drivers)
endif()
add_subdirectory(segger_rtt)