add_executable(tb_dma_write_queues tb_dma_write_queues.cpp)
target_link_libraries(tb_dma_write_queues PRIVATE smesh_model cascade -lz -ltermcap -lpthread)
set_target_properties(tb_dma_write_queues PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
add_test(NAME smesh_unit_dma_write_queues COMMAND tb_dma_write_queues)
set_tests_properties(smesh_unit_dma_write_queues PROPERTIES LABELS "unit;dma" TIMEOUT 10)

add_executable(tb_dma_writer_beat4 tb_dma_writer_beat4.cpp)
target_link_libraries(tb_dma_writer_beat4 PRIVATE
  smesh_model_beat4 cascade -lz -ltermcap -lpthread)
set_target_properties(tb_dma_writer_beat4 PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
add_test(NAME smesh_unit_dma_writer_beat4 COMMAND tb_dma_writer_beat4)
set_tests_properties(smesh_unit_dma_writer_beat4 PROPERTIES
  LABELS "unit;dma;beat4" TIMEOUT 10)

foreach(width IN ITEMS 8 4)
  set(target tb_dma_reader_stream_beat${width})
  add_executable(${target} tb_dma_reader_stream.cpp)
  if(width EQUAL 4)
    target_link_libraries(${target} PRIVATE
      smesh_model_beat4 cascade -lz -ltermcap -lpthread)
  else()
    target_link_libraries(${target} PRIVATE
      smesh_model cascade -lz -ltermcap -lpthread)
  endif()
  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
  add_test(NAME smesh_unit_dma_reader_stream_beat${width} COMMAND ${target})
  set_tests_properties(smesh_unit_dma_reader_stream_beat${width} PROPERTIES
    LABELS "unit;dma;beat${width}" TIMEOUT 10)
endforeach()

foreach(variant IN ITEMS default beat4 dim8)
  set(target tb_dma_writer_pool_${variant})
  add_executable(${target} tb_dma_writer_pool.cpp)
  if(variant STREQUAL "default")
    set(model smesh_model)
  else()
    set(model smesh_model_${variant})
  endif()
  target_link_libraries(${target} PRIVATE ${model} cascade -lz -ltermcap -lpthread)
  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
  add_test(NAME smesh_unit_dma_writer_pool_${variant} COMMAND ${target})
  set_tests_properties(smesh_unit_dma_writer_pool_${variant} PROPERTIES
    LABELS "unit;dma;pooling;${variant}" TIMEOUT 10)
endforeach()
