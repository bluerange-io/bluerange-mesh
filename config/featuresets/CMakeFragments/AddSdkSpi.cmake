# This will add the SDK drivers for SPI (Serial Peripheral Interface)

if (${PLATFORM} STREQUAL "NRF52832")
  file(GLOB   LOCAL_SRC CONFIGURE_DEPENDS  "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/drivers_nrf/spi_master/nrf_drv_spi.cpp" 
                                           "${PROJECT_SOURCE_DIR}/sdk/sdk14/components/drivers_nrf/spi_master/nrf_drv_spi.c")
  target_sources(${FEATURE_SET} PRIVATE ${LOCAL_SRC})
elseif (${PLATFORM} STREQUAL "NRF52840")
  file(GLOB   LOCAL_SRC CONFIGURE_DEPENDS  "${PROJECT_SOURCE_DIR}/sdk/sdk15/modules/nrfx/drivers/src/nrfx_spi.c"
                                           "${PROJECT_SOURCE_DIR}/sdk/sdk15/modules/nrfx/drivers/src/prs/nrfx_prs.c"
                                           "${PROJECT_SOURCE_DIR}/sdk/sdk15/integration/nrfx/legacy/nrf_drv_spi.c")
  target_sources(${FEATURE_SET} PRIVATE ${LOCAL_SRC})
  target_include_directories(${FEATURE_SET} PRIVATE "${PROJECT_SOURCE_DIR}/sdk/sdk15/modules/nrfx/drivers/src")
  target_include_directories(${FEATURE_SET} PRIVATE "${PROJECT_SOURCE_DIR}/sdk/sdk15/integration/nrfx/legacy")
elseif (${PLATFORM} STREQUAL "ARM")
  # Arm has no implementation hence no additional files needed
else()
  message(FATAL_ERROR "PLATFORM ${PLATFORM} does not support the SDK drivers for SPI!")
endif()
