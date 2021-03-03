if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/eink")
  string(FIND "${FEATURESET}" "eink"    EINK_TARGET)

  if(CMAKE_CROSSCOMPILING)
    if(EINK_TARGET GREATER -1)
      add_subdirectory(eink)
    endif(EINK_TARGET GREATER -1)
  else(CMAKE_CROSSCOMPILING)
    add_subdirectory(eink)
  endif(CMAKE_CROSSCOMPILING)
endif(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/eink")

if(CMAKE_CROSSCOMPILING)
  if(FEATURESET STREQUAL "prod_asset_nrf52" OR
     FEATURESET STREQUAL "dev_all_nrf52" OR
     FEATURESET STREQUAL "prod_asset_ins_nrf52840")
    file(GLOB   DRIVERS_SRC         ./bme280.cpp
                                    ./bmg250.cpp
                                    ./lis2dh12.cpp
                                    ./tlv493d.cpp)
    list(APPEND DRIVERS_INC         ./)
    add_library(DriversIf INTERFACE)
    target_link_libraries(DriversIf INTERFACE nrfSdkIf
                                    INTERFACE ConfigIf)
    target_include_directories(DriversIf INTERFACE ${DRIVERS_INC})

    add_library(Drivers STATIC EXCLUDE_FROM_ALL ${DRIVERS_SRC})
    target_link_libraries(Drivers  PUBLIC DriversIf
                                   PRIVATE nrfSdk
                                   PRIVATE ConfigIf)
  endif(FEATURESET STREQUAL "prod_asset_nrf52" OR
        FEATURESET STREQUAL "dev_all_nrf52" OR
        FEATURESET STREQUAL "prod_asset_ins_nrf52840")
endif(CMAKE_CROSSCOMPILING)
