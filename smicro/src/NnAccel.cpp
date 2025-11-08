// **********************************************************************
// smicro/src/NnAccel.cpp
// **********************************************************************
// S Magierowski Aug 16 2025
// 
#include "NnAccel.hpp"
#include "SoC.hpp"

extern SoC* g_soc;

NnAccel::NnAccel(std::string /*name*/, AttachMode mode, IMPL_CTOR)
  : mode_(mode)
{
  UPDATE(update); // this macro registers the update fn (to be called on every clock cycle)
}
// copies addr & size params into accelerator's internal state variables
void NnAccel::set_src_dst(u64 a, u64 b, u64 c, u32 n) {
  a_addr_ = a;
  b_addr_ = b;
  c_addr_ = c;
  n_ = n;
}
// sets busy_ flag and pushes false to the done FIFO port, signalling to outside that accel is busy
void NnAccel::kick() {
  busy_ = true;
  done.push(false);
}
// returns true if accel is not busy
bool NnAccel::is_done() {
  return !busy_;
}
// heart of the accel
void NnAccel::update() {
  if (busy_) { // accel only does work if "kicked" into action
    std::vector<uint64_t> A(n_), B(n_), C(n_);
    g_soc->dram_->read(a_addr_, A.data(), n_ * sizeof(uint64_t));
    g_soc->dram_->read(b_addr_, B.data(), n_ * sizeof(uint64_t));

    for (uint32_t i = 0; i < n_; i++) {
      C[i] = A[i] + B[i];
    }

    g_soc->dram_->write(c_addr_, C.data(), n_ * sizeof(uint64_t));

    busy_ = false;
    done.push(true);
  }
}

void NnAccel::reset() {
  busy_ = false;
  done.push(true);
}