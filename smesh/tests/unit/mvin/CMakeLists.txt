add_executable(tb_mvin_handshake tb_mvin_handshake.cpp)
target_link_libraries(tb_mvin_handshake PRIVATE
  smesh_model cascade -lz -ltermcap -lpthread)
set_target_properties(tb_mvin_handshake PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
add_test(NAME smesh_unit_mvin_handshake COMMAND tb_mvin_handshake)
set_tests_properties(smesh_unit_mvin_handshake PROPERTIES
  LABELS "unit;mvin" TIMEOUT 10)

add_executable(tb_mvin_repeat tb_mvin_repeat.cpp)
target_link_libraries(tb_mvin_repeat PRIVATE
  smesh_model cascade -lz -ltermcap -lpthread)
set_target_properties(tb_mvin_repeat PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
add_test(NAME smesh_unit_mvin_repeat COMMAND tb_mvin_repeat)
set_tests_properties(smesh_unit_mvin_repeat PROPERTIES
  LABELS "unit;mvin" TIMEOUT 10)

add_executable(tb_mvin_latency tb_mvin_latency.cpp)
target_link_libraries(tb_mvin_latency PRIVATE
  smesh_model cascade -lz -ltermcap -lpthread)
set_target_properties(tb_mvin_latency PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
add_test(NAME smesh_unit_mvin_latency COMMAND tb_mvin_latency)
set_tests_properties(smesh_unit_mvin_latency PROPERTIES
  LABELS "unit;mvin" TIMEOUT 10)
