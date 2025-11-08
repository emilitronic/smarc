// **********************************************************************
// smicro/include/hal.h
// **********************************************************************
// S Magierowski Aug 17 2025
/*
Defines the hardware abstraction layer (HAL), a set of C-style functions that provide a 
clea, high-level interface for the testbench to interact with sim'd HW.  Implemented
in hal_cascade.cpp.
*/
#pragma once
#include <cstdint>

void* hal_alloc(uint64_t bytes);                              // allocates block of mem of bytes size in sim'd DRAM
void  hal_write(void* addr, const void* src, uint64_t bytes); // writes data from host (src) to addr of sim'd DRAM
void  hal_read (void* addr, void* dst, uint64_t bytes);       // reads data from sim'd DRAM addr to host (dst)
void  accel_launch(void* A, void* B, void* C, uint32_t N);    // launches accel, telling src & dst addr
bool  accel_done();                                           // checks if accel has finished computation
void  hal_run_for(uint64_t ps);                               // advances sim by specified number of ps
