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

# The same cycle checks cover two grid sizes without changing the implementation.
foreach(variant IN ITEMS default dim8)
  if(variant STREQUAL "default")
    set(transposer_target tb_transposer)
    set(transposer_model smesh_model)
    set(transposer_test smesh_unit_transposer)
  else()
    set(transposer_target tb_transposer_dim8)
    set(transposer_model smesh_model_dim8)
    set(transposer_test smesh_unit_transposer_dim8)
  endif()
  add_executable(${transposer_target} tb_transposer.cpp)
  target_link_libraries(${transposer_target} PRIVATE
    ${transposer_model} cascade -lz -ltermcap -lpthread)
  set_target_properties(${transposer_target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
  add_test(NAME ${transposer_test} COMMAND ${transposer_target})
  set_tests_properties(${transposer_test} PROPERTIES LABELS "unit;mesh;transposer" TIMEOUT 10)
endforeach()
set_property(TEST smesh_unit_transposer_dim8 APPEND PROPERTY LABELS dim8)

foreach(variant IN ITEMS default dim8)
  if(variant STREQUAL "default")
    set(mesher_target tb_mesher_transpose)
    set(mesher_model smesh_model)
    set(mesher_test smesh_unit_mesher_transpose)
  else()
    set(mesher_target tb_mesher_transpose_dim8)
    set(mesher_model smesh_model_dim8)
    set(mesher_test smesh_unit_mesher_transpose_dim8)
  endif()
  add_executable(${mesher_target} tb_mesher_transpose.cpp)
  target_link_libraries(${mesher_target} PRIVATE
    ${mesher_model} cascade -lz -ltermcap -lpthread)
  set_target_properties(${mesher_target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/smesh")
  add_test(NAME ${mesher_test} COMMAND ${mesher_target})
  set_tests_properties(${mesher_test} PROPERTIES LABELS "unit;mesh;transposer" TIMEOUT 10)
endforeach()
set_property(TEST smesh_unit_mesher_transpose_dim8 APPEND PROPERTY LABELS dim8)
