add_executable(tb_smesh_top_load
  tb_smesh_top_load.cpp
)

add_executable(tb_smesh_top_acc_load
  tb_smesh_top_acc_load.cpp
)

add_executable(tb_smesh_top_spad_store
  tb_smesh_top_spad_store.cpp
)

foreach(target tb_smesh_top_load tb_smesh_top_acc_load tb_smesh_top_spad_store)
  target_link_libraries(${target}
    PRIVATE
      smesh_model
      smem_memory
      cascade
      -lz
      -ltermcap
      -lpthread
  )
  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
  )
endforeach()

add_test(NAME smesh_top_load COMMAND tb_smesh_top_load)
add_test(NAME smesh_top_acc_load COMMAND tb_smesh_top_acc_load)
add_test(NAME smesh_top_spad_store COMMAND tb_smesh_top_spad_store)

set_tests_properties(
  smesh_top_load
  smesh_top_acc_load
  smesh_top_spad_store
  PROPERTIES
    LABELS "integration;top"
    TIMEOUT 30
)
