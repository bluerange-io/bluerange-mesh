if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(SANITIZER  "-fsanitize=address -fsanitize=pointer-compare -fsanitize=pointer-subtract -fsanitize=undefined -fsanitize=integer-divide-by-zero -fsanitize=unreachable -fsanitize=vla-bound -fsanitize=null -fsanitize=return -fsanitize=signed-integer-overflow -fsanitize=bounds-strict -fsanitize=enum -fsanitize=bool -fsanitize=vptr -fsanitize=pointer-overflow")
  
  set(CMAKE_C_FLAGS           "-include ${PROJECT_SOURCE_DIR}/cherrysim/SystemTest.h -DSDK=11 -m32 -Wno-unknown-pragmas -fno-builtin -fno-strict-aliasing -fomit-frame-pointer -std=gnu99" CACHE INTERNAL "c compiler flags")
  set(CMAKE_CXX_FLAGS         "-include ${PROJECT_SOURCE_DIR}/cherrysim/SystemTest.h -DSDK=11 -m32 -Wno-unknown-pragmas -fprofile-arcs -ftest-coverage -fno-builtin -fno-strict-aliasing -fomit-frame-pointer -fdata-sections -ffunction-sections -fsingle-precision-constant -std=c++17 -pthread ${SANITIZER} -fno-omit-frame-pointer" CACHE INTERNAL "cxx compiler flags")
  set(CMAKE_EXE_LINKER_FLAGS  "-fprofile-arcs -ftest-coverage ${SANITIZER} -fno-omit-frame-pointer"  CACHE INTERNAL "exe link flags")

  set(CMAKE_C_FLAGS_DEBUG     "-Og -g3 -ggdb3"  CACHE INTERNAL "c debug compiler flags")
  set(CMAKE_CXX_FLAGS_DEBUG   "-Og -g3 -ggdb3"  CACHE INTERNAL "cxx debug compiler flags")
  set(CMAKE_ASM_FLAGS_DEBUG   "-g -ggdb3"       CACHE INTERNAL "asm debug compiler flags")
   
  set(CMAKE_C_FLAGS_RELEASE   "-O3 -g3 -ggdb3"  CACHE INTERNAL "c release compiler flags")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g3 -ggdb3"  CACHE INTERNAL "cxx release compiler flags")
  set(CMAKE_ASM_FLAGS_RELEASE "-g3 -ggdb3"      CACHE INTERNAL "asm release compiler flags")
    
  set(CMAKE_C_FLAGS_MINSIZEREL   "-Os -g3 -ggdb3"   CACHE INTERNAL "c mininum size compiler flags")
  set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -g3 -ggdb3"   CACHE INTERNAL "cxx mininum size compiler flags")
  set(CMAKE_ASM_FLAGS_MINSIZEREL "-g3 -ggdb3"       CACHE INTERNAL "asm mininum size compiler flags")
else(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options_multi("${SIMULATOR_TARGETS}" "/FI ${CMAKE_CURRENT_LIST_DIR}/../cherrysim/SystemTest.h")
  target_compile_options_multi("${SIMULATOR_TARGETS}" "/std:c++latest")
  target_compile_options_multi("${SIMULATOR_TARGETS}" "/MP")
endif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")