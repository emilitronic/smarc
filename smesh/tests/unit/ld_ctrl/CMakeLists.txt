add_executable(tb_ld_ctrl_cmd_queue tb_ld_ctrl_cmd_queue.cpp)

target_link_libraries(tb_ld_ctrl_cmd_queue PRIVATE smesh_model cascade -lz -ltermcap -lpthread)
set_target_properties(tb_ld_ctrl_cmd_queue PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")

add_test(NAME smesh_unit_ld_ctrl_cmd_queue COMMAND tb_ld_ctrl_cmd_queue)
set_tests_properties(smesh_unit_ld_ctrl_cmd_queue PROPERTIES LABELS "unit;ld_ctrl" TIMEOUT 10)
