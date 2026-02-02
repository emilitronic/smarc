// **********************************************************************
// smile/include/Tile1_exec.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2025

// Per-instruction execution helpers for Tile1.

#pragma once

#include "Instruction.hpp"

class Tile1;

void exec_addi(Tile1& tile, const Instruction& instr);
void exec_add(Tile1& tile, const Instruction& instr);
void exec_slli(Tile1& tile, const Instruction& instr);
void exec_srli(Tile1& tile, const Instruction& instr);
void exec_srai(Tile1& tile, const Instruction& instr);
void exec_slti(Tile1& tile, const Instruction& instr);
void exec_sltiu(Tile1& tile, const Instruction& instr);
void exec_xori(Tile1& tile, const Instruction& instr);
void exec_ori(Tile1& tile, const Instruction& instr);
void exec_andi(Tile1& tile, const Instruction& instr);
void exec_sub(Tile1& tile, const Instruction& instr);
void exec_xor(Tile1& tile, const Instruction& instr);
void exec_or(Tile1& tile, const Instruction& instr);
void exec_and(Tile1& tile, const Instruction& instr);
void exec_ecall(Tile1& tile, const Instruction& instr);
void exec_ebreak(Tile1& tile, const Instruction& instr);
void exec_uret(Tile1& tile, const Instruction& instr);
void exec_sret(Tile1& tile, const Instruction& instr);
void exec_mret(Tile1& tile, const Instruction& instr);
void exec_lw(Tile1& tile, const Instruction& instr);
void exec_sw(Tile1& tile, const Instruction& instr);
bool exec_beq(Tile1& tile, const Instruction& instr);
bool exec_bne(Tile1& tile, const Instruction& instr);
bool exec_blt(Tile1& tile, const Instruction& instr);
void exec_lui(Tile1& tile, const Instruction& instr);
void exec_auipc(Tile1& tile, const Instruction& instr, uint32_t curr_pc);
uint32_t exec_jal(Tile1& tile, const Instruction& instr, uint32_t curr_pc);
uint32_t exec_jalr(Tile1& tile, const Instruction& instr, uint32_t curr_pc);
void exec_csrrw(Tile1& tile, const Instruction& instr);
void exec_csrrs(Tile1& tile, const Instruction& instr);
void exec_csrrc(Tile1& tile, const Instruction& instr);
void exec_csrrwi(Tile1& tile, const Instruction& instr);
void exec_csrrsi(Tile1& tile, const Instruction& instr);
void exec_csrrci(Tile1& tile, const Instruction& instr);
// New: accelerator hooks
void exec_custom0(Tile1& tile, const Instruction& instr);
void exec_custom1(Tile1& tile, const Instruction& instr); // optional, future
