add_executable(tb_smesh_rs
  tb_smesh_rs.cpp
)

target_link_libraries(tb_smesh_rs
  PRIVATE
    smesh_model
)

set_target_properties(tb_smesh_rs PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)

add_test(NAME smesh_unit_rs COMMAND tb_smesh_rs)

set_tests_properties(smesh_unit_rs PROPERTIES
  LABELS "unit;rs"
  TIMEOUT 10
)
