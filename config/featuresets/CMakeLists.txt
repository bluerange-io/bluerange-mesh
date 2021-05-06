add_all_files_to_targets("${SIMULATOR_TARGETS}")
target_include_directories_multi("${NATIVE_TARGETS}" .)
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/fragments")
  # Github does not have any fragments so we should not include it.
  target_include_directories_multi("${ALL_TARGETS}" "./fragments")
endif()

# This will define macro for all the featureset in the folder. Note that macro
# will have the same name as featureset but in uppercase.
file(GLOB FEATURESET_SRC_NAME CONFIGURE_DEPENDS "*.cpp")
foreach(file ${FEATURESET_SRC_NAME})
  get_filename_component(FEATURE_SET_SRC_NAME ${file} NAME_WE)
  string(TOUPPER ${FEATURE_SET_SRC_NAME} FEATURE_SET_SRC_NAME_UPPER)
  target_compile_definitions_multi("${SIMULATOR_TARGETS}" ${FEATURE_SET_SRC_NAME_UPPER})
endforeach()
