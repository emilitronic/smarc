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

  // ******************
  // 1. FETCH
  // ******************
  const uint32_t curr_pc = pc_;
  const uint32_t instr   = mem_port_->read32(curr_pc); // load current instr
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
  switch (decoded.category) { // determine what kind of instruction you're dealing with
    // ALU
    case Instruction::Category::ALU:
      if (decoded.type == Instruction::Type::I) {
        if (decoded.opcode == 0x13) {
          if (decoded.funct3 == 0x1) {
            exec_slli(*this, decoded);
          } else if (decoded.funct3 == 0x2) {
            exec_slti(*this, decoded);
          } else if (decoded.funct3 == 0x3) {
            exec_sltiu(*this, decoded);
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
        exec_add(*this, decoded);
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
        switch (decoded.i.imm) {
          case 0x000: exec_ecall(*this, decoded); break;
          case 0x001: exec_ebreak(*this, decoded); break;
          case 0x002: exec_uret(*this, decoded); break;
          case 0x102: exec_sret(*this, decoded); break;
          case 0x302: exec_mret(*this, decoded); break;
          default: break;
        }
        advance_pc = false;
      }
      break;
    // MEMORY
    case Instruction::Category::LOAD:
      if (decoded.type == Instruction::Type::I) {
        exec_lw(*this, decoded);
      }
      break;
    case Instruction::Category::STORE:
      if (decoded.type == Instruction::Type::S) {
        exec_sw(*this, decoded);
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
        bool taken = false;
        switch (decoded.funct3) {
          case 0x0: taken = exec_beq(*this, decoded); break; // BEQ
          case 0x1: taken = exec_bne(*this, decoded); break; // BNE
          case 0x4: taken = exec_blt(*this, decoded); break; // BLT
          default: break;
        }
        if (taken) {
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
  halted_              = false;
  exited_              = false;
  exit_code_           = 0;
  trap_pending_        = false;
  pc_override_pending_ = false;
  priv_mode_           = PrivMode::Machine; // init priv_mode_ to M
  reset_trap_csrs();
  csrs_.clear();
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
