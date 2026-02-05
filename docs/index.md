---
layout: default
title: smarc Documentation
description: smarchitectures
---

<div align="center">
<picture>
  <img alt="smarc" src="emil_arch.png" width="30%">
</picture>
</div>

**<center>smarchitectures</center>**

---
<center>  
Alleluia.  Sequentia.
</center>

# Intro

`smarc` is where I prototype RISC-V cores + memory system + accelerators for future **sequencing SoCs**.  Sequence what?  I keep an open-mind, but worklaods of interest include molecules (sequence DNA), pictures (sequence senor streams), path planning (sequence movement).  The project consists of two main parts.  `smile` checks what sequencing code does on the silicon-vectored core and `smicro` checks how efficiently the architecture hardware transforms and moves the data that make up those sequences.

## Packages

- **smile** – core model and programs  
  - `Tile1`: a simple RV32 core with a clean `MemoryPort` interface  
  - Tiny test programs and support code for instruction decode/execute
  
- **smicro** – SoC harness and testbenches  
  - Instantiates cores, DRAM, memory controllers, testers  
  - Drives experiments via `./smicro -suite=... -topo=...`


## What can I do here?

- Run protocol and DRAM tests (`-suite=proto_*`, `-suite=hal_*`)
- Let the core run directly against DRAM (`-suite=proto_core`) and watch instruction traces
- Evolve the core and memory system together, while keeping the same high-level driver interface

## When to use `smile` vs `smicro`

**Use `smile` for…**

- Checking what a small RISC-V program actually does to Tile1’s registers and memory
- Stepping instruction-by-instruction with a debugger to debug decode/execute/trap behavior
- Trying out CUSTOM-0 instructions and accelerators and verifying their results

**Use `smicro` for…**

- Testing the MemCtrl ↔ DRAM protocol and timing with scripted traffic
- Running Tile1 inside a small SoC harness to check system-level integration
- Exploring how different memory latencies/topologies affect request/response timing

For more detail, see:

- [smile](./smile.md) – Tile1 core, instruction decode/execute, and how it talks to memory
- [smicro](./smicro.md) – SoC wiring, memory paths, test suites  
- [kernels](./kernels.md) – focused microkernel notes and run examples
