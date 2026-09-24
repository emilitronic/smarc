set(LD_CTRL_UNIT_TESTS
  tb_ld_ctrl_cmd_queue
  tb_ld_ctrl_cmd_dec
)

foreach(test_target IN LISTS LD_CTRL_UNIT_TESTS)
  add_executable(${test_target} ${test_target}.cpp)
  target_link_libraries(${test_target} PRIVATE smesh_model cascade -lz -ltermcap -lpthread)
  set_target_properties(${test_target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
endforeach()

add_test(NAME smesh_unit_ld_ctrl_cmd_queue COMMAND tb_ld_ctrl_cmd_queue)
add_test(NAME smesh_unit_ld_ctrl_cmd_dec COMMAND tb_ld_ctrl_cmd_dec)
set_tests_properties(smesh_unit_ld_ctrl_cmd_queue smesh_unit_ld_ctrl_cmd_dec
  PROPERTIES LABELS "unit;ld_ctrl" TIMEOUT 10)
