set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY" )


set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_C_COMPILER   "${GCC_PATH}/bin/arm-none-eabi-gcc${exe_suffix}")
set(CMAKE_ASM_COMPILER "${GCC_PATH}/bin/arm-none-eabi-gcc${exe_suffix}")
set(CMAKE_CXX_COMPILER "${GCC_PATH}/bin/arm-none-eabi-g++${exe_suffix}")
SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
SET(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

set(CMAKE_ASM_SOURCE_FILE_EXTENSIONS ".s")
