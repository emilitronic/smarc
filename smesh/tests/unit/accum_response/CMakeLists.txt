add_executable(tb_normalizer_slots tb_normalizer_slots.cpp)

target_link_libraries(tb_normalizer_slots
  PRIVATE smesh_model cascade -lz -ltermcap -lpthread
)

set_target_properties(tb_normalizer_slots PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)

add_test(NAME smesh_unit_normalizer_slots COMMAND tb_normalizer_slots)
set_tests_properties(smesh_unit_normalizer_slots PROPERTIES
  LABELS "unit;accum_response"
  TIMEOUT 10
)
