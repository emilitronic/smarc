// **********************************************************************
// smile/progs/include/accel.h
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 21 2026
/*
Tiny bare-metal API for invoking Tile1 CUSTOM-0 accelerators from C.
- Exposes a generic helper: accel_custom0_r(funct3, rs1, rs2)
- Exposes a v1 convenience wrapper: accel_array_sum(base, len_words)
This matches the v1 accelerator contract: CUSTOM-0 opcode, funct3=verb,
rs1/rs2 as arguments, rd as the 32-bit return payload/status.

Generating all 0-7 funct3's now although we only support funct3=0 currently, 
to future-proof the encoding against toolchain support and to make it easy to 
add new verbs in the future without changing the encoding scheme.

Presently exec_custom0 in Tile1_exec.cpp only supports funct3=0 and will 
return ACCEL_E_UNSUPPORTED for other funct3 values (if rd!=x0 in the custom instr), 
but the helper below can be used by software to generate any of the 8 possible encodings 
if/when the accelerator implementation is expanded to support more verbs.
*/ 
#pragma once

#include <stdint.h>

// RISC-V custom instruction encoding (v1 contract): CUSTOM-0 opcode.
#define ACCEL_CUSTOM0_OPCODE 0x0bu

// v1 verb ids (funct3 field)
#define ACCEL0_ARRAY_SUM_U32 0u

// Generic CUSTOM-0 R-type helper:
// rd = custom0(funct3, rs1, rs2), with funct7 fixed to 0 for v1.
static inline uint32_t accel_custom0_r(uint32_t funct3, uint32_t rs1, uint32_t rs2) {
  uint32_t       rd = 0u;          // return value from accel (written to rd by the custom instr)
  const uint32_t f3 = funct3 & 7u; // only 3 bits of funct3 are valid in the encoding

#if !defined(ACCEL_NO_DOT_INSN) // if toolchain supports .insn w/ custom opcodes, use it to avoid hardcoding encodings
  switch (f3) {
    //  don't delete this instr( opcode|f3|f7| rd|rs1|rs2| : copy o/p to C var rd : put C vars rs1/rs2 into right regs
    //  : assume memory might be read/written by this instr
    case 0u: asm volatile(".insn r 0x0b, 0, 0, %0, %1, %2" : "=r"(rd) : "r"(rs1), "r"(rs2) : "memory"); break;
    case 1u: asm volatile(".insn r 0x0b, 1, 0, %0, %1, %2" : "=r"(rd) : "r"(rs1), "r"(rs2) : "memory"); break;
    case 2u: asm volatile(".insn r 0x0b, 2, 0, %0, %1, %2" : "=r"(rd) : "r"(rs1), "r"(rs2) : "memory"); break;
    case 3u: asm volatile(".insn r 0x0b, 3, 0, %0, %1, %2" : "=r"(rd) : "r"(rs1), "r"(rs2) : "memory"); break;
    case 4u: asm volatile(".insn r 0x0b, 4, 0, %0, %1, %2" : "=r"(rd) : "r"(rs1), "r"(rs2) : "memory"); break;
    case 5u: asm volatile(".insn r 0x0b, 5, 0, %0, %1, %2" : "=r"(rd) : "r"(rs1), "r"(rs2) : "memory"); break;
    case 6u: asm volatile(".insn r 0x0b, 6, 0, %0, %1, %2" : "=r"(rd) : "r"(rs1), "r"(rs2) : "memory"); break;
    default: asm volatile(".insn r 0x0b, 7, 0, %0, %1, %2" : "=r"(rd) : "r"(rs1), "r"(rs2) : "memory"); break;
  }
#else
  // Fallback path using raw .word encoding with fixed registers:
  // rs1=a0 (x10), rs2=a1 (x11), rd=a2 (x12), funct7=0.
  register uint32_t r_rs1 asm("a0") = rs1;
  register uint32_t r_rs2 asm("a1") = rs2;
  register uint32_t r_rd asm("a2");
  switch (f3) {
    case 0u: asm volatile(".word 0x00B5060B" : "=r"(r_rd) : "r"(r_rs1), "r"(r_rs2) : "memory"); break;
    case 1u: asm volatile(".word 0x00B5160B" : "=r"(r_rd) : "r"(r_rs1), "r"(r_rs2) : "memory"); break;
    case 2u: asm volatile(".word 0x00B5260B" : "=r"(r_rd) : "r"(r_rs1), "r"(r_rs2) : "memory"); break;
    case 3u: asm volatile(".word 0x00B5360B" : "=r"(r_rd) : "r"(r_rs1), "r"(r_rs2) : "memory"); break;
    case 4u: asm volatile(".word 0x00B5460B" : "=r"(r_rd) : "r"(r_rs1), "r"(r_rs2) : "memory"); break;
    case 5u: asm volatile(".word 0x00B5560B" : "=r"(r_rd) : "r"(r_rs1), "r"(r_rs2) : "memory"); break;
    case 6u: asm volatile(".word 0x00B5660B" : "=r"(r_rd) : "r"(r_rs1), "r"(r_rs2) : "memory"); break;
    default: asm volatile(".word 0x00B5760B" : "=r"(r_rd) : "r"(r_rs1), "r"(r_rs2) : "memory"); break;
  }
  rd = r_rd;
#endif

  return rd;
}

// Convenience wrapper for v1 array-sum verb (funct3 = 0).
static inline uint32_t accel_array_sum(uint32_t base, uint32_t len_words) {
  return accel_custom0_r(ACCEL0_ARRAY_SUM_U32, base, len_words);
}
