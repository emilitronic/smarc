add_executable(tb_normalizer_slots tb_normalizer_slots.cpp)
add_executable(tb_normalizer_sum tb_normalizer_sum.cpp)
add_executable(tb_normalizer_max tb_normalizer_max.cpp)
add_executable(tb_normalizer_mean tb_normalizer_mean.cpp)
add_executable(tb_normalizer_variance tb_normalizer_variance.cpp)

foreach(test_target tb_normalizer_slots tb_normalizer_sum tb_normalizer_max tb_normalizer_mean tb_normalizer_variance)
  target_link_libraries(${test_target}
    PRIVATE smesh_model cascade -lz -ltermcap -lpthread
  )

  set_target_properties(${test_target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
  )
endforeach()

add_test(NAME smesh_unit_normalizer_slots COMMAND tb_normalizer_slots)
add_test(NAME smesh_unit_normalizer_sum COMMAND tb_normalizer_sum)
add_test(NAME smesh_unit_normalizer_max COMMAND tb_normalizer_max)
add_test(NAME smesh_unit_normalizer_mean COMMAND tb_normalizer_mean)
add_test(NAME smesh_unit_normalizer_variance COMMAND tb_normalizer_variance)
set_tests_properties(smesh_unit_normalizer_slots smesh_unit_normalizer_sum smesh_unit_normalizer_max smesh_unit_normalizer_mean smesh_unit_normalizer_variance PROPERTIES
  LABELS "unit;accum_response"
  TIMEOUT 10
)
