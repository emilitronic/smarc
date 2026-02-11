# smarc $ cmake -build build --target tb_tile1 -j
add_library(tile1
  src/Tile1.cpp
  src/Instruction.cpp
  src/Tile1_exec.cpp
  src/Diagnostics.cpp
)

target_include_directories(tile1
  PUBLIC
    include
)

target_link_libraries(tile1
  PUBLIC
    cascade
)

add_executable(tb_tile1
  src/AccelArraySum.cpp
  src/AccelDemoAdd.cpp
  src/Debugger.cpp
  src/MemCtrlTimedPort.cpp
  src/tb_tile1.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../smicro/src/Dram.cpp
)

target_include_directories(tb_tile1
  PRIVATE
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../smicro/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../smicro/src
)

target_link_libraries(tb_tile1
  PRIVATE
    tile1
    -lz
    -ltermcap
    -lpthread
)

target_sources(tile1
  PRIVATE
    src/util/FlatBinLoader.cpp
    src/util/FlatBinLoader.hpp
)
# ------- Bare metal program build for smile_progs target -------
# Look for riscv64-unknown-elf toolchain
find_program(SMILE_RISCV_GCC riscv64-unknown-elf-gcc)
find_program(SMILE_RISCV_OBJCOPY riscv64-unknown-elf-objcopy)

if (SMILE_RISCV_GCC AND SMILE_RISCV_OBJCOPY)
  set(SMILE_PROGS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/progs)
  set(SMILE_PROG_FLAGS
    -Os
    -march=rv32i_zicsr
    -mabi=ilp32
    -ffreestanding
    -nostdlib
    -nostartfiles
    -Wl,-T${SMILE_PROGS_DIR}/link_rv32.ld
    -Wl,-e,_start
    -Wl,--no-relax
  )

  # Helper function to add a program: compiles C source to ELF, 
  # then converts ELF to flat binary, and tracks the binary for the smile_progs target.
  function(add_smile_prog PROG_NAME PROG_SRC_REL)
    set(PROG_SRC ${SMILE_PROGS_DIR}/${PROG_SRC_REL})
    set(PROG_ELF ${SMILE_PROGS_DIR}/${PROG_NAME}.elf)
    set(PROG_BIN ${SMILE_PROGS_DIR}/${PROG_NAME}.bin)
    # prog.c -> prog.elf
    add_custom_command(
      OUTPUT ${PROG_ELF}
      COMMAND ${SMILE_RISCV_GCC} ${SMILE_PROG_FLAGS} ${PROG_SRC} -o ${PROG_ELF}
      DEPENDS ${PROG_SRC} ${SMILE_PROGS_DIR}/link_rv32.ld
      COMMENT "Building RV32 ELF ${PROG_NAME}.elf"
      VERBATIM
    )
    # prog.elf -> prog.bin
    add_custom_command(
      OUTPUT ${PROG_BIN}
      COMMAND ${SMILE_RISCV_OBJCOPY} -O binary ${PROG_ELF} ${PROG_BIN}
      DEPENDS ${PROG_ELF}
      COMMENT "Building flat binary ${PROG_NAME}.bin"
      VERBATIM
    )

    set_property(GLOBAL APPEND PROPERTY SMILE_PROG_BINS ${PROG_BIN})
  endfunction()

  add_smile_prog(smurf core/smurf.c)
  add_smile_prog(mem_stress core/mem_stress.c)
  add_smile_prog(hmm_step sci/hmm_step.c)

  get_property(SMILE_PROG_BINS GLOBAL PROPERTY SMILE_PROG_BINS)
  # custom target smile_progs that does bare metal program build
  # smarc $ cmake --build build --target smile_progs -j
  add_custom_target(smile_progs DEPENDS ${SMILE_PROG_BINS})
else()
  message(STATUS "riscv64-unknown-elf toolchain not found; skipping smile_progs target")
endif()
# ------- End of bare metal program build for smile_progs target -------
