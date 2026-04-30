add_library(smem_memory
  src/DramMemoryPort.cpp
)

target_include_directories(smem_memory
  PUBLIC
    include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../smile/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../smicro/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../smicro/src
)

target_link_libraries(smem_memory
  PUBLIC
    cascade
)
