add_executable(tb_rs_to_mem_suite
  tb_rs_to_mem_suite.cpp
  tb_rs_to_mem_harness.cpp
  tb_rs_to_mem_test_cases.cpp
)

target_link_libraries(tb_rs_to_mem_suite
  PRIVATE
    smesh_model
    smem_memory
    cascade
    -lz
    -ltermcap
    -lpthread
)

set_target_properties(tb_rs_to_mem_suite PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)

add_test(NAME smesh_rs_to_mem_basic COMMAND tb_rs_to_mem_suite -test=basic)
add_test(NAME smesh_rs_to_mem_mul_pre COMMAND tb_rs_to_mem_suite -test=mul_pre)
add_test(NAME smesh_rs_to_mem_concurrent_banks COMMAND tb_rs_to_mem_suite -test=concurrent_banks)
add_test(NAME smesh_rs_to_mem_same_bank_serializes COMMAND tb_rs_to_mem_suite -test=same_bank_serializes)

set_tests_properties(
  smesh_rs_to_mem_basic
  smesh_rs_to_mem_mul_pre
  smesh_rs_to_mem_concurrent_banks
  smesh_rs_to_mem_same_bank_serializes
  PROPERTIES
    LABELS "integration;rs_to_mem"
    TIMEOUT 30
)
