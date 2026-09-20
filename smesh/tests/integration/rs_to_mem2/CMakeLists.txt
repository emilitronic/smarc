# rs_to_mem2 reuses rs_to_mem's own test-case data file directly (same
# RsMemTestCase struct, same basic/mul_pre/concurrent_banks/
# same_bank_serializes scenarios) rather than duplicating it, so the two
# suites are guaranteed to run identical test data.
add_executable(tb_rs_to_mem2_suite
  tb_rs_to_mem2_suite.cpp
  tb_rs_to_mem2_harness.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../rs_to_mem/tb_rs_to_mem_test_cases.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../rs_to_mem/tb_rs_to_mem_harness.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../src/Smesh.cpp
)

target_include_directories(tb_rs_to_mem2_suite
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../rs_to_mem
)

target_link_libraries(tb_rs_to_mem2_suite
  PRIVATE
    smesh_model
    smem_memory
    cascade
    -lz
    -ltermcap
    -lpthread
)

set_target_properties(tb_rs_to_mem2_suite PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)

add_test(
  NAME smesh_rs_to_mem2_basic
  COMMAND tb_rs_to_mem2_suite -test=basic
)

add_test(
  NAME smesh_rs_to_mem2_mul_pre
  COMMAND tb_rs_to_mem2_suite -test=mul_pre
)

add_test(
  NAME smesh_rs_to_mem2_concurrent_banks
  COMMAND tb_rs_to_mem2_suite -test=concurrent_banks
)

add_test(
  NAME smesh_rs_to_mem2_same_bank_serializes
  COMMAND tb_rs_to_mem2_suite -test=same_bank_serializes
)

set_tests_properties(
  smesh_rs_to_mem2_basic
  smesh_rs_to_mem2_mul_pre
  smesh_rs_to_mem2_concurrent_banks
  smesh_rs_to_mem2_same_bank_serializes
  PROPERTIES
    LABELS "integration;rs_to_mem2"
    TIMEOUT 30
)
