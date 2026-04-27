add_library(smesh_model
  src/SmeshDevice.cpp
)

target_include_directories(smesh_model
  PUBLIC
    include
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
