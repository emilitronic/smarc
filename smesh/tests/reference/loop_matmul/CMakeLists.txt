add_executable(tb_loop_matmul_reference tb_loop_matmul_reference.cpp)
target_include_directories(tb_loop_matmul_reference PRIVATE
  ${PROJECT_SOURCE_DIR}/smesh/include/common
)
set_target_properties(tb_loop_matmul_reference PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
add_test(NAME smesh_reference_loop_matmul_small_ws COMMAND tb_loop_matmul_reference)
set_tests_properties(smesh_reference_loop_matmul_small_ws PROPERTIES
  LABELS "reference;loop_matmul"
)
