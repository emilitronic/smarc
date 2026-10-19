add_executable(tb_smesh_perf
  tb_smesh_perf.cpp
  ../../integration/rs_to_mem/tb_rs_to_mem_perf.cpp)
target_include_directories(tb_smesh_perf PRIVATE ../../integration/rs_to_mem)
target_link_libraries(tb_smesh_perf PRIVATE smesh_test_perf)
set_target_properties(tb_smesh_perf PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
add_test(NAME smesh_unit_perf COMMAND tb_smesh_perf)
set_tests_properties(smesh_unit_perf PROPERTIES LABELS "unit;perf" TIMEOUT 10)
