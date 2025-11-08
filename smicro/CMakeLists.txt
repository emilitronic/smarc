# smicro example target
file(GLOB SMICRO_BASE_SRCS
  src/SoC.cpp
  src/RvCore.cpp
  src/MemCtrl.cpp
  src/L1.cpp
  src/L2.cpp
  src/Dram.cpp
  src/NnAccel.cpp
  src/MemXbar.cpp
  src/MemTester.cpp
)

# Original smicro testbench
add_executable(smicro
  src/tb_smicro.cpp
  ${SMICRO_BASE_SRCS}
)

target_include_directories(smicro
  PUBLIC
    include
)

target_link_libraries(smicro cascade -lz -ltermcap -lpthread)

target_compile_options(smicro PRIVATE -Wno-deprecated-builtins) # hide Cascade's depracation warnings

# New HAL-based test
add_executable(test_vectadd
  src/test_vectadd.cpp
  src/hal_cascade.cpp
  ${SMICRO_BASE_SRCS}
)

target_include_directories(test_vectadd
  PUBLIC
    include
)

target_link_libraries(test_vectadd cascade -lz -ltermcap -lpthread)
