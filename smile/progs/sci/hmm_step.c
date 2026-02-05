// **********************************************************************
// smile/progs/sci/hmm_step.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 3 2026
/*
- Minimal HMM-style basecalling kernel for Tile1 / smile.
- Computes a Viterbi-like DP in fixed-point and returns a simple checksum:
  result = (sum(final_column) ^ (end_state << 16))
  The program:
  * writes result to 0x0100
  * exits via ECALL 93 with exit code = (result & 0xff)
- No malloc, no stdlib, no I/O.
*/
#include <stdint.h>

#define K_MER    3                  // size of DNA k-mer processed
#define NUM_PATH 21                 // number of transitions per state (stay/step/skip)
#define M        (1 << (2 * K_MER)) // 64 states
#define N        26                 // number of events (matches toy data)

// Where to write a result so you can inspect from the debugger
#define OUT_ADDR 0x0100u
#define OUT      ((volatile uint32_t*)OUT_ADDR)

// ---------------------------------------------------------------------
// Fixed data (copied from original HMM toy)
// ---------------------------------------------------------------------

// The fixed-point values for -log(p_stay, p_step, p_skip)
static const int32_t Neg_log_prob_fxd[NUM_PATH] = {
  18,
  12,12,12,12,
  41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41
};

// The fixed-point values for (level mean/stdv)
static const int32_t mu_over_stdv_fxd[M] = {
  192,100,158,120, 38,  6, 22, 10,134, 18,138,  0,142,102,110,134,
  188, 68,176,100,148,156,156,140,150, 20,160, 14,196,140,176,152,
  160, 60,134, 90, 34, 14, 24, 16,134, 20,144, 12,128, 84, 78,108,
  214,100,186,134,150,132,146,132,146, 28,154, 24,232,166,188,190
};

// Event features (mean/stdv) for N = 26 events
static const int32_t event_over_stdv_fxd[N] = {
  24,142,164,51,63,50,70,75,136,181,101,13,172,137,133,177,191,29,148,79,94,142,200,97,70,126
};

// ---------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------

// Software 32-bit signed multiply used by GCC on RV32I (no M extension).
// Implements: (int32_t)a * (int32_t)b without using hardware MUL or libgcc.
// This is because our simple compilation uses -march=rv32i_zicsr meaning
// we're compiling RV32I without M (multiply/divide) extension.  But our HMM code has some multiplies.  So this defines __mulsi3() with only shifts/adds
// GCC will generate call to __mulsi3() for each multiply it sees in the code and we'll use this software implementation.
int __mulsi3(int a, int b)
{
  // Work in unsigned for the core loop, track sign separately.
  unsigned ua = (a < 0) ? (unsigned)(-a) : (unsigned)a;
  unsigned ub = (b < 0) ? (unsigned)(-b) : (unsigned)b;
  unsigned res = 0;

  while (ub != 0u) {
    if (ub & 1u) {
      res += ua;
    }
    ua <<= 1;
    ub >>= 1;
  }

  // Apply sign
  if ((a < 0) ^ (b < 0)) {
    return -(int)res;
  }
  return (int)res;
}

// Simple emission: squared distance in mean/stdv space
static inline int32_t log_emission(int32_t ev, int32_t mu)
{
  int32_t d = ev - mu;
  return d * d; // now we explicitly use our __mulsi3
}

// Prefix over k-mer index, using your original convention
// k = 1 -> x**  (i >> 4), k = 2 -> xy* (i >> 2)
static inline int prefix(int idx, int k)
{
  return idx >> (2 * (K_MER - k));
}

// Arrays for DP
static int32_t Post_prev[M];
static int32_t Post_curr[M];
static int32_t temp_paths[NUM_PATH];
static int32_t Trans_path[NUM_PATH];

static int find_min_loc(const int32_t *A, int len)
{
  int32_t best = 0x7fffffff;
  int     loc  = -1;
  for (int i = 0; i < len; ++i) {
    if (A[i] < best) {
      best = A[i];
      loc  = i;
    }
  }
  return loc;
}

// Fill Trans_path[0..20] with the indices of the 21 predecessor states
// for a given destination state "state", same mapping as original picker().
static void picker(int state)
{
  int first_two_bases = prefix(state, 2); // xy*
  int first_base      = prefix(state, 1); // x**

  // stay: xyz -> xyz
  Trans_path[0] = state;

  // step: *xy -> xy* (A**, C**, G**, T**)
  Trans_path[1] = 0*16 + first_two_bases; // A**
  Trans_path[2] = 1*16 + first_two_bases; // C**
  Trans_path[3] = 2*16 + first_two_bases; // G**
  Trans_path[4] = 3*16 + first_two_bases; // T**

  // skip: **x -> x**
  // A**
  Trans_path[5]  = 0*16 + 0*4 + first_base; // AA*
  Trans_path[6]  = 0*16 + 1*4 + first_base; // AC*
  Trans_path[7]  = 0*16 + 2*4 + first_base; // AG*
  Trans_path[8]  = 0*16 + 3*4 + first_base; // AT*
  // C**
  Trans_path[9]  = 1*16 + 0*4 + first_base; // CA*
  Trans_path[10] = 1*16 + 1*4 + first_base; // CC*
  Trans_path[11] = 1*16 + 2*4 + first_base; // CG*
  Trans_path[12] = 1*16 + 3*4 + first_base; // CT*
  // G**
  Trans_path[13] = 2*16 + 0*4 + first_base; // GA*
  Trans_path[14] = 2*16 + 1*4 + first_base; // GC*
  Trans_path[15] = 2*16 + 2*4 + first_base; // GG*
  Trans_path[16] = 2*16 + 3*4 + first_base; // GT*
  // T**
  Trans_path[17] = 3*16 + 0*4 + first_base; // TA*
  Trans_path[18] = 3*16 + 1*4 + first_base; // TC*
  Trans_path[19] = 3*16 + 2*4 + first_base; // TG*
  Trans_path[20] = 3*16 + 3*4 + first_base; // TT*
}

// Run the trellis and return a small checksum summarizing the result
static uint32_t run_trellis(void)
{
  // Initial column (event 0)
  for (int j = 0; j < M; ++j) {
    Post_prev[j] = log_emission(event_over_stdv_fxd[0], mu_over_stdv_fxd[j]);
  }

  // Iterate over events 1..N-1
  for (int i = 1; i < N; ++i) {
    for (int j = 0; j < M; ++j) {
      // Populate predecessors of state j
      picker(j);

      // First adder: previous column + transition costs
      for (int v = 0; v < NUM_PATH; ++v) {
        int path_idx = Trans_path[v];
        temp_paths[v] = Post_prev[path_idx] + Neg_log_prob_fxd[v];
      }

      // Take best of 21 transitions
      int best_v = find_min_loc(temp_paths, NUM_PATH);

      // Second adder: add emission for this state/time
      Post_curr[j] =
        log_emission(event_over_stdv_fxd[i], mu_over_stdv_fxd[j]) +
        temp_paths[best_v];
    }

    // Normalize column by subtracting its minimum (avoid blow-up)
    int     col_min_idx = find_min_loc(Post_curr, M);
    int32_t col_min     = Post_curr[col_min_idx];
    for (int j = 0; j < M; ++j) {
      Post_prev[j] = Post_curr[j] - col_min;
    }
  }

  // Final column: choose end state and build checksum
  int end_state = find_min_loc(Post_prev, M);
  int32_t sum   = 0;
  for (int j = 0; j < M; ++j) {
    sum += Post_prev[j];
  }

  // Cheap 32-bit checksum: combine sum + end_state
  uint32_t u_sum = (uint32_t)sum;
  uint32_t res   = u_sum ^ ((uint32_t)end_state << 16);
  return res;
}

// ---------------------------------------------------------------------
// Bare-metal entry / exit glue
// ---------------------------------------------------------------------

// Set a0 = code, a7 = 93, then ECALL.
static inline void exit_with_code(uint32_t code)
{
  __asm__ volatile(
    "mv a0, %0\n"
    "li a7, 93\n"
    "ecall\n"
    :
    : "r"(code)
    : "a0", "a7", "memory"
  );
}

// Entry point: set stack pointer and jump to main.
__attribute__((naked, section(".text.start")))
void _start(void)
{
  __asm__ volatile(
    "li   sp, 0x00004000\n"
    "j    main\n"
  );
}

// ---------------------------------------------------------------------
// MAIN
// ---------------------------------------------------------------------
int main(void)
{
  uint32_t result = run_trellis();

  // Store full 32-bit checksum where the debugger can find it.
  *OUT = result;

  // Exit with a small code (low 8 bits)
  exit_with_code(result & 0xffu);

  // Should never reach here; safety spin.
  for (;;) { }
}