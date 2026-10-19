add_library(smesh_test_perf STATIC SmeshPerfCounters.cpp)
target_include_directories(smesh_test_perf PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(smesh_test_perf PUBLIC descore)
