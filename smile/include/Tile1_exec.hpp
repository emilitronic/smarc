// **********************************************************************
// smile/include/Tile1_exec.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2025

// Per-instruction execution helpers for Tile1.

#pragma once

#include "Instruction.hpp"

class Tile1;

// RV32I base - R-type
void exec_add(Tile1& tile, const Instruction& instr);
void exec_sub(Tile1& tile, const Instruction& instr);
void exec_xor(Tile1& tile, const Instruction& instr);
void exec_or(Tile1& tile, const Instruction& instr);
void exec_and(Tile1& tile, const Instruction& instr);
void exec_slt(Tile1& tile, const Instruction& instr);
void exec_sltu(Tile1& tile, const Instruction& instr);
void exec_sll(Tile1& tile, const Instruction& instr);
void exec_srl(Tile1& tile, const Instruction& instr);
void exec_sra(Tile1& tile, const Instruction& instr);

// RV32I base - I-type
void exec_addi(Tile1& tile, const Instruction& instr);
void exec_slli(Tile1& tile, const Instruction& instr);
void exec_srli(Tile1& tile, const Instruction& instr);
void exec_srai(Tile1& tile, const Instruction& instr);
void exec_slti(Tile1& tile, const Instruction& instr);
void exec_sltiu(Tile1& tile, const Instruction& instr);
void exec_xori(Tile1& tile, const Instruction& instr);
void exec_ori(Tile1& tile, const Instruction& instr);
void exec_andi(Tile1& tile, const Instruction& instr);
void exec_lb(Tile1& tile, const Instruction& instr);
void exec_lh(Tile1& tile, const Instruction& instr);
void exec_lbu(Tile1& tile, const Instruction& instr);
void exec_lhu(Tile1& tile, const Instruction& instr);

// RV32I base - S-type
void exec_sb(Tile1& tile, const Instruction& instr);
void exec_sh(Tile1& tile, const Instruction& instr);

// RV32I base - U-type
void exec_lui(Tile1& tile, const Instruction& instr);
void exec_auipc(Tile1& tile, const Instruction& instr, uint32_t curr_pc);

// RV32I base - B-type
bool exec_beq(Tile1& tile, const Instruction& instr);
bool exec_bne(Tile1& tile, const Instruction& instr);
bool exec_blt(Tile1& tile, const Instruction& instr);
bool exec_bge(Tile1& tile, const Instruction& instr);
bool exec_bltu(Tile1& tile, const Instruction& instr);
bool exec_bgeu(Tile1& tile, const Instruction& instr);

// RV32I base - J-type
uint32_t exec_jal(Tile1& tile, const Instruction& instr, uint32_t curr_pc);
uint32_t exec_jalr(Tile1& tile, const Instruction& instr, uint32_t curr_pc);

// System / trap / CSR
void exec_ecall(Tile1& tile, const Instruction& instr);
void exec_ebreak(Tile1& tile, const Instruction& instr);
void exec_uret(Tile1& tile, const Instruction& instr);
void exec_sret(Tile1& tile, const Instruction& instr);
void exec_mret(Tile1& tile, const Instruction& instr);
void exec_csrrw(Tile1& tile, const Instruction& instr);
void exec_csrrs(Tile1& tile, const Instruction& instr);
void exec_csrrc(Tile1& tile, const Instruction& instr);
void exec_csrrwi(Tile1& tile, const Instruction& instr);
void exec_csrrsi(Tile1& tile, const Instruction& instr);
void exec_csrrci(Tile1& tile, const Instruction& instr);

// Fence
void exec_fence(Tile1& tile, const Instruction& instr);
void exec_fence_i(Tile1& tile, const Instruction& instr);

// M extension
void exec_mul(Tile1& tile, const Instruction& instr);
void exec_mulw(Tile1& tile, const Instruction& instr);
void exec_mulh(Tile1& tile, const Instruction& instr);
void exec_mulhu(Tile1& tile, const Instruction& instr);
void exec_mulhsu(Tile1& tile, const Instruction& instr);
void exec_div(Tile1& tile, const Instruction& instr);
void exec_divu(Tile1& tile, const Instruction& instr);
void exec_rem(Tile1& tile, const Instruction& instr);
void exec_remu(Tile1& tile, const Instruction& instr);

// Custom extension hooks
void exec_custom0(Tile1& tile, const Instruction& instr);
void exec_custom1(Tile1& tile, const Instruction& instr); // optional, future
