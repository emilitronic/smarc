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

`smarc` is a playground for building and simulating small RISC-V systems aimed at **sequence-centric workloads** (molecule chains like DNA, streaming sensors, path planners, etc.). It glues together a cycle-accurate system-on-chip (SoC) harness (`smicro`) with a simple RV32 core and tools (`smile`) so you can experiment with architectures before committing to RTL.

## Packages

- **smicro** – SoC harness and testbenches  
  - Instantiates cores, DRAM, memory controllers, testers  
  - Drives experiments via `./smicro -suite=... -topo=...`

- **smile** – core model and programs  
  - `Tile1`: a simple RV32 core with a clean `MemoryPort` interface  
  - Tiny test programs and support code for instruction decode/execute

## What can I do here?

- Run protocol and DRAM tests (`-suite=proto_*`, `-suite=hal_*`)
- Let the core run directly against DRAM (`-suite=proto_core`) and watch instruction traces
- Evolve the core and memory system together, while keeping the same high-level driver interface

For more detail, see:

- [smicro](./smicro.md) – SoC wiring, memory paths, test suites  
- [smile](./smile.md) – Tile1 core, instruction decode/execute, and how it talks to memory
