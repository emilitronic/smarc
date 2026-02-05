// **********************************************************************
// smile/progs/sci/hmm_step.c
// **********************************************************************
// Sebastian Claudiusz Magierowski Feb 3 2026
/*
Minimal HMM-style basecalling trellis kernel for Tile1 / smile.

This is a direct, malloc-free adaptation of the original basecaller_trellis()
forward pass:

- Same K_MER, NUM_PATH, M (=64) and N (=26)
- Same Neg_log_prob_fxd, mu_over_stdv_fxd, event_over_stdv_fxd
- Same picker() mapping from destination state -> 21 predecessor states
- Same Viterbi-style recurrence and per-column normalization

Differences vs original file:
- No malloc / free: all arrays are static with fixed M, N
- No printf / I/O
- No traceback / suffix() / basecall string assembly
- The result is summarized as a simple 32-bit checksum:
    result = sum(final_column) ^ (end_state << 16)
  which is written to 0x0100 and also used as the ECALL exit code (low 8 bits).

This way, the numerical trellis behavior (forward pass) matches the original
code, but is runnable as a flat RV32I binary on Tile1.
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
static const int32_t neg_log_ptau[NUM_PATH] = {
  18,
  12,12,12,12,
  41,41,41,41,41,41,41,41,41,41,41,41,41,41,41,41
};

// The fixed-point values for (level mean/stdv)
static const int32_t mu_over_stdv[M] = {
  192,100,158,120, 38,  6, 22, 10,134, 18,138,  0,142,102,110,134,
  188, 68,176,100,148,156,156,140,150, 20,160, 14,196,140,176,152,
  160, 60,134, 90, 34, 14, 24, 16,134, 20,144, 12,128, 84, 78,108,
  214,100,186,134,150,132,146,132,146, 28,154, 24,232,166,188,190
};

// Event features (mean/stdv) for N = 26 events
static const int32_t event_over_stdv[N] = {
  24,142,164,51,63,50,70,75,136,181,101,13,172,137,133,177,191,29,148,79,94,142,200,97,70,126
};

// ---------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------

// Software 32-bit signed multiply used by GCC on RV32I (no M extension).
// GCC will generate a call to __mulsi3 for "a * b" when compiling for rv32i.
// We provide it here in pure shifts/adds so we don't need libgcc.
int __mulsi3(int a, int b)
{
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

  if ((a < 0) ^ (b < 0)) {
    return -(int)res;
  }
  return (int)res;
}

// Simple emission: squared distance in mean/stdv space
static inline int32_t log_em(int32_t event_mean_over_stdv, int32_t level_mean_over_stdv)
{
  int32_t dist = event_mean_over_stdv - level_mean_over_stdv;
  return dist * dist; // uses __mulsi3 under the hood on RV32I
}

// Prefix over k-mer index, using original convention:
// k = 1 -> x** (i >> 4), k = 2 -> xy* (i >> 2)
// if k=1 return state/16 (i.e., which of  4 positions does prefix-1 have)
// if k=2 return state/4  (i.e., which of 16 positions does prefix-2 have)
static inline int prefix(int i, int k)
{
  return i >> (2 * (K_MER - k));
}

// Arrays for DP and transitions (malloc-free versions of original locals)
static int32_t log_post[M];         // normalized previous posteriors
static int32_t cur_log_post[M];     // current event posteriors
static int32_t temp[NUM_PATH];      // 21 transition candidates
static int32_t tran_prev[NUM_PATH]; // indices of 21 predecessor states
static int32_t pointers[M][N - 1];  // backpointer table (kept for fidelity)
static int32_t last_col[M];         // final column snapshot

static int find_min_location(const int32_t *A, int len)
{
  int32_t U   = 0x7fffffff;
  int     loc = -1;
  for (int i = 0; i < len; i++) {
    if (A[i] < U) {
      U   = A[i];
      loc = i;
    }
  }
  return loc;
}

// transition filter
// fill tran_prev[0..20] with the indices of the 21 predecessor states
// for a given destination state "state" xyz (k=3)
static void tran_filter(int state)
{
  int first_two_bases = prefix(state, 2); // k=2: xy* 
  int first_base      = prefix(state, 1); // k=1: x**

  // stay: prv:xyz -> curr:xyz
  tran_prev[0]  = state;                   // xyz

  // step: prev:*xy -> curr:xy* (new bases come from the right)
  tran_prev[1]  = 0*16 + first_two_bases;  // Axy
  tran_prev[2]  = 1*16 + first_two_bases;  // Cxy
  tran_prev[3]  = 2*16 + first_two_bases;  // Gxy
  tran_prev[4]  = 3*16 + first_two_bases;  // Txy

  // skip: prev:**x -> curr:x**
  // A**
  tran_prev[5]  = 0*16 + 0*4 + first_base; // AAx
  tran_prev[6]  = 0*16 + 1*4 + first_base; // ACx
  tran_prev[7]  = 0*16 + 2*4 + first_base; // AGx
  tran_prev[8]  = 0*16 + 3*4 + first_base; // ATx
  // C**
  tran_prev[9]  = 1*16 + 0*4 + first_base; // CAx
  tran_prev[10] = 1*16 + 1*4 + first_base; // CCx
  tran_prev[11] = 1*16 + 2*4 + first_base; // CGx
  tran_prev[12] = 1*16 + 3*4 + first_base; // CTx
  // G**
  tran_prev[13] = 2*16 + 0*4 + first_base; // GAx
  tran_prev[14] = 2*16 + 1*4 + first_base; // GCx
  tran_prev[15] = 2*16 + 2*4 + first_base; // GGx
  tran_prev[16] = 2*16 + 3*4 + first_base; // GTx
  // T**
  tran_prev[17] = 3*16 + 0*4 + first_base; // TAx
  tran_prev[18] = 3*16 + 1*4 + first_base; // TCx
  tran_prev[19] = 3*16 + 2*4 + first_base; // TGx
  tran_prev[20] = 3*16 + 3*4 + first_base; // TTx
}

// ---------------------------------------------------------------------
// Trellis (forward pass) – direct port of basecaller_trellis()
// ---------------------------------------------------------------------
// run_trellis:
//  - builds Viterbi-style trellis over N events × M states
//  - keeps only previous and current columns in log_post/cur_log_post
//  - returns checksum of final column + best end state
static uint32_t run_trellis(void)
{
  int i; // event index
  int j; // state index
  int v; // transition index
  int h; // state index for normalization loop
  int path_index;
  int index;
  int col_min;
  int col_min_index;
  int end_state;

  // Initial probabilities (for event 0)
  for (j = 0; j < M; j++) {
    log_post[j] = log_em(event_over_stdv[0], mu_over_stdv[j]);
  }

  // Create the trellis iteratively, events 1..N-1
  for (i = 1; i < N; i++) {        // events loop
    for (j = 0; j < M; j++) {      // states loop
      tran_filter(j);              // fill tran_prev with 21 predecessors of state j

      // First adder: compute the 21 possible transitions for state j
      for (v = 0; v < NUM_PATH; v++) {
        path_index = tran_prev[v];
        temp[v] = log_post[path_index] + neg_log_ptau[v];
      }

      // Comparator: choose the min log(alpha) + log(tau)
      index = find_min_location(temp, NUM_PATH); // index 0..20
      pointers[j][i - 1] = index;                // ptr vals indexed from 0-20 (NOT by actual state number)

      // Second adder: emission + chosen transition
      cur_log_post[j] = log_em( event_over_stdv[i], mu_over_stdv[j] ) + temp[index];
    } // states loop

    // Normalize the posterior probs (avoid overflow)
    col_min_index = find_min_location(cur_log_post, M);
    col_min       = cur_log_post[col_min_index];
    for (h = 0; h < M; h++) {
      log_post[h] = cur_log_post[h] - col_min;
    }
  } // events loop

  // Optimal end state – as in original
  for (j = 0; j < M; j++) {
    last_col[j] = log_post[j];
  }
  end_state = find_min_location(last_col, M);

  // Build a cheap checksum combining last column and end_state.
  // This is not part of the original C, but gives us a scalar to
  // compare between host (gcc) and Tile1 runs.
  int32_t sum = 0;
  for (j = 0; j < M; j++) {
    sum += last_col[j];
  }

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