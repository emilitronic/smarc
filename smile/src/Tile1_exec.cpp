// **********************************************************************
// smile/src/Tile1_exec.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2025
/*
How to execute RISC-V commands.
*/
 
#include "Tile1_exec.hpp"
#include "Tile1.hpp"
#include "AccelPort.hpp"
#include <cstdint>

// RV32I base - R-type
void exec_add(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.r; // alias for R-type decoded fields
  const uint32_t lhs = tile.read_reg(op.rs1);
  const uint32_t rhs = tile.read_reg(op.rs2);
  const uint32_t result = lhs + rhs;
  tile.write_reg(op.rd, result);
}

void exec_sub(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.r; // alias for R-type decoded fields
  const uint32_t lhs = tile.read_reg(op.rs1);
  const uint32_t rhs = tile.read_reg(op.rs2);
  const uint32_t result = lhs - rhs;
  tile.write_reg(op.rd, result);
}

void exec_xor(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.r; // alias for R-type decoded fields
  const uint32_t lhs = tile.read_reg(op.rs1);
  const uint32_t rhs = tile.read_reg(op.rs2);
  tile.write_reg(op.rd, lhs ^ rhs);
}

void exec_or(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.r; // alias for R-type decoded fields
  const uint32_t lhs = tile.read_reg(op.rs1);
  const uint32_t rhs = tile.read_reg(op.rs2);
  tile.write_reg(op.rd, lhs | rhs);
}

void exec_and(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.r; // alias for R-type decoded fields
  const uint32_t lhs = tile.read_reg(op.rs1);
  const uint32_t rhs = tile.read_reg(op.rs2);
  tile.write_reg(op.rd, lhs & rhs);
}

void exec_slt(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.r; // alias for R-type decoded fields
  const int32_t lhs = static_cast<int32_t>(tile.read_reg(op.rs1));
  const int32_t rhs = static_cast<int32_t>(tile.read_reg(op.rs2));
  tile.write_reg(op.rd, lhs < rhs ? 1u : 0u);
}

void exec_sltu(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.r; // alias for R-type decoded fields
  const uint32_t lhs = tile.read_reg(op.rs1);
  const uint32_t rhs = tile.read_reg(op.rs2);
  tile.write_reg(op.rd, lhs < rhs ? 1u : 0u);
}

void exec_sll(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.r; // alias for R-type decoded fields
  const uint32_t lhs = tile.read_reg(op.rs1);
  const uint32_t rhs = tile.read_reg(op.rs2);
  const uint32_t shamt = rhs & 0x1fu;
  tile.write_reg(op.rd, lhs << shamt);
}

void exec_srl(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.r; // alias for R-type decoded fields
  const uint32_t lhs = tile.read_reg(op.rs1);
  const uint32_t rhs = tile.read_reg(op.rs2);
  const uint32_t shamt = rhs & 0x1fu;
  tile.write_reg(op.rd, lhs >> shamt);
}

void exec_sra(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.r; // alias for R-type decoded fields
  const int32_t lhs = static_cast<int32_t>(tile.read_reg(op.rs1));
  const uint32_t rhs = tile.read_reg(op.rs2);
  const uint32_t shamt = rhs & 0x1fu;
  const int32_t result = lhs >> shamt;
  tile.write_reg(op.rd, static_cast<uint32_t>(result));
}

// RV32I base - I-type
void exec_addi(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.i; // alias for I-type decoded fields
  const uint32_t src = tile.read_reg(op.rs1);
  const uint32_t result = static_cast<uint32_t>(static_cast<int32_t>(src) + op.imm);
  tile.write_reg(op.rd, result);
}

void exec_slli(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.i; // alias for I-type decoded fields
  const uint32_t src = tile.read_reg(op.rs1);
  const uint32_t shamt = static_cast<uint32_t>(op.imm) & 0x1fu;
  const uint32_t result = src << shamt;
  tile.write_reg(op.rd, result);
}

void exec_srli(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.i; // alias for I-type decoded fields
  const uint32_t src = tile.read_reg(op.rs1);
  const uint32_t shamt = static_cast<uint32_t>(op.imm) & 0x1fu;
  const uint32_t result = src >> shamt;
  tile.write_reg(op.rd, result);
}

void exec_srai(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.i; // alias for I-type decoded fields
  const int32_t src = static_cast<int32_t>(tile.read_reg(op.rs1));
  const uint32_t shamt = static_cast<uint32_t>(op.imm) & 0x1fu;
  const int32_t result = src >> shamt;
  tile.write_reg(op.rd, static_cast<uint32_t>(result));
}

void exec_slti(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.i; // alias for I-type decoded fields
  const int32_t lhs = static_cast<int32_t>(tile.read_reg(op.rs1));
  const int32_t rhs = static_cast<int32_t>(op.imm);
  tile.write_reg(op.rd, lhs < rhs ? 1u : 0u);
}

void exec_sltiu(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.i; // alias for I-type decoded fields
  const uint32_t lhs = tile.read_reg(op.rs1);
  const uint32_t rhs = static_cast<uint32_t>(op.imm);
  tile.write_reg(op.rd, lhs < rhs ? 1u : 0u);
}

void exec_xori(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.i; // alias for I-type decoded fields
  const uint32_t src = tile.read_reg(op.rs1);
  const uint32_t imm = static_cast<uint32_t>(op.imm);
  tile.write_reg(op.rd, src ^ imm);
}

void exec_ori(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.i; // alias for I-type decoded fields
  const uint32_t src = tile.read_reg(op.rs1);
  const uint32_t imm = static_cast<uint32_t>(op.imm);
  tile.write_reg(op.rd, src | imm);
}

void exec_andi(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.i; // alias for I-type decoded fields
  const uint32_t src = tile.read_reg(op.rs1);
  const uint32_t imm = static_cast<uint32_t>(op.imm);
  tile.write_reg(op.rd, src & imm);
}

void exec_lw(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.i;  // alias for I-type decoded fields
  auto* mem = tile.memory(); // get a pointer to attached memory port
  if (!mem) return;
  const int32_t  base = static_cast<int32_t>(tile.read_reg(op.rs1)); // read rs1, cast to signed
  const uint32_t addr = static_cast<uint32_t>(base + op.imm);        // add imm to base for mem addr
  const uint32_t data = mem->read32(addr);                           // load from mem
  tile.write_reg(op.rd, data);                                       // write to reg
}

void exec_lb(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.i;
  auto* mem = tile.memory();
  if (!mem) return;
  const int32_t  base = static_cast<int32_t>(tile.read_reg(op.rs1));
  const uint32_t addr = static_cast<uint32_t>(base + op.imm);
  const uint32_t word = mem->read32(addr & ~0x3u);
  const uint32_t shift = (addr & 0x3u) * 8u;
  const int8_t   byte = static_cast<int8_t>((word >> shift) & 0xffu);
  tile.write_reg(op.rd, static_cast<uint32_t>(byte));
}

void exec_lh(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.i;
  auto* mem = tile.memory();
  if (!mem) return;
  const int32_t  base = static_cast<int32_t>(tile.read_reg(op.rs1));
  const uint32_t addr = static_cast<uint32_t>(base + op.imm);
  const uint32_t word = mem->read32(addr & ~0x3u);
  const uint32_t shift = (addr & 0x2u) * 8u;
  const int16_t  half = static_cast<int16_t>((word >> shift) & 0xffffu);
  tile.write_reg(op.rd, static_cast<uint32_t>(half));
}

void exec_lbu(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.i;
  auto* mem = tile.memory();
  if (!mem) return;
  const int32_t  base = static_cast<int32_t>(tile.read_reg(op.rs1));
  const uint32_t addr = static_cast<uint32_t>(base + op.imm);
  const uint32_t word = mem->read32(addr & ~0x3u);
  const uint32_t shift = (addr & 0x3u) * 8u;
  const uint32_t byte = (word >> shift) & 0xffu;
  tile.write_reg(op.rd, byte);
}

void exec_lhu(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.i;
  auto* mem = tile.memory();
  if (!mem) return;
  const int32_t  base = static_cast<int32_t>(tile.read_reg(op.rs1));
  const uint32_t addr = static_cast<uint32_t>(base + op.imm);
  const uint32_t word = mem->read32(addr & ~0x3u);
  const uint32_t shift = (addr & 0x2u) * 8u;
  const uint32_t half = (word >> shift) & 0xffffu;
  tile.write_reg(op.rd, half);
}

// RV32I base - S-type
void exec_sw(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.s; // alias for S-type decoded fields
  auto* mem = tile.memory();
  if (!mem) return;
  const int32_t  base = static_cast<int32_t>(tile.read_reg(op.rs1));
  const uint32_t addr = static_cast<uint32_t>(base + op.imm);
  const uint32_t data = tile.read_reg(op.rs2);                       // read data to store from reg
  mem->write32(addr, data);                                          // write to mem
}

void exec_sb(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.s;
  auto* mem = tile.memory();
  if (!mem) return;
  const int32_t  base = static_cast<int32_t>(tile.read_reg(op.rs1));
  const uint32_t addr = static_cast<uint32_t>(base + op.imm);
  const uint32_t data = tile.read_reg(op.rs2);
  const uint32_t aligned = addr & ~0x3u;
  const uint32_t shift = (addr & 0x3u) * 8u;
  const uint32_t mask = 0xffu << shift;
  const uint32_t word = mem->read32(aligned);
  const uint32_t merged = (word & ~mask) | ((data & 0xffu) << shift);
  // MemoryPort is word-only, so SB is a read-modify-write of the containing word.
  mem->write32(aligned, merged);
}

void exec_sh(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.s;
  auto* mem = tile.memory();
  if (!mem) return;
  const int32_t  base = static_cast<int32_t>(tile.read_reg(op.rs1));
  const uint32_t addr = static_cast<uint32_t>(base + op.imm);
  const uint32_t data = tile.read_reg(op.rs2);
  const uint32_t aligned = addr & ~0x3u;
  const uint32_t shift = (addr & 0x2u) * 8u;
  const uint32_t mask = 0xffffu << shift;
  const uint32_t word = mem->read32(aligned);
  const uint32_t merged = (word & ~mask) | ((data & 0xffffu) << shift);
  // MemoryPort is word-only, so SH is a read-modify-write of the containing word.
  mem->write32(aligned, merged);
}

// RV32I base - U-type
void exec_lui(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.u;
  tile.write_reg(op.rd, static_cast<uint32_t>(op.imm));
}

void exec_auipc(Tile1& tile, const Instruction& instr, uint32_t curr_pc) {
  const auto& op = instr.u;
  const int64_t sum = static_cast<int64_t>(curr_pc) + static_cast<int64_t>(op.imm);
  tile.write_reg(op.rd, static_cast<uint32_t>(sum));
}

// RV32I base - B-type
bool exec_beq(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.b;
  return tile.read_reg(op.rs1) == tile.read_reg(op.rs2);
}

bool exec_bne(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.b;
  return tile.read_reg(op.rs1) != tile.read_reg(op.rs2);
}

bool exec_blt(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.b;
  const int32_t lhs = static_cast<int32_t>(tile.read_reg(op.rs1));
  const int32_t rhs = static_cast<int32_t>(tile.read_reg(op.rs2));
  return lhs < rhs;
}

bool exec_bge(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.b;
  const int32_t lhs = static_cast<int32_t>(tile.read_reg(op.rs1));
  const int32_t rhs = static_cast<int32_t>(tile.read_reg(op.rs2));
  return lhs >= rhs;
}

bool exec_bltu(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.b;
  const uint32_t lhs = tile.read_reg(op.rs1);
  const uint32_t rhs = tile.read_reg(op.rs2);
  return lhs < rhs;
}

bool exec_bgeu(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.b;
  const uint32_t lhs = tile.read_reg(op.rs1);
  const uint32_t rhs = tile.read_reg(op.rs2);
  return lhs >= rhs;
}

// RV32I base - J-type
uint32_t exec_jal(Tile1& tile, const Instruction& instr, uint32_t curr_pc) {
  const auto& op = instr.j;
  tile.write_reg(op.rd, curr_pc + 4u);
  const int64_t sum = static_cast<int64_t>(curr_pc) + static_cast<int64_t>(op.imm);
  return static_cast<uint32_t>(sum);
}

uint32_t exec_jalr(Tile1& tile, const Instruction& instr, uint32_t curr_pc) {
  const auto& op = instr.i;
  const uint32_t base = tile.read_reg(op.rs1);
  const int64_t sum = static_cast<int64_t>(base) + static_cast<int64_t>(op.imm);
  const uint32_t target = static_cast<uint32_t>(sum) & ~1u;
  tile.write_reg(op.rd, curr_pc + 4u);
  return target;
}

// System / trap / CSR
void exec_ecall(Tile1& tile, const Instruction& /*instr*/) { // raise a trap
  const uint32_t syscall = tile.read_reg(17); // a7=x17 holds syscall id
  if (syscall == 93u) {                       // is a7 = 93 when we ecall then exit()
    const uint32_t code = tile.read_reg(10);  // a0=x10 holds exit code
    tile.request_exit(code);                  // set exit_code & flags: exited_ & halted_
    return;
  }                                           // if we don't have this a7 setting (not a special "exit" syscall)…
  Tile1::TrapCause cause = Tile1::TrapCause::EnvironmentCallFromMMode;
  switch (tile.priv_mode()) { // respect the active privilege mode
    case Tile1::PrivMode::User:        cause = Tile1::TrapCause::EnvironmentCallFromUMode; break;
    case Tile1::PrivMode::Supervisor:  cause = Tile1::TrapCause::EnvironmentCallFromSMode; break;
    case Tile1::PrivMode::Machine:     cause = Tile1::TrapCause::EnvironmentCallFromMMode; break;
  }
  tile.request_trap(cause);                   // …treat ecall as real trap, set trap_pending_ = true
}                                             // on next tick, Tile1.cpp enters trap pipeline (raise_trap)

void exec_ebreak(Tile1& tile, const Instruction& /*instr*/) {
  tile.request_trap(Tile1::TrapCause::Breakpoint);
}

void exec_uret(Tile1& tile, const Instruction& /*instr*/) {
  if (tile.priv_mode() != Tile1::PrivMode::User) { // validate active privilege mode before proceeding
    tile.request_illegal_instruction();            // fall back to illegal-instruction trap if invoked from wrong level
    return;
  }
  tile.resume_from_trap();
}

void exec_sret(Tile1& tile, const Instruction& /*instr*/) {
  if (tile.priv_mode() != Tile1::PrivMode::Supervisor) {
    tile.request_illegal_instruction();
    return;
  }
  tile.resume_from_trap();
}

void exec_mret(Tile1& tile, const Instruction& /*instr*/) {
  if (tile.priv_mode() != Tile1::PrivMode::Machine) {
    tile.request_illegal_instruction();
    return;
  }
  tile.resume_from_trap(); // reuse shared plumbing
}

void exec_csrrw(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.c;
  const uint32_t old = tile.read_csr(op.csr);
  if (op.rd != 0) tile.write_reg(op.rd, old);
  tile.write_csr(op.csr, tile.read_reg(op.rs1));
}

void exec_csrrs(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.c;
  const uint32_t old = tile.read_csr(op.csr);
  if (op.rd != 0) tile.write_reg(op.rd, old);
  if (op.rs1 != 0) {
    const uint32_t mask = tile.read_reg(op.rs1);
    tile.write_csr(op.csr, old | mask);
  }
}

void exec_csrrc(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.c;
  const uint32_t old = tile.read_csr(op.csr);
  if (op.rd != 0) tile.write_reg(op.rd, old);
  if (op.rs1 != 0) {
    const uint32_t mask = tile.read_reg(op.rs1);
    tile.write_csr(op.csr, old & ~mask);
  }
}

void exec_csrrwi(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.ci;
  const uint32_t old = tile.read_csr(op.csr);
  if (op.rd != 0) tile.write_reg(op.rd, old);
  tile.write_csr(op.csr, op.zimm);
}

void exec_csrrsi(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.ci;
  const uint32_t old = tile.read_csr(op.csr);
  if (op.rd != 0) tile.write_reg(op.rd, old);
  if (op.zimm != 0) {
    tile.write_csr(op.csr, old | op.zimm);
  }
}

void exec_csrrci(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.ci;
  const uint32_t old = tile.read_csr(op.csr);
  if (op.rd != 0) tile.write_reg(op.rd, old);
  if (op.zimm != 0) {
    tile.write_csr(op.csr, old & ~op.zimm);
  }
}

// Fence
void exec_fence(Tile1& /*tile*/, const Instruction& /*instr*/) {
  // No-op in this single-core, in-order Tile1 model.
}

void exec_fence_i(Tile1& /*tile*/, const Instruction& /*instr*/) {
  // No-op in this single-core, in-order Tile1 model.
}

// M extension
void exec_mul(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.r; // alias for R-type decoded fields
  const uint64_t lhs = static_cast<uint64_t>(tile.read_reg(op.rs1));
  const uint64_t rhs = static_cast<uint64_t>(tile.read_reg(op.rs2));
  tile.write_reg(op.rd, static_cast<uint32_t>(lhs * rhs));
}

void exec_mulw(Tile1& tile, const Instruction& instr) {
  // RV64 MULW semantics collapse to 32-bit low product on this RV32 core.
  exec_mul(tile, instr);
}

void exec_mulh(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.r; // alias for R-type decoded fields
  const int64_t lhs = static_cast<int64_t>(static_cast<int32_t>(tile.read_reg(op.rs1)));
  const int64_t rhs = static_cast<int64_t>(static_cast<int32_t>(tile.read_reg(op.rs2)));
  const int64_t prod = lhs * rhs;
  tile.write_reg(op.rd, static_cast<uint32_t>(static_cast<uint64_t>(prod) >> 32));
}

void exec_mulhu(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.r; // alias for R-type decoded fields
  const uint64_t lhs = static_cast<uint64_t>(tile.read_reg(op.rs1));
  const uint64_t rhs = static_cast<uint64_t>(tile.read_reg(op.rs2));
  const uint64_t prod = lhs * rhs;
  tile.write_reg(op.rd, static_cast<uint32_t>(prod >> 32));
}

void exec_mulhsu(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.r; // alias for R-type decoded fields
  const int64_t lhs = static_cast<int64_t>(static_cast<int32_t>(tile.read_reg(op.rs1)));
  const uint64_t rhs = static_cast<uint64_t>(tile.read_reg(op.rs2));
  const int64_t prod = lhs * static_cast<int64_t>(rhs);
  tile.write_reg(op.rd, static_cast<uint32_t>(static_cast<uint64_t>(prod) >> 32));
}

void exec_div(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.r; // alias for R-type decoded fields
  const int32_t lhs = static_cast<int32_t>(tile.read_reg(op.rs1));
  const int32_t rhs = static_cast<int32_t>(tile.read_reg(op.rs2));
  uint32_t result = 0;
  if (rhs == 0) {
    result = 0xffffffffu;
  } else if (lhs == static_cast<int32_t>(0x80000000u) && rhs == -1) {
    result = 0x80000000u;
  } else {
    result = static_cast<uint32_t>(lhs / rhs);
  }
  tile.write_reg(op.rd, result);
}

void exec_divu(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.r; // alias for R-type decoded fields
  const uint32_t lhs = tile.read_reg(op.rs1);
  const uint32_t rhs = tile.read_reg(op.rs2);
  uint32_t result = 0;
  if (rhs == 0u) {
    result = 0xffffffffu;
  } else {
    result = lhs / rhs;
  }
  tile.write_reg(op.rd, result);
}

void exec_rem(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.r; // alias for R-type decoded fields
  const int32_t lhs = static_cast<int32_t>(tile.read_reg(op.rs1));
  const int32_t rhs = static_cast<int32_t>(tile.read_reg(op.rs2));
  uint32_t result = 0;
  if (rhs == 0) {
    result = static_cast<uint32_t>(lhs);
  } else if (lhs == static_cast<int32_t>(0x80000000u) && rhs == -1) {
    result = 0u;
  } else {
    result = static_cast<uint32_t>(lhs % rhs);
  }
  tile.write_reg(op.rd, result);
}

void exec_remu(Tile1& tile, const Instruction& instr) {
  const auto& op = instr.r; // alias for R-type decoded fields
  const uint32_t lhs = tile.read_reg(op.rs1);
  const uint32_t rhs = tile.read_reg(op.rs2);
  uint32_t result = 0;
  if (rhs == 0u) {
    result = lhs;
  } else {
    result = lhs % rhs;
  }
  tile.write_reg(op.rd, result);
}

// Custom extension hooks
void exec_custom0(Tile1& tile, const Instruction& instr) {
  AccelPort* accel = tile.accelerator(); // call Tile1's accessor to get accel pointer accel_port_
  if (!accel) {
    tile.request_illegal_instruction();  // trap if nullptr
    return;
  }

  const auto& op = instr.r;                       // treat instr as R-type
  const uint32_t rs1_val = tile.read_reg(op.rs1); // read rs1
  const uint32_t rs2_val = tile.read_reg(op.rs2); // read rs2

  accel->issue(instr.raw, tile.pc(), rs1_val, rs2_val); // issue custom instruction to accel

  if (accel->has_response() && op.rd != 0) {      // if accel has response & rd != x0
    const uint32_t resp = accel->read_response(); // read rd result from accel
    tile.write_reg(op.rd, resp);
  }
}

void exec_custom1(Tile1& tile, const Instruction& /*instr*/) { // stubbed to trap for now
  tile.request_illegal_instruction();
}
