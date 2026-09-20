add_executable(tb_spad_banked
  tb_spad_banked.cpp
)

add_executable(tb_accum_banked
  tb_accum_banked.cpp
)

target_link_libraries(tb_spad_banked
  PRIVATE
    smesh_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

target_link_libraries(tb_accum_banked
  PRIVATE
    smesh_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

set_target_properties(tb_spad_banked PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)

set_target_properties(tb_accum_banked PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)

add_test(NAME smesh_unit_spad_banked COMMAND tb_spad_banked)
add_test(NAME smesh_unit_accum_banked COMMAND tb_accum_banked)
set_tests_properties(smesh_unit_spad_banked PROPERTIES
  LABELS "unit;memory;spad"
  TIMEOUT 10
)
set_tests_properties(smesh_unit_accum_banked PROPERTIES
  LABELS "unit;memory;accum"
  TIMEOUT 10
)
