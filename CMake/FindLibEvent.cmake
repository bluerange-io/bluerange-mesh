set(EVENT__DISABLE_BENCHMARK ON CACHE BOOL "Disable SSL" FORCE)
set(EVENT__DISABLE_REGRESS   ON CACHE BOOL "Disable SSL" FORCE)
set(EVENT__DISABLE_OPENSSL   ON CACHE BOOL "Disable SSL" FORCE)
set(EVENT__DISABLE_SAMPLES   ON CACHE BOOL "Disable SSL" FORCE)
set(EVENT__DISABLE_TESTS     ON CACHE BOOL "Disable SSL" FORCE)
include(FetchContent)
FetchContent_Declare(
  libevent
  GIT_REPOSITORY https://github.com/libevent/libevent.git
  GIT_TAG        release-2.1.8-stable
)
FetchContent_GetProperties(libevent)
if(NOT libevent_POPULATED)
  FetchContent_Populate(libevent)
  add_subdirectory(${libevent_SOURCE_DIR} ${libevent_BINARY_DIR})
endif()
target_include_directories(cherrySim_tester PRIVATE ${libevent_SOURCE_DIR}/include)
target_include_directories(cherrySim_runner PRIVATE ${libevent_SOURCE_DIR}/include)
target_include_directories(cherrySim_tester PRIVATE ${libevent_BINARY_DIR}/include)
target_include_directories(cherrySim_runner PRIVATE ${libevent_BINARY_DIR}/include)

target_link_libraries(cherrySim_tester PRIVATE event_core event_extra)
target_link_libraries(cherrySim_runner PRIVATE event_core event_extra)
