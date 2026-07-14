add_library(smesh_model
  src/AccScaleUnit.cpp
  src/Accum.cpp
  src/DmaIssueQueues.cpp
  src/DmaReadCompletionMux.cpp
  src/DmaReader.cpp
  src/DmaWriter.cpp
  src/ExCtrl.cpp
  src/LdCtrl.cpp
  src/MvinLocalRouter.cpp
  src/MvinPixelRepeater.cpp
  src/MvinScale.cpp
  src/Normalizer.cpp
  src/SmeshCmdQueues.cpp
  src/SmeshDevice.cpp
  src/SmeshRS.cpp
  src/SmeshTop.cpp
  src/Spad.cpp
  src/SpadReadPipes.cpp
  src/SpadWriter.cpp
  src/StCtrl.cpp
  src/StIssueCtrl.cpp
  src/StNormCtrl.cpp
  src/StReadCtrl.cpp
  src/StScaleCtrl.cpp
)

target_include_directories(smesh_model
  PUBLIC
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../smem/include
)

target_link_libraries(smesh_model
  PUBLIC
    cascade
)

add_executable(tb_smesh_m0
  src/tb_smesh_m0.cpp
)

target_link_libraries(tb_smesh_m0
  PRIVATE
    smesh_model
)

add_executable(tb_smesh_m1
  src/tb_smesh_m1.cpp
)

target_link_libraries(tb_smesh_m1
  PRIVATE
    smesh_model
)

add_executable(tb_smesh_rs
  src/tb_smesh_rs.cpp
)

target_link_libraries(tb_smesh_rs
  PRIVATE
    smesh_model
)

add_executable(tb_ex_ctrl
  src/tb_ex_ctrl.cpp
)

target_link_libraries(tb_ex_ctrl
  PRIVATE
    smesh_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

add_executable(tb_rs_ld_issue
  src/tb_rs_ld_issue.cpp
)

target_link_libraries(tb_rs_ld_issue
  PRIVATE
    smesh_model
    smem_memory
    cascade
    -lz
    -ltermcap
    -lpthread
)

add_executable(tb_smesh_top_load
  src/tb_smesh_top_load.cpp
)

target_link_libraries(tb_smesh_top_load
  PRIVATE
    smesh_model
    smem_memory
    cascade
    -lz
    -ltermcap
    -lpthread
)

add_executable(tb_smesh_top_acc_load
  src/tb_smesh_top_acc_load.cpp
)

target_link_libraries(tb_smesh_top_acc_load
  PRIVATE
    smesh_model
    smem_memory
    cascade
    -lz
    -ltermcap
    -lpthread
)

add_executable(tb_smesh_top_spad_store
  src/tb_smesh_top_spad_store.cpp
)

target_link_libraries(tb_smesh_top_spad_store
  PRIVATE
    smesh_model
    smem_memory
    cascade
    -lz
    -ltermcap
    -lpthread
)

add_executable(tb_smesh_m2
  src/SmeshCommandDriver.cpp
  src/SmeshShell.cpp
  src/tb_smesh_m2.cpp
)

target_link_libraries(tb_smesh_m2
  PRIVATE
    smesh_model
    cascade
    -lz
    -ltermcap
    -lpthread
)

add_executable(tb_smesh_m3
  src/SmeshCommandDriver.cpp
  src/SmeshShell.cpp
  src/tb_smesh_m3.cpp
)

target_link_libraries(tb_smesh_m3
  PRIVATE
    smesh_model
    smem_memory
    cascade
    -lz
    -ltermcap
    -lpthread
)
