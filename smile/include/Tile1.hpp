// **********************************************************************
// smile/include/Tile1.hpp
// **********************************************************************
// Sebastian Claudiusz Magierowski Oct 6 2025 
/* 
Define tile processing sequence upon ticks as well as support methods and it's memory port.
*/
#pragma once

#include <cascade/Cascade.hpp>
#include <array>
#include <cstdint>
#include <unordered_map>
#include "Instruction.hpp"
struct ThreadContext {       // structure to hold thread context
  uint32_t pc       = 0;     // what pc to start the thread at
  uint32_t regs[32] = {};    // value of regs for thread
  bool active       = false; // denote whether thread active (or not)
};

// Software interface (not a simulatable object), just a protocol
/* Lightweight memory port (sim how CPU talks to memory). Implementation (in tb_tile1.cpp for now)
can back this with any Cascade component that exposes load/store helpers (e.g., smicro's Dram).
Note: Tile1's byte/halfword loads (LB/LH/LBU/LHU) are synthesized by reading a 32-bit word
via read32() and extracting the requested byte/halfword. */
class MemoryPort {
public:
  virtual          ~MemoryPort()                          = default;
  virtual uint32_t read32(uint32_t addr)                  = 0;
  virtual void     write32(uint32_t addr, uint32_t value) = 0;
  virtual void     cycle()                                = 0;
  virtual bool     can_request() const                    = 0; // returns true when no pending response
  virtual void     request_read32(uint32_t addr)          = 0; // enque read request
  virtual void     request_write32(uint32_t addr, uint32_t value) = 0; // enque write request
  virtual bool     resp_valid() const                     = 0;
  virtual uint32_t resp_data() const                      = 0;
  virtual void     resp_consume()                         = 0;
};

class AccelPort;

// Define the tile
class Tile1 : public Component { // inherit from Component, 
  DECLARE_COMPONENT(Tile1);      // macro boilerplate to plug Component into sim engine

public:
  // Construction and lifecycle
  Tile1(std::string name, COMPONENT_CTOR); // constructor medthod declaration
  Clock(clk);
  void tick();
  void reset();

  // External interfaces
  void attach_memory(MemoryPort* mem); // assigns pointer to a memory port
  void attach_accelerator(AccelPort* accel) { accel_port_ = accel; } // how the testbench tells Tile1 what to talk to

  // Trap and privilege enums
  enum class TrapCause : uint32_t {
    EnvironmentCallFromUMode =  8u,
    EnvironmentCallFromSMode =  9u,
    EnvironmentCallFromMMode = 11u,
    IllegalInstruction       =  2u,
    Breakpoint               =  3u,
  };
  enum class PrivMode : uint32_t { // allows Tile1 to model current privelege (alongside trap CSRs)
    User        = 0u,
    Supervisor  = 1u,
    Machine     = 3u,
  };
  // public CSR addres constants
  static constexpr uint32_t CSR_MSTATUS = 0x300u;
  static constexpr uint32_t CSR_MTVEC   = 0x305u;
  static constexpr uint32_t CSR_MEPC    = 0x341u;
  static constexpr uint32_t CSR_MCAUSE  = 0x342u;

  // MSTATUS bit masks
  static constexpr uint32_t MSTATUS_MIE         = 1u << 3;
  static constexpr uint32_t MSTATUS_MPIE        = 1u << 7;
  static constexpr uint32_t MSTATUS_MPP_SHIFT   = 11u;
  static constexpr uint32_t MSTATUS_MPP_MASK    = 3u << MSTATUS_MPP_SHIFT;
  static constexpr uint32_t MSTATUS_MPP_SUPERVISOR = 1u << MSTATUS_MPP_SHIFT;
  static constexpr uint32_t MSTATUS_MPP_MACHINE = 3u << MSTATUS_MPP_SHIFT;
  static constexpr uint32_t MSTATUS_MPP_USER    = 0u << MSTATUS_MPP_SHIFT;

  // Core state accessors
  uint32_t pc()                    const { return pc_; }
  uint32_t last_pc()               const { return last_pc_; }
  uint32_t last_instr()            const { return last_instr_; }
  bool     halted()                const { return halted_; }      // status helper, tracks halted flag
  uint32_t reg(size_t idx)         const { return idx < regs_.size() ? regs_[idx] : 0; } // status helper
  uint32_t read_reg(uint32_t idx)  const { return idx < regs_.size() ? regs_[idx] : 0; }
  void     write_reg(uint32_t idx, uint32_t value);
  void     halt();

  // Thread context save/restore and exit control
  void     save_context(ThreadContext &t) const {  // save thread's state
    t.pc = pc_;
    for (int i = 0; i < 32; ++i) {
      t.regs[i] = regs_[static_cast<size_t>(i)];
    }
    t.regs[0] = 0;
  }
  void     load_context(const ThreadContext &t) {  // load thread's state
    pc_ = t.pc;
    for (int i = 0; i < 32; ++i) {
      regs_[static_cast<size_t>(i)] = t.regs[i];
    }
    regs_[0] = 0;
    halted_ = false;
    exited_ = false;
    exit_code_ = 0;
  }
  void     request_exit(uint32_t code) { // what to set when an exit scenario is reached
    exit_code_ = code;
    exited_ = true;
    halted_ = true;
  }
  bool     has_exited()            const { return exited_; }
  uint32_t exit_code()             const { return exit_code_; }
  uint64_t inst_count()            const { return inst_count_; }
  uint64_t arith_count()           const { return arith_count_; }
  uint64_t add_count()             const { return add_count_; }
  uint64_t mul_count()             const { return mul_count_; }
  uint64_t load_count()            const { return load_count_; }
  uint64_t store_count()           const { return store_count_; }
  uint64_t branch_count()          const { return branch_count_; }
  uint64_t branch_taken_count()    const { return branch_taken_count_; }
  void     set_pc(uint32_t pc);                                         // a way to set the PC

  // CSR accessors
  uint32_t read_csr(uint32_t addr) const;
  void     write_csr(uint32_t addr, uint32_t value);
  uint32_t mstatus()               const { return trap_csrs_.mstatus; } // CSR accessor
  uint32_t mtvec()                 const { return trap_csrs_.mtvec; }   // CSR accessor
  uint32_t mepc()                  const { return trap_csrs_.mepc; }    // CSR accessor
  uint32_t mcause()                const { return trap_csrs_.mcause; }  // CSR accessor
  PrivMode priv_mode()             const { return priv_mode_; }         // prilege mode (current priv kept in custom TIle1 HW) accessor for debugging
  void     request_trap(TrapCause cause);                               // latches cause of trap (exec_ecall uses this)
  void     request_illegal_instruction();
  void     raise_trap(TrapCause cause);                                 // enter trap handler
  void     resume_from_trap();                                          // exit trap handler (exec_mret uses this) so mret restores crl flow w/o fighting fetch loop)
  bool     trap_pending()          const { return trap_pending_; }      // trap pending flag
  TrapCause pending_trap_cause()   const { return pending_trap_; }

  // External port accessors
  // overloaded fns. based on constness for load/store memory ops (accessors for private mem_port_ in Tile1)
  MemoryPort* memory()                   { return mem_port_; } // non-const ver.: used on non-const Tile1 obj. (so can update memory)
  const MemoryPort* memory()       const { return mem_port_; } // const ver. used on const Tile1 obj. (so debug code won't accidetally change things)
  AccelPort* accelerator()               { return accel_port_; }
  const AccelPort* accelerator()   const { return accel_port_; }

private:
  // Trap CSRs
  struct TrapCsrState { // CSR states for trap handler to read/write
    uint32_t mstatus = 0;
    uint32_t mtvec   = 0;
    uint32_t mepc    = 0;
    uint32_t mcause  = 0;
  };

  // Privilege mode encoding helpers
  static inline uint32_t encode_mpp(PrivMode mode) {
    switch (mode) {
      case PrivMode::Machine:    return MSTATUS_MPP_MACHINE;
      case PrivMode::Supervisor: return MSTATUS_MPP_SUPERVISOR;
      case PrivMode::User:
      default:                   return MSTATUS_MPP_USER;
    }
  }
  static inline PrivMode decode_mpp(uint32_t mstatus) {
    const uint32_t field = (mstatus & MSTATUS_MPP_MASK) >> MSTATUS_MPP_SHIFT;
    switch (field) {
      case 3u: return PrivMode::Machine;
      case 1u: return PrivMode::Supervisor;
      default: return PrivMode::User;
    }
  }
  void reset_trap_csrs();
  void complete_dmem(uint32_t resp_data); // helper for completing dmem access after stall (update RF, clear fields)

  // Attached interfaces
  MemoryPort* mem_port_ = nullptr;   // tile's pointer to external mem port   (lets it fetch instr & read/write data)
  AccelPort*  accel_port_ = nullptr; // currently attached accelerator, seen through the AccelPort interface

  // Private state for core execution state
  uint32_t pc_ = 0;                  // 32b PC
  uint32_t last_pc_ = 0;             // book-keeping
  uint32_t last_instr_ = 0;          // book-keeping
  std::array<uint32_t, 32> regs_{};  // 32b RF
  bool ifetch_wait_ = false;         // are we waiting on an instr fetch response? (stalls fetch until resp arrives)
  bool ifetch_valid_ = false;        // do we have a valid buffered instr? (if so, fetch can proceed without requesting from memory)
  uint32_t ifetch_word_ = 0;         // buffered instr word for fetch stage (holds fetched instr until fetch stage consumes it)

  // Private state for data stalling (separate from IFetch)
  bool dmem_wait_ = false;
  bool dmem_is_load_ = false;
  uint32_t dmem_rd_ = 0;
  uint32_t dmem_addr_ = 0;    // optional for debug
  uint32_t dmem_wdata_ = 0;   // optional for debug
  uint32_t dmem_next_pc_ = 0; // PC to apply after completion

  // Private state for halt/exit tracking
  bool halted_ = false;              // has core stopped (due to some interrupt or exit)
  bool exited_ = false;              // has core's program intentionally finished
  uint32_t exit_code_ = 0;

  // Simple micro-architectural counters
  uint64_t inst_count_         = 0;
  uint64_t arith_count_        = 0;
  uint64_t add_count_          = 0;
  uint64_t mul_count_          = 0;
  uint64_t load_count_         = 0;
  uint64_t store_count_        = 0;
  uint64_t branch_count_       = 0;
  uint64_t branch_taken_count_ = 0;

  // Trap/CSR state
  TrapCsrState trap_csrs_{};
  std::unordered_map<uint32_t, uint32_t> csrs_{};

  // Trap flow control
  bool trap_pending_ = false;
  TrapCause pending_trap_ = TrapCause::EnvironmentCallFromUMode;
  bool pc_override_pending_ = false; // small PC-override queue, set true when ready to resume from trap 
  uint32_t pc_override_value_ = 0;   // small PC-override queue, will hold mepc when ready to resume
  PrivMode priv_mode_ = PrivMode::Machine;
};
