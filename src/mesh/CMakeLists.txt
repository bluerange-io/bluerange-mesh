if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/sig")
  add_subdirectory(sig)
endif()

add_all_files_to_targets("${ALL_TARGETS}")