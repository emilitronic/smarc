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

add_executable(tb_smesh_rs_first_layer tb_smesh_rs.cpp)
target_link_libraries(tb_smesh_rs_first_layer PRIVATE smesh_model_first_layer)
set_target_properties(tb_smesh_rs_first_layer PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
add_test(NAME smesh_unit_rs_first_layer COMMAND tb_smesh_rs_first_layer)
set_tests_properties(smesh_unit_rs_first_layer PROPERTIES
  LABELS "unit;rs;first_layer" TIMEOUT 10)

add_executable(tb_rs_store_issue tb_rs_store_issue.cpp)
target_link_libraries(tb_rs_store_issue PRIVATE smesh_model)
set_target_properties(tb_rs_store_issue PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
add_test(NAME smesh_unit_rs_store_issue COMMAND tb_rs_store_issue)

add_executable(tb_rs_execute_issue tb_rs_execute_issue.cpp)
target_link_libraries(tb_rs_execute_issue PRIVATE smesh_model)
set_target_properties(tb_rs_execute_issue PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
add_test(NAME smesh_unit_rs_execute_issue COMMAND tb_rs_execute_issue)
set_tests_properties(smesh_unit_rs_execute_issue PROPERTIES
  LABELS "unit;rs;ex_ctrl"
  TIMEOUT 10
)
set_tests_properties(smesh_unit_rs_store_issue PROPERTIES
  LABELS "unit;rs;st_ctrl"
  TIMEOUT 10
)

set_tests_properties(smesh_unit_rs PROPERTIES
  LABELS "unit;rs"
  TIMEOUT 10
)
