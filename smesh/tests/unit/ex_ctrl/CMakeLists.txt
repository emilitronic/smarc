set(EX_CTRL_UNIT_TESTS
  tb_ex_ctrl_cmd_queue
  tb_ex_ctrl_completion
  tb_ex_ctrl_decoder
  tb_ex_ctrl_feed_signals
  tb_ex_ctrl_mesh_cntl_deq_ctrl
  tb_ex_ctrl_mesh_cntl_pack
  tb_ex_ctrl_mesh_cntl_queue
  tb_ex_ctrl_mesh_in_sel_pad
  tb_ex_ctrl_mesh_tag_select
  tb_ex_ctrl_operand_pack
  tb_ex_ctrl_read_priority
  tb_ex_ctrl_read_req_logic
  tb_ex_ctrl_row_addr
  tb_ex_ctrl_row_feed_state
  tb_ex_ctrl_row_pad
  tb_ex_ctrl_state_mul_pre
  tb_ex_ctrl_writeback
)

foreach(test_target IN LISTS EX_CTRL_UNIT_TESTS)
  add_executable(${test_target}
    ${test_target}.cpp
  )

  target_link_libraries(${test_target}
    PRIVATE
      smesh_model
      cascade
      -lz
      -ltermcap
      -lpthread
  )

  set_target_properties(${test_target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
  )
endforeach()

add_test(NAME smesh_unit_ex_ctrl_cmd_queue COMMAND tb_ex_ctrl_cmd_queue)
add_test(NAME smesh_unit_ex_ctrl_completion COMMAND tb_ex_ctrl_completion)
add_test(NAME smesh_unit_ex_ctrl_decoder COMMAND tb_ex_ctrl_decoder)
add_test(NAME smesh_unit_ex_ctrl_feed_signals COMMAND tb_ex_ctrl_feed_signals)
add_test(NAME smesh_unit_ex_ctrl_mesh_cntl_deq_ctrl COMMAND tb_ex_ctrl_mesh_cntl_deq_ctrl)
add_test(NAME smesh_unit_ex_ctrl_mesh_cntl_pack COMMAND tb_ex_ctrl_mesh_cntl_pack)
add_test(NAME smesh_unit_ex_ctrl_mesh_cntl_queue COMMAND tb_ex_ctrl_mesh_cntl_queue)
add_test(NAME smesh_unit_ex_ctrl_mesh_in_sel_pad COMMAND tb_ex_ctrl_mesh_in_sel_pad)
add_test(NAME smesh_unit_ex_ctrl_mesh_tag_select COMMAND tb_ex_ctrl_mesh_tag_select)
add_test(NAME smesh_unit_ex_ctrl_operand_pack COMMAND tb_ex_ctrl_operand_pack)
add_test(NAME smesh_unit_ex_ctrl_read_priority COMMAND tb_ex_ctrl_read_priority)
add_test(NAME smesh_unit_ex_ctrl_read_req_logic COMMAND tb_ex_ctrl_read_req_logic)
add_test(NAME smesh_unit_ex_ctrl_row_addr COMMAND tb_ex_ctrl_row_addr)
add_test(NAME smesh_unit_ex_ctrl_row_feed_state COMMAND tb_ex_ctrl_row_feed_state)
add_test(NAME smesh_unit_ex_ctrl_row_pad COMMAND tb_ex_ctrl_row_pad)
add_test(NAME smesh_unit_ex_ctrl_state_mul_pre COMMAND tb_ex_ctrl_state_mul_pre)
add_test(NAME smesh_unit_ex_ctrl_writeback COMMAND tb_ex_ctrl_writeback)

set_tests_properties(
  smesh_unit_ex_ctrl_cmd_queue
  smesh_unit_ex_ctrl_completion
  smesh_unit_ex_ctrl_decoder
  smesh_unit_ex_ctrl_feed_signals
  smesh_unit_ex_ctrl_mesh_cntl_deq_ctrl
  smesh_unit_ex_ctrl_mesh_cntl_pack
  smesh_unit_ex_ctrl_mesh_cntl_queue
  smesh_unit_ex_ctrl_mesh_in_sel_pad
  smesh_unit_ex_ctrl_mesh_tag_select
  smesh_unit_ex_ctrl_operand_pack
  smesh_unit_ex_ctrl_read_priority
  smesh_unit_ex_ctrl_read_req_logic
  smesh_unit_ex_ctrl_row_addr
  smesh_unit_ex_ctrl_row_feed_state
  smesh_unit_ex_ctrl_row_pad
  smesh_unit_ex_ctrl_state_mul_pre
  smesh_unit_ex_ctrl_writeback
  PROPERTIES
    LABELS "unit;ex_ctrl"
    TIMEOUT 10
)
