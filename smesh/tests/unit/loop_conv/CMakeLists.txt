add_executable(tb_loop_conv_cmd_queue tb_loop_conv_cmd_queue.cpp)
add_executable(tb_loop_conv_ld_weight tb_loop_conv_ld_weight.cpp)
target_link_libraries(tb_loop_conv_ld_weight PRIVATE smesh_model cascade -lz -ltermcap -lpthread)
set_target_properties(tb_loop_conv_ld_weight PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
add_test(NAME smesh_unit_loop_conv_ld_weight COMMAND tb_loop_conv_ld_weight)
set_tests_properties(smesh_unit_loop_conv_ld_weight PROPERTIES LABELS "unit;loop_conv" TIMEOUT 10)
add_executable(tb_loop_conv_weight_request tb_loop_conv_weight_request.cpp)
target_link_libraries(tb_loop_conv_weight_request PRIVATE smesh_model cascade -lz -ltermcap -lpthread)
set_target_properties(tb_loop_conv_weight_request PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
add_test(NAME smesh_unit_loop_conv_weight_request COMMAND tb_loop_conv_weight_request)
set_tests_properties(smesh_unit_loop_conv_weight_request PROPERTIES LABELS "unit;loop_conv" TIMEOUT 10)
add_executable(tb_loop_conv_load_stream tb_loop_conv_load_stream.cpp)
target_link_libraries(tb_loop_conv_load_stream PRIVATE smesh_model cascade -lz -ltermcap -lpthread)
set_target_properties(tb_loop_conv_load_stream PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
add_test(NAME smesh_unit_loop_conv_load_stream COMMAND tb_loop_conv_load_stream)
set_tests_properties(smesh_unit_loop_conv_load_stream PROPERTIES LABELS "unit;loop_conv" TIMEOUT 10)
add_executable(tb_loop_conv_state tb_loop_conv_state.cpp)
add_executable(tb_loop_conv_scheduler tb_loop_conv_scheduler.cpp)
add_executable(tb_loop_conv_input_request tb_loop_conv_input_request.cpp)
target_link_libraries(tb_loop_conv_input_request PRIVATE smesh_model cascade -lz -ltermcap -lpthread)
set_target_properties(tb_loop_conv_input_request PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
add_test(NAME smesh_unit_loop_conv_input_request COMMAND tb_loop_conv_input_request)
set_tests_properties(smesh_unit_loop_conv_input_request PROPERTIES
  LABELS "unit;loop_conv"
  TIMEOUT 10
)
add_executable(tb_loop_conv_ld_bias tb_loop_conv_ld_bias.cpp)
add_executable(tb_loop_conv_ld_input tb_loop_conv_ld_input.cpp)
target_link_libraries(tb_loop_conv_ld_input PRIVATE smesh_model cascade -lz -ltermcap -lpthread)
set_target_properties(tb_loop_conv_ld_input PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
add_test(NAME smesh_unit_loop_conv_ld_input COMMAND tb_loop_conv_ld_input)
set_tests_properties(smesh_unit_loop_conv_ld_input PROPERTIES
  LABELS "unit;loop_conv"
  TIMEOUT 10
)
add_executable(tb_loop_conv_bias_stream tb_loop_conv_bias_stream.cpp)
target_link_libraries(tb_loop_conv_bias_stream PRIVATE smesh_model cascade -lz -ltermcap -lpthread)
set_target_properties(tb_loop_conv_bias_stream PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
add_test(NAME smesh_unit_loop_conv_bias_stream COMMAND tb_loop_conv_bias_stream)
set_tests_properties(smesh_unit_loop_conv_bias_stream PROPERTIES
  LABELS "unit;loop_conv"
  TIMEOUT 10
)
target_link_libraries(tb_loop_conv_ld_bias PRIVATE smesh_model cascade -lz -ltermcap -lpthread)
set_target_properties(tb_loop_conv_ld_bias PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
add_test(NAME smesh_unit_loop_conv_ld_bias COMMAND tb_loop_conv_ld_bias)
set_tests_properties(smesh_unit_loop_conv_ld_bias PROPERTIES
  LABELS "unit;loop_conv"
  TIMEOUT 10
)
target_link_libraries(tb_loop_conv_scheduler PRIVATE smesh_model cascade -lz -ltermcap -lpthread)
set_target_properties(tb_loop_conv_scheduler PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
add_test(NAME smesh_unit_loop_conv_scheduler COMMAND tb_loop_conv_scheduler)
set_tests_properties(smesh_unit_loop_conv_scheduler PROPERTIES
  LABELS "unit;loop_conv"
  TIMEOUT 10
)
target_link_libraries(tb_loop_conv_state PRIVATE smesh_model cascade -lz -ltermcap -lpthread)
set_target_properties(tb_loop_conv_state PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
add_test(NAME smesh_unit_loop_conv_state COMMAND tb_loop_conv_state)
set_tests_properties(smesh_unit_loop_conv_state PROPERTIES
  LABELS "unit;loop_conv"
  TIMEOUT 10
)
target_link_libraries(tb_loop_conv_cmd_queue PRIVATE smesh_model cascade -lz -ltermcap -lpthread)
set_target_properties(tb_loop_conv_cmd_queue PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
add_test(NAME smesh_unit_loop_conv_cmd_queue COMMAND tb_loop_conv_cmd_queue)
set_tests_properties(smesh_unit_loop_conv_cmd_queue PROPERTIES
  LABELS "unit;loop_conv"
  TIMEOUT 10
)

add_executable(tb_loop_conv_routing tb_loop_conv_routing.cpp)
add_executable(tb_loop_conv_cmd_dec tb_loop_conv_cmd_dec.cpp)
target_link_libraries(tb_loop_conv_cmd_dec PRIVATE smesh_model cascade -lz -ltermcap -lpthread)
set_target_properties(tb_loop_conv_cmd_dec PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
add_test(NAME smesh_unit_loop_conv_cmd_dec COMMAND tb_loop_conv_cmd_dec)
set_tests_properties(smesh_unit_loop_conv_cmd_dec PROPERTIES
  LABELS "unit;loop_conv"
  TIMEOUT 10
)
target_link_libraries(tb_loop_conv_routing PRIVATE smesh_model cascade -lz -ltermcap -lpthread)
set_target_properties(tb_loop_conv_routing PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)
add_test(NAME smesh_unit_loop_conv_routing COMMAND tb_loop_conv_routing)
set_tests_properties(smesh_unit_loop_conv_routing PROPERTIES
  LABELS "unit;loop_conv"
  TIMEOUT 10
)
