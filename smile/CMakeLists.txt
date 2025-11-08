add_library(tile1
  src/Tile1.cpp
  src/Instruction.cpp
  src/Tile1_exec.cpp
  src/Diagnostics.cpp
)

target_include_directories(tile1
  PUBLIC
    include
)

target_link_libraries(tile1
  PUBLIC
    cascade
)

add_executable(tb_tile1
  src/tb_tile1.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../smicro/src/Dram.cpp
)

target_include_directories(tb_tile1
  PRIVATE
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../smicro/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../smicro/src
)

target_link_libraries(tb_tile1
  PRIVATE
    tile1
    -lz
    -ltermcap
    -lpthread
)

target_sources(tile1
  PRIVATE
    src/util/FlatBinLoader.cpp
    src/util/FlatBinLoader.hpp
)
