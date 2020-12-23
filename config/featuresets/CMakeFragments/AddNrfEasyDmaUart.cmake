file(GLOB   LOCAL_SRC CONFIGURE_DEPENDS  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/drivers_nrf/uart/nrf_drv_uart.c"
                                         "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/libraries/serial/nrf_serial.c"
                                         "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/libraries/queue/nrf_queue.c"
                                         "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/libraries/atomic/nrf_atomic.c"
                                         "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/libraries/sha256/sha256.c")
target_sources(${FEATURE_SET} PRIVATE ${LOCAL_SRC})

target_include_directories(${FEATURE_SET} PRIVATE "${PROJECT_SOURCE_DIR}/sdk/sdk14/modules/nrfx/drivers/src"
                                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/modules/nrfx/drivers/include"
                                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/libraries/serial/"
                                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/libraries/queue/"
                                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/libraries/mutex/"
                                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/libraries/atomic/"
                                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/libraries/sha256/"
                                                  "${PROJECT_SOURCE_DIR}/sdk/sdk14/external/nrf_cc310/include")