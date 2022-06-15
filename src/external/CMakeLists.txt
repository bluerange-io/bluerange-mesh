
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/micro-ecc")
  add_subdirectory(micro-ecc)
endif()
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/sha256")
  add_subdirectory(sha256)
endif()