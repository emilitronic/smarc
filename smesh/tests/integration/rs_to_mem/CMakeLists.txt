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

add_executable(tb_rs_to_mem_suite_dim8
  tb_rs_to_mem_suite.cpp
  tb_rs_to_mem_harness.cpp
  tb_rs_to_mem_test_cases.cpp
)
target_link_libraries(tb_rs_to_mem_suite_dim8 PRIVATE
  smesh_model_dim8 smem_memory cascade -lz -ltermcap -lpthread)
set_target_properties(tb_rs_to_mem_suite_dim8 PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
add_test(NAME smesh_rs_to_mem_dim8_loads
  COMMAND tb_rs_to_mem_suite_dim8 -test=dim8_loads)
add_test(NAME smesh_rs_to_mem_dim8_accum_scale_store
  COMMAND tb_rs_to_mem_suite_dim8 -test=accum_scale_store)
set_tests_properties(smesh_rs_to_mem_dim8_loads smesh_rs_to_mem_dim8_accum_scale_store PROPERTIES
  LABELS "integration;rs_to_mem;dim8" TIMEOUT 30)

add_executable(tb_rs_to_mem_suite_beat4
  tb_rs_to_mem_suite.cpp
  tb_rs_to_mem_harness.cpp
  tb_rs_to_mem_test_cases.cpp
)
target_link_libraries(tb_rs_to_mem_suite_beat4 PRIVATE
  smesh_model_beat4 smem_memory cascade -lz -ltermcap -lpthread)
set_target_properties(tb_rs_to_mem_suite_beat4 PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
add_test(NAME smesh_rs_to_mem_beat4_accum_load
  COMMAND tb_rs_to_mem_suite_beat4 -test=full_width_accum_load)
add_test(NAME smesh_rs_to_mem_beat4_loop_ws_dma
  COMMAND tb_rs_to_mem_suite_beat4 -test=loop_ws_dma)
add_test(NAME smesh_rs_to_mem_beat4_posted_loop_ws_dma
  COMMAND tb_rs_to_mem_suite_beat4 -test=beat4_posted_loop_ws_dma)
add_test(NAME smesh_rs_to_mem_beat4_tail
  COMMAND tb_rs_to_mem_suite_beat4 -test=beat4_tail)
add_test(NAME smesh_rs_to_mem_beat4_accum_scale_store
  COMMAND tb_rs_to_mem_suite_beat4 -test=accum_scale_store)
set_tests_properties(smesh_rs_to_mem_beat4_accum_load smesh_rs_to_mem_beat4_loop_ws_dma
  smesh_rs_to_mem_beat4_posted_loop_ws_dma smesh_rs_to_mem_beat4_tail smesh_rs_to_mem_beat4_accum_scale_store
  PROPERTIES LABELS "integration;rs_to_mem;beat4" TIMEOUT 30)

add_test(NAME smesh_rs_to_mem_basic COMMAND tb_rs_to_mem_suite -test=basic)
add_test(NAME smesh_rs_to_mem_mul_pre COMMAND tb_rs_to_mem_suite -test=mul_pre)
add_test(NAME smesh_rs_to_mem_concurrent_banks COMMAND tb_rs_to_mem_suite -test=concurrent_banks)
add_test(NAME smesh_rs_to_mem_same_bank_serializes COMMAND tb_rs_to_mem_suite -test=same_bank_serializes)
add_test(NAME smesh_rs_to_mem_loop_ws COMMAND tb_rs_to_mem_suite -test=loop_ws)
add_test(NAME smesh_rs_to_mem_loop_ws_dma COMMAND tb_rs_to_mem_suite -test=loop_ws_dma)
add_test(NAME smesh_rs_to_mem_loop_ws_dma_i2 COMMAND tb_rs_to_mem_suite -test=loop_ws_dma_i2)
add_test(NAME smesh_rs_to_mem_loop_ws_dma_k2 COMMAND tb_rs_to_mem_suite -test=loop_ws_dma_k2)
add_test(NAME smesh_rs_to_mem_full_width_accum_load COMMAND tb_rs_to_mem_suite -test=full_width_accum_load)
add_test(NAME smesh_rs_to_mem_repeated_loads COMMAND tb_rs_to_mem_suite -test=repeated_loads)
add_test(NAME smesh_rs_to_mem_scaled_shrink_load COMMAND tb_rs_to_mem_suite -test=scaled_shrink_load)
add_test(NAME smesh_rs_to_mem_shared_accum_read COMMAND tb_rs_to_mem_suite -test=shared_accum_read)
add_test(NAME smesh_rs_to_mem_accum_scale_store COMMAND tb_rs_to_mem_suite -test=accum_scale_store)

set_tests_properties(
  smesh_rs_to_mem_basic
  smesh_rs_to_mem_mul_pre
  smesh_rs_to_mem_concurrent_banks
  smesh_rs_to_mem_same_bank_serializes
  smesh_rs_to_mem_loop_ws
  smesh_rs_to_mem_loop_ws_dma
  smesh_rs_to_mem_loop_ws_dma_i2
  smesh_rs_to_mem_loop_ws_dma_k2
  smesh_rs_to_mem_full_width_accum_load
  smesh_rs_to_mem_repeated_loads
  smesh_rs_to_mem_scaled_shrink_load
  smesh_rs_to_mem_shared_accum_read
  smesh_rs_to_mem_accum_scale_store
  PROPERTIES
    LABELS "integration;rs_to_mem"
    TIMEOUT 30
)
