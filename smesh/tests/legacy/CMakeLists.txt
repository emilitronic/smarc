# Pre-Cascade milestone testbenches (M0-M3) for the old plain-C++ SmeshDevice
# behavioral model. These predate the Cascade hardware model (ExCtrl/Spad/
# Accum/SmeshTop) covered by unit/ and integration/, and do not exercise it
# at all. Kept for historical reference, not part of the active regression
# suite -- none of these are registered as ctest targets.

add_executable(tb_smesh_m0
  tb_smesh_m0.cpp
)

target_link_libraries(tb_smesh_m0
  PRIVATE
    smesh_model
)

add_executable(tb_smesh_m1
  tb_smesh_m1.cpp
)

target_link_libraries(tb_smesh_m1
  PRIVATE
    smesh_model
)

add_executable(tb_smesh_m2
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/SmeshCommandDriver.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/SmeshShell.cpp
  tb_smesh_m2.cpp
)

target_include_directories(tb_smesh_m2
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
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
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/SmeshCommandDriver.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/SmeshShell.cpp
  tb_smesh_m3.cpp
)

target_include_directories(tb_smesh_m3
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
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

set_target_properties(tb_smesh_m0 tb_smesh_m1 tb_smesh_m2 tb_smesh_m3 PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
