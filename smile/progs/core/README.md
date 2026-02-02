# core programs

This directory holds core C programs used to exercise the micro-architecture,
debugger, trap plumbing, and other baseline system behaviors.

Regression tests (exit code 1 on pass, 0 on fail):
- shift_ri_test.c: SRLI/SRAI sanity check
- bitwise_imm_test.c: XORI/ORI/ANDI sanity check
- rtype_logic_test.c: SUB/XOR/OR/AND sanity check
- rtype_slt_test.c: SLT/SLTU sanity check
