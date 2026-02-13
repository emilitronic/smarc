// **********************************************************************
// smile/src/Tile1.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 6 2025
/*
How the tile processes instructions and talks to memory.
*/
#include "Tile1.hpp"
#include "Tile1_exec.hpp"
#include "AccelPort.hpp"
#include <cstdint>

Tile1::Tile1(std::string /*name*/, IMPL_CTOR) {
  regs_.fill(0);
  priv_mode_ = PrivMode::Machine;
  reset_trap_csrs();
}
// Connects external memory to tile
void Tile1::attach_memory(MemoryPort* mem) {
  mem_port_ = mem; // tile stores pointer (mem_port_) to memory port to fetch instr and read/write data
}                  // will allow us to access a memory port class's methods for mem read/write

// Tile's execution sequence, fetch/decode/etc.
void Tile1::tick() {

  // ******************
  // 0. Some checks
  // ******************
  if (halted_) return; // stop sim if ECALL previously halted the tile
  if (!mem_port_) {    // prevent sim from running w/o memory port
    last_pc_ = pc_;
    last_instr_ = 0;
    return;
  }

  mem_port_->cycle(); // advance mem model by CPU cycle (to simulate latency)

  // If we're waiting on an instr fetch response, stall until it arrives
  if (ifetch_wait_) {
    if (!mem_port_->resp_valid()) return;   // stall until mem has valid instr resp
    ifetch_word_  = mem_port_->resp_data(); // if it has valid resp: copy resp into ifetch buffer
    mem_port_->resp_consume();              // tell mem we've consumed response (it can now accept new requests)
    ifetch_valid_ = true;                   // mark the ifetch buffer valid
    ifetch_wait_  = false;                  // clear the fetch-wait flag
  }
  // If we're waiting on a data memory access, stall until it completes
  if (dmem_wait_) {
    if (!mem_port_->resp_valid()) return;  // stall until mem has valid data resp
    const uint32_t resp = mem_port_->resp_data();
    mem_port_->resp_consume();
    complete_dmem(resp);                   // finishes load/store op and advances PC
    return;
  }

  // ******************
  // 1. FETCH
  // ******************
  const uint32_t curr_pc = pc_;
  uint32_t instr = 0;
  if (mem_model_ == MemModel::Ideal) { // ideal mem…
    // Ideal mem is a functional sanity mode: synchronous read32/write32, no stalls.
    ifetch_wait_ = false;
    ifetch_valid_ = false;
    instr = mem_port_->read32(curr_pc);
  } else {                             // …or timed mem (default), sims realistic mem latency with req/resp and stalling
    // Timed mem is the cycle-accurate mode using request/resp.
    // If no buffered instruction is available, request one from memory.
    if (!ifetch_valid_) {
      if (!mem_port_->can_request()) return; // check can_request() before requesting to avoid overwriting pending requests
      mem_port_->request_read32(curr_pc);
      ifetch_wait_ = true;
      last_pc_ = curr_pc;
      last_instr_ = 0;
      return;                                // return right after request issue, so core consumes resp in next cycle
    }
    instr = ifetch_word_;
    ifetch_valid_ = false;
  }
  last_pc_    = curr_pc;
  last_instr_ = instr;
  trace("pc=0x%08x instr=0x%08x\n", curr_pc, instr);
  uint32_t next_pc    = curr_pc + 4u;
  bool     advance_pc = true;

  // ******************
  // 2. DECODE
  // ******************  
  Instruction decoded(instr); // construct a new Instruction object called decoded by passing in instr

  // ******************
  // 3. EXECUTE
  // ******************
  inst_count_++;
  switch (decoded.category) { // determine what kind of instruction you're dealing with
    // ALU
    case Instruction::Category::ALU:
      arith_count_++; // increment arithmetic (ALU category) count
      if (decoded.type == Instruction::Type::I) {
        if (decoded.opcode == 0x13) {
          if (decoded.funct3 == 0x1) {
            exec_slli(*this, decoded);
          } else if (decoded.funct3 == 0x2) {
            exec_slti(*this, decoded);
          } else if (decoded.funct3 == 0x3) {
            exec_sltiu(*this, decoded);
          } else if (decoded.funct3 == 0x4) {
            exec_xori(*this, decoded);
          } else if (decoded.funct3 == 0x6) {
            exec_ori(*this, decoded);
          } else if (decoded.funct3 == 0x7) {
            exec_andi(*this, decoded);
          } else if (decoded.funct3 == 0x5) {
            if (decoded.funct7 == 0x00) {
              exec_srli(*this, decoded);
            } else if (decoded.funct7 == 0x20) {
              exec_srai(*this, decoded);
            } else {
              exec_addi(*this, decoded);
            }
          } else {
            exec_addi(*this, decoded);
          }
        } else {
          exec_addi(*this, decoded);
        }
      } else if (decoded.type == Instruction::Type::R) {
        if (decoded.opcode == 0x33) {
          switch (decoded.funct3) {
            case 0x0:
              if (decoded.funct7 == 0x00) {
                add_count_++;
                exec_add(*this, decoded);
              } else if (decoded.funct7 == 0x20) {
                add_count_++; // count subs as adds
                exec_sub(*this, decoded);
              } else if (decoded.funct7 == 0x01) {
                mul_count_++;
                exec_mul(*this, decoded);
              } else {
                exec_add(*this, decoded);
              }
              break;
            case 0x1:
              if (decoded.funct7 == 0x00) {
                exec_sll(*this, decoded);
              } else if (decoded.funct7 == 0x01) {
                exec_mulh(*this, decoded);
              } else {
                exec_add(*this, decoded);
              }
              break;
            case 0x2:
              if (decoded.funct7 == 0x00) {
                exec_slt(*this, decoded);
              } else if (decoded.funct7 == 0x01) {
                exec_mulhsu(*this, decoded);
              } else {
                exec_add(*this, decoded);
              }
              break;
            case 0x3:
              if (decoded.funct7 == 0x00) {
                exec_sltu(*this, decoded);
              } else if (decoded.funct7 == 0x01) {
                exec_mulhu(*this, decoded);
              } else {
                exec_add(*this, decoded);
              }
              break;
            case 0x4:
              if (decoded.funct7 == 0x00) {
                exec_xor(*this, decoded);
              } else if (decoded.funct7 == 0x01) {
                exec_div(*this, decoded);
              } else {
                exec_add(*this, decoded);
              }
              break;
            case 0x5:
              if (decoded.funct7 == 0x00) {
                exec_srl(*this, decoded);
              } else if (decoded.funct7 == 0x20) {
                exec_sra(*this, decoded);
              } else if (decoded.funct7 == 0x01) {
                exec_divu(*this, decoded);
              } else {
                exec_add(*this, decoded);
              }
              break;
            case 0x6:
              if (decoded.funct7 == 0x00) {
                exec_or(*this, decoded);
              } else if (decoded.funct7 == 0x01) {
                exec_rem(*this, decoded);
              } else {
                exec_add(*this, decoded);
              }
              break;
            case 0x7:
              if (decoded.funct7 == 0x00) {
                exec_and(*this, decoded);
              } else if (decoded.funct7 == 0x01) {
                exec_remu(*this, decoded);
              } else {
                exec_add(*this, decoded);
              }
              break;
            default:
              exec_add(*this, decoded);
              break;
          }
        } else if (decoded.opcode == 0x3b) {
          if (decoded.funct3 == 0x0 && decoded.funct7 == 0x01) {
            exec_mulw(*this, decoded);
          } else {
            exec_add(*this, decoded);
          }
        } else {
          exec_add(*this, decoded);
        }
      } else if (decoded.type == Instruction::Type::U) {
        if (decoded.opcode == 0x37) {
          exec_lui(*this, decoded);
        } else if (decoded.opcode == 0x17) {
          exec_auipc(*this, decoded, curr_pc);
        }
      }
      break;
    // SYSTEM
    case Instruction::Category::SYSTEM:
      if (decoded.type == Instruction::Type::I) {
        if (decoded.opcode == 0x73) {
          switch (decoded.i.imm) {
            case 0x000: exec_ecall(*this, decoded); break;
            case 0x001: exec_ebreak(*this, decoded); break;
            case 0x002: exec_uret(*this, decoded); break;
            case 0x102: exec_sret(*this, decoded); break;
            case 0x302: exec_mret(*this, decoded); break;
            default: break;
          }
          advance_pc = false;
        } else if (decoded.opcode == 0x0f) {
          if (decoded.funct3 == 0x0) {
            exec_fence(*this, decoded);
          } else if (decoded.funct3 == 0x1) {
            exec_fence_i(*this, decoded);
          }
        }
      }
      break;
    // MEMORY
    case Instruction::Category::LOAD:
      if (decoded.type == Instruction::Type::I) {
        load_count_++;
        const auto& op = decoded.i;
        const int32_t base = static_cast<int32_t>(read_reg(op.rs1));
        const uint32_t addr = static_cast<uint32_t>(base + op.imm);
        // Ideal mem is a functional sanity mode: synchronous read32/write32, no stalls.
        if (mem_model_ == MemModel::Ideal) { // if ideal mem…
          const uint32_t word = mem_port_->read32(addr & ~0x3u);
          uint32_t value = 0;
          switch (decoded.funct3) {
            case 0x0: {
              const uint32_t shift = (addr & 0x3u) * 8u;
              const int8_t byte = static_cast<int8_t>((word >> shift) & 0xffu);
              value = static_cast<uint32_t>(byte);
              break;
            }
            case 0x1: {
              assert_always((addr & 0x1u) == 0u, "LH requires 2-byte alignment");
              const uint32_t shift = (addr & 0x2u) * 8u;
              const int16_t half = static_cast<int16_t>((word >> shift) & 0xffffu);
              value = static_cast<uint32_t>(half);
              break;
            }
            case 0x2:
              assert_always((addr & 0x3u) == 0u, "LW requires 4-byte alignment");
              value = word;
              break;
            case 0x4: {
              const uint32_t shift = (addr & 0x3u) * 8u;
              value = (word >> shift) & 0xffu;
              break;
            }
            case 0x5: {
              assert_always((addr & 0x1u) == 0u, "LHU requires 2-byte alignment");
              const uint32_t shift = (addr & 0x2u) * 8u;
              value = (word >> shift) & 0xffffu;
              break;
            }
            default:
              assert_always(false, "Unsupported load funct3 in ideal data path");
              break;
          }
          if (op.rd != 0) write_reg(op.rd, value);
        } else {                             // …or timed mem (default), sims realistic mem latency with req/resp and stalling
          // Timed mem is the cycle-accurate mode using request/resp.
          DmemOp dmem_op = DmemOp::None;
          switch (decoded.funct3) {
            case 0x0: dmem_op = DmemOp::LB; break;
            case 0x1:
              assert_always((addr & 0x1u) == 0u, "LH requires 2-byte alignment");
              dmem_op = DmemOp::LH;
              break;
            case 0x2:
              assert_always((addr & 0x3u) == 0u, "LW requires 4-byte alignment");
              dmem_op = DmemOp::LW;
              break;
            case 0x4: dmem_op = DmemOp::LBU; break;
            case 0x5:
              assert_always((addr & 0x1u) == 0u, "LHU requires 2-byte alignment");
              dmem_op = DmemOp::LHU;
              break;
            default:
              assert_always(false, "Unsupported load funct3 in timed data path");
              break;
          }
          if (!mem_port_->can_request()) return;   // before issue, check that we can make new request
          mem_port_->request_read32(addr & ~0x3u); // if we can, issue load request
          dmem_wait_ = true;                       // we're no waiting on data mem
          dmem_op_ = dmem_op;                      // load flavour
          dmem_rmw_write_issued_ = false;
          dmem_rd_ = op.rd;                        // which reg to write
          dmem_addr_ = addr;                       // bookkeeping/debug
          dmem_store_data_ = 0;
          dmem_store_mask_ = 0;
          dmem_store_shift_ = 0;
          dmem_next_pc_ = next_pc;
          return;                                  // jump out of Tile1::tick()
        }
      }
      break;
    case Instruction::Category::STORE:
      if (decoded.type == Instruction::Type::S) {
        store_count_++;
        const auto& op = decoded.s;
        const int32_t base = static_cast<int32_t>(read_reg(op.rs1));
        const uint32_t addr = static_cast<uint32_t>(base + op.imm);
        const uint32_t data = read_reg(op.rs2);
        const uint32_t aligned = addr & ~0x3u;
        // Ideal mem is a functional sanity mode: synchronous read32/write32, no stalls.
        if (mem_model_ == MemModel::Ideal) { // if ideal mem…
          switch (decoded.funct3) {
            case 0x0: {
              const uint32_t shift = (addr & 0x3u) * 8u;
              const uint32_t mask = 0xffu << shift;
              const uint32_t prior = mem_port_->read32(aligned);
              const uint32_t merged = (prior & ~mask) | ((data << shift) & mask);
              mem_port_->write32(aligned, merged);
              break;
            }
            case 0x1: {
              assert_always((addr & 0x1u) == 0u, "SH requires 2-byte alignment");
              const uint32_t shift = (addr & 0x2u) * 8u;
              const uint32_t mask = 0xffffu << shift;
              const uint32_t prior = mem_port_->read32(aligned);
              const uint32_t merged = (prior & ~mask) | ((data << shift) & mask);
              mem_port_->write32(aligned, merged);
              break;
            }
            case 0x2:
              assert_always((addr & 0x3u) == 0u, "SW requires 4-byte alignment");
              mem_port_->write32(aligned, data);
              break;
            default:
              assert_always(false, "Unsupported store funct3 in ideal data path");
              break;
          }
        } else {                             // …or timed mem (default), sims realistic mem latency with req/resp and stalling
          // Timed mem is the cycle-accurate mode using request/resp.
          if (!mem_port_->can_request()) return;

          dmem_wait_ = true;
          dmem_rd_ = 0;
          dmem_addr_ = addr;
          dmem_next_pc_ = next_pc;
          dmem_rmw_write_issued_ = false;

          switch (decoded.funct3) {
            case 0x0: {
              // Word-only memory port: SB is implemented as timed read-modify-write (2 requests).
              dmem_op_ = DmemOp::SB;
              dmem_store_data_ = data & 0xffu;
              dmem_store_shift_ = (addr & 0x3u) * 8u;
              dmem_store_mask_ = 0xffu << dmem_store_shift_;
              mem_port_->request_read32(aligned); // transaction 1: READ in what you want to modify
              return;                             // jump out of Tile1::tick()
            }
            case 0x1: {
              assert_always((addr & 0x1u) == 0u, "SH requires 2-byte alignment");
              // Word-only memory port: SH is implemented as timed read-modify-write (2 requests).
              // Transaction 1: READ request
              dmem_op_ = DmemOp::SH;
              dmem_store_data_ = data & 0xffffu;
              dmem_store_shift_ = (addr & 0x2u) * 8u;
              dmem_store_mask_ = 0xffffu << dmem_store_shift_;
              mem_port_->request_read32(aligned); // transaction 1: READ in what you want to modify
              return;                             // jump out of Tile1::tick()
            }
            case 0x2:
              assert_always((addr & 0x3u) == 0u, "SW requires 4-byte alignment");
              dmem_op_ = DmemOp::SW;
              dmem_store_data_ = data;
              dmem_store_shift_ = 0;
              dmem_store_mask_ = 0xffffffffu;
              mem_port_->request_write32(aligned, data); // WRITE in what you want to modify
              return;                                    // jump out of Tile1::tick()
            default:
              assert_always(false, "Unsupported store funct3 in timed data path");
              break;
          }
        }
      }
      break;
    // JUMP
    case Instruction::Category::JUMP:
      if (decoded.type == Instruction::Type::J) {
        next_pc = exec_jal(*this, decoded, curr_pc);
      } else if (decoded.type == Instruction::Type::I) {
        next_pc = exec_jalr(*this, decoded, curr_pc);
      }
      break;
    // CSR
    case Instruction::Category::CSR:
      if (decoded.type == Instruction::Type::CSR) {
        switch (decoded.funct3) {
          case 0x1: exec_csrrw(*this, decoded); break;
          case 0x2: exec_csrrs(*this, decoded); break;
          case 0x3: exec_csrrc(*this, decoded); break;
          default: break;
        }
      }
      break;
    case Instruction::Category::CSR_IMM:
      if (decoded.type == Instruction::Type::CSR) {
        switch (decoded.funct3) {
          case 0x5: exec_csrrwi(*this, decoded); break;
          case 0x6: exec_csrrsi(*this, decoded); break;
          case 0x7: exec_csrrci(*this, decoded); break;
          default: break;
        }
      }
      break;
    // BRANCH
    case Instruction::Category::BRANCH:
      if (decoded.type == Instruction::Type::B) {
        branch_count_++;
        bool taken = false;
        switch (decoded.funct3) {
          case 0x0: taken = exec_beq(*this, decoded); break; // BEQ
          case 0x1: taken = exec_bne(*this, decoded); break; // BNE
          case 0x4: taken = exec_blt(*this, decoded); break; // BLT
          case 0x5: taken = exec_bge(*this, decoded); break; // BGE
          case 0x6: taken = exec_bltu(*this, decoded); break; // BLTU
          case 0x7: taken = exec_bgeu(*this, decoded); break; // BGEU
          default: break;
        }
        if (taken) {
          branch_taken_count_++;
          const int32_t offset = decoded.b.imm;
          next_pc = static_cast<uint32_t>(static_cast<int32_t>(curr_pc) + offset);
        }
      }
      break;
    // CUSTOM
    case Instruction::Category::CUSTOM:
      exec_custom0(*this, decoded); // execute the custom instruction (Tile1_exec.cpp)
      break;
    default:
      break;
  }
  
  // ******************
  // 4. TRAP HANDLING
  // ******************  
  if (trap_pending_) {
    raise_trap(pending_trap_);
    return;
  }
  if (pc_override_pending_) { // let mret jump to mepc
    pc_override_pending_ = false;
    pc_ = pc_override_value_;
    regs_[0] = 0;
    return;
  }
  regs_[0] = 0; // Enforce x0 hard-wiring.
  pc_ = advance_pc ? next_pc : curr_pc;
}

void Tile1::reset() {
  pc_                  = 0;
  last_pc_             = 0;
  last_instr_          = 0;
  regs_.fill(0);
  regs_[0]             = 0;
  ifetch_wait_         = false;
  ifetch_valid_        = false;
  ifetch_word_         = 0;
  dmem_wait_           = false;
  dmem_op_             = DmemOp::None;
  dmem_rmw_write_issued_ = false;
  dmem_rd_             = 0;
  dmem_addr_           = 0;
  dmem_store_data_     = 0;
  dmem_store_mask_     = 0;
  dmem_store_shift_    = 0;
  dmem_next_pc_        = 0;
  halted_              = false;
  exited_              = false;
  exit_code_           = 0;
  inst_count_          = 0;
  arith_count_         = 0;
  add_count_           = 0;
  mul_count_           = 0;
  load_count_          = 0;
  store_count_         = 0;
  branch_count_        = 0;
  branch_taken_count_  = 0;
  trap_pending_        = false;
  pc_override_pending_ = false;
  priv_mode_           = PrivMode::Machine; // init priv_mode_ to M
  reset_trap_csrs();
  csrs_.clear();
}

// Helper for completing a data memory access after a stall: 
// updates RF for loads, clears dmem-related fields, and applies next PC
void Tile1::complete_dmem(uint32_t resp_data) {
  switch (dmem_op_) {
    case DmemOp::LW:
      if (dmem_rd_ != 0) write_reg(dmem_rd_, resp_data);
      break;
    case DmemOp::LB: {
      const uint32_t shift = (dmem_addr_ & 0x3u) * 8u;
      const int8_t byte = static_cast<int8_t>((resp_data >> shift) & 0xffu);
      if (dmem_rd_ != 0) write_reg(dmem_rd_, static_cast<uint32_t>(byte));
      break;
    }
    case DmemOp::LBU: {
      const uint32_t shift = (dmem_addr_ & 0x3u) * 8u;
      const uint32_t byte = (resp_data >> shift) & 0xffu;
      if (dmem_rd_ != 0) write_reg(dmem_rd_, byte);
      break;
    }
    case DmemOp::LH: {
      const uint32_t shift = (dmem_addr_ & 0x2u) * 8u;
      const int16_t half = static_cast<int16_t>((resp_data >> shift) & 0xffffu);
      if (dmem_rd_ != 0) write_reg(dmem_rd_, static_cast<uint32_t>(half));
      break;
    }
    case DmemOp::LHU: {
      const uint32_t shift = (dmem_addr_ & 0x2u) * 8u;
      const uint32_t half = (resp_data >> shift) & 0xffffu;
      if (dmem_rd_ != 0) write_reg(dmem_rd_, half);
      break;
    }
    case DmemOp::SW:
      break;
    // Transaction 2: WRITE request
    case DmemOp::SB:
    case DmemOp::SH:
      if (!dmem_rmw_write_issued_) {
        const uint32_t merged = (resp_data & ~dmem_store_mask_) |
          ((dmem_store_data_ << dmem_store_shift_) & dmem_store_mask_);
        assert_always(mem_port_->can_request(), "Timed SB/SH RMW write phase requires request slot");
        mem_port_->request_write32(dmem_addr_ & ~0x3u, merged); // transaction 2: WRITE in what you want to modify
        dmem_rmw_write_issued_ = true; // read-modify-write (RMW) for sub-word stores
        dmem_store_data_ = merged;
        return;                        // jump out of Tile1::tick() 
      }
      break;
    case DmemOp::None:
      assert_always(false, "complete_dmem called with no active dmem op");
      break;
  }

  pc_ = dmem_next_pc_;
  dmem_wait_ = false;
  dmem_op_ = DmemOp::None;
  dmem_rmw_write_issued_ = false;
  dmem_rd_ = 0;
  dmem_addr_ = 0;
  dmem_store_data_ = 0;
  dmem_store_mask_ = 0;
  dmem_store_shift_ = 0;
  dmem_next_pc_ = 0;
  regs_[0] = 0;
}

void Tile1::write_reg(uint32_t idx, uint32_t value) {
  if (idx == 0 || idx >= regs_.size()) return;
  regs_[idx] = value;
  trace("x%u <= 0x%x\n", idx, value);
}

void Tile1::halt() {
  halted_ = true;
  trace("halted\n");
}

void Tile1::set_pc(uint32_t pc) { // a way to set your PC
  pc_ = pc;
  pc_override_pending_ = false;
}

uint32_t Tile1::read_csr(uint32_t addr) const {
  switch (addr) {
    case CSR_MSTATUS: return trap_csrs_.mstatus;
    case CSR_MTVEC:   return trap_csrs_.mtvec;
    case CSR_MEPC:    return trap_csrs_.mepc;
    case CSR_MCAUSE:  return trap_csrs_.mcause;
    default: break;
  }
  auto it = csrs_.find(addr);
  return (it == csrs_.end()) ? 0u : it->second;
}

void Tile1::write_csr(uint32_t addr, uint32_t value) {
  bool handled = true;
  switch (addr) {
    case CSR_MSTATUS: trap_csrs_.mstatus = value; break;
    case CSR_MTVEC:   trap_csrs_.mtvec   = value; break;
    case CSR_MEPC:    trap_csrs_.mepc    = value; break;
    case CSR_MCAUSE:  trap_csrs_.mcause  = value; break;
    default:
      handled = false;
      break;
  }
  if (!handled) {
    csrs_[addr] = value;
  }
  trace("csr[0x%x] <= 0x%x\n", addr, value);
}

void Tile1::reset_trap_csrs() {
  trap_csrs_ = TrapCsrState{};
  pending_trap_ = TrapCause::EnvironmentCallFromUMode;
}
// latch cause of trap API
void Tile1::request_trap(TrapCause cause) {
  trap_pending_ = true;
  pending_trap_ = cause;
}

void Tile1::request_illegal_instruction() {
  request_trap(TrapCause::IllegalInstruction);
}
// trap entry API
void Tile1::raise_trap(TrapCause cause) {
  trap_pending_ = false;      // clears pending flag
  trap_csrs_.mepc = last_pc_; // records mepc (last_pc_ captures faulting instr addr)
  trap_csrs_.mcause = static_cast<uint32_t>(cause); // stamps mcause
  const PrivMode prev_mode = priv_mode_;  // get current privilege mode from HW
  uint32_t mstatus = trap_csrs_.mstatus;
  const uint32_t mie = mstatus & MSTATUS_MIE;
  mstatus = (mstatus & ~MSTATUS_MPIE) | (mie ? MSTATUS_MPIE : 0u); // MPIE <= old MIE
  mstatus &= ~MSTATUS_MIE;                                         // MIE cleared on entry
  mstatus = (mstatus & ~MSTATUS_MPP_MASK) | encode_mpp(prev_mode); // push previous privilege into MPP
  trap_csrs_.mstatus = mstatus;
  pc_override_pending_ = false;
  trace("trap: cause=%u mtvec=0x%x mepc=0x%x\n", static_cast<unsigned>(cause), trap_csrs_.mtvec, trap_csrs_.mepc); // trace event
  pc_ = trap_csrs_.mtvec;    // redirect pc to mtvec
  regs_[0] = 0; // keep enforcing the X0 invariant, since this takes us off typical Tile1::tick flow
  priv_mode_ = PrivMode::Machine;
}
// trap return API
void Tile1::resume_from_trap() {
  pc_override_pending_ = true;
  pc_override_value_ = trap_csrs_.mepc;
  uint32_t mstatus = trap_csrs_.mstatus;
  const uint32_t mpie = mstatus & MSTATUS_MPIE;
  mstatus &= ~MSTATUS_MIE;
  if (mpie) mstatus |= MSTATUS_MIE;       // MIE <= MPIE
  mstatus |= MSTATUS_MPIE;                // MPIE <= 1 per spec
  const PrivMode target_mode = decode_mpp(mstatus); // pop (restore) previous privilege from MSTATUS (I think)
  priv_mode_ = target_mode;
  mstatus = (mstatus & ~MSTATUS_MPP_MASK) | MSTATUS_MPP_USER; // spec: MPP cleared to U after mret
  trap_csrs_.mstatus = mstatus;
  trace("mret -> pc=0x%x\n", pc_override_value_);
}
