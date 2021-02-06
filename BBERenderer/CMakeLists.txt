set(BBE_ADD_TEST_PROJECTS    OFF CACHE BOOL "" FORCE)
set(BBE_ADD_EXAMPLE_PROJECTS OFF CACHE BOOL "" FORCE)
set(BBE_ADD_AUDIO            OFF CACHE BOOL "" FORCE) # The current Audio System from BBE does not really like the Build Directory structure of CherrySim.
include(FetchContent)
FetchContent_Declare(
  bbe
  GIT_REPOSITORY https://github.com/Brotcrunsher/BrotboxEngine
  GIT_TAG        master
)
FetchContent_GetProperties(bbe)
if(NOT bbe_POPULATED)
  FetchContent_Populate(bbe)
endif()

add_subdirectory(${bbe_SOURCE_DIR} ${bbe_BINARY_DIR})

add_compile_definitions(BBE_APPLICATION_ASSET_PATH="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(cherrySim_tester PRIVATE BrotBoxEngine)
target_link_libraries(cherrySim_runner PRIVATE BrotBoxEngine)
file(GLOB local_src CONFIGURE_DEPENDS "*.cpp")
target_sources(cherrySim_tester PUBLIC "${local_src}")
target_sources(cherrySim_runner PUBLIC "${local_src}")
install_compiled_shaders(cherrySim_tester)
install_compiled_shaders(cherrySim_runner)
target_include_directories(cherrySim_tester PUBLIC .)
target_include_directories(cherrySim_runner PUBLIC .)

