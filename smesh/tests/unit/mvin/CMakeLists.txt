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

add_executable(tb_zero_writer tb_zero_writer.cpp)
target_link_libraries(tb_zero_writer PRIVATE
  smesh_model cascade -lz -ltermcap -lpthread)
set_target_properties(tb_zero_writer PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
add_test(NAME smesh_unit_zero_writer COMMAND tb_zero_writer)
set_tests_properties(smesh_unit_zero_writer PROPERTIES
  LABELS "unit;mvin" TIMEOUT 10)

add_executable(tb_mvin_pixel_repeater tb_mvin_pixel_repeater.cpp)
target_link_libraries(tb_mvin_pixel_repeater PRIVATE
  smesh_model cascade -lz -ltermcap -lpthread)
set_target_properties(tb_mvin_pixel_repeater PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
add_test(NAME smesh_unit_mvin_pixel_repeater COMMAND tb_mvin_pixel_repeater)
add_test(NAME smesh_unit_mvin_pixel_passthrough COMMAND tb_mvin_pixel_repeater -passthrough=1)
set_tests_properties(smesh_unit_mvin_pixel_repeater smesh_unit_mvin_pixel_passthrough PROPERTIES
  LABELS "unit;mvin" TIMEOUT 10)

add_executable(tb_mvin_pixel_repeater_dim8 tb_mvin_pixel_repeater.cpp)
target_link_libraries(tb_mvin_pixel_repeater_dim8 PRIVATE
  smesh_model_dim8 cascade -lz -ltermcap -lpthread)
set_target_properties(tb_mvin_pixel_repeater_dim8 PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
add_test(NAME smesh_unit_mvin_pixel_repeater_dim8 COMMAND tb_mvin_pixel_repeater_dim8)
set_tests_properties(smesh_unit_mvin_pixel_repeater_dim8 PROPERTIES
  LABELS "unit;mvin;dim8" TIMEOUT 10)
