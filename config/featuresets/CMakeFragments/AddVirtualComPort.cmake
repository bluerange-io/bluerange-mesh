if (${PLATFORM} STREQUAL "NRF52840")
  file(GLOB   LOCAL_SRC CONFIGURE_DEPENDS  "${PROJECT_SOURCE_DIR}/src/c/drivers/nrf/virtual_com_port.c"
                                           "${PROJECT_SOURCE_DIR}/sdk/sdk15/components/drivers_nrf/usbd/nrf_drv_usbd.c"
                                           "${PROJECT_SOURCE_DIR}/sdk/sdk15/components/libraries/usbd/app_usbd.c"
                                           "${PROJECT_SOURCE_DIR}/sdk/sdk15/components/libraries/atomic/nrf_atomic.c"
                                           "${PROJECT_SOURCE_DIR}/sdk/sdk15/components/libraries/usbd/app_usbd_core.c"
                                           "${PROJECT_SOURCE_DIR}/sdk/sdk15/components/libraries/usbd/app_usbd_serial_num.c"
                                           "${PROJECT_SOURCE_DIR}/sdk/sdk15/components/libraries/usbd/app_usbd_string_desc.c"
                                           "${PROJECT_SOURCE_DIR}/sdk/sdk15/components/libraries/usbd/class/cdc/acm/app_usbd_cdc_acm.c"
                                           "${PROJECT_SOURCE_DIR}/sdk/sdk15/components/libraries/atomic_fifo/nrf_atfifo.c"
                                           "${PROJECT_SOURCE_DIR}/sdk/sdk15/external/utf_converter/utf.c"
                                           "${PROJECT_SOURCE_DIR}/sdk/sdk15/integration/nrfx/legacy/nrf_drv_clock.c"
                                           "${PROJECT_SOURCE_DIR}/sdk/sdk15/integration/nrfx/legacy/nrf_drv_power.c"
                                           "${PROJECT_SOURCE_DIR}/sdk/sdk15/modules/nrfx/drivers/src/nrfx_clock.c"
                                           "${PROJECT_SOURCE_DIR}/sdk/sdk15/modules/nrfx/drivers/src/nrfx_power.c")
  target_sources(${FEATURE_SET} PRIVATE ${LOCAL_SRC})
  target_include_directories(${FEATURE_SET} PRIVATE "${PROJECT_SOURCE_DIR}/sdk/sdk15/components/drivers_nrf/usbd"
                                                    "${PROJECT_SOURCE_DIR}/sdk/sdk15/components/libraries/usbd"
                                                    "${PROJECT_SOURCE_DIR}/sdk/sdk15/components/libraries/memobj"
                                                    "${PROJECT_SOURCE_DIR}/sdk/sdk15/components/libraries/usbd/class/cdc/"
                                                    "${PROJECT_SOURCE_DIR}/sdk/sdk15/components/libraries/usbd/class/cdc/acm/"
                                                    "${PROJECT_SOURCE_DIR}/src/c/drivers/nrf")
  target_include_directories(${FEATURE_SET} SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/sdk/sdk15/components/libraries/atomic"
                                                           "${PROJECT_SOURCE_DIR}/sdk/sdk15/components/libraries/atomic_fifo"
                                                           "${PROJECT_SOURCE_DIR}/sdk/sdk15/components/libraries/atomic_fifo"
                                                           "${PROJECT_SOURCE_DIR}/sdk/sdk15/external/utf_converter")
  
else(${PLATFORM} STREQUAL "NRF52840")
  message(FATAL_ERROR "PLATFORM ${PLATFORM} does not support Virtual Com Port!")
endif(${PLATFORM} STREQUAL "NRF52840")

list(APPEND VIRTUAL_COM_TARGETS ${FEATURE_SET})



