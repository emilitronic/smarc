add_executable(tb_loop_matmul_config tb_loop_matmul_config.cpp)

target_link_libraries(tb_loop_matmul_config
  PRIVATE
    smesh_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

set_target_properties(tb_loop_matmul_config PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)

add_test(NAME smesh_unit_loop_matmul_config COMMAND tb_loop_matmul_config)
set_tests_properties(smesh_unit_loop_matmul_config PROPERTIES
  LABELS "unit;loop_matmul"
  TIMEOUT 10
)
