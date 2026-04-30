# smicro example target
file(GLOB SMICRO_BASE_SRCS
  src/SoC.cpp
  src/RvCore.cpp
  src/Tile1Core.cpp
  src/AccelMemBridge.cpp
  src/AccelArraySumSoc.cpp
  src/MemCtrl.cpp
  src/L1.cpp
  src/L2.cpp
  src/NnAccel.cpp
  src/MemXbar.cpp
  src/MemTester.cpp
)

# Core pieces of the Tile1-based RV core from smile
set(SMILE_CORE_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/../smile/src/Tile1.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../smile/src/Tile1_exec.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../smile/src/Instruction.cpp
)

# Original smicro testbench
add_executable(smicro
  src/tb_smicro.cpp
  ${SMICRO_BASE_SRCS}
  ${SMILE_CORE_SRCS}
)

target_include_directories(smicro
  PUBLIC
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../smile/include 
)

target_link_libraries(smicro cascade smem_memory -lz -ltermcap -lpthread)

target_compile_options(smicro PRIVATE -Wno-deprecated-builtins) # hide Cascade's depracation warnings

# New HAL-based test
add_executable(test_vectadd
  src/test_vectadd.cpp
  src/hal_cascade.cpp
  ${SMICRO_BASE_SRCS}
  ${SMILE_CORE_SRCS}
)

target_include_directories(test_vectadd
  PUBLIC
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../smile/include
)

target_link_libraries(test_vectadd cascade smem_memory -lz -ltermcap -lpthread)
