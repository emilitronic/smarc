# The same command requests two pixels in both builds. The disabled build must
# fall back to one; the enabled build must preserve two through the load path.
foreach(mode IN ITEMS enabled disabled)
  set(target tb_loop_conv_pixel_config_${mode})
  add_executable(${target} tb_loop_conv_pixel_config.cpp)
  if(mode STREQUAL "enabled")
    target_link_libraries(${target} PRIVATE smesh_model_first_layer)
    target_compile_definitions(${target} PRIVATE SMESH_TEST_EXPECT_FIRST_LAYER=1)
  else()
    target_link_libraries(${target} PRIVATE smesh_model)
    target_compile_definitions(${target} PRIVATE SMESH_TEST_EXPECT_FIRST_LAYER=0)
  endif()
  target_link_libraries(${target} PRIVATE cascade -lz -ltermcap -lpthread)
  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
  add_test(NAME smesh_loop_conv_pixel_config_${mode} COMMAND ${target})
  set_tests_properties(smesh_loop_conv_pixel_config_${mode} PROPERTIES
    LABELS "integration;loop_conv;ld_ctrl;first_layer" TIMEOUT 10)
endforeach()
