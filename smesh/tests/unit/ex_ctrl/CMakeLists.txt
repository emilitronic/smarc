set(EX_CTRL_UNIT_TESTS
  tb_ex_ctrl_completion
  tb_ex_ctrl_decoder
  tb_ex_ctrl_feed_signals
  tb_ex_ctrl_operand_pack
  tb_ex_ctrl_read_priority
  tb_ex_ctrl_read_req_logic
  tb_ex_ctrl_row_addr
  tb_ex_ctrl_row_pad
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

add_test(NAME smesh_unit_ex_ctrl_completion COMMAND tb_ex_ctrl_completion)
add_test(NAME smesh_unit_ex_ctrl_decoder COMMAND tb_ex_ctrl_decoder)
add_test(NAME smesh_unit_ex_ctrl_feed_signals COMMAND tb_ex_ctrl_feed_signals)
add_test(NAME smesh_unit_ex_ctrl_operand_pack COMMAND tb_ex_ctrl_operand_pack)
add_test(NAME smesh_unit_ex_ctrl_read_priority COMMAND tb_ex_ctrl_read_priority)
add_test(NAME smesh_unit_ex_ctrl_read_req_logic COMMAND tb_ex_ctrl_read_req_logic)
add_test(NAME smesh_unit_ex_ctrl_row_addr COMMAND tb_ex_ctrl_row_addr)
add_test(NAME smesh_unit_ex_ctrl_row_pad COMMAND tb_ex_ctrl_row_pad)

set_tests_properties(
  smesh_unit_ex_ctrl_completion
  smesh_unit_ex_ctrl_decoder
  smesh_unit_ex_ctrl_feed_signals
  smesh_unit_ex_ctrl_operand_pack
  smesh_unit_ex_ctrl_read_priority
  smesh_unit_ex_ctrl_read_req_logic
  smesh_unit_ex_ctrl_row_addr
  smesh_unit_ex_ctrl_row_pad
  PROPERTIES
    LABELS "unit;ex_ctrl"
    TIMEOUT 10
)
