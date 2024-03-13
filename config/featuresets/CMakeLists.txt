# Adds all .c, .cpp or .h files to our LOCAL_SRC list
file(GLOB   LOCAL_SRC CONFIGURE_DEPENDS *.cpp *.c *.h)
if(EMSCRIPTEN)
  list(FILTER LOCAL_SRC EXCLUDE REGEX ".*eink.*")
endif(EMSCRIPTEN)
foreach(target ${SIMULATOR_TARGETS})
  target_sources(${target} PRIVATE ${LOCAL_SRC})
  target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
endforeach(target)
# Adds all files from LOCAL_SRC to our visual_studio list to later build a nice file tree
SET(visual_studio_source_list ${visual_studio_source_list} ${LOCAL_SRC} CACHE INTERNAL "")

target_include_directories_multi("${NATIVE_TARGETS}" .)
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/fragments")
  # Github does not have any fragments so we should not include it.
  target_include_directories_multi("${ALL_TARGETS}" "./fragments")
endif()

# This will define macro for all the featureset in the folder. Note that macro
# will have the same name as featureset but in uppercase.
file(GLOB FEATURESET_SRC_NAME CONFIGURE_DEPENDS "*.cpp")
foreach(file ${FEATURESET_SRC_NAME})
  if(EMSCRIPTEN)
    if(file MATCHES eink)
      continue()
    endif(file MATCHES eink)
  endif(EMSCRIPTEN)
  get_filename_component(FEATURE_SET_SRC_NAME ${file} NAME_WE)
  string(TOUPPER ${FEATURE_SET_SRC_NAME} FEATURE_SET_SRC_NAME_UPPER)
  target_compile_definitions_multi("${SIMULATOR_TARGETS}" ${FEATURE_SET_SRC_NAME_UPPER})
endforeach()
