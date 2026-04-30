add_library(smem_memory
  src/Dram.cpp
  src/DramMemoryPort.cpp
)

target_include_directories(smem_memory
  PUBLIC
    include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../smile/include
)

target_link_libraries(smem_memory
  PUBLIC
    cascade
)
