add_subdirectory(../config ${CMAKE_CURRENT_BINARY_DIR}/config)

add_subdirectory(base)
add_subdirectory(c)
add_subdirectory(examples)
add_subdirectory(mesh)
add_subdirectory(modules)
add_subdirectory(types)
add_subdirectory(utility)
add_subdirectory(hal)
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/vendor")
  add_subdirectory(vendor)
endif(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/vendor")

file(GLOB   LOCAL_SRC CONFIGURE_DEPENDS "./Boardconfig.cpp"
                                        "./Config.cpp")

foreach(target ${ALL_TARGETS})
  target_sources(${target} PRIVATE ${LOCAL_SRC})
  target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
  message(STATUS "target = ${target}")
endforeach(target)
SET(visual_studio_source_list ${visual_studio_source_list} ${LOCAL_SRC} CACHE INTERNAL "")

set(NRF_FAMILY      ${TARGET_BOARD})


file(GLOB   MAIN_SRC CONFIGURE_DEPENDS "./Main.cpp")
target_sources_multi("${NATIVE_TARGETS}" "${MAIN_SRC}")
