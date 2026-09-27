add_executable(tb_loop_matmul_config tb_loop_matmul_config.cpp)
add_executable(tb_loop_matmul_ld_a tb_loop_matmul_ld_a.cpp)
add_executable(tb_loop_matmul_ld_b tb_loop_matmul_ld_b.cpp)
add_executable(tb_loop_matmul_ld_d tb_loop_matmul_ld_d.cpp)
add_executable(tb_loop_matmul_ld_utilization tb_loop_matmul_ld_utilization.cpp)
add_executable(tb_loop_matmul_ex tb_loop_matmul_ex.cpp)
add_executable(tb_loop_matmul_st_c tb_loop_matmul_st_c.cpp)

target_link_libraries(tb_loop_matmul_config
  PRIVATE
    smesh_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

target_link_libraries(tb_loop_matmul_ld_a
  PRIVATE
    smesh_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

target_link_libraries(tb_loop_matmul_ld_b
  PRIVATE
    smesh_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

target_link_libraries(tb_loop_matmul_ld_d
  PRIVATE
    smesh_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

target_link_libraries(tb_loop_matmul_ld_utilization
  PRIVATE
    smesh_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

target_link_libraries(tb_loop_matmul_ex
  PRIVATE
    smesh_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

target_link_libraries(tb_loop_matmul_st_c
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
set_target_properties(tb_loop_matmul_ld_a PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
set_target_properties(tb_loop_matmul_ld_b PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
set_target_properties(tb_loop_matmul_ld_d PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
set_target_properties(tb_loop_matmul_ld_utilization PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
set_target_properties(tb_loop_matmul_ex PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
set_target_properties(tb_loop_matmul_st_c PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)

add_test(NAME smesh_unit_loop_matmul_config COMMAND tb_loop_matmul_config)
add_test(NAME smesh_unit_loop_matmul_ld_a COMMAND tb_loop_matmul_ld_a)
add_test(NAME smesh_unit_loop_matmul_ld_b COMMAND tb_loop_matmul_ld_b)
add_test(NAME smesh_unit_loop_matmul_ld_d COMMAND tb_loop_matmul_ld_d)
add_test(NAME smesh_unit_loop_matmul_ld_utilization COMMAND tb_loop_matmul_ld_utilization)
add_test(NAME smesh_unit_loop_matmul_ex COMMAND tb_loop_matmul_ex)
add_test(NAME smesh_unit_loop_matmul_st_c COMMAND tb_loop_matmul_st_c)
set_tests_properties(smesh_unit_loop_matmul_config smesh_unit_loop_matmul_ld_a smesh_unit_loop_matmul_ld_b smesh_unit_loop_matmul_ld_d smesh_unit_loop_matmul_ld_utilization smesh_unit_loop_matmul_ex smesh_unit_loop_matmul_st_c PROPERTIES
  LABELS "unit;loop_matmul"
  TIMEOUT 10
)
