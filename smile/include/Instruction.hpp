// **********************************************************************
// smile/include/Instruction.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 11 2025
//
// Minimal RV32I decoder scaffolding for Tile1 experiments.

#pragma once

#include <cstdint>

// Instruction structure, feed it 32b word and it extracts all the useful fields from it
// fields (e.g., opcode), imm values, type (e.g., R, I, etc.), category (ALU, SYSTEM, BRANCH, etc.)
struct Instruction {
  enum class Type {
    R,
    I,
    S,
    B,
    U,
    J,
    CSR,
    Unknown,
  };

  enum class Category {
    ALU,
    SYSTEM,
    LOAD,
    STORE,
    BRANCH,
    JUMP,
    CSR,
    CSR_IMM,
    CUSTOM,
    Unknown,
  };

  struct RType {
    uint32_t rd  = 0;
    uint32_t rs1 = 0;
    uint32_t rs2 = 0;
  };

  struct IType {
    uint32_t rd  = 0;
    uint32_t rs1 = 0;
    int32_t  imm = 0;
  };

  struct SType {
    uint32_t rs1 = 0;
    uint32_t rs2 = 0;
    int32_t  imm = 0;
  };

  struct BType {
    uint32_t rs1 = 0;
    uint32_t rs2 = 0;
    int32_t  imm = 0;
  };

  struct UType {
    uint32_t rd  = 0;
    int32_t  imm = 0;
  };

  struct JType {
    uint32_t rd  = 0;
    int32_t  imm = 0;
  };

  struct CsrType {
    uint32_t rd  = 0;
    uint32_t rs1 = 0;
    uint32_t csr = 0;
  };
  struct CsrImmType {
    uint32_t rd  = 0;
    uint32_t zimm = 0;
    uint32_t csr = 0;
  };

  explicit Instruction(uint32_t raw_instr); // takes raw instr & extracts all fields, call it like: Instruction decoded(instr)

  uint32_t raw      = 0; // full 32b instruction
  uint32_t opcode   = 0;
  uint32_t funct3   = 0;
  uint32_t funct7   = 0;
  uint32_t rd       = 0;
  uint32_t rs1      = 0;
  uint32_t rs2      = 0;
  Type     type     = Type::Unknown;
  Category category = Category::Unknown;

  RType r{};
  IType i{};
  SType s{};
  BType b{};
  UType u{};
  JType j{};
  CsrType c{};
  CsrImmType ci{};
};
