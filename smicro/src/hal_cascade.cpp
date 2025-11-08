// **********************************************************************
// smicro/src/hal_cascade.cpp
// **********************************************************************
// S Magierowski Aug 17 2025
/*
Implementation of HAL functions.  A straightforward mapping from simple C-style HAL fns. to C++
methods of HW models.
*/
#include "hal.h"
#include "SoC.hpp"

extern SoC* g_soc; // declares that there's a global pointer to SoC, extern allows us this file to access it

void* hal_alloc(uint64_t bytes) { return g_soc->dram_->alloc(bytes); } // calls alloc method in dram_ member of SoC
void  hal_write(void* addr,const void* src,uint64_t n){ g_soc->dram_->write((uint64_t)addr,src,n); } // call write method
void  hal_read (void* addr,void* dst,uint64_t n){ g_soc->dram_->read((uint64_t)addr,dst,n); }        // call read method
void  accel_launch(void* A,void* B,void* C,uint32_t N){
  g_soc->accel_->set_src_dst((uint64_t)A,(uint64_t)B,(uint64_t)C,N); // calls set_src_dst method in accel_
  g_soc->accel_->kick();                                             // calls kick metho in accel_
}
bool  accel_done(){ return g_soc->accel_->is_done(); }
void  hal_run_for(uint64_t ps){ Sim::run(ps); }
