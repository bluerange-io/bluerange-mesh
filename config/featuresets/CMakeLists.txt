add_all_files_to_targets("${SIMULATOR_TARGETS}")
target_include_directories_multi("${NATIVE_TARGETS}" .)
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/fragments")
  # Github does not have any fragments so we should not include it.
  target_include_directories_multi("${ALL_TARGETS}" "./fragments")
endif()
