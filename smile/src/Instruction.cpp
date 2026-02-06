// **********************************************************************
// smile/src/Instruction.cpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2025
/* 
A RV instruction decoder.  Allows you to inspect instructions symbolically 
(e.g., decoded.category, decoded.r.rs1, decoded.i.imm, etc.) without need for bit-twiddling.
*/

#include "Instruction.hpp"

namespace {
inline int32_t sign_extend(uint32_t value, unsigned bits) {
  const unsigned shift = 32u - bits;
  return static_cast<int32_t>(value << shift) >> shift;
}
} // namespace

// Constructor written in member initializer syntax
Instruction::Instruction(uint32_t raw_instr) : raw(raw_instr) { // initializer list initializer raw with value of raw_instr
  opcode = raw         & 0x7fu; // 7b
  rd     = (raw >>  7) & 0x1fu; // 5b
  funct3 = (raw >> 12) & 0x07u; // 3b
  rs1    = (raw >> 15) & 0x1fu; // 5b
  rs2    = (raw >> 20) & 0x1fu; // 5b
  funct7 = (raw >> 25) & 0x7fu; // 7b

  type     = Type::Unknown;
  category = Category::Unknown;

  switch (opcode) {
    case 0x33: { // R-type ALU, 011_0011
      if ((funct3 == 0x0 && (funct7 == 0x00 || funct7 == 0x20)) || // ADD/SUB
          (funct3 == 0x0 && funct7 == 0x01) ||                     // MUL
          (funct3 == 0x1 && funct7 == 0x00) ||                     // SLL
          (funct3 == 0x1 && funct7 == 0x01) ||                     // MULH
          (funct3 == 0x2 && funct7 == 0x00) ||                     // SLT
          (funct3 == 0x2 && funct7 == 0x01) ||                     // MULHSU
          (funct3 == 0x3 && funct7 == 0x00) ||                     // SLTU
          (funct3 == 0x3 && funct7 == 0x01) ||                     // MULHU
          (funct3 == 0x4 && funct7 == 0x00) ||                     // XOR
          (funct3 == 0x5 && (funct7 == 0x00 || funct7 == 0x20)) || // SRL/SRA
          (funct3 == 0x6 && funct7 == 0x00) ||                     // OR
          (funct3 == 0x7 && funct7 == 0x00)) {                     // AND
        type     = Type::R;
        category = Category::ALU;
        r.rd  = rd;
        r.rs1 = rs1;
        r.rs2 = rs2;
      }
      break;
    }
    case 0x3B: { // R-type ALU-32 in RV64 (accept MULW for compatibility testing)
      if (funct3 == 0x0 && funct7 == 0x01) { // MULW
        type     = Type::R;
        category = Category::ALU;
        r.rd  = rd;
        r.rs1 = rs1;
        r.rs2 = rs2;
      }
      break;
    }
    case 0x13: { // I-type 001_0011 (ADDI, etc.)
      if (funct3 == 0x0) {
        type     = Type::I;
        category = Category::ALU;
        i.rd  = rd;
        i.rs1 = rs1;
        i.imm = sign_extend(raw >> 20, 12);
      } else if (funct3 == 0x1) { // SLLI
        type     = Type::I;
        category = Category::ALU;
        i.rd  = rd;
        i.rs1 = rs1;
        i.imm = static_cast<int32_t>((raw >> 20) & 0x1f);
      } else if (funct3 == 0x2 || funct3 == 0x3) { // SLTI/SLTIU
        type     = Type::I;
        category = Category::ALU;
        i.rd  = rd;
        i.rs1 = rs1;
        i.imm = sign_extend(raw >> 20, 12);
      } else if (funct3 == 0x4 || funct3 == 0x6 || funct3 == 0x7) { // XORI/ORI/ANDI
        type     = Type::I;
        category = Category::ALU;
        i.rd  = rd;
        i.rs1 = rs1;
        i.imm = sign_extend(raw >> 20, 12);
      } else if (funct3 == 0x5) { // SRLI/SRAI
        const uint32_t funct7_i = raw >> 25;
        if (funct7_i == 0x00 || funct7_i == 0x20) {
          type     = Type::I;
          category = Category::ALU;
          i.rd  = rd;
          i.rs1 = rs1;
          i.imm = static_cast<int32_t>((raw >> 20) & 0x1f);
        }
      }
      break;
    }
    case 0x03: { // I-type Loads 000_0011 (LB/LH/LW/LBU/LHU)
      if (funct3 == 0x0 || funct3 == 0x1 || funct3 == 0x2 ||
          funct3 == 0x4 || funct3 == 0x5) {
        type     = Type::I;
        category = Category::LOAD;
        i.rd  = rd;
        i.rs1 = rs1;
        i.imm = sign_extend(raw >> 20, 12);
      }
      break;
    }
    case 0x23: { // S-type Stores 010_0011 (SB/SH/SW)
      if (funct3 == 0x0 || funct3 == 0x1 || funct3 == 0x2) {
        type     = Type::S;
        category = Category::STORE;
        s.rs1 = rs1;
        s.rs2 = rs2;
        s.imm = sign_extend(((raw >> 25) << 5) | ((raw >> 7) & 0x1f), 12);
      }
      break;
    }
    case 0x63: { // B-type Branches 110_0011 (BEQ/BNE/BLT/BGE/BLTU/BGEU)
      if (funct3 == 0x0 || funct3 == 0x1 || funct3 == 0x4 ||
          funct3 == 0x5 || funct3 == 0x6 || funct3 == 0x7) {
        uint32_t imm_bits = 0;
        imm_bits |= ((raw >> 31) & 0x01) << 12;
        imm_bits |= ((raw >>  7) & 0x01) << 11;
        imm_bits |= ((raw >> 25) & 0x3f) <<  5;
        imm_bits |= ((raw >>  8) & 0x0f) <<  1;
        type     = Type::B;
        category = Category::BRANCH;
        b.rs1 = rs1;
        b.rs2 = rs2;
        b.imm = sign_extend(imm_bits, 13);
      }
      break;
    }
    case 0x37: { // U-type LUI 011_0111
      type     = Type::U;
      category = Category::ALU;
      u.rd  = rd;
      u.imm = static_cast<int32_t>(raw & 0xfffff000u);
      break;
    }
    case 0x17: { // U-type AUIPC 001_0111
      type     = Type::U;
      category = Category::ALU;
      u.rd  = rd;
      u.imm = static_cast<int32_t>(raw & 0xfffff000u);
      break;
    }
    case 0x6F: { // J-type Jump (JAL)
      uint32_t imm_bits = 0;
      imm_bits |= ((raw >> 31) & 0x001) << 20;
      imm_bits |= ((raw >> 21) & 0x3ff) <<  1;
      imm_bits |= ((raw >> 20) & 0x001) << 11;
      imm_bits |= ((raw >> 12) & 0x0ff) << 12;
      type     = Type::J;
      category = Category::JUMP;
      j.rd  = rd;
      j.imm = sign_extend(imm_bits, 21);
      break;
    }
    case 0x67: { // I-type Jump (JALR)
      if (funct3 == 0x0) {
        type     = Type::I;
        category = Category::JUMP;
        i.rd  = rd;
        i.rs1 = rs1;
        i.imm = sign_extend(raw >> 20, 12);
      }
      break;
    }
    case 0x73: { // SYSTEM
      if (funct3 == 0x0) {
        const uint32_t imm12 = raw >> 20;
        if (imm12 == 0x000) { // ECALL
          type     = Type::I;
          category = Category::SYSTEM;
          i.rd  = rd;
          i.rs1 = rs1;
          i.imm = 0;
        } else if (imm12 == 0x001) { // EBREAK
          type     = Type::I;
          category = Category::SYSTEM;
          i.rd  = rd;
          i.rs1 = rs1;
          i.imm = 1;
        } else if (imm12 == 0x002) { // URET
          type     = Type::I;
          category = Category::SYSTEM;
          i.rd  = rd;
          i.rs1 = rs1;
          i.imm = 0x002;
        } else if (imm12 == 0x102) { // SRET
          type     = Type::I;
          category = Category::SYSTEM;
          i.rd  = rd;
          i.rs1 = rs1;
          i.imm = 0x102;
        } else if (imm12 == 0x302) { // MRET
          type     = Type::I;
          category = Category::SYSTEM;
          i.rd  = rd;
          i.rs1 = rs1;
          i.imm = 0x302;
        }
      } else if (funct3 == 0x1 || funct3 == 0x2 || funct3 == 0x3) { // CSR ops (register rs1)
        type     = Type::CSR;
        category = Category::CSR;
        c.rd  = rd;
        c.rs1 = rs1;
        c.csr = raw >> 20;
      } else if (funct3 == 0x5 || funct3 == 0x6 || funct3 == 0x7) { // CSR ops (immediate zimm)
        type     = Type::CSR;
        category = Category::CSR_IMM;
        ci.rd   = rd;
        ci.zimm = rs1;
        ci.csr  = raw >> 20;
      }
      break;
    }
    case 0x0F: { // FENCE/FENCE.I (000_1111)
      if (funct3 == 0x0 || funct3 == 0x1) {
        type     = Type::I;
        category = Category::SYSTEM;
        i.rd  = rd;
        i.rs1 = rs1;
        i.imm = sign_extend(raw >> 20, 12);
      }
      break;
    }
    case 0x0b: { // CUSTOM-0
      type     = Type::R;
      category = Category::CUSTOM;
      r.rd  = rd;
      r.rs1 = rs1;
      r.rs2 = rs2;
      break;
    }
    default:
      break;
  }
}
