if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/vendor")
  add_subdirectory(vendor)
endif(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/vendor")

add_all_files_to_targets(cherrySim_tester)