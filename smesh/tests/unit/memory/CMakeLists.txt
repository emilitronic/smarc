add_executable(tb_spad_banked
  tb_spad_banked.cpp
)

target_link_libraries(tb_spad_banked
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

add_test(NAME smesh_unit_spad_banked COMMAND tb_spad_banked)
set_tests_properties(smesh_unit_spad_banked PROPERTIES
  LABELS "unit;memory;spad"
  TIMEOUT 10
)
