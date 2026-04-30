add_library(smem_memory
  src/Dram.cpp
  src/DramMemoryPort.cpp
  src/MemCtrl.cpp
  src/MemCtrlTimedPort.cpp
)

target_include_directories(smem_memory
  PUBLIC
    include
)

target_link_libraries(smem_memory
  PUBLIC
    cascade
)
