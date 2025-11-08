// **********************************************************************
// smicro/src/test_vectadd.cpp
// **********************************************************************
// S Magierowski Aug 17 2025
/*
Testbench program.  Interacts with sim'd HW via the hardware abstraction layer (HAL).
Uses HAL functions to reach into sim'd HW and manipulate its state w/o needing to know
complex low-level details of how Cascade simulator or C++ hardware models work.
*/
#include "hal.h"
#include "SoC.hpp"
#include <vector>
#include <iostream>

extern SoC* g_soc;

int main(){
  // bring up sim
  SoC soc(ViaL2, false);  // create instance of entire SoC; connect accel via L2
  g_soc = &soc;           // set global pointer to our SoC instance, HAL uses this to access SoC's components
  Clock clk;
  soc.clk << clk;
  clk.generateClock();

  Sim::init();
  Sim::reset();

  // inputs, create three standard C++ vectors, A, B, C
  const uint32_t N=1024;
  std::vector<uint64_t> A(N),B(N),C(N);
  for(uint32_t i=0;i<N;i++){ A[i]=i; B[i]=2*i+1; } // fill A & B with some initial data, C is empty

  // allocate device buffers in simulated DRAM and copy in them
  void* dA=hal_alloc(N*8);      
  void* dB=hal_alloc(N*8);      
  void* dC=hal_alloc(N*8);      
  hal_write(dA,A.data(),N*8); // copy from host vector into buffer dA
  hal_write(dB,B.data(),N*8);

  // accelerator launch and run until done
  accel_launch(dA,dB,dC,N); // HAL fn. telling accelerator to start work, passes addresses of i/p & o/p bufs in DRAM
  while(!accel_done()) {    // keep checking if accel is finished
    hal_run_for(1000);      // advance sim time in 1000 ps chunks
  }

  // once sim is finished copy back and check
  hal_read(dC,C.data(),N*8);   // copy results from simulated DRAM buffer dC back into host vector C
  for(uint32_t i=0;i<N;i++) {  // iterate through C to make sure accelerator produced correct result
    if(C[i] != A[i] + B[i]) {
      std::cout << "Error at index " << i << ": " << C[i] << " != " << A[i] + B[i] << std::endl;
      return 1;
    }
  }

  std::cout << "Vector addition successful!" << std::endl;
  return 0;
}
