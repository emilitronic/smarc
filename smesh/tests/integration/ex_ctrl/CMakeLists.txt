add_executable(tb_ex_ctrl_suite
  tb_ex_ctrl_suite.cpp
  tb_ex_ctrl_harness.cpp
  tb_ex_ctrl_test_cases.cpp
)

target_link_libraries(tb_ex_ctrl_suite
  PRIVATE
    smesh_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

set_target_properties(tb_ex_ctrl_suite PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)

add_test(
  NAME smesh_ex_ctrl_basic
  COMMAND tb_ex_ctrl_suite -test=basic
)

add_test(
  NAME smesh_ex_ctrl_mul_pre
  COMMAND tb_ex_ctrl_suite -test=mul_pre
)

set_tests_properties(
  smesh_ex_ctrl_basic
  smesh_ex_ctrl_mul_pre
  PROPERTIES
    LABELS "integration;ex_ctrl"
    TIMEOUT 10
)
