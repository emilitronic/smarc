add_executable(tb_rs_to_mem_suite
  tb_rs_to_mem_suite.cpp
)

target_link_libraries(tb_rs_to_mem_suite
  PRIVATE
    smesh_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

set_target_properties(tb_rs_to_mem_suite PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)

add_test(
  NAME smesh_rs_to_mem_basic
  COMMAND tb_rs_to_mem_suite -test=basic
)

add_test(
  NAME smesh_rs_to_mem_mul_pre
  COMMAND tb_rs_to_mem_suite -test=mul_pre
)

set_tests_properties(
  smesh_rs_to_mem_basic
  smesh_rs_to_mem_mul_pre
  PROPERTIES
    LABELS "integration;rs_to_mem"
    TIMEOUT 10
)
