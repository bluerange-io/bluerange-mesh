set(PLATFORM NRF52832)

# add the SDK driver for SPI
include(${PROJECT_SOURCE_DIR}/config/featuresets/CMakeFragments/AddSdkSpi.cmake)

# add the FruityMesh driver for BME280
file(GLOB LOCAL_SRC CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/c/drivers/bme280.cpp")
target_sources(${FEATURE_SET} PRIVATE ${LOCAL_SRC})
target_include_directories(${FEATURE_SET} PRIVATE "${PROJECT_SOURCE_DIR}/src/c/drivers")
