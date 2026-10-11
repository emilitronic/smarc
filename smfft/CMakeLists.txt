add_library(smfft_model
  src/SdfDelayLine.cpp
  src/SdfStage.cpp
  src/SdfChain.cpp
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

# Pure arithmetic test: no Cascade component, no simulation.
add_executable(tb_sdf_twiddle
  tests/tb_sdf_twiddle.cpp
)

target_link_libraries(tb_sdf_twiddle
  PRIVATE
    smfft_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

target_compile_definitions(tb_sdf_twiddle
  PRIVATE
    SMFFT_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors"
)

set_target_properties(tb_sdf_twiddle PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smfft"
)

add_test(NAME smfft_twiddle COMMAND tb_sdf_twiddle)

set_tests_properties(smfft_twiddle PROPERTIES
  LABELS "unit;smfft"
  TIMEOUT 10
)

add_executable(tb_sdf_multiply
  tests/tb_sdf_multiply.cpp
)

target_link_libraries(tb_sdf_multiply
  PRIVATE
    smfft_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

set_target_properties(tb_sdf_multiply PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smfft"
)

add_test(NAME smfft_multiply COMMAND tb_sdf_multiply)

set_tests_properties(smfft_multiply PROPERTIES
  LABELS "unit;smfft"
  TIMEOUT 10
)

add_executable(tb_sdf_stage
  tests/tb_sdf_stage.cpp
)

target_link_libraries(tb_sdf_stage
  PRIVATE
    smfft_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

set_target_properties(tb_sdf_stage PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smfft"
)

add_test(NAME smfft_stage COMMAND tb_sdf_stage)

set_tests_properties(smfft_stage PROPERTIES
  LABELS "unit;smfft"
  TIMEOUT 10
)

add_executable(tb_sdf_chain
  tests/tb_sdf_chain.cpp
)

target_link_libraries(tb_sdf_chain
  PRIVATE
    smfft_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

target_compile_definitions(tb_sdf_chain
  PRIVATE
    SMFFT_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors"
)

set_target_properties(tb_sdf_chain PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smfft"
)

add_test(NAME smfft_chain COMMAND tb_sdf_chain)

set_tests_properties(smfft_chain PROPERTIES
  LABELS "smfft;vectors"
  TIMEOUT 300
)
