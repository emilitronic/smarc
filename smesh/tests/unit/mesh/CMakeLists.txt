set(MESH_UNIT_TESTS
  tb_mesh_core
  tb_mesh_hull
  tb_mesh_hull_math
  tb_mesh_hull_matmul
  tb_mesh_hull_matmul_preload
)

foreach(test_target IN LISTS MESH_UNIT_TESTS)
  add_executable(${test_target}
    ${test_target}.cpp
  )

  target_link_libraries(${test_target}
    PRIVATE
      smesh_model
  )

  set_target_properties(${test_target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
  )
endforeach()

add_executable(tb_mesher
  tb_mesher.cpp
)

target_link_libraries(tb_mesher
  PRIVATE
    smesh_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

set_target_properties(tb_mesher PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh"
)

add_test(NAME smesh_unit_mesher COMMAND tb_mesher)
add_test(NAME smesh_unit_mesh_core COMMAND tb_mesh_core)
add_test(NAME smesh_unit_mesh_hull COMMAND tb_mesh_hull)
add_test(NAME smesh_unit_mesh_hull_math COMMAND tb_mesh_hull_math)
add_test(NAME smesh_unit_mesh_hull_matmul COMMAND tb_mesh_hull_matmul)
add_test(NAME smesh_unit_mesh_hull_matmul_preload COMMAND tb_mesh_hull_matmul_preload)

set_tests_properties(
  smesh_unit_mesher
  smesh_unit_mesh_core
  smesh_unit_mesh_hull
  smesh_unit_mesh_hull_math
  smesh_unit_mesh_hull_matmul
  smesh_unit_mesh_hull_matmul_preload
  PROPERTIES
    LABELS "unit;mesh"
    TIMEOUT 10
)
