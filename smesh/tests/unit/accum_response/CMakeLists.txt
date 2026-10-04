add_executable(tb_normalizer_slots tb_normalizer_slots.cpp)
add_executable(tb_normalizer_sum tb_normalizer_sum.cpp)

foreach(test_target tb_normalizer_slots tb_normalizer_sum)
  target_link_libraries(${test_target}
    PRIVATE smesh_model cascade -lz -ltermcap -lpthread
  )

  set_target_properties(${test_target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
  )
endforeach()

add_test(NAME smesh_unit_normalizer_slots COMMAND tb_normalizer_slots)
add_test(NAME smesh_unit_normalizer_sum COMMAND tb_normalizer_sum)
set_tests_properties(smesh_unit_normalizer_slots smesh_unit_normalizer_sum PROPERTIES
  LABELS "unit;accum_response"
  TIMEOUT 10
)
