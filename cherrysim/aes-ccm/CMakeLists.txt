file(GLOB   LOCAL_SRC    CONFIGURE_DEPENDS   "${CMAKE_CURRENT_LIST_DIR}/aes-ccm-sw.cpp"
                                             "${CMAKE_CURRENT_LIST_DIR}/aes-ccm.cpp"
                                             "${CMAKE_CURRENT_LIST_DIR}/aes-enc.cpp"
                                             "${CMAKE_CURRENT_LIST_DIR}/aes-crackle.cpp")
foreach(target ${SIMULATOR_TARGETS})
  target_sources(${target} PRIVATE ${LOCAL_SRC})
endforeach(target)
SET(visual_studio_source_list ${visual_studio_source_list} ${LOCAL_SRC} CACHE INTERNAL "")