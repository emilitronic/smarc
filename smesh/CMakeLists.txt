add_library(smesh_model
  src/SmeshDevice.cpp
)

target_include_directories(smesh_model
  PUBLIC
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../smem/include
)

target_link_libraries(smesh_model
  PUBLIC
    cascade
)

add_executable(tb_smesh_m0
  src/tb_smesh_m0.cpp
)

target_link_libraries(tb_smesh_m0
  PRIVATE
    smesh_model
)

add_executable(tb_smesh_m1
  src/tb_smesh_m1.cpp
)

target_link_libraries(tb_smesh_m1
  PRIVATE
    smesh_model
)

add_executable(tb_smesh_m2
  src/SmeshCommandDriver.cpp
  src/SmeshShell.cpp
  src/tb_smesh_m2.cpp
)

target_link_libraries(tb_smesh_m2
  PRIVATE
    smesh_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

add_executable(tb_smesh_m3
  src/SmeshCommandDriver.cpp
  src/SmeshShell.cpp
  src/tb_smesh_m3.cpp
)

target_link_libraries(tb_smesh_m3
  PRIVATE
    smesh_model
    smem_memory
    cascade
    -lz
    -ltermcap
    -lpthread
)
