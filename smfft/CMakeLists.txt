add_library(smfft_model
  src/SdfDelayLine.cpp
)

target_include_directories(smfft_model
  PUBLIC
    include
)

target_link_libraries(smfft_model
  PUBLIC
    cascade
)

add_executable(tb_sdf_delay_line
  tests/tb_sdf_delay_line.cpp
)

target_link_libraries(tb_sdf_delay_line
  PRIVATE
    smfft_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

set_target_properties(tb_sdf_delay_line PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smfft"
)

add_test(NAME smfft_delay_line COMMAND tb_sdf_delay_line)

set_tests_properties(smfft_delay_line PROPERTIES
  LABELS "unit;smfft"
  TIMEOUT 10
)
