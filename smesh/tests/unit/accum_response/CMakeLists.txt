add_executable(tb_normalizer_slots tb_normalizer_slots.cpp)
add_executable(tb_normalizer_sum tb_normalizer_sum.cpp)
add_executable(tb_normalizer_max tb_normalizer_max.cpp)
add_executable(tb_normalizer_mean tb_normalizer_mean.cpp)
add_executable(tb_normalizer_variance tb_normalizer_variance.cpp)
add_executable(tb_norm_variance_divide tb_norm_variance_divide.cpp)
add_executable(tb_normalizer_inv_stddev tb_normalizer_inv_stddev.cpp)
add_executable(tb_normalizer_sum_exp tb_normalizer_sum_exp.cpp)
add_executable(tb_normalizer_inv_sum_exp tb_normalizer_inv_sum_exp.cpp)
add_executable(tb_acc_scale_regs tb_acc_scale_regs.cpp)
add_executable(tb_acc_scale_lane tb_acc_scale_lane.cpp)

foreach(test_target tb_normalizer_slots tb_normalizer_sum tb_normalizer_max tb_normalizer_mean tb_normalizer_variance tb_norm_variance_divide tb_normalizer_inv_stddev tb_normalizer_sum_exp tb_normalizer_inv_sum_exp tb_acc_scale_regs tb_acc_scale_lane)
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
add_test(NAME smesh_unit_norm_variance_divide COMMAND tb_norm_variance_divide)
add_test(NAME smesh_unit_normalizer_inv_stddev COMMAND tb_normalizer_inv_stddev)
add_test(NAME smesh_unit_normalizer_sum_exp COMMAND tb_normalizer_sum_exp)
add_test(NAME smesh_unit_normalizer_inv_sum_exp COMMAND tb_normalizer_inv_sum_exp)
add_test(NAME smesh_unit_acc_scale_regs COMMAND tb_acc_scale_regs)
set_tests_properties(smesh_unit_normalizer_slots smesh_unit_normalizer_sum smesh_unit_normalizer_max smesh_unit_normalizer_mean smesh_unit_normalizer_variance smesh_unit_norm_variance_divide smesh_unit_normalizer_inv_stddev smesh_unit_normalizer_sum_exp smesh_unit_normalizer_inv_sum_exp smesh_unit_acc_scale_regs PROPERTIES
  LABELS "unit;accum_response"
  TIMEOUT 10
)

add_executable(tb_acc_scale_regs_dim8 tb_acc_scale_regs.cpp)
target_link_libraries(tb_acc_scale_regs_dim8 PRIVATE smesh_model_dim8 cascade -lz -ltermcap -lpthread)
set_target_properties(tb_acc_scale_regs_dim8 PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
add_test(NAME smesh_unit_acc_scale_regs_dim8 COMMAND tb_acc_scale_regs_dim8)
set_tests_properties(smesh_unit_acc_scale_regs_dim8 PROPERTIES LABELS "unit;accum_response;dim8" TIMEOUT 10)

add_test(NAME smesh_unit_acc_scale_lane COMMAND tb_acc_scale_lane)
set_tests_properties(smesh_unit_acc_scale_lane PROPERTIES LABELS "unit;accum_response" TIMEOUT 10)
add_executable(tb_acc_scale_lane_dim8 tb_acc_scale_lane.cpp)
target_link_libraries(tb_acc_scale_lane_dim8 PRIVATE smesh_model_dim8 cascade -lz -ltermcap -lpthread)
set_target_properties(tb_acc_scale_lane_dim8 PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
add_test(NAME smesh_unit_acc_scale_lane_dim8 COMMAND tb_acc_scale_lane_dim8)
set_tests_properties(smesh_unit_acc_scale_lane_dim8 PROPERTIES LABELS "unit;accum_response;dim8" TIMEOUT 10)

foreach(variant two_lanes two_lanes_dim8)
  add_executable(tb_acc_scale_${variant} tb_acc_scale_lane.cpp)
  target_compile_definitions(tb_acc_scale_${variant} PRIVATE SMESH_ACC_SCALE_TEST_LANES=2)
  if(variant STREQUAL "two_lanes_dim8")
    target_link_libraries(tb_acc_scale_${variant} PRIVATE smesh_model_dim8 cascade -lz -ltermcap -lpthread)
  else()
    target_link_libraries(tb_acc_scale_${variant} PRIVATE smesh_model cascade -lz -ltermcap -lpthread)
  endif()
  set_target_properties(tb_acc_scale_${variant} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
  add_test(NAME smesh_unit_acc_scale_${variant} COMMAND tb_acc_scale_${variant})
  set_tests_properties(smesh_unit_acc_scale_${variant} PROPERTIES LABELS "unit;accum_response" TIMEOUT 10)
endforeach()
set_tests_properties(smesh_unit_acc_scale_two_lanes_dim8 PROPERTIES LABELS "unit;accum_response;dim8")
