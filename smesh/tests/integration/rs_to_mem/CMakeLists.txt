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

add_executable(tb_rs_to_mem_suite_first_layer
  tb_rs_to_mem_suite.cpp
  tb_rs_to_mem_harness.cpp
  tb_rs_to_mem_test_cases.cpp
)
target_link_libraries(tb_rs_to_mem_suite_first_layer PRIVATE
  smesh_model_first_layer smem_memory cascade -lz -ltermcap -lpthread)
set_target_properties(tb_rs_to_mem_suite_first_layer PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
add_test(NAME smesh_rs_to_mem_loop_conv_temporal3_pixel2
  COMMAND tb_rs_to_mem_suite_first_layer -test=loop_conv_temporal3_pixel2)
add_test(NAME smesh_rs_to_mem_loop_conv_temporal3_padded_pixel2
  COMMAND tb_rs_to_mem_suite_first_layer -test=loop_conv_temporal3_padded_pixel2)
set_tests_properties(smesh_rs_to_mem_loop_conv_temporal3_pixel2
  smesh_rs_to_mem_loop_conv_temporal3_padded_pixel2 PROPERTIES
  LABELS "integration;rs_to_mem;loop_conv;first_layer" TIMEOUT 30)

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
add_test(NAME smesh_rs_to_mem_loop_conv_dma COMMAND tb_rs_to_mem_suite -test=loop_conv_dma)
add_test(NAME smesh_rs_to_mem_loop_conv_kernel2 COMMAND tb_rs_to_mem_suite -test=loop_conv_kernel2)
add_test(NAME smesh_rs_to_mem_loop_conv_temporal3 COMMAND tb_rs_to_mem_suite -test=loop_conv_temporal3)
add_test(NAME smesh_rs_to_mem_loop_conv_temporal3_2x6 COMMAND tb_rs_to_mem_suite -test=loop_conv_temporal3_2x6)
add_test(NAME smesh_rs_to_mem_loop_conv_temporal3_padded COMMAND tb_rs_to_mem_suite -test=loop_conv_temporal3_padded)
add_test(NAME smesh_rs_to_mem_loop_conv_padding COMMAND tb_rs_to_mem_suite -test=loop_conv_padding)
add_test(NAME smesh_rs_to_mem_loop_conv_images COMMAND tb_rs_to_mem_suite -test=loop_conv_images)
add_test(NAME smesh_rs_to_mem_loop_ws_dma_i2 COMMAND tb_rs_to_mem_suite -test=loop_ws_dma_i2)
add_test(NAME smesh_rs_to_mem_loop_ws_dma_k2 COMMAND tb_rs_to_mem_suite -test=loop_ws_dma_k2)
add_test(NAME smesh_rs_to_mem_full_width_accum_load COMMAND tb_rs_to_mem_suite -test=full_width_accum_load)
add_test(NAME smesh_rs_to_mem_repeated_loads COMMAND tb_rs_to_mem_suite -test=repeated_loads)
add_test(NAME smesh_rs_to_mem_scaled_shrink_load COMMAND tb_rs_to_mem_suite -test=scaled_shrink_load)
add_test(NAME smesh_rs_to_mem_shared_accum_read COMMAND tb_rs_to_mem_suite -test=shared_accum_read)
add_test(NAME smesh_rs_to_mem_accum_scale_store COMMAND tb_rs_to_mem_suite -test=accum_scale_store)
add_test(NAME smesh_rs_to_mem_zero_writer COMMAND tb_rs_to_mem_suite -test=zero_writer)
add_test(NAME smesh_rs_to_mem_beat4_zero_writer COMMAND tb_rs_to_mem_suite_beat4 -test=zero_writer)
add_test(NAME smesh_rs_to_mem_dim8_zero_writer COMMAND tb_rs_to_mem_suite_dim8 -test=zero_writer)
set_tests_properties(smesh_rs_to_mem_beat4_zero_writer PROPERTIES
  LABELS "integration;rs_to_mem;beat4" TIMEOUT 30)
set_tests_properties(smesh_rs_to_mem_dim8_zero_writer PROPERTIES
  LABELS "integration;rs_to_mem;dim8" TIMEOUT 30)

foreach(variant IN ITEMS default dim8 beat4)
  if(variant STREQUAL "default")
    set(pool_runner tb_rs_to_mem_suite)
  else()
    set(pool_runner tb_rs_to_mem_suite_${variant})
  endif()
  foreach(case IN ITEMS max_pool max_pool_padded max_pool_spad)
    set(pool_test smesh_rs_to_mem_${variant}_${case})
    add_test(NAME ${pool_test} COMMAND ${pool_runner} -test=${case})
    set_tests_properties(${pool_test} PROPERTIES
      LABELS "integration;rs_to_mem;pooling;${variant}" TIMEOUT 30)
  endforeach()
endforeach()

# Every size/feature variant uses the same test monitor and performance helper.
foreach(runner tb_rs_to_mem_suite tb_rs_to_mem_suite_first_layer
    tb_rs_to_mem_suite_dim8 tb_rs_to_mem_suite_beat4)
  target_sources(${runner} PRIVATE tb_rs_to_mem_monitor.cpp tb_rs_to_mem_perf.cpp)
  target_link_libraries(${runner} PRIVATE smesh_test_perf)
endforeach()

set_tests_properties(
  smesh_rs_to_mem_basic
  smesh_rs_to_mem_mul_pre
  smesh_rs_to_mem_concurrent_banks
  smesh_rs_to_mem_same_bank_serializes
  smesh_rs_to_mem_loop_ws
  smesh_rs_to_mem_loop_ws_dma
  smesh_rs_to_mem_loop_conv_dma
  smesh_rs_to_mem_loop_conv_kernel2
  smesh_rs_to_mem_loop_conv_temporal3
  smesh_rs_to_mem_loop_conv_temporal3_2x6
  smesh_rs_to_mem_loop_conv_temporal3_padded
  smesh_rs_to_mem_loop_conv_padding
  smesh_rs_to_mem_loop_conv_images
  smesh_rs_to_mem_loop_ws_dma_i2
  smesh_rs_to_mem_loop_ws_dma_k2
  smesh_rs_to_mem_full_width_accum_load
  smesh_rs_to_mem_repeated_loads
  smesh_rs_to_mem_scaled_shrink_load
  smesh_rs_to_mem_shared_accum_read
  smesh_rs_to_mem_accum_scale_store
  smesh_rs_to_mem_zero_writer
  PROPERTIES
    LABELS "integration;rs_to_mem"
    TIMEOUT 30
)

foreach(case IN ITEMS transpose_a transpose_a_same_bank transpose_b transpose_b_expanded)
  add_test(NAME smesh_rs_to_mem_${case} COMMAND tb_rs_to_mem_suite -test=${case})
  set_tests_properties(smesh_rs_to_mem_${case} PROPERTIES
    LABELS "integration;rs_to_mem;transposer" TIMEOUT 30)
endforeach()
foreach(case IN ITEMS transpose_a transpose_b transpose_b_expanded)
  add_test(NAME smesh_rs_to_mem_dim8_${case} COMMAND tb_rs_to_mem_suite_dim8 -test=${case})
  set_tests_properties(smesh_rs_to_mem_dim8_${case} PROPERTIES
    LABELS "integration;rs_to_mem;transposer;dim8" TIMEOUT 30)
endforeach()

foreach(variant IN ITEMS default dim8)
  if(variant STREQUAL "default")
    set(helper_target tb_transpose_helper)
    set(helper_model smesh_model)
    set(helper_test smesh_rs_to_mem_transpose_helper)
  else()
    set(helper_target tb_transpose_helper_dim8)
    set(helper_model smesh_model_dim8)
    set(helper_test smesh_rs_to_mem_dim8_transpose_helper)
  endif()
  add_executable(${helper_target} tb_transpose_helper.cpp tb_rs_to_mem_harness.cpp
    tb_rs_to_mem_test_cases.cpp tb_rs_to_mem_monitor.cpp)
  target_link_libraries(${helper_target} PRIVATE ${helper_model} smem_memory cascade -lz -ltermcap -lpthread)
  set_target_properties(${helper_target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
  add_test(NAME ${helper_test} COMMAND ${helper_target})
  set_tests_properties(${helper_test} PROPERTIES LABELS "integration;rs_to_mem;transposer" TIMEOUT 30)
  if(variant STREQUAL "dim8")
    set_property(TEST ${helper_test} APPEND PROPERTY LABELS dim8)
  endif()
  if(variant STREQUAL "default")
    set(integration_target tb_transpose_integration)
    set(integration_test smesh_rs_to_mem_transpose_unroller)
  else()
    set(integration_target tb_transpose_integration_dim8)
    set(integration_test smesh_rs_to_mem_dim8_transpose_unroller)
  endif()
  add_executable(${integration_target} tb_transpose_integration.cpp tb_rs_to_mem_harness.cpp
    tb_rs_to_mem_test_cases.cpp tb_rs_to_mem_monitor.cpp)
  target_link_libraries(${integration_target} PRIVATE ${helper_model} smem_memory cascade -lz -ltermcap -lpthread)
  set_target_properties(${integration_target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
  add_test(NAME ${integration_test} COMMAND ${integration_target})
  set_tests_properties(${integration_test} PROPERTIES LABELS "integration;rs_to_mem;transposer" TIMEOUT 30)
  if(variant STREQUAL "dim8")
    set_property(TEST ${integration_test} APPEND PROPERTY LABELS dim8)
  endif()
endforeach()
