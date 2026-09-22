set(ST_CTRL_UNIT_TESTS
  tb_st_ctrl_cmd_dec
  tb_st_ctrl_dma_req
  tb_st_ctrl_geom
)

foreach(test_target IN LISTS ST_CTRL_UNIT_TESTS)
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

add_test(NAME smesh_unit_st_ctrl_cmd_dec COMMAND tb_st_ctrl_cmd_dec)
add_test(NAME smesh_unit_st_ctrl_dma_req COMMAND tb_st_ctrl_dma_req)
add_test(NAME smesh_unit_st_ctrl_geom COMMAND tb_st_ctrl_geom)

set_tests_properties(
  smesh_unit_st_ctrl_cmd_dec
  smesh_unit_st_ctrl_dma_req
  smesh_unit_st_ctrl_geom
  PROPERTIES
    LABELS "unit;st_ctrl"
    TIMEOUT 10
)
