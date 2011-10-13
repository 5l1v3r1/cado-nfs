#include "cado.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h> /* for PRIx64 macro and strtoumax */
#include <math.h>   // for ceiling, floor in cfrac
#include "fb.h"
#include "utils.h"           /* lots of stuff */
#include "basicnt.h"         /* ctzl bin_gcd */
#include "ecm/facul.h"
#include "bucket.h"
#include "trialdiv.h"
#include <pthread.h>

#ifdef HAVE_SSE2
#define SSE_NORM_INIT
#endif

#ifdef SSE_NORM_INIT
#include <emmintrin.h>
#endif

#ifndef HAVE_LOG2
static double
log2 (double x)
{
  return log (x) / log (2.0);
}
#endif

/* As its name says, this is a ugly hack that initializes all lognorms to the
   maximal value (255) on the rational side. But it seems to work well, and to
   miss only about 7% to 8% relations wrt a more accurate estimation. */
//#define UGLY_HACK

/* number of bits used to estimate the norms */
#define NORM_BITS 10

/* define PROFILE to keep certain function from being inlined, in order to
   make them show up on profiler output */
//#define PROFILE

/* Trick to discard lognorms that will probably lead to non L-smooth
   cofactors. Disabled for now since it requires accurate sieving
   (in particular by prime powers and bad primes). */
// #define COFACTOR_TRICK

/* This triggers code which fills bucket in several passes, one for each
 * congruence class mod 2 (three such, the trivial one leading to
 * spurious reports). It's currently only part of the story, and at the
 * moment it is almost neutral in terms of efficiency (small slowdown
 * because of access pattern).  But it's the way to go if one wants to
 * support I=16. There are many other places where changes must be made.
 * This particular flag affects only the treatment of the ``bucket
 * sieved'' primes, not the pattern-sieved, or small-sieved.
 */
#define MOD2_CLASSES_BS 0       /* define to 0 or 1 */

/* default sieve region side is 2^DEFAULT_I */
#define DEFAULT_I 12

/* default bucket region: 2^16 = 64K == close to L1 size, but this is the
   (current) largest possible value, otherwise bucket.h must be changed,
   since it stores positions on 16 bits */
#ifndef LOG_BUCKET_REGION
#define LOG_BUCKET_REGION 16
#endif

#if LOG_BUCKET_REGION > 16
#error "Too large LOG_BUCKET_REGION, please adapt bucket.h first"
#endif

/* This flag is necessary to support I=16. Otherwise it's a useless
 * burden
 */
#define SUPPORT_I16

/* Define SKIP_GCD3 to skip updates where 3 divides gcd(i,j) in the
   bucket sieving phase. Slightly slower than not skipping them
   in single-thread mode, but might be useful for multi-threading,
   or when memory is tight */
// #define SKIP_GCD3

/* These parameters control the size of the buckets.
 * The number of updates that a bucket can accumulate is estimated as
 *   (loglog(factor base bound) - loglog(bucket sieving threshold)) 
 *     * BUCKET_LIMIT_FACTOR * I * J + BUCKET_LIMIT_ADD 
 * We don't store updates where 2 divides gcd(i,j) which reduces the number 
 * of updates to about (1-1/4)=3/4, so 0.8 should be safe.
 * If we don't store updates where 3 divides gcd(i,j) either, their number
 * is reduced to about (1-1/4)*(1-1/9)=2/3, then 0.7 should be ok
 */

#ifndef BUCKET_LIMIT_FACTOR
#ifdef SKIP_GCD3
#define BUCKET_LIMIT_FACTOR 0.7
#else
#define BUCKET_LIMIT_FACTOR 0.8
#endif
#endif

#ifndef BUCKET_LIMIT_ADD
#define BUCKET_LIMIT_ADD 0
#endif

#define LOG_SCALE 1.4426950408889634 /* 1/log(2) to 17 digits, rounded to
                                        nearest. This is enough to uniquely
                                        identify the corresponding IEEE 754
                                        double precision number */

/* Guard for the logarithms of norms, so that the value does not wrap around
   zero due to roundoff errors. */
#define GUARD 4

/* GUARD+LOG_MAX should be as near as possible from 256, to get more accuracy
   in the norm computations, but not too much, otherwise a norm might be
   rounded to zero. */
#define LOG_MAX (255.9 - (double) GUARD)

/* define TRACE_K, and exactly one of the trace_* values to something
 * non-zero, in order to get tracing information on a particular
 * relation.  In particular this traces the sieve array entry
 * corresponding to the relation. Upon startup, the three values below
 * are reconciled.
 *
 * (see Coordinate systems further down in this file)
 */
#define xxxTRACE_K

struct { unsigned int N; unsigned int x; } trace_Nx = { 0, UINT_MAX};
struct { int64_t a; uint64_t b; } trace_ab = { -1430419,2652 };
struct { int i; unsigned int j; } trace_ij = { 0, UINT_MAX, };

static inline int trace_on_spot_Nx(unsigned int N, unsigned int x) {
    if (trace_Nx.x == UINT_MAX) return 0;
    return N == trace_Nx.N && x == trace_Nx.x;
}

static inline int trace_on_range_Nx(unsigned int N, unsigned int x0, unsigned int x1) {
    if (trace_Nx.x == UINT_MAX) return 0;
    return N == trace_Nx.N && x0 <= trace_Nx.x && trace_Nx.x < x1;
}

static inline int trace_on_spot_x(unsigned int x) {
    return x == (trace_Nx.N << LOG_BUCKET_REGION) + trace_Nx.x;
}

static inline int trace_on_spot_ab(int a, unsigned int b) {
    return a == trace_ab.a && b == trace_ab.b;
}

static inline int trace_on_spot_ij(int i, unsigned int j) {
    return i == trace_ij.i && j == trace_ij.j;
}


/* Define CHECK_UNDERFLOW to check for underflow when subtracting
   the rounded log(p) from sieve array locations */
//#define CHECK_UNDERFLOW

/* Some functions should not be inlined when we profile or it's hard or
   impossible to tell them apart from the rest in the profiler output */
#ifdef PROFILE
#define NOPROFILE_INLINE
#define NOPROFILE_STATIC
#else
#define NOPROFILE_INLINE static inline
#define NOPROFILE_STATIC static
#endif

/* uintmax_t is guaranteed to be larger or equal to uint64_t */
#define strtouint64(nptr,endptr,base) (uint64_t) strtoumax(nptr,endptr,base)



/* This global mutex should be locked in multithreaded parts when a
 * thread does a read / write, especially on stdout, stderr...
 */
pthread_mutex_t io_mutex = PTHREAD_MUTEX_INITIALIZER; 

const int bucket_region = 1 << LOG_BUCKET_REGION;

struct sieve_side_info_s {
    double scale;      /* norm scale used on the algebraic side */
    double logmax;     /* norms on the alg-> side are < 2^alg->logmax */
    unsigned char Bound[256]; /* zero for good lognorms, 127 otherwise */
    fbprime_t *trialdiv_primes;
    trialdiv_divisor_t *trialdiv_data;
    unsigned char S[1 << NORM_BITS];
    factorbase_degn_t * fb;
};
typedef struct sieve_side_info_s sieve_side_info[1];
typedef struct sieve_side_info_s * sieve_side_info_ptr;
typedef const struct sieve_side_info_s * sieve_side_info_srcptr;

// General information about the siever
typedef struct {
    int bench;
    // multithreading info
    int nb_threads;
    // sieving area
    uint32_t I;
    uint32_t J;
    int logI; // such that I = 1<<logI
    // description of the q-lattice
    int ratq;   // 0 means special q on alg side, otherwise on rat side
    uint64_t q;
    uint64_t rho;
    int32_t a0, b0, a1, b1;
    // parameters for bucket sieving
    int bucket_thresh;    // bucket sieve primes >= bucket_thresh
    int nb_buckets;
    int bucket_limit;   // maximal number of bucket_reports allowed in one bucket.
    unsigned int degree;   /* polynomial degree */
    cado_poly_ptr cpoly;
    sieve_side_info sides[2];
    mpz_t *fij, gij[2];    /* coefficients of F,G(a0*i+a1*j, b0*i+b1*j)  */
    double *fijd;     /* coefficients of F_q/q */
    double B;         /* bound for the norm computation */
    unsigned int *lpf; /* lpf[i] is largest prime factor of i, for i < I */
    facul_strategy_t *strategy;

    FILE *output;
    int verbose;
} sieve_info_t;

#define RATIONAL_SIDE   0
#define ALGEBRAIC_SIDE   1
const char * sidenames[2] = {
    [RATIONAL_SIDE] = "rational",
    [ALGEBRAIC_SIDE] = "algebraic", };

/* {{{ Forward declarations of conversion functions */
MAYBE_UNUSED static void xToIJ(int *i, unsigned int *j, const unsigned int X, const sieve_info_t * si);
MAYBE_UNUSED static void NxToIJ(int *i, unsigned int *j, const unsigned int N, const unsigned int x, const sieve_info_t * si);

MAYBE_UNUSED static void IJTox(unsigned int * x, int i, unsigned int j, const sieve_info_t * si);
MAYBE_UNUSED static void IJToNx(unsigned int *N, unsigned int * x, int i, unsigned int j, const sieve_info_t * si);
MAYBE_UNUSED static void IJToAB(int64_t *a, uint64_t *b, const int i, const unsigned int j, const sieve_info_t * si);
MAYBE_UNUSED static void xToAB(int64_t *a, uint64_t *b, const unsigned int x, const sieve_info_t *si);
MAYBE_UNUSED static void NxToAB(int64_t *a, uint64_t *b, const unsigned int N, const unsigned int x, const sieve_info_t *si);
MAYBE_UNUSED static int ABToIJ(int *i, unsigned int *j, const int64_t a, const uint64_t b, const sieve_info_t * si);
MAYBE_UNUSED static int ABTox(unsigned int *x, const int64_t a, const uint64_t b, const sieve_info_t * si);
MAYBE_UNUSED static int ABToNx(unsigned int * N, unsigned int *x, const int64_t a, const uint64_t b, const sieve_info_t * si);
/* }}} */

/* Test if entry x in bucket region n is divisible by p */
void test_divisible_x (const fbprime_t p, const unsigned long x, const int n,
		       const sieve_info_t *si, const char side);
int factor_leftover_norm (mpz_t n, unsigned int b, mpz_array_t* const factors,
			  uint32_array_t* const multis,
			  facul_strategy_t *strategy);
NOPROFILE_STATIC void
eval_fij (mpz_t v, const mpz_t *f, const unsigned int d, const long i,
	  const unsigned long j);


/************************** sieve info stuff *********************************/

static double get_maxnorm (cado_poly, sieve_info_t *, uint64_t);

/* initialize array C[0..255]: C[i] is non-zero whenever the log-norm i
   is considered as a potential report.
   The large prime bound is L = 2^l.
*/
static void
sieve_info_init_lognorm (unsigned char *C, unsigned char threshold,
                         unsigned long B MAYBE_UNUSED,
                         unsigned long l MAYBE_UNUSED,
                         double scale MAYBE_UNUSED)
{
  unsigned long k;

  for (k = 0; k < 256; k++)
    C[k] = (k <= threshold) ? 0 : 127;

#ifdef COFACTOR_TRICK
  {
    unsigned char k0, k1;
    /* for L < R <= B^2, R cannot be L-smooth (see lattice.tex) */
    k0 = (unsigned char) ((double) l * scale + 0.5) + GUARD;
    k1 = (unsigned char) (2.0 * log2 ((double) B) * scale + 0.5)
      + GUARD;
    for (k = k0 + GUARD; k + GUARD <= k1 && k < 256; k++)
      C[k] = 0;
  }
#endif
}

static void sieve_info_init_refactoring(sieve_info_t * si, int td_thresh)
{
    /* Our trial division needs odd divisors, 2 is handled by mpz_even_p().
       If the FB primes to trial divide contain 2, we skip over it.
       We assume that if 2 is in the list, it is the first list entry,
       and that it appears at most once. */
    for(int side = 0 ; side < 2 ; side++) {
        sieve_side_info_ptr s = si->sides[side];
        s->trialdiv_primes = fb_extract_bycost (s->fb, si->bucket_thresh,
                td_thresh);
        int n;
        for (n = 0; s->trialdiv_primes[n] != FB_END; n++);
        int skip2 = n > 0 && s->trialdiv_primes[0] == 2;
        s->trialdiv_data = trialdiv_init (s->trialdiv_primes + skip2,
                n - skip2);
    }
}

static void sieve_info_clear_refactoring(sieve_info_t * si)
{
    for(int side = 0 ; side < 2 ; side++) {
        trialdiv_clear (si->sides[side]->trialdiv_data);
        free (si->sides[side]->trialdiv_primes);
    }
}

static void
sieve_info_init (sieve_info_t *si, cado_poly cpoly, int logI, uint64_t q0,
		 const int bucket_thresh, FILE *output, int nb_threads)
{
  unsigned int d = cpoly->degree;
  unsigned int k;
  double r, scale;
  unsigned char alg_bound, rat_bound;

  /* Don't zero out *si, since some parameters may have been set directly
   * from the command line. */
  si->nb_threads = nb_threads;
  si->degree = d;
  si->cpoly = cpoly;
  si->fij = (mpz_t*) malloc ((d + 1) * sizeof (mpz_t));
  si->fijd = (double*) malloc ((d + 1) * sizeof (double));
  for (k = 0; k <= d; k++)
    mpz_init (si->fij[k]);
  mpz_init (si->gij[0]);
  mpz_init (si->gij[1]);
  si->logI = logI;
  si->I = 1 << si->logI;
  si->J = 1 << (si->logI - 1);

  sieve_side_info_ptr rat = si->sides[RATIONAL_SIDE];
  sieve_side_info_ptr alg = si->sides[ALGEBRAIC_SIDE];

  /* initialize bounds for the norm computation, see lattice.tex */
  si->B = sqrt (2.0 * (double) q0 / (cpoly->skew * sqrt (3.0)));
  alg->logmax = get_maxnorm (cpoly, si, q0); /* log2(max norm) */

  /* We want some margin, (see below), so that we can set 255 to discard
   * non-survivors.*/
  scale = alg->logmax + cpoly->alambda * (double) cpoly->lpba;

  fprintf (output, "# Alg. side: log2(maxnorm)=%1.2f logbase=%1.6f",
           scale, exp2 (scale / LOG_MAX));
  // second guard, due to the 255 trick!
  scale = (LOG_MAX - GUARD) / scale;
  alg_bound = (unsigned char) (cpoly->alambda * (double) cpoly->lpba *  scale)
            + GUARD;
  fprintf (output, " bound=%u\n", alg_bound);
  sieve_info_init_lognorm (alg->Bound, alg_bound, cpoly->alim, cpoly->lpba,
                           scale);
  alg->scale = scale;

  /* similar bound on the rational size: |a| <= s*I*B and |b| <= I*B */
  scale = fabs (mpz_get_d (cpoly->g[1])) * cpoly->skew
        + fabs (mpz_get_d (cpoly->g[0]));
  scale *= si->B * (double) si->I;
  if (si->ratq)
      scale /= (double) q0;
  rat->logmax = scale = log2 (scale);
  /* on the rational side, we want that the non-reports on the algebraic
     side, which are set to 255, remain over the report bound R, even if
     the rational norm is totally smooth. For this, we simply add R to the
     maximal lognorm to compute the log base */
  r = cpoly->rlambda * (double) cpoly->lpbr; /* base-2 logarithm of the
                                                report bound */
  fprintf (output, "# Rat. side: log2(maxnorm)=%1.2f ", scale);
  fprintf (output, "logbase=%1.6f", exp2 (scale / LOG_MAX ));
  /* we subtract again GUARD to avoid that non-reports overlap the report
     region due to roundoff errors */
  rat->scale = LOG_MAX / scale;
  rat_bound = (unsigned char) (r * rat->scale) + GUARD;
  fprintf (output, " bound=%u\n", rat_bound);
  sieve_info_init_lognorm (rat->Bound, rat_bound, cpoly->rlim, cpoly->lpbr,
                           rat->scale);

  // bucket info
  // TODO: be more clever, here.
  si->bucket_thresh = si->I; /* Default value */
  if (si->bucket_thresh < bucket_thresh)
    si->bucket_thresh = bucket_thresh;
  /* If LOG_BUCKET_REGION == si->logI, then one bucket (whose size is the
   * L1 cache size) is actually one line. This changes some assumptions
   * in sieve_small_bucket_region and resieve_small_bucket_region, where
   * we want to differentiate on the parity on j.
   */
  ASSERT_ALWAYS(LOG_BUCKET_REGION >= si->logI);

#ifndef SUPPORT_I16
  if (si->logI >= 16) {
      fprintf(stderr, "Error: -I 16 requires setting the SUPPORT_I16 flag at compile time\n");
      abort();
  }
#endif

  si->nb_buckets = 1 + (si->I * si->J - 1) / bucket_region;

  double limit_factor = log(log(MAX(cpoly->rlim, cpoly->alim))) - 
                        log(log(si->bucket_thresh));
  si->bucket_limit = limit_factor * bucket_region;
  if (si->logI < LOG_BUCKET_REGION) {
      /* If there's only one row per bucket, then on every other row
       * we can be sure that 2 cannot divide gcd(i,j), thus the 0.75
       * factor which accounts for the gcd is incorrect. OTOH, we can
       * imagine saving some memory: the global allocated space is still
       * correct, we merely have to accomodate the bucket sizes to be
       * possibly different across buckets. The better fix for this would
       * be to treat parity classes anyway.
       */
      si->bucket_limit *= BUCKET_LIMIT_FACTOR;
  }
  si->bucket_limit += BUCKET_LIMIT_ADD;
  /* an experimental study on the bucket fill with respect to the number of
     threads shows that each new thread increases by about 1.15% the fill */
  si->bucket_limit += (si->bucket_limit * (nb_threads - 1)) / 87;
  if (si->logI == 16) {
      /* FIXME: This is very odd. I just don't understand. */
      si->bucket_limit += si->bucket_limit / 20;
  }
#if LOG_BUCKET_REGION < 16
  /* For debuggging purposes, we sometimes want to have very small
   * buckets. However, these are also inherently unbalanced, so we have
   * to compensate for this.
   */
  si->bucket_limit *= 2;
#endif

  /* Store largest prime factor of k in si->lpf[k], 0 for k=0, 1 for k=1 */
  si->lpf = (unsigned int *) malloc (si->I * sizeof (unsigned int));
  ASSERT_ALWAYS (si->lpf != NULL);
  si->lpf[0] = 0U;
  si->lpf[1] = 1U;
  for (k = 2U; k < si->I; k++)
    {
      unsigned int p, c = k;
      for (p = 2U; p * p <= c; p += 1U + p % 2U)
        {
          while (c % p == 0U)
            c /= p;
          if (c == 1U)
            break;
        }
      si->lpf[k] = (c == 1U) ? p : c;
    }

  fprintf(output, "# bucket_region = %u\n", bucket_region);
  fprintf(output, "# nb_buckets = %u\n", si->nb_buckets);
  fprintf(output, "# bucket_limit = %u\n", si->bucket_limit);
}


/* Finds prime factors p < lim of n and returns a pointer to a zero-terminated
   list of those factors. Repeated factors are stored only once. */
static fbprime_t *
factor_small (mpz_t n, fbprime_t lim)
{
  unsigned long p;
  unsigned long l; /* number of prime factors */
  fbprime_t *f;

  l = 0;
  f = (fbprime_t*) malloc (sizeof (fbprime_t));
  for (p = 2; p <= lim; p = getprime (p))
    {
      if (mpz_divisible_ui_p (n, p))
        {
          l ++;
          f = (fbprime_t*) realloc (f, (l + 1) * sizeof (fbprime_t));
          f[l - 1] = p;
        }
    }
  f[l] = 0; /* end of list marker */
  getprime (0);
  return f;
}

static void
sieve_info_update (sieve_info_t *si)
{
  unsigned int k;
  double invq;
  
  if (si->verbose)
    fprintf (si->output, "# I=%u; J=%u\n", si->I, si->J);

  /* update number of buckets */
  
  si->nb_buckets = 1 + (si->I * si->J - 1) / bucket_region;
  
  /* Update floating point version of algebraic poly */
  if (!si->ratq)
      invq = 1.0 / (double) si->q;
  else 
      invq = 1.0;
  for (k = 0; k <= si->degree; k++)
    si->fijd[k] = mpz_get_d (si->fij[k]) * invq;
}

static void
sieve_info_clear (sieve_info_t *si)
{
  unsigned int d = si->degree;
  unsigned int k;

  for (k = 0; k <= d; k++)
    mpz_clear (si->fij[k]);
  free (si->fij);
  mpz_clear (si->gij[0]);
  mpz_clear (si->gij[1]);
  free (si->fijd);
  free (si->lpf);
}

/*****************************************************************************/

// Redc_32 based on 64-bit arithmetic
// Assume:
//   * p is an odd prime < 2^32. FIXME: p < 2^31? (see below)
//   * invp is -1/p mod 2^32.
//   * x is some integer in [0, 2^32*p[
// Compute:
//   * x/2^32 mod p as an integer in [0, p[
static inline uint32_t
redc_u32(const uint64_t x, const uint32_t p, const uint32_t invp)
{
    uint32_t t = x*invp;                            /* t = x * invp mod 2^32 */
    uint32_t y = (x + (uint64_t)t*(uint64_t)p) >> 32; /* x + t*p is bounded
            by 2^32*p-1+(2^32-1)*p < 2*2^32*p: we might want p < 2^31 so that
            there is no overflow */
    if (y >= p)
        y -= p;
    ASSERT(y<p);
    return y;
}

// Signed redc_32 based on 64-bit arithmetic
// Assume:
//   * p is an odd prime < 2^32.
//   * invp is -1/p mod 2^32.
//   * x is some signed integer in ]-2^32*p, 2^32*p[
// Compute:
//   * x/2^32 mod p as an integer in [0, p[
static inline uint32_t
redc_32(const int64_t x, const uint32_t p, const uint32_t invp)
{
    uint32_t t = ((uint64_t)x)*invp;
    uint32_t y = (x + (uint64_t)t*(uint64_t)p) >> 32;
    // might be too large by p, or too small by p.
    if ((int32_t)y < 0)
        y += p;
    else if (y >= p)
        y -= p;
#ifndef NDEBUG
    if (y >= p) {
        fprintf(stderr, "BUG in redc_32. x = %" PRId64
                " p = %u, invp = %u, y = %d\n", x, p, invp, y);
        ASSERT(0);
    }
#endif

    return y;
}

NOPROFILE_INLINE int
invmod (uint64_t *pa, uint64_t b)
{
  modulusul_t m;
  residueul_t r;
  int rc;
  modul_initmod_ul (m, b);
  modul_init (r, m);
  modul_set_ul (r, *pa, m); /* With mod reduction */
  if ((rc = modul_inv(r, r, m)))
    *pa = modul_get_ul (r, m);
  modul_clear (r, m);
  modul_clearmod (m);
  return rc;
}


/* Define to 0 or 1. Table lookup seems to be slightly faster than
   ctzl() on Opteron and slightly slower on Core2, but doesn't really
   make much difference in either case. */
#define LOOKUP_TRAILING_ZEROS 1

// Compute 2^32/a mod b for b odd,
// and 1/a mod b for b even, by binary xgcd.
// a must be less than b.
// a is modified in place
// return 1 on succes, 0 on failure
NOPROFILE_INLINE int
invmod_REDC(uint64_t *pa, uint64_t b) {
#if LOOKUP_TRAILING_ZEROS
  static const unsigned char trailing_zeros[256] =
    {8,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
     5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
     6,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
     5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
     7,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
     5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
     6,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
     5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0};
#endif
  uint64_t a, u, v, fix, p = b;
  int t, lsh;
  a = *pa;

  if (UNLIKELY(*pa == 0))
    return 0; /* or we get infinite loop */

  if (UNLIKELY(b % 2UL == 0))
    return invmod(pa, b);

  fix = (b+1)>>1;

  ASSERT (a < b);

  u = 1; v = 0; t = 0;

  // make a odd
  lsh = ctzl(a);
  a >>= lsh;
  t += lsh;
  /* v <<= lsh; ??? v is 0 here */

  // Here a and b are odd, and a < b
  do {
    do {
      b -= a; v += u;
#if LOOKUP_TRAILING_ZEROS
      do {
	lsh = trailing_zeros [(unsigned char) b];
	b >>= lsh;
	t += lsh;
	u <<= lsh;
      } while (lsh == 8);
#else
      lsh = ctzl(b);
      b >>= lsh;
      t += lsh;
      u <<= lsh;
#endif
    } while (a<b);
    if (UNLIKELY(a == b))
      break;
    do {
      a -= b; u += v;
#if LOOKUP_TRAILING_ZEROS
      do {
	lsh = trailing_zeros [(unsigned char) a];
	a >>= lsh;
	t += lsh;
	v <<= lsh;
      } while (lsh == 8);
#else
      lsh = ctzl(a);
      a >>= lsh;
      t += lsh;
      v <<= lsh;
#endif
    } while (b < a);
  } while (a != b);
  if (a != 1)
    return 0;

  // Here, the inverse of a is u/2^t mod b.
  while (t>32) {
    unsigned long sig = u & 1UL;
    u >>= 1;
    if (sig)
      u += fix;
    --t;
  }
  while (t < 32)
    {
      u <<= 1;
      if (u >= p)
        u -= p;
      t ++;
    }
  *pa = u;
  return 1;
}

MAYBE_UNUSED
static inline fbprime_t
invmod_po2 (fbprime_t n)
{
  fbprime_t r;

  ASSERT (n % 2 != 0);

  r = (3 * n) ^ 2;
  r = 2 * r - r * r * n;
  r = 2 * r - r * r * n;
  r = 2 * r - r * r * n;
  if (sizeof (fbprime_t) > 4)
    r = 2 * r - r * r * n;

  return r;
}

/* Return if a is divisible by 3 */
static inline int 
is_divisible_3_u32 (uint32_t a)
{
  return (a * (- (UINT32_MAX / 3)) <= UINT32_MAX / 3);
}

// Compute the root r describing the lattice inside the q-lattice
// corresponding to the factor base prime (p,R).
// Formula: r = - (a1-R*b1)/(a0-R*b0) mod p
// Assumes p < 2^32

/* General version of the lattice transform function. Allows projective
   roots in input and output, and handles prime powers.
   In input, if the root is projective, say s/t (mod p) and t is
   non-invertible (mod p), then we expect R = p + (t/s mod p).
   On output, if the root is projective, say u/v (mod p) and v is
   non-invertible (mod p), then return value r = p + (v/u mod p).
   So projective roots are stored as their reciprocal, and have p added
   to signal the fact that it's a reciprocal value.
*/

NOPROFILE_INLINE fbprime_t
fb_root_in_qlattice (const fbprime_t p, const fbprime_t R,
        const uint32_t invp, const sieve_info_t * si)
{
  int64_t aux1, aux2;
  uint64_t u, v;
  fbprime_t add;

    /* Handle powers of 2 separately, REDC doesn't like them */
    if (UNLIKELY(p % 2 == 0))
      {
	fbprime_t u, v;
	ASSERT(p == (p & -p)); /* Test that p is power of 2 */
	if (R < p) /* Root in a,b-plane is non-projective */
	  {
	    u = R * si->b1 - si->a1;
	    v = si->a0 - R * si->b0;
	  }
	else /* Root in a,b-plane is projective */
	  {
	    u = si->b1 - (R - p) * si->a1;
	    v = (R - p) * si->a0 - si->b0;
	  }

	if (v % 2 != 0)
	  {
	    /* root in i,j-plane is non-projective */
	    v = invmod_po2 (v);
	    return (u * v) & (p - 1);
	  }
	else
	  {
	    /* root in i,j-plane is projective */
	    u = invmod_po2 (u);
	    return p + ((u * v) & (p - 1));
	  }
      }

    // Use Signed Redc for the computation:
    // Numerator and denominator will get divided by 2^32, but this does
    // not matter, since we take their quotient.

    if (LIKELY(R < p)) /* Root in a,b-plane is affine */
      {
	aux1 = ((int64_t)R)*((int64_t)si->b1) - ((int64_t)si->a1);
	aux2 = ((int64_t)si->a0) - ((int64_t)R)*((int64_t)si->b0);
      }
    else /* Root in a,b-plane is projective */
      {
	aux1 = ((int64_t)si->b1) - ((int64_t)(R - p))*((int64_t)si->a1);
	aux2 = ((int64_t)(R - p))*((int64_t)si->a0) - ((int64_t)si->b0);
      }
    u = redc_32(aux1, p, invp); /* 0 <= u < p */
    v = redc_32(aux2, p, invp); /* 0 <= den < p */

    add = 0;
    if (UNLIKELY(!invmod_REDC(&v, p)))
      {
	/* root in i,j-plane is projective */
	if (UNLIKELY(!invmod_REDC(&u, p)))
          {
            fprintf (stderr, "Error, root in (i,j)-plane is projective\n");
            exit (EXIT_FAILURE); /* Should never happen! */
          }
	add = p;
      }
    u *= v;
    return (fbprime_t) redc_u32 (u, p, invp) + add;
}



/*
 * Algorithm by Franke and Kleinjung for lattice sieving of largish
 * primes.
 */

typedef struct {
    int32_t a0,b0;
    uint32_t a1,b1;
} plattice_info_t;

// Proposition 1 of [FrKl05]:
// Compute a basis <(alpha, beta), (gamma, delta)> of the p-lattice
// inside the q-lattice, such that
//    beta, delta > 0
//    -I < alpha <= 0 <= gamma < I
//    gamma-alpha >= I
//
// Sizes:
//    p is less than 32 bits and I fits easily in 32 bits.
//    So, alpha and beta fit easily in 32 bits, since they are less than I
//    Now, gamma and delta are also bounded by p, so 32 bits is enough
//    However: a and c can be as large as p*I (not both ?).
//    We still store them in 32 bits, since if they are larger, it means
//    that as soon as they are added to the offset for S, the index will
//    be out of range for S and the loop stops. Hence, this is safe to
//    replace a and c by a large value within 32 bits, when they are
//    larger than 32 bits.
//    Except that the choice of this ``large value'' requires some
//    caution. We need a value which can be used either for a or c, or
//    both, so that adding a, or c, or both, to a value within [0,IJ[ is
//    guaranteed to exceed IJ, but can't wrap around. Up to I=15, it's
//    rather easy. With the rescaling of J, at worst we may have IJ
//    within the range [2^29,2^30[. Thus if a and c are set to 2^30-1,
//    a.k.a INT32_MAX/2, then adding either, or the sum of both, to a
//    valid value x is guaranteed to be at most 3*2^30, which fits within
//    32 bits.
//    For I=16, it's much, much harder. Unless somebody comes up with a
//    nice idea, I see no way to avoid 64-bit arithmetic (which has some
//    cost, sure, but not terribly expensive). For consistency, we make
//    all data types for x, a, and c 64-bit in this case, and choose the
//    overflow constants as UINT32_MAX.
//

#ifdef SUPPORT_I16
typedef uint64_t plattice_x_t;
#else
typedef uint32_t plattice_x_t;
#endif

// Return value:
// * non-zero if everything worked ok
// * zero when the algorithm failed. This can happen when p is a prime power,
//   and g, gcd(p,r) >= I, since then the subtractive Euclidean algorithm will
//   yield (a0=g, b0=0) at some point --- or the converse --- and the loop
//   while (|a0| >= I) a0 += b0 will loop forever.
//
// Note that on a c166 example, this code alone accounts for almost 20%
// of the computation time.

NOPROFILE_INLINE int
reduce_plattice(plattice_info_t *pli, const fbprime_t p, const fbprime_t r, const sieve_info_t * si)
{
    int32_t I = si->I;
    int32_t a0=-(int32_t)p, a1=0, b0=r, b1=1;
    int32_t hI = I;
#if MOD2_CLASSES_BS
    hI/=2;
#endif
    /* subtractive variant of Euclid's algorithm */
    for(;;) {
        /* a0 < 0 <= b0 < -a0 */
        if (b0 < hI) break;
        /* a0 < 0 < b0 < -a0 */
        for( ; a0 += b0, a1 += b1, a0 + b0 <= 0 ; );
        /* -b0 < a0 <= 0 < b0 */
        if (-a0 < hI) break;
        /* -b0 < a0 < 0 < b0 */
        for( ; b0 += a0, b1 += a1, b0 + a0 >= 0 ; );
        /* a0 < 0 <= b0 < -a0 */
    }
    if (b0 > -a0) {
        if (UNLIKELY(a0 == 0)) return 0;
        /* Now that |a0| < hI, we switch to classical division, since
           if say |a0|=1 and b0 is large, the subtractive variant
           will be very expensive.
           We want b0 + k*a0 < hI, i.e., b0 - hI + 1 <= k*(-a0),
           i.e., k = ceil((b0-hI+1)/a0). */
        int32_t k = 1 + (b0 - hI) / (-a0);
        b0 += k * a0;
        b1 += k * a1;
    } else {
        if (UNLIKELY(b0 == 0)) return 0;
        /* we switch to the classical algorithm here too */
        int32_t k = 1 + (-a0 - hI) / b0;
        a0 += k * b0;
        a1 += k * b1;
    }
    ASSERT (a1 > 0);
    ASSERT (b1 > 0);
    ASSERT ((a0 <= 0) && (a0 > -hI));
    ASSERT ((b0 >= 0) && (b0 <  hI));
    ASSERT (b0 - a0 >= hI);

    pli->a0 = a0;
    pli->a1 = a1;
    pli->b0 = b0;
    pli->b1 = b1;

#if 0
    int32_t J = si->J;
#if MOD2_CLASSES_BS
#if 1
#endif

#if 1 || !MOD2_CLASSES
    // left-shift everybody, since the following correspond to the
    // lattice 2p.
    a0 <<= 1; a1 <<= 1;
    b0 <<= 1; b1 <<= 1;
#endif
#endif

    pli->a = ((a1 << si->logI) + a0);
    pli->c = ((b1 << si->logI) + b0);
    if (a1 > J || (a1 == J && a0 > 0)) { pli->a = INT32_MAX/2; }
    if (b1 > J || (b1 == J && b0 > 0)) { pli->c = INT32_MAX/2; }

    /* It's difficult to encode everybody in 32 bits, and still keep
     * relevant information...
     */
    pli->bound0 = -a0;
    pli->bound1 = I - b0;
#endif
    return 1;
}

#if MOD2_CLASSES_BS
#define PLI_COEFF(pli, ab01) (pli->ab01 << 1)
#else
#define PLI_COEFF(pli, ab01) (pli->ab01)
#endif
static inline plattice_x_t plattice_a(const plattice_info_t * pli, const sieve_info_t * si)
{
    int32_t a0 = PLI_COEFF(pli, a0);
    uint32_t a1 = PLI_COEFF(pli, a1);
    if (a1 > (uint32_t) si->J || (a1 == (uint32_t) si->J && a0 > 0))
#ifdef SUPPORT_I16
        return UINT32_MAX;
#else
        return INT32_MAX/2;
#endif
    else
        return (a1 << si->logI) + a0;
}

static inline plattice_x_t plattice_c(const plattice_info_t * pli, const sieve_info_t * si)
{
    int32_t b0 = PLI_COEFF(pli, b0);
    uint32_t b1 = PLI_COEFF(pli, b1);
    if (b1 > (uint32_t) si->J || (b1 == (uint32_t) si->J && b0 > 0))
#ifdef SUPPORT_I16
        return UINT32_MAX;
#else
        return INT32_MAX/2;
#endif
    else
        return (b1 << si->logI) + b0;
}

static inline uint32_t plattice_bound0(const plattice_info_t * pli, const sieve_info_t * si MAYBE_UNUSED)
{
    return - PLI_COEFF(pli, a0);
}

static inline uint32_t plattice_bound1(const plattice_info_t * pli, const sieve_info_t * si)
{
    return si->I - PLI_COEFF(pli, b0);
}


/* This is for working with congruence classes only */
NOPROFILE_INLINE
plattice_x_t plattice_starting_vector(const plattice_info_t * pli, const sieve_info_t * si, int par MAYBE_UNUSED)
{
    /* With MOD2_CLASSES_BS set up, we have computed by the function
     * above an adapted basis for the band of total width I/2 (thus from
     * -I/4 to I/4). This adapted basis is in the coefficients a0 a1 b0
     *  b1 of the pli data structure.
     *
     * Now as per Proposition 1 of FrKl05 applied to I/2, any vector
     * whose i-coordinates are within ]-I/2,I/2[ (<ugly>We would like a
     * closed interval on the left. Read further on for that case</ugly>)
     * can actually be written as a combination with positive integer
     * coefficients of these basis vectors a and b.
     *
     * We also know that the basis (a,b) has determinant p, thus odd. The
     * congruence class mod 2 that we want to reach is thus accessible.
     * It is even possible to find coefficients (k,l) in {0,1} such that
     * ka+lb is within this congruence class. This means that we're going
     * to consider either a,b,or a+b as a starting vector. The
     * i-coordinates of these, as per the properties of Proposition 1, are
     * within ]-I/2,I/2[. Now all other vectors with i-coordinates in
     * ]-I/2,I/2[ which also belong to the same congruence class, can be
     * written as (2k'+k)a+(2l'+l)b, with k' and l' necessarily
     * nonnegative.
     *
     * The last ingredient is that (2a, 2b) forms an adapted basis for
     * the band of width I with respect to the lattice 2p. It's just an
     * homothety.
     *
     * To find (k,l), we proceed like this. First look at the (a,b)
     * matrix mod 2:
     *                 a0&1    a1&1
     *                 b0&1    b1&1
     * Its determinant is odd, thus the inverse mod 2 is:
     *                 b1&1    a1&1
     *                 b0&1    a0&1
     * Now the congruence class is given by the parity argument. The
     * vector is:
     *                par&1,   par>>1
     * Multiplying this vector by the inverse matrix above, we obtain the
     * coordinates k,l, which are:
     *            k = (b1&par&1)^(b0&(par>>1));
     *            l = (a1&par&1)^(a0&(par>>1));
     * Now our starting vector is ka+lb. Instead of multiplying by k and
     * l with values in {0,1}, we mask with -k and -l, which both are
     * either all zeroes or all ones in binary
     *
     */
    /* Now for the extra nightmare. Above, we do not have the guarantee
     * that a vector whose i-coordinate is precisely -I/2 has positive
     * coefficients in our favorite basis. It's annoying, because it may
     * well be that if such a vector also has positive j-coordinate, then
     * it is in fact the first vector we will meet. An example is given
     * by the following data:
     *
            f:=Polynomial(StringToIntegerSequence("
                -1286837891385482936880099433527136908899552
                55685111236629140623629786639929578
                13214494134209131997428776417
                -319664171270205889372
                -17633182261156
                40500"));

            q:=165017009; rho:=112690811;
            a0:=52326198; b0:=-1; a1:=60364613; b1:=2;
            lI:=13; I:=8192; J:=5088;
            p:=75583; r0:=54375;
            > M;
            [-2241    19]
            [ 1855    18]
            > M[1]-M[2];
            (-4096     1)

    * Clearly, above, for the congruence class (0,1), we must start with
    * this vector, not with the sum.
    */
#if !MOD2_CLASSES_BS
    /* In case we don't consider congruence classes at all, then there is
     * nothing very particular to be done */
    plattice_x_t x = (1 << (si->logI-1));
    uint32_t i = x;
    if (i >= plattice_bound1(pli, si)) x += plattice_a(pli, si);
    if (i <  plattice_bound0(pli, si)) x += plattice_c(pli, si);
    return x;
#else
    int32_t a0 = pli->a0;
    int32_t a1 = pli->a1;
    int32_t b0 = pli->b0;
    int32_t b1 = pli->b1;

    int k = -((b1&par&1)^(b0&(par>>1)));
    int l = -((a1&par&1)^(a0&(par>>1)));
    int32_t v[2]= { (a0&k)+(b0&l), (a1&k)+(b1&l)};

    /* handle exceptional case as described above */
    if (k && l && a0-b0 == -(1 << (si->logI-1)) && a1 > b1) {
        v[0] = a0-b0;
        v[1] = a1-b1;
    }

    if (v[1] > (int32_t) si->J)
#ifdef SUPPORT_I16
        return UINT32_MAX;
#else
        return INT32_MAX/2;
#endif
    return (v[1] << si->logI) | (v[0] + (1 << (si->logI-1)));
#endif
}

/***************************************************************************/
/* {{{ las_report: Structure for gathering reports on sieving */
struct las_report_s {
    unsigned long reports;
    unsigned long survivors0;
    unsigned long survivors1;
    unsigned long survivors2;
    double tn[2];
    double ttsm;
    double ttf;
    unsigned long report_sizes[2][256];
};
typedef struct las_report_s las_report[1];
typedef struct las_report_s * las_report_ptr;
typedef const struct las_report_s * las_report_srcptr;

static void las_report_init(las_report_ptr p)
{
    memset(p, 0, sizeof(las_report));
}

static void las_report_clear(las_report_ptr p)
{
    memset(p, 0, sizeof(las_report));
}

static void las_report_accumulate(las_report_ptr p, las_report_ptr q)
{
    p->reports += q->reports;
    p->survivors0 += q->survivors0;
    p->survivors1 += q->survivors1;
    p->survivors2 += q->survivors2;
    p->ttsm    += q->ttsm;
    p->ttf     += q->ttf;
    for(int side = 0 ; side < 2 ; side++) {
        p->tn[side]  += q->tn[side];
        for (int i = 0; i < 256; ++i) {
            p->report_sizes[side][i] += q->report_sizes[side][i];
        }
    }
    memset(q, 0, sizeof(las_report));
}
/* }}} */

/***************************************************************************/
/* {{{ Structures for small sieves */

/* TODO: move the definition of these small_* structures closer to the
 * code for small sieves, to the extent possible */

typedef struct {
    fbprime_t p;
    fbprime_t r;        // in [ 0, p [
    fbprime_t offset;   // in [ 0, p [
    int next_position;  // start of the sieve for next bucket_region
    unsigned char logp;
} small_typical_prime_data_t;

typedef struct {
    fbprime_t g, q, U;
    int next_position;
    unsigned char logp;
} small_bad_prime_data_t;

typedef struct {
    // primes with non-projective root
    small_typical_prime_data_t *nice_p;
    // primes with projective root
    small_bad_prime_data_t *bad_p;
    int nb_nice_p;
    int nb_bad_p;
} small_sieve_data_t;

/* }}} */

/***************************************************************************/
/********        Main bucket sieving functions                    **********/

/* All of this exists _for each thread_ */
struct thread_side_data_s {
    small_sieve_data_t ssd;  /* only for process_region */
    fbprime_t pmax;          /* XXX seems to be currently unused */
    bucket_array_t BA;
    factorbase_degn_t *fb_bucket; /* in reality a pointer into a shared array */
};
typedef struct thread_side_data_s thread_side_data[1];
typedef struct thread_side_data_s * thread_side_data_ptr;
typedef const struct thread_side_data_s * thread_side_data_srcptr;

struct thread_data_s {
    int id;
    thread_side_data sides[2];
    sieve_info_t *si;
    las_report rep;
};
typedef struct thread_data_s thread_data[1];
typedef struct thread_data_s * thread_data_ptr;
typedef const struct thread_data_s * thread_data_srcptr;

/* {{{ disabled dispatch_fb code. */
#ifdef SPLIT_BUCKET_FB_PER_THREAD
#error "disabled"
static void dispatch_fb(factorbase_degn_t ** fb_dst, factorbase_degn_t ** fb_main, factorbase_degn_t * fb0, int nparts)
{
    /* Given fb0, which is a pointer in the fb array * fb_main, allocates
     * fb_dst[0] up to fb_dst[nparts-1] as independent fb arrays, each of
     * appropriate length to contain equivalent portions of the _tail_ of
     * the fb array fb_main, starting at pointer fb0. Reallocates *fb_main
     * in the end.
     */
    /* Start by counting, unsurprisingly */
    size_t * fb_sizes = (size_t *) malloc(nparts * sizeof(size_t));
    memset(fb_sizes, 0, nparts * sizeof(size_t));
    size_t headsize = 0;
    int i = 0;
    for(factorbase_degn_t * fb = *fb_main ; fb < fb0 ; fb = fb_next (fb)) {
        size_t sz = fb_entrysize (fb); 
        headsize += sz;
    }
    for(factorbase_degn_t * fb = fb0 ; fb->p != FB_END ; fb = fb_next (fb)) {
        size_t sz = fb_entrysize (fb); 
        fb_sizes[i] += sz;
        i++;
        i %= nparts;
    }
    factorbase_degn_t ** fbi = (factorbase_degn_t **) malloc(nparts * sizeof(factorbase_degn_t *));
    for(i = 0 ; i < nparts ; i++) {
        // add one for end marker
        fb_sizes[i] += sizeof(factorbase_degn_t);
        fb_dst[i] = (factorbase_degn_t *) malloc(fb_sizes[i]);
        fbi[i] = fb_dst[i];
    }
    free(fb_sizes); fb_sizes = NULL;
    i = 0;
    int k = 0;
    for(factorbase_degn_t * fb = fb0 ; fb->p != FB_END ; fb = fb_next (fb)) {
        k++;
        size_t sz = fb_entrysize (fb); 
        memcpy(fbi[i], fb, sz);
        fbi[i] = fb_next(fbi[i]);
        i++;
        i %= nparts;
    }
    for(i = 0 ; i < nparts ; i++) {
        memset(fbi[i], 0, sizeof(factorbase_degn_t));
        fbi[i]->p = FB_END;
    }
    free(fbi); fbi = NULL;
    *fb_main = realloc(*fb_main, (headsize + sizeof(factorbase_degn_t)));
    fb0 = fb_skip(*fb_main, headsize);
    memset(fb0, 0, sizeof(factorbase_degn_t));
    fb0->p = FB_END;
}
#endif
/* }}} */

void
fill_in_buckets(thread_data_ptr th, int side)
{
    factorbase_degn_t * fb = th->sides[side]->fb_bucket;
    fbprime_t pmax = th->sides[side]->pmax;
    const sieve_info_t * si = th->si;
    bucket_array_t BA = th->sides[side]->BA;  /* local copy */
    // Loop over all primes in the factor base >= bucket_thresh
    // and up to FB_END or pmax (pmax included)

    /* Skip forward to first FB prime p >= si->bucket_thresh */
    while (fb->p != FB_END && fb->p < (fbprime_t) si->bucket_thresh)
        fb = fb_next (fb); // cannot do fb++, due to variable size !

    /* Init first logp value */
    if (fb->p != FB_END && fb->p <= pmax)
      bucket_new_logp (&BA, fb->plog);

    while (fb->p != FB_END && fb->p <= pmax) {
        unsigned char nr;
        fbprime_t p = fb->p;
        unsigned char logp = fb->plog;
        ASSERT_ALWAYS (p % 2 == 1);

        /* Write new set of pointers if the logp value changed */
        bucket_new_logp (&BA, logp);

        /* If we sieve for special-q's smaller than the factor
           base bound, the prime p might equal the special-q prime q. */
        if (UNLIKELY(p == si->q))
          goto next_fb;

        for (nr = 0; nr < fb->nr_roots; ++nr) {
            const uint32_t I = si->I;
            const int logI = si->logI;
            const uint32_t even_mask = (1U << logI) | 1U;
            const uint32_t maskI = I-1;
            const uint32_t maskbucket = bucket_region - 1;
            const int shiftbucket = LOG_BUCKET_REGION;
            const uint32_t IJ = si->I * si->J;
            fbprime_t r, R;

            R = fb->roots[nr];
            r = fb_root_in_qlattice(p, R, fb->invp, si);
            // TODO: should be line sieved in the non-bucket phase?
            // Or should we have a bucket line siever?
            if (UNLIKELY(r == 0))
              {
                /* If r == 0 (mod p), this prime hits for i == 0 (mod p),
                   but since p > I, this implies i = 0 or i > I. We don't
                   sieve i > I. Since gcd(i,j) | gcd(a,b), for i = 0 we
                   only need to sieve j = 1 */
                /* x = j*I + (i + I/2) = I + I/2 */
                bucket_update_t update;
                uint32_t x = I + I / 2;
                update.x = (uint16_t) (x & maskbucket);
                update.p = bucket_encode_prime (p);
                push_bucket_update(BA, x >> shiftbucket, update);
                continue;
              }
            if (UNLIKELY(r == p))
              {
                /* r == p means root at infinity, which hits for
                   j == 0 (mod p). Since q > I > J, this implies j = 0
                   or j > J. This means we sieve only (i,j) = (1,0) here.
                   Since I < bucket_region, this always goes in bucket 0.
                   FIXME: what about (-1,0)? It's the same (a,b) as (1,0)
                   but which of these two (if any) do we sieve? */
                bucket_update_t update;
                update.x = (uint16_t) I / 2 + 1;
                update.p = bucket_encode_prime (p);
                push_bucket_update(BA, 0, update);
                continue;
              }
            if (UNLIKELY(r > p))
              {
                continue;
              }

            /* If working with congruence classes, once the loop on the
             * parity goes at the level above, this initialization
             * should in fact either be done for each congruence class,
             * or saved for later use within the factor base structure.
             */
            plattice_info_t pli;
            if (reduce_plattice(&pli, p, r, si) == 0)
              {
                  pthread_mutex_lock(&io_mutex);
                  fprintf (stderr, "# fill_in_buckets: reduce_plattice() "
                          "returned 0 for p = " FBPRIME_FORMAT ", r = "
                          FBPRIME_FORMAT "\n", p, r);
                  pthread_mutex_unlock(&io_mutex);
                  continue; /* Simply don't consider that (p,r) for now.
                             FIXME: can we find the locations to sieve? */
              }

            uint32_t bound0 = plattice_bound0(&pli, si);
            uint32_t bound1 = plattice_bound1(&pli, si);

            for(int parity = MOD2_CLASSES_BS ; parity < (MOD2_CLASSES_BS?4:1) ; parity++) {

                // The sieving point (0,0) is I/2 in x-coordinate
                plattice_x_t x = plattice_starting_vector(&pli, si, parity);
                // TODO: check the generated assembly, in particular, the
                // push function should be reduced to a very simple step.
                bucket_update_t update;
                update.p = bucket_encode_prime (p);
                __asm__("## Inner bucket sieving loop starts here!!!\n");
                plattice_x_t inc_a = plattice_a(&pli, si);
                plattice_x_t inc_c = plattice_c(&pli, si);
                // ASSERT_ALWAYS(inc_a == pli.a);
                // ASSERT_ALWAYS(inc_c == pli.c);
                while (x < IJ) {
                    uint32_t i;
                    i = x & maskI;   // x mod I
                    /* if both i = x % I and j = x / I are even, then
                       both a, b are even, thus we can't have a valid relation */
                    /* i-coordinate = (x % I) - I/2
                       (I/2) % 3 == (-I) % 3, hence
                       3|i-coordinate iff (x%I+I) % 3 == 0 */
                    if (MOD2_CLASSES_BS || (x & even_mask) 
#ifdef SKIP_GCD3
                            && (!is_divisible_3_u32 (i + I) ||
                                !is_divisible_3_u32 (x >> logI))
#endif
                       )
                    {
#if LOG_BUCKET_REGION == 16 && defined(__x86_64__) && defined(__GNUC__)
                        /* The x value in update can be set by a write to 
                           the low word of the register, but gcc does not 
                           do so - it writes the word to memory, then reads 
                           the dword back again. */
                        __asm__ (
                                "movw %1, %w0\n\t"
                                : "+r" (update)
                                : "r" ((uint16_t) (x & maskbucket))
                                );
#else
                        update.x = (uint16_t) (x & maskbucket);
#endif
#ifdef PROFILE
                        /* To make it visible in profiler */
                        *(BA.bucket_write[x >> shiftbucket])++ = update;
#else
                        push_bucket_update(BA, x >> shiftbucket, update);
#endif
                    }
#ifdef TRACE_K
                    if (trace_on_spot_x(x)) {
                        fprintf (stderr, "# Pushed (%u, %u) (%u, %s) to BA[%u]\n",
                                (unsigned int) (x & maskbucket), logp, p, sidenames[side], (unsigned int) (x >> shiftbucket));
                    }
#endif
                    if (i >= bound1) x += inc_a;
                    if (i < bound0)  x += inc_c;
                }
                __asm__("## Inner bucket sieving loop stops here!!!\n");
            }
        }
        int i;
next_fb:
        /* XXX [ET] unimportant, but a packed fb list for each thread
         * would be cleaner. A small try shows no gain, so the code is
         * not in...  */
        for (i = 0; i < si->nb_threads; ++i) {
            fb = fb_next (fb);
            if (fb->p == FB_END || fb->p > pmax) {
                break;
            }
        }
    }
    /* Write back BA so the nr_logp etc get copied to caller */
    th->sides[side]->BA = BA;
}

void * fill_in_buckets_both(thread_data_ptr th)
{
    fill_in_buckets(th, ALGEBRAIC_SIDE);
    fill_in_buckets(th, RATIONAL_SIDE);
    return NULL;
}

void thread_do(thread_data * thrs, void * (*f) (thread_data_ptr))
{
    sieve_info_t * si = thrs[0]->si;
    if (si->nb_threads == 1) {
        /* Then don't bother with pthread calls */
        (*f)(thrs[0]);
        return;
    }
    pthread_t * th = malloc(si->nb_threads*sizeof(pthread_t)); 
    ASSERT_ALWAYS(th);
    for (int i = 0; i < si->nb_threads; ++i) {
        int ret = pthread_create(&(th[i]), NULL, 
		(void * (*)(void *)) f,
                (void *)(thrs[i]));
        ASSERT_ALWAYS(ret == 0);
    }
    for (int i = 0; i < si->nb_threads; ++i) {
        int ret = pthread_join(th[i], NULL);
        ASSERT_ALWAYS(ret == 0);
    }
    free(th);
}


/* Decrease the sieve array entry *S by logp, with underflow checking 
   and tracing if desired. Variables x, bucket_nr, p, si, and caller 
   are used only for trace test and output */

static inline void
sieve_decrease (unsigned char *S, const unsigned char logp, 
                MAYBE_UNUSED const uint16_t x, 
                MAYBE_UNUSED const unsigned int bucket_nr,
                MAYBE_UNUSED const fbprime_t p,
                MAYBE_UNUSED const sieve_info_t *si, 
                MAYBE_UNUSED const int caller)
{
#ifdef TRACE_K
  if (trace_on_spot_Nx(bucket_nr, x))
    fprintf(stderr, "# (%d) Subtract log(" FBPRIME_FORMAT ") = %u from "
            "S[%u] = %hhu, from BA[%u], ", 
            caller, p, logp,
            x, *S, bucket_nr);
#endif

#ifdef CHECK_UNDERFLOW
  if (*S < logp)
    {
      int i;
      unsigned int j;
      int64_t a;
      uint64_t b;
      NxToIJ (&i, &j, bucket_nr, x, si);
      IJToAB (&a, &b, i, j, si);
      if (i % 2 == 0 && j % 2 == 0)
        fprintf (stderr, "# Error, underflow (%d) at (N,x)=(%u, %u), "
                 "(i,j)=(%d, %u), both even!\n", caller, bucket_nr, x, i, j);
        
      else
        fprintf (stderr, "# Error, underflow (%d) at (N,x)=(%u, %u), "
                 "(i,j)=(%d, %u), (a,b)=(%ld, %lu), S[x] = %hhu, log(" 
                 FBPRIME_FORMAT ") = %hhu\n", 
                 caller, bucket_nr, x, i, j, a, b, S, p, logp);
      *S = 0;
    }
  else
    *S -= logp;
#else
  *S -= logp;
#endif

#ifdef TRACE_K
  if (trace_on_spot_Nx(bucket_nr, x))
    fprintf(stderr, "new value is %hhu\n", *S);
#endif
}


NOPROFILE_STATIC void
apply_one_bucket (unsigned char *S, bucket_array_t BA, const int i,
                     sieve_info_t * si MAYBE_UNUSED)
{
    int j = nb_of_updates(BA, i);
    int next_logp_j = 0;
    unsigned char logp = 0;
    bucket_update_t *next_logp_change, *read_ptr;

    /* Having the read_ptr here defeats the whole idea of having 
       nice inline functions to handle all the BA stuff, but I yet 
       need to figure out how to keep gcc from writing 
       BA.bucket_read[i] back to memory and reading it from memory again
       on every get_next_bucket_update()  */
    read_ptr = BA.bucket_read[i];

    /* Init so that first access fetches logp */
    next_logp_j = 0;
    next_logp_change = read_ptr;

    for (; j > 0; --j) {
       uint16_t x;

       /* Do we need a new logp ? */
       if (read_ptr >= next_logp_change)
         {
           ASSERT_ALWAYS (next_logp_j < BA.nr_logp);
           ASSERT_ALWAYS (BA.logp_idx[next_logp_j * BA.n_bucket + i] 
                           == next_logp_change);
           logp = BA.logp_val[next_logp_j++];
           /* Get pointer telling when to fetch new logp next time */
           if (next_logp_j < BA.nr_logp)
             next_logp_change = BA.logp_idx[next_logp_j * BA.n_bucket + i];
           else
             next_logp_change = BA.bucket_write[i]; /* effectively: never */
         }
       
       x = (read_ptr++)->x;
       sieve_decrease (S + x, logp, x, i, 0, si, 1);
    }
}

/* {{{ initializing norms */
/* Knowing the norm on the rational side is bounded by 2^(2^k), compute
   lognorms approximations for k bits of exponent + NORM_BITS-k bits
   of mantissa */
NOPROFILE_STATIC void
init_rat_norms (sieve_info_t *si)
{
  double e, m, norm, h;
  int i, j, k, l, K, L;
  sieve_side_info_ptr rat = si->sides[RATIONAL_SIDE];

  unsigned char *S = rat->S;

  k = (int) ceil (log2 (rat->logmax));
  K = 1 << k;
  l = NORM_BITS - k;
  L = 1 << l;

  /* extract k bits from the exponent, and l bits from the mantissa */
  h = 1.0 / (double) L;
  for (e = 1.0, i = 0; i < K; i++, e *= 2.0)
    {
      /* e = 2^i for 0 <= i < 2^k */
      for (m = 1.0, j = 0; j < L; j++, m += h)
        {
          /* m = 1 + j/2^l for 0 <= j < 2^l */
          norm = m * e;
          /* Warning: since rat->logmax does not usually correspond to
             a power a two, and we consider full binades here, we have to
             take care that values > rat->logmax do not wrap around to 0 */
          norm = log2 (norm);
          if (norm >= rat->logmax)
            S[(i << l) + j] = 255;
          else
            {
              norm = norm * rat->scale;
              S[(i << l) + j] = GUARD + (unsigned char) norm;
            }
        }
    }
}

/* idem as init_rat_norms, but for the algebraic side */
NOPROFILE_STATIC void
init_alg_norms (sieve_info_t *si)
{
  double e, m, norm, h;
  int i, j, k, l, K, L;
  sieve_side_info_ptr alg = si->sides[ALGEBRAIC_SIDE];

  unsigned char *S = alg->S;

  k = (int) ceil (log2 (alg->logmax));
  K = 1 << k;
  ASSERT_ALWAYS(NORM_BITS >= k);
  l = NORM_BITS - k;
  L = 1 << l;

  /* extract k bits from the exponent, and l bits from the mantissa */
  h = 1.0 / (double) L;
  for (e = 1.0, i = 0; i < K; i++, e *= 2.0)
    {
      for (m = 1.0, j = 0; j < L; j++, m += h)
        {
          norm = m * e;
          norm = log2 (norm);
          if (norm >= alg->logmax)
            S[(i << l) + j] = 255;
          else
            {
              norm = norm * alg->scale;
              S[(i << l) + j] = GUARD + (unsigned char) norm;
            }
        }
    }
}
/* }}} */

/* {{{ initialize norms for bucket regions */
/* Initialize lognorms on the rational side for the bucket_region
 * number N.
 * For the moment, nothing clever, wrt discarding (a,b) pairs that are
 * not coprime, except for the line j=0.
 */
NOPROFILE_STATIC void
init_rat_norms_bucket_region (unsigned char *S, int N, sieve_info_t *si)
{
    cado_poly_ptr cpoly = si->cpoly;
    sieve_side_info_ptr rat = si->sides[RATIONAL_SIDE];
    double g1, g0, gi, gj, norm MAYBE_UNUSED, gjj;
    int i MAYBE_UNUSED, halfI MAYBE_UNUSED, l;
    unsigned int j, lastj;
    uint64_t mask = (1 << NORM_BITS) - 1;
    union { double z; uint64_t x; } zx[1];

    l = NORM_BITS - (int) ceil (log2 (rat->logmax));

    /* G_q(i,j) = g1 * (a0*i+a1*j) + g0 * (b0*i+b1*j)
       = (g1*a0+g0*b0) * i + (g1*a1+g0*b1) * j
       = gi * i + gj * j
    */

    g1 = mpz_get_d (cpoly->g[1]);
    g0 = mpz_get_d (cpoly->g[0]);
    gi = g1 * (double) si->a0 + g0 * (double) si->b0;
    gj = g1 * (double) si->a1 + g0 * (double) si->b1;

    if (si->ratq) {
        double invq = 1.0 / (double) si->q;
        gi *= invq;
        gj *= invq;
    }

    /* bucket_region is a multiple of I. Let's find the starting j
     * corresponding to N and the last j.
     */
    halfI = si->I / 2;
    j = (N*bucket_region) >> si->logI;
    lastj = j + (bucket_region >> si->logI);
#ifdef UGLY_HACK
    memset (S, 255, (lastj - j) * si->I);
#else
    /* if j = 0, it will be the first value */
    if (j == 0)
      {
        // compute only the norm for i = 1. Everybody else is 255.
        norm = log2 (fabs (gi));
        norm = norm * rat->scale;
        for (i = -halfI; i < halfI; i++)
          *S++ = UCHAR_MAX;
        S[-halfI + 1] = GUARD + (unsigned char) (norm);
        ++j;
      }
    for (; j < lastj; ++j)
      {
        gjj = gj * (double) j;
        zx->z = gjj - gi * (double) halfI;
        __asm__("### Begin rational norm loop\n");
#ifndef SSE_NORM_INIT
        uint64_t y;
        unsigned char n;
        for (i = 0; i < (int) si->I; i++) {
          /* the double precision number 1.0 has high bit 0 (sign),
             then 11-bit biased exponent 1023, and 52-bit mantissa 0 */
          /* the magic constant here is simply 1023*2^52, where
             1023 is the exponent bias in binary64 */
          y = (zx->x - (uint64_t) 0x3FF0000000000000) >> (52 - l);
          n = rat->S[y & mask];
          ASSERT (n > 0);
          *S++ = n;
          zx->z += gi;
        }
#else
        union { __v2df dble;
            __v2di intg;
            struct {uint64_t y0; uint64_t y1; } intpair;
        } y_vec;
        if ((j & 1) == 1) {
            __v2df gi_vec = { 2*gi, 2*gi };
            __v2di mask_vec = { mask, mask };
            __v2di cst_vec = { (uint64_t) 0x3FF0000000000000,
                (uint64_t) 0x3FF0000000000000 };

            // in spite of the appearance, only the low word gives the shift
            // count. The high word is ignored.
            __v2di shift_value = { (uint64_t)(52 - l), (uint64_t)(52 - l) };
            __v2df z_vec = { zx->z, zx->z+gi };

            for (i = 0; i < halfI; ++i) {
                y_vec.dble = z_vec;
                y_vec.intg -= cst_vec;
                y_vec.intg = _mm_srl_epi64(y_vec.intg, shift_value);
                y_vec.intg &= mask_vec;
                //            *S++ = rat->S[((uint32_t *)(&y_vec.intg))[0]];
                //             *S++ = rat->S[((uint32_t *)(&y_vec.intg))[2]];
                *S++ = rat->S[y_vec.intpair.y0];
                *S++ = rat->S[y_vec.intpair.y1];
                z_vec += gi_vec;
            }
        } else { // skip when i and j are both even
            __v2df gi_vec = { 4*gi, 4*gi };
            __v2di mask_vec = { mask, mask };
            __v2di cst_vec = { (uint64_t) 0x3FF0000000000000,
                (uint64_t) 0x3FF0000000000000 };

            // in spite of the appearance, only the low word gives the shift
            // count. The high word is ignored.
            __v2di shift_value = { (uint64_t)(52 - l), (uint64_t)(52 - l) };
            __v2df z_vec = { zx->z + gi, zx->z+3*gi };

            for (i = 0; i < halfI; i+=2) {
                y_vec.dble = z_vec;
                y_vec.intg -= cst_vec;
                y_vec.intg = _mm_srl_epi64(y_vec.intg, shift_value);
                y_vec.intg &= mask_vec;
                //            *S++ = rat->S[((uint32_t *)(&y_vec.intg))[0]];
                //             *S++ = rat->S[((uint32_t *)(&y_vec.intg))[2]];
                *S++ = 255;
                *S++ = rat->S[y_vec.intpair.y0];
                *S++ = 255;
                *S++ = rat->S[y_vec.intpair.y1];
                z_vec += gi_vec;
            }
        }

#endif
        __asm__("### End rational norm loop\n");
      }
#endif
}

/* {{{ some utility stuff */
#ifdef SSE_NORM_INIT
static inline void
init_fpoly_v2df(__v2df *F, const double *f, const int deg)
{
    int i;
    for (i = 0; i <= deg; ++i) {
        __v2df tmp = { f[i], f[i] };
        F[i] = tmp;
    }
}

static inline __v2df
fpoly_eval_v2df(const __v2df *f, const __v2df x, int d)
{
    __v2df r;
    r = f[d--];
    for (; d>=0; --d)
        r = r * x + f[d];
    return r;
}
#endif
/* }}} */

/* Initialize lognorms on the algebraic side for the bucket
 * number N.
 * Only the survivors of the rational sieve will be initialized, the
 * others are set to 255. Case GCD(i,j)!=1 also gets 255.
 * return the number of reports (= number of norm initialisations)
 */
NOPROFILE_STATIC int
init_alg_norms_bucket_region (unsigned char *alg_S, 
			      const unsigned char *rat_S, const int N, 
			      sieve_info_t *si)
{
  cado_poly_ptr cpoly = si->cpoly;
  sieve_side_info_ptr rat = si->sides[RATIONAL_SIDE];
  sieve_side_info_ptr alg = si->sides[ALGEBRAIC_SIDE];
  unsigned int j, lastj, k, d = cpoly->degree;
  double *t, *u, powj, i, halfI;
  int report = 0, l;
  uint64_t mask = (1 << NORM_BITS) - 1;
  union { double z; uint64_t x; } zx[1];
  uint64_t y;
  unsigned char *T = alg->S;
#ifdef TRACE_K
  unsigned char *orig_alg_S = alg_S;
#endif
  l = NORM_BITS - (int) ceil (log2 (alg->logmax));
  halfI = (double) (si->I >> 1);

  t = si->fijd;
  u = (double*) malloc ((si->degree + 1) * sizeof (double));
  ASSERT(u != NULL);

  /* bucket_region is a multiple of I. Let's find the starting j
   * corresponding to N and the last j.
   */
  j = (N*bucket_region) >> si->logI;
  lastj = j + (bucket_region >> si->logI);
  for (; j < lastj; j++)
    {
      /* scale by j^(d-k) the coefficients of fij */
      for (k = 0, powj = 1.0; k <= d; k++, powj *= (double) j)
        u[d - k] = t[d - k] * powj;

#ifdef SSE_NORM_INIT
      __v2df u_vec[d];
      init_fpoly_v2df(u_vec, u, d);
      double i0 = 0.0 ;
      unsigned char *S_ptr0 = NULL;
      int cpt = 0;
#endif
      /* now compute norms */
      for (i = -halfI; i < halfI; i += 1.0)
        {
#ifdef TRACE_K
	      if (trace_on_spot_Nx(N, alg_S - orig_alg_S )) {
		  fprintf (stderr, "init_alg_norms_bucket_region: "
			   "rat_S[%zd] = %u, -log(prob) = %u\n",
			   alg_S - orig_alg_S, 
			   (unsigned int) *rat_S, 
			   (unsigned int) rat->Bound[*rat_S]);
		}
#endif
          if (rat->Bound[*rat_S] < 127)
            {
//  The SSE version seems to be slower on the algebraic side...
//  Let's forget it for the moment.
//  TODO: try with single precision.
//#ifndef SSE_NORM_INIT
#if 1
              unsigned char n;
              zx->z = fpoly_eval (u, d, i);
              /* 4607182418800017408 = 1023*2^52 */
              y = (zx->x - (uint64_t) 0x3FF0000000000000) >> (52 - l);
              report++;
              n = T[y & mask];
              ASSERT (n > 0);
              *alg_S = n;
#else
              __v2di mask_vec = { mask, mask };
              __v2di cst_vec = { (uint64_t) 0x3FF0000000000000,
                  (uint64_t) 0x3FF0000000000000 };
              __v2di shift_value = { (uint64_t)(52 - l), (uint64_t)(52 - l) };
              report++;
              if (cpt == 0) {
                  i0 = i;
                  cpt++;
                  S_ptr0 = alg_S;
              } else if (cpt == 1) {
                  cpt--;
                  __v2df i_vec = {i0, i};
                  union { __v2df dble;
                      __v2di intg;
                      struct {uint64_t y0; uint64_t y1; } intpair;
                  } fi_vec;
                  fi_vec.dble = fpoly_eval_v2df(u_vec, i_vec, d);
                  fi_vec.intg -= cst_vec;
                  fi_vec.intg = _mm_srl_epi64(fi_vec.intg, shift_value);
                  fi_vec.intg &= mask_vec;
                  *S_ptr0 = T[fi_vec.intpair.y0];
                  *alg_S = T[fi_vec.intpair.y1];
              }
#endif
            }
          else
	    {
	      *alg_S = UCHAR_MAX;
	    }
	  alg_S++;
	  rat_S++;
        }
#ifdef SSE_NORM_INIT
      if (cpt == 1) { // odd number of computations
          zx->z = fpoly_eval (u, d, i0);
          y = (zx->x - (uint64_t) 0x3FF0000000000000) >> (52 - l);
          *S_ptr0 = T[y & mask];
      }
#endif
    }

  free(u);
  return report;
}
/* }}} */

/* {{{ small sieve and resieving */
/* Small primes or powers of small primes p^k with projective root.
   These hit at 
     i*v == j*u (mod p^k) 
   for some u,v in Z, but gcd(v, p^k) > 1.
   We may assume gcd(u,p)==1, or we divide the entire equation by p.
   XXX [ET]: we should also assume that v is a prime power, and that u
   XXX [ET]: is within [0..p^k/v-1[ ; 
   We store g = gcd(v, p^k), q = p^k / g, and U = u * (v/g)^(-1) (mod q).
   XXX [ET]: which would then imply g==v, q=p^k/v, and U=u

   Then we have
     i*v == j*u (mod p^k)  <==>  i == (j/g)*U (mod q)
   with g|j. 
   
   In other words, we can sieve this bad prime (powers) much like a 
   normal prime (power) q with root U, except that after sieving a line 
   we don't advance by one line, but by g lines.
   The case where g = q^k and thus q = 1 can be sieved more efficiently,
   of course, since every entry in each g-th line will be hit, so that
   the sieving should use long word transfers.

   Just like for normal primes, the next_position value points at the first
   position to sieve relative to the start of the current sieve region.

   Within a line that starts at index line_start, for array element of 
   index x, we have x - line_start = i+I/2. 
   We skip j=0, as it contains only the single possible relation 
   (i,j) = (1,0). 
   For j=1*g, we want i=U (mod q), so x - line_start == I/2+U (mod q),
   so we initialise 
     next_position = I*g + (I/2 + U) % q
   to get the first array index in line j=g, 
   then within a line sieve next_position + t*q < I, t in N,
   and update 
     next_position = (next_position - line_start + U) % q + line_start + g*I 
   to get the first position to sieve in the next suitable line. */

/* FIXME: This next_position update above is similar to the offset field
 * for typical primes, except that we have these larger jumps...
 */

/* TODO: put back small_sieve_data_t structure once code movement is
 * finished.
 */
void clear_small_sieve(small_sieve_data_t ssd) {
    free(ssd.nice_p); ssd.nice_p = NULL;
    free(ssd.bad_p); ssd.bad_p = NULL;
}

void
clone_small_sieve(small_sieve_data_t *r, const small_sieve_data_t *s) {
    memcpy(r, s, sizeof(small_sieve_data_t));
    r->nice_p = malloc(r->nb_nice_p*sizeof(small_typical_prime_data_t));
    r->bad_p = malloc(r->nb_bad_p*sizeof(small_bad_prime_data_t));
    ASSERT(!r->nb_nice_p || r->nice_p);
    ASSERT(!r->nb_bad_p || r->bad_p);
    memcpy(r->nice_p, s->nice_p,
            r->nb_nice_p*sizeof(small_typical_prime_data_t));
    memcpy(r->bad_p, s->bad_p,
            r->nb_bad_p*sizeof(small_bad_prime_data_t));
}

/* Assume q is a prime power p^k with k>=1, return p if k > 1, 0 otherwise. */
/* This is cheap */
static fbprime_t
is_prime_power (fbprime_t q)
{
  unsigned int maxk, k;
  uint32_t p;

  for (maxk = 0, p = q; p > 1; p /= 2, maxk ++);
  for (k = maxk; k >= 2; k--)
    {
      p = (fbprime_t) (pow ((double) q, 1.0 / (double) k) + 0.5);
      if (q % p == 0)
        return p;
    }
  return 0;
}

/* Copy those primes in s to r that need to be resieved, i.e., those
   that are not in trialdiv_primes and that are not prime powers */
/* XXX The name chosen for this function is catastrophic. */
static void
copy_small_sieve (small_sieve_data_t *r, const small_sieve_data_t *s, 
		  const fbprime_t *trialdiv_primes)
{
  int i, j, td_idx;

  r->nb_nice_p = 0;
  r->nice_p = NULL;
  if (s->nb_nice_p > 0)
    {
      const size_t size = s->nb_nice_p * sizeof (small_typical_prime_data_t);
      r->nice_p = (small_typical_prime_data_t *) malloc (size);
      ASSERT (r->nice_p != NULL);
      
      td_idx = 0;
      j = 0;
      for (i = 0; i < s->nb_nice_p; i++)
	{
	  if (is_prime_power(s->nice_p[i].p))
	    continue;
	  
	  while (trialdiv_primes[td_idx] != FB_END && 
		 trialdiv_primes[td_idx] < s->nice_p[i].p)
	    td_idx++;
	  
	  if (trialdiv_primes[td_idx] == FB_END || 
	      trialdiv_primes[td_idx] !=s->nice_p[i].p)
	    r->nice_p[j++] = s->nice_p[i];
	}
      
      r->nb_nice_p = j;
      if (j == 0)
	{
	  free (r->nice_p);
	  r->nice_p = NULL;
	}
    }
  
  r->nb_bad_p = 0;
  r->bad_p = NULL;
  if (s->nb_bad_p > 0)
    {
      const size_t size = s->nb_bad_p * sizeof (small_bad_prime_data_t);
      r->bad_p = (small_bad_prime_data_t *) malloc (size);
      ASSERT (r->bad_p != NULL);
      
      td_idx = 0;
      j = 0;
      for (i = 0; i < s->nb_bad_p; i++)
	{
	  /* p^k = q*g, g > 1, so k>1 if g is power or if q > 1 */
	  if (s->bad_p[i].q > 1 || is_prime_power(s->bad_p[i].g))
	    continue;
	  
	  while (trialdiv_primes[td_idx] != FB_END && 
		 trialdiv_primes[td_idx] < s->bad_p[i].g)
	    td_idx++;
	  
	  if ((trialdiv_primes[td_idx] == FB_END || 
	       trialdiv_primes[td_idx] !=  s->bad_p[i].g))
	    r->bad_p[j++] = s->bad_p[i];
	}
      
      r->nb_bad_p = j;
      if (j == 0)
	{
	  free (r->bad_p);
	  r->bad_p = NULL;
	}
    }
}

// Prepare sieving of small primes: initialize a small_sieve_data_t
// structure to be used thereafter during sieving each region.
// next_position points at the next position that will be hit by sieving,
// relative to the start of the next bucket region to sieve. It may exceed I 
// and even BUCKET_REGION
void init_small_sieve(small_sieve_data_t *ssd, const factorbase_degn_t *fb,
                      const sieve_info_t *si, const char side)
{
    const factorbase_degn_t *fb_sav = fb;
    int size = 0, n;
    const unsigned int thresh = si->bucket_thresh;
    const int verbose = 0;
    const int do_bad_primes = 1;

    ASSERT (side == 'a' || side == 'r');

    // Count prime ideals of factor base primes p < thresh
    while (fb->p != FB_END && fb->p < thresh) {
        size += fb->nr_roots;
        fb = fb_next (fb); // cannot do fb++, due to variable size !
    }
    fb = fb_sav;

    // allocate space for these. n is an upper bound, since some of the
    // ideals might become special ones.
    ssd->nice_p = (small_typical_prime_data_t *)
      malloc(size * sizeof(small_typical_prime_data_t));
    n = 0;
    ssd->nb_bad_p = 0;
    ssd->bad_p = NULL;
    // Do another pass on fb and badprimes, to fill in the data
    // while we have any regular primes or bad primes < thresh left
    while (fb->p != FB_END && fb->p < thresh) {
      const fbprime_t p = fb->p;
      int nr;
      fbprime_t r;

      for (nr = 0; nr < fb->nr_roots; nr++)
        {
	  r = fb_root_in_qlattice (p, fb->roots[nr], fb->invp, si);
	  /* If this root is somehow interesting (projective in (a,b) or
	     in (i,j) plane), print a message */
	  if (verbose && (fb->roots[nr] >= p || r >= p))
	    fprintf (si->output, "# init_small_sieve: Side %c, prime " 
		     FBPRIME_FORMAT " root " FBPRIME_FORMAT " -> " 
		     FBPRIME_FORMAT "\n", side, p, fb->roots[nr], r);

	  /* Handle projective roots */
          if (r >= p)
	    {
	      fbprime_t q, g;
	      r -= p;
              /* [ET] If my understanding is correct, a projective root
               * (u:v) with v=p^k and u coprime to p is stored as r=p+u*v
               * (which is <2p), so that recovering v is a gcd, and
               * recovering u is a division.
               */
	      g = gcd_ul (p, r);
              /* If g exceeds J, then the only reached locations in the
               * sieving are will be on line (j=0), thus (1,0) only since
               * the other are equivalent.
               */
	      if (do_bad_primes && g < si->J)
		{
		  // Fill in data for this bad prime
		  /* Shouldn't happen that often so a realloc is ok */
		  ssd->bad_p = (small_bad_prime_data_t *)
		    realloc (ssd->bad_p, (ssd->nb_bad_p + 1) *
			     sizeof (small_bad_prime_data_t));
		  q = p / g;
		  ssd->bad_p[ssd->nb_bad_p].g = g;
		  ssd->bad_p[ssd->nb_bad_p].q = q;
		  if (q == 1)
		    {
		      ASSERT (r == 0);
		      ssd->bad_p[ssd->nb_bad_p].U = 0;
		      ssd->bad_p[ssd->nb_bad_p].next_position = g * si->I;
		    }
		  else
		    {
		      int rc;
		      /* gcd(r / g, q) = 1 here */
		      uint64_t U = r / g;
                      /* XXX [ET]
                       * This invmod is quite probably unnecessary */
		      rc = invmod (&U, q);
		      ASSERT_ALWAYS (rc != 0);
		      ssd->bad_p[ssd->nb_bad_p].U = U;
		      ssd->bad_p[ssd->nb_bad_p].next_position = 
			g * si->I + (si->I / 2 + U) % q;
		    }
		  ssd->bad_p[ssd->nb_bad_p].logp = fb->plog;
		  ssd->nb_bad_p++;
		} 
	      else 
		{
		  if (verbose && !do_bad_primes)
		    fprintf (si->output, "# init_small_sieve: not adding bad "
			     "prime " FBPRIME_FORMAT " to small sieve because "
			     "do_bad_primes = 0\n", g);
		  else if (verbose && g >= si->J)
		    fprintf (si->output, "# init_small_sieve: not adding bad "
			     "prime g = " FBPRIME_FORMAT " to small sieve "
			     " because  g >= si->J = %d\n", g, si->J);
		}
	    }
	  else
	    {
	      // Fill in data for this nice prime
	      ASSERT (n < size);
	      ssd->nice_p[n].p = p;
	      ssd->nice_p[n].r = r;
	      ssd->nice_p[n].logp = fb->plog;
	      ssd->nice_p[n].next_position = (si->I >> 1)%p;
              // The processing of bucket region by nb_threads is interleaved.
              // It means that the positions for the small sieve must jump
              // over the (nb_threads - 1) regions after each region.
              // For typical primes, this jump can be easily precomputed:
              ssd->nice_p[n].offset=(ssd->nice_p[n].r*
                      (bucket_region >> si->logI)*(si->nb_threads-1))%p;
	      /* For powers of 2, we sieve only odd lines (*) and 
	       * next_position needs to point at line j=1. We assume
	       * that in this case (si->I/2) % p == 0
               * (*) for lines with j even, we have a root mod the prime
               * power for i-j*r multiple of our power of 2, which means
               * i even too. Thus a useless report.
               */
	      if (UNLIKELY(p % 2 == 0))
	        {
	          ASSERT (ssd->nice_p[n].next_position == 0);
	          ssd->nice_p[n].next_position = r + si->I;
                }
	      n++;
	    }
        }
      fb = fb_next (fb); // cannot do fb++, due to variable size !
    }
    ssd->nb_nice_p = n;
    if (verbose)
      fprintf (si->output, 
	       "# init_small_sieve: side %c has %d nice and %d bad primes\n",
	       side, ssd->nb_nice_p, ssd->nb_bad_p);
}

static void thread_data_init_small_sieve(thread_data * thrs)
{
    sieve_info_t * si = thrs[0]->si;
    for(int i = 0 ; i < si->nb_threads ; i++) {
        for(int side = 0 ; side < 2 ; side++) {
            init_small_sieve(&(thrs[i]->sides[side]->ssd), si->sides[side]->fb, si, *sidenames[side]);
        }
    }
}

static void thread_data_clear_small_sieve(thread_data * thrs)
{
    sieve_info_t * si = thrs[0]->si;
    for(int i = 0 ; i < si->nb_threads ; i++) {
        for(int side = 0 ; side < 2 ; side++) {
            clear_small_sieve(thrs[i]->sides[side]->ssd);
        }
    }
}

// Update the positions in the small_sieve_data ssd for going up in the
// sieve region by nl lines 
// This takes the position in ref_ssd as a reference.
// For typical primes, and if use_offset is set to 1, one uses the
// precomputed offset to jump without mod p reduction (yet still a
// subtraction, though).
void ssd_update_positions(small_sieve_data_t *ssd, 
        small_sieve_data_t *ref_ssd, sieve_info_t *si, int nl,
        int use_offset)
{
    const int row0_is_oddj = nl & 1;
    // nice primes
    for (int n = 0; n < ssd->nb_nice_p; ++n) {
        unsigned long i0;
        fbprime_t p = ssd->nice_p[n].p;
        fbprime_t r = ssd->nice_p[n].r;

        if (p == 2) {
            /* Make sure that next_position points to a location
               where i and j are not both even */
            /* [ET] previous code was overly complicated. */
            i0 = r;
            if (!row0_is_oddj) i0 += si->I;
            ssd->nice_p[n].next_position = i0;
        } else if (p % 2 == 0) {
            if (row0_is_oddj) {
                i0 = (nl * r) & (p-1);
            } else {
                i0 = (((nl + 1) * r) & (p-1)) + si->I;
            }
            ssd->nice_p[n].next_position = i0;
        } else {
            /* We want to add nl*r to the offset *relative to the 
               start of the line*, but next_position may be larger 
               than I, so we treat the multiple-of-I and mod-I parts
               separately */
            /* XXX [ET] Hmmm. Can one give me a case, beyond 2, where
             * next_position>I ?  */
            ASSERT(p % 2 == 0 || ssd->nice_p[n].next_position < (int) si->I);
            if (use_offset) {
                i0 = ssd->nice_p[n].next_position & (si->I - 1);
                ASSERT (i0 < p);
                i0 += ssd->nice_p[n].offset;
                if (i0 >= p)
                    i0 -= p;
                ssd->nice_p[n].next_position = i0 + 
                    (ssd->nice_p[n].next_position & (~(si->I - 1)));
            } else {
                i0 = ref_ssd->nice_p[n].next_position & (si->I - 1);
                ASSERT (i0 < p);
                i0 += nl*r;
                i0 = i0 % p;
                ssd->nice_p[n].next_position = i0 + 
                    (ref_ssd->nice_p[n].next_position & (~(si->I - 1)));
            }
        }
    }

    // bad primes
    for (int n = 0; n < ssd->nb_bad_p; ++n) {
        /* First line to sieve is the smallest j with g|j and j >= nl,
           however, if nl == 0 we don't sieve j==0 since it contains
           only one possible relation (i,j) = (1,0). */
        unsigned int ng, x, j;
        ng = iceildiv(nl, ssd->bad_p[n].g);
        if (ng == 0)
            ng++;
        x = (si->I / 2 + ng * ssd->bad_p[n].U) % ssd->bad_p[n].q;
        j = ng * ssd->bad_p[n].g;
        ssd->bad_p[n].next_position = (j - nl) * si->I + x;
    }
}


// Sieve small primes (up to p < bucket_thresh) of the factor base fb in the
// next sieve region S.
// Information about where we are is in ssd.
void sieve_small_bucket_region(unsigned char *S, const int bucket_nr,
			       small_sieve_data_t ssd, sieve_info_t *si,
			       const unsigned char side)
{
    const uint32_t I = si->I;
    const fbprime_t pattern2_size = 2 * sizeof(unsigned long);
    unsigned long j;
    int n;
    const int test_divisibility = 0; /* very slow, but nice for debugging */
    const unsigned long nj = bucket_region >> si->logI; /* Nr. of lines 
                                                        per bucket region */
    /* In order to check whether a j coordinate is even, we need to take
     * into account the bucket number, especially in case buckets are as
     * large as the sieve region. The row number corresponding to a given
     * i0 is i0/I, but we also need to add bucket_nr*bucket_size/I to
     * this, which is what this flag is for.
     */
    const int row0_is_oddj = (bucket_nr << (LOG_BUCKET_REGION - si->logI)) & 1;


    /* Handle powers of 2 up to 2 * sizeof(long) separately. 
       TODO: use SSE2 */
    /* First collect updates for powers of two in a pattern,
       then apply pattern to sieve line.
       Repeat for each line in bucket region. */
    for (j = 0; j < nj; j++)
      {
        unsigned long pattern[2] = {0,0};

        /* Prepare the pattern */

        /* This loop assumes that entries in ssd.nice_p are in order of 
           increasing p, or more accurately, that all powers of 2 up to
           2*sizeof(long) appear before any p > 2*sizeof(long) */
        for (n = 0; n < ssd.nb_nice_p && ssd.nice_p[n].p <= pattern2_size; 
             n++)
          if (ssd.nice_p[n].p % 2 == 0) 
            {
              const fbprime_t p = ssd.nice_p[n].p;
              unsigned int i0 = ssd.nice_p[n].next_position;
              if (i0 < I)
                {
                  ASSERT (i0 < p);
                  ASSERT ((nj * bucket_nr + j) % 2 == 1);
                  for (unsigned int i = i0; i < pattern2_size; i += p)
                    ((unsigned char *)pattern)[i] += ssd.nice_p[n].logp;
                  /* Skip two lines above, since we sieve only odd lines.
                   * Even lines would correspond to useless reports.
                   */
                  i0 = ((i0 + 2 * ssd.nice_p[n].r) & (p - 1)) + 2 * I;
                }
              /* In this loop, next_position gets updated to the first 
                 index to sieve relative to the start of the next line, 
                 but after all lines of this bucket region are processed, 
                 it will point the the first position to sieve relative  
                 to the start of the next bucket region, as required */
              ssd.nice_p[n].next_position = i0 - I;
            }
        
        /* Apply the pattern */
        if (pattern[0] || pattern[1]) {
            unsigned long *S_ptr = (unsigned long *) (S + j * I);
            const unsigned long *end = (unsigned long *)(S + j * I + I);
            
#ifdef TRACE_K /* {{{ */
            if (trace_on_range_Nx(bucket_nr, j*I, j*I+I)) {
                unsigned int x = trace_Nx.x;
                unsigned int k = x % I;
                unsigned int v = (((unsigned char *)(pattern+((k/sizeof(unsigned long))&1)))[k%sizeof(unsigned long)]);
                if (v) {
                    fprintf(stderr, "# (2) Subtract log(powers of 2) = %u from "
                            "S[%u] = %hhu, from BA[%u], new value is %hhu\n", 
                            v, x, S[x], bucket_nr, S[x]-v);
                }
            }
#endif /* }}} */

            while (S_ptr < end)
              {
                *(S_ptr) -= pattern[0];
                *(S_ptr + 1) -= pattern[1];
                *(S_ptr + 2) -= pattern[0];
                *(S_ptr + 3) -= pattern[1];
                S_ptr += 4;
              }
          }
      }


    /* Handle 3 */
    /* First collect updates for powers of two in a pattern,
       then apply pattern to sieve line.
       Repeat for each line in bucket region. */
    for (j = 0; j < nj; j++)
      {
        unsigned long pattern[3];

        pattern[0] = pattern[1] = pattern[2] = 0UL;

        for (n = 0; n < ssd.nb_nice_p && ssd.nice_p[n].p <= 3; 
             n++)
          if (ssd.nice_p[n].p == 3) 
            {
              const fbprime_t p = 3;
              unsigned int i0 = ssd.nice_p[n].next_position;
              unsigned int i;
              ASSERT (i0 < p);
              for (i = i0; i < 3 * sizeof(unsigned long); i += p)
                ((unsigned char *)pattern)[i] += ssd.nice_p[n].logp;
              i0 += ssd.nice_p[n].r;
              if (i0 >= p)
                i0 -= p;
              ssd.nice_p[n].next_position = i0;
            }
        
        if (pattern[0])
          {
            unsigned long *S_ptr = (unsigned long *) (S + j * I);
            const unsigned long *end = (unsigned long *)(S + j * I + I) - 2;
            
            while (S_ptr < end)
              {
                *(S_ptr) -= pattern[0];
                *(S_ptr + 1) -= pattern[1];
                *(S_ptr + 2) -= pattern[2];
                S_ptr += 3;
              }
            
            end += 2;
            if (S_ptr < end)
              *(S_ptr++) -= pattern[0];
            if (S_ptr < end)
              *(S_ptr) -= pattern[1];
          }
      }

    for (n = 0 ; n < ssd.nb_nice_p; ++n) {
        const fbprime_t p = ssd.nice_p[n].p;
        const fbprime_t r = ssd.nice_p[n].r;
        const unsigned char logp = ssd.nice_p[n].logp;
        unsigned char *S_ptr = S;
        fbprime_t twop;
        unsigned int i, i0, linestart = 0;
        /* Always S_ptr = S + linestart. S_ptr is used for the actual array
           updates, linestart keeps track of position relative to start of
           bucket region and is used only for computing i,j-coordinates
           in overflow and divisibility checking, and relation tracing. */

        i0 = ssd.nice_p[n].next_position;

        /* Powers of 2 are treated separately */
        if (UNLIKELY(p % 2 == 0))
          {
            /* Don't sieve powers of 2 again that were pattern-sieved */
            if (p <= pattern2_size)
              continue;
            
            for (j = 0; j < nj; j++)
              {
                if (i0 < I)
                  {
                    ASSERT(i0 < p);
                    ASSERT ((nj * bucket_nr + j) % 2 == 1);
                    for (i = i0; i < I; i += p)
                      {
                        if (test_divisibility)
                          test_divisible_x (p, linestart + i, bucket_nr, si, 
                                            side);
                        sieve_decrease (S_ptr + i, logp, 
                                        linestart + i, bucket_nr, p, si, 2);
                      }
                    // odd lines only.
                    i0 = ((i0 + 2 * r) & (p - 1)) + 2 * I;
                  }
                i0 -= I;
                linestart += I;
                S_ptr += I;
              }
            ssd.nice_p[n].next_position = i0;
            continue;
          }

        /* Don't sieve 3 again as it was pattern-sieved */
        if (p == 3)
          continue;
          
        ASSERT(i0 < p);
        for (j = 0; j < nj; j++)
          {
            twop = p;
            i = i0;
            if ((((nj & bucket_nr) ^ j) & 1) == 0) /* (nj*bucket_nr+j)%2 */
              {
                /* for j even, we sieve only odd i. */
                twop += p;
                i += (i0 & 1) ? 0 : p;
              }
            for ( ; i < I; i += twop)
              {
                if (test_divisibility)
                  test_divisible_x (p, linestart + i, bucket_nr, si, side);
                sieve_decrease (S_ptr + i, logp, linestart + i, bucket_nr, 
                                p, si, 3);
              }
            i0 += r;
            if (i0 >= p)
              i0 -= p;
            S_ptr += I;
            linestart += I;
          }
        ssd.nice_p[n].next_position = i0;
    }

    /* Sieve the bad primes. We have p^k | fij(i,j) for i,j such that
       i * g == j * U (mod p^k) where g = p^l and gcd(U, p) = 1. 
       This hits only for g|j, then j = j' * g, and i == j' * U (mod p^(k-l)).
       In every g-th line, we sieve the entries with i == (j/g)*U (mod q).
       In ssd we have stored g, q = p^(k-l), U, and next_position so that
       S + next_position is the next sieve entry that needs to be sieved.
       So if S + next_position is in the current bucket region, we
       update all  S + next_position + n*q  where  next_position + n*q < I,
       then set next_position = ((next_position % I) + U) % q) + I * g.  */
    for (n = 0; n < ssd.nb_bad_p; ++n) {
      if (!test_divisibility && ssd.bad_p[n].q == 1)
        {
	  /* q = 1, therefore U = 0, and we sieve all entries in lines
	     with g|j, beginning with the line starting at S[next_position] */
          unsigned long logps;
          unsigned int i0 = ssd.bad_p[n].next_position;
	  ASSERT (ssd.bad_p[n].U == 0);
          ASSERT (i0 % I == 0);
          ASSERT (I % (4 * sizeof (unsigned long)) == 0);
	  for (j = 0; j < sizeof (unsigned long); j++)
	    ((unsigned char *)&logps)[j] = ssd.bad_p[n].logp;
          while (i0 < (unsigned int) bucket_region)
            {
              unsigned long *S_ptr = (unsigned long *) (S + i0);
              unsigned long *end = S_ptr + I / sizeof (unsigned long);
              unsigned long logps2 = logps;
              /* Check whether the j coordinate is even. */
              if (((i0 & I) == 0) ^ row0_is_oddj) {
                  /* Yes, j is even. We update only odd i-coordinates */
                  /* Use array indexing to avoid endianness issues. */
		  for (j = 0; j < sizeof (unsigned long); j += 2)
		    ((unsigned char *)&logps2)[j] = 0;
                }
#ifdef TRACE_K
              if (trace_on_range_Nx(bucket_nr, i0, i0 + I)) {
                  unsigned int N = N;
                  unsigned int x = trace_Nx.x;
                  unsigned char lo = ((unsigned char *)&logps2)[(trace_Nx.x-i0)%sizeof (unsigned long)];
                  if (1 || trace_Nx.x & 1) {
                      fprintf(stderr, "# (3) Subtract log(" FBPRIME_FORMAT ") = %u[%c,bad] from "
                              "S[%u] = %hhu, from BA[%u], new value is %u\n", 
                              ssd.bad_p[n].g, lo, side,
                              x, S[x], N, S[x] - lo);
                  }
              }
#endif
              while (S_ptr < end)
                {
                  *(S_ptr) -= logps2;
                  *(S_ptr + 1) -= logps2;
                  *(S_ptr + 2) -= logps2;
                  *(S_ptr + 3) -= logps2;
                  S_ptr += 4;
                }
              i0 += ssd.bad_p[n].g * I;
            }
          ssd.bad_p[n].next_position = i0 - (1U << LOG_BUCKET_REGION);
        }
      else
	{
	  /* q > 1, more general sieving code. */
	  const unsigned int i0 = ssd.bad_p[n].next_position;
	  const fbprime_t g = ssd.bad_p[n].g;
	  const fbprime_t q = ssd.bad_p[n].q;
	  const fbprime_t U = ssd.bad_p[n].U;
	  const unsigned char logp = ssd.bad_p[n].logp;
	  const fbprime_t evenq = (q % 2 == 0) ? q : 2 * q;
	  unsigned int lineoffset = i0 & (I - 1U),
	               linestart = i0 - lineoffset;
	  ASSERT (U < q);
	  while (linestart < (1U << LOG_BUCKET_REGION))
	    {
	      unsigned int i = lineoffset;
	      if (((linestart & I) == 0) ^ row0_is_oddj) /* Is j even? */
		{
		  /* Yes, sieve only odd i values */
		  if (i % 2 == 0) /* Make i odd */
		    i += q;
		  if (i % 2 == 1) /* If not both i,q are even */
		    for ( ; i < I; i += evenq)
		      {
			if (test_divisibility)
			  test_divisible_x (g*q, linestart + i, bucket_nr, si, side);
                        sieve_decrease (S + linestart + i, logp, 
                                        linestart + i, bucket_nr, q, si, 4);
		      }
		}
	      else
		{
		  for ( ; i < I; i += q)
		    {
		      if (test_divisibility)
			test_divisible_x (g*q, linestart + i, bucket_nr, 
			                  si, side);
                      sieve_decrease (S + linestart + i, logp, linestart + i, 
                                      bucket_nr, q, si, 5);
		    }
		}
              
	      linestart += g * I;
	      lineoffset += U;
	      if (lineoffset >= q)
	        lineoffset -= q;
	    }
	  ssd.bad_p[n].next_position = linestart + lineoffset - 
	                               (1U << LOG_BUCKET_REGION);
	}
    }
}

/* Sieve small primes (p < I, p not in trialdiv_primes list) of the factor
   base fb in the next sieve region S, and add primes and the x position
   where they divide and where there's a sieve report to a bucket (rather
   than subtracting the log norm from S, as during sieving).
   Information about where we are is in ssd.
   Primes in trialdiv_primes must be in increasing order. */
void
resieve_small_bucket_region (bucket_primes_t *BP, int N, unsigned char *S,
			     small_sieve_data_t *ssd,
			     const sieve_info_t *si)
{
  const uint32_t I = si->I;
  unsigned char *S_ptr;
  unsigned long j, nj;
  const int resieve_very_verbose = 0, resieve_very_verbose_bad = 0;
  /* See comment above about the variable of the same name */
  const int row0_is_oddj = (N << (LOG_BUCKET_REGION - si->logI)) & 1;

  nj = (bucket_region >> si->logI);

  for (int n = 0 ; n < ssd->nb_nice_p; ++n) 
  {
      const fbprime_t p = ssd->nice_p[n].p;
      fbprime_t r;
      unsigned int i0;

      r = ssd->nice_p[n].r;
      i0 = ssd->nice_p[n].next_position;
      S_ptr = S;
      ASSERT(i0 < p);
      /* for j even, we sieve only odd i. This translates into loops
       * which look as follows:
       *
       * j even: (sieve only odd i)
       *   for(i = i0 + (p & -!(i0&1)) ; i < I ; i += p+p)
       *   (where (p & -!(i0&1)) is 0 if i0 is odd, and p otherwise)
       * j odd: (sieve all values of i)
       *   for(i = i0                  ; i < I ; i += p)
       *
       * we may merge the two by setting q=p&-!((j&1)^row0_is_oddj)
       *
       * which, when (j+row0_is_oddj) is even, is p, and is 0
       * otherwise.
       *
       * In turn, since q changes for each j, 1 xor within the loop
       * is enough to make it alternate between 0 and p, once the
       * starting value is correct.
       */
      unsigned int q = p&-!row0_is_oddj;
      for (j = 0; j < nj; j ++)
      {
          for (unsigned int i = i0 + (q& -!(i0&1)) ; i < I; i += p+q) {
              if (S_ptr[i] == 255) continue;
              bucket_prime_t prime;
              unsigned int x = (j << (si->logI)) + i;
              if (resieve_very_verbose) {
                  pthread_mutex_lock(&io_mutex);
                  fprintf (stderr, "resieve_small_bucket_region: root "
                          FBPRIME_FORMAT ",%d divides at x = "
                          "%d = %lu * %u + %d\n",
                          p, r, x, j, 1 << si->logI, i);
                  pthread_mutex_unlock(&io_mutex);
              }
              prime.p = p;
              prime.x = x;
              push_bucket_prime (BP, prime);
          }
          i0 += r;
          if (i0 >= p)
              i0 -= p;
          S_ptr += I;
          q ^= p;
      }
      ssd->nice_p[n].next_position = i0;
  }


  /* Resieve bad primes */
  if (resieve_very_verbose_bad)
    {
        pthread_mutex_lock(&io_mutex);
      fprintf (stderr, "# %d bad primes to resieve: ", ssd->nb_bad_p);
      for (int n = 0; n < ssd->nb_bad_p; ++n)
	fprintf (stderr, "%s" FBPRIME_FORMAT, 
		 (n>0) ? ", " : "", ssd->bad_p[n].g);
      fprintf (stderr, "\n");
      pthread_mutex_unlock(&io_mutex);
    }
  for (int n = 0; n < ssd->nb_bad_p; ++n) 
    {
      const fbprime_t g = ssd->bad_p[n].g;

      /* Test every p-th line, starting at S[next_position] */
      unsigned int i, i0 = ssd->bad_p[n].next_position;
      ASSERT (n == 0 || ssd->bad_p[n - 1].g <= ssd->bad_p[n].g);
      ASSERT (i0 % I == 0); /* make sure next_position points at start
                               of line */
      if (resieve_very_verbose_bad) {
          pthread_mutex_lock(&io_mutex);
          fprintf (stderr, "# resieving bad prime " FBPRIME_FORMAT
                  ", i0 = %u\n", g, i0);
          pthread_mutex_unlock(&io_mutex);
      }
      while (i0 < (unsigned int) bucket_region)
        {
          unsigned char *S_ptr = S + i0;
          if ((i0 >> si->logI) % 2 == 0) /* Even j coordinate? */
            {
              /* Yes, test only odd i-coordinates */
              for (i = 1; i < I; i += 2)
                {
                  if (S_ptr[i] != 255)
                    {
                      bucket_prime_t prime;
                      const unsigned int x = i0 + i;
                      if (resieve_very_verbose_bad) {
                          pthread_mutex_lock(&io_mutex);
                          fprintf (stderr, "resieve_small_bucket_region even j: root "
                                  FBPRIME_FORMAT ",inf divides at x = %u\n",
                                  g, x);
                          pthread_mutex_unlock(&io_mutex);
                      }
                      prime.p = g;
                      prime.x = x;
                      push_bucket_prime (BP, prime);
                    }
                }
            }
          else
            {
              /* No, test all i-coordinates */
              for (i = 0; i < I; i++)
                {
                  if (S_ptr[i] != 255)
                    {
                      bucket_prime_t prime;
                      const unsigned int x = i0 + i;
                      if (resieve_very_verbose_bad) {
                          pthread_mutex_lock(&io_mutex);
                          fprintf (stderr, "resieve_small_bucket_region odd j: root "
                                  FBPRIME_FORMAT ",inf divides at x = %u\n",
                                  g, x);
                          pthread_mutex_unlock(&io_mutex);
                      }
                      prime.p = g;
                      prime.x = x;
                      push_bucket_prime (BP, prime);
                    }
                }
            }
          i0 += g * I;
        }
      ssd->bad_p[n].next_position = i0 - bucket_region;
      if (resieve_very_verbose_bad) {
          pthread_mutex_lock(&io_mutex);
          fprintf (stderr, "# resieving: new i0 = %u, bucket_region = %d, "
                  "new next_position = %d\n",
                  i0, bucket_region, ssd->bad_p[n].next_position);
          pthread_mutex_unlock(&io_mutex);
      }
    }
}
/* }}} */

/* {{{ Trial division */
typedef struct {
    uint64_t *fac;
    int n;
} factor_list_t;

#define FL_MAX_SIZE 200

void factor_list_init(factor_list_t *fl) {
    fl->fac = (uint64_t *) malloc (FL_MAX_SIZE * sizeof(uint64_t));
    ASSERT_ALWAYS(fl->fac != NULL);
    fl->n = 0;
}

void factor_list_clear(factor_list_t *fl) {
    free(fl->fac);
}

static void 
factor_list_add(factor_list_t *fl, const uint64_t p)
{
  ASSERT_ALWAYS(fl->n < FL_MAX_SIZE);
  fl->fac[fl->n++] = p;
}

// print a comma-separated list of factors.
// returns the number of factor printed (in particular, a comma is needed
// after this output only if the return value is non zero)
int factor_list_fprint(FILE *f, factor_list_t fl) {
    int i;
    for (i = 0; i < fl.n; ++i) {
        if (i) fprintf(f, ",");
        fprintf(f, "%" PRIx64, fl.fac[i]);
    }
    return i;
}


static const int bucket_prime_stats = 0;
static long nr_bucket_primes = 0;
static long nr_div_tests = 0;
static long nr_composite_tests = 0;
static long nr_wrap_was_composite = 0;
/* The entries in BP must be sorted in order of increasing x */
static void
divide_primes_from_bucket (factor_list_t *fl, mpz_t norm, const unsigned int N MAYBE_UNUSED, const int x,
                           bucket_primes_t *BP, const unsigned long fbb)
{
  bucket_prime_t prime;
  while (!bucket_primes_is_end (BP)) {
      prime = get_next_bucket_prime (BP);
      if (prime.x > x)
        {
          rewind_primes_by_1 (BP);
          break;
        }
      if (prime.x == x) {
          if (bucket_prime_stats) nr_bucket_primes++;
          unsigned long p = prime.p;
          while (p <= fbb) {
              if (bucket_prime_stats) nr_div_tests++;
              if (mpz_divisible_ui_p (norm, p)) {
                  int isprime;
                  modulusul_t m; 
                  modul_initmod_ul (m, (unsigned long) p);
                  if (bucket_prime_stats) nr_composite_tests++;
                  isprime = modul_isprime (m);
                  modul_clearmod (m);
                  if (isprime) {
                      break;
                  } else {
                    if (bucket_prime_stats) nr_wrap_was_composite++;
                  }
              }

              /* It may have been a case of incorrectly reconstructing p
                 from bits 1...16, so let's try if a bigger prime works.

                 Warning: this strategy may fail, since we might find a
                 composite p+k1*BUCKET_P_WRAP dividing the norm, while we
                 really want a larger prime p+k2*BUCKET_P_WRAP. In that case,
                 if a prime dividing p+k1*BUCKET_P_WRAP also divides the norm,
                 it might lead to a bucket error (p = ... does not divide),
                 moreover the wanted prime p+k2*BUCKET_P_WRAP will not be found
                 and we might miss some relations. */
              p += BUCKET_P_WRAP;
          }
          if (p > fbb) {
              pthread_mutex_lock(&io_mutex);
              fprintf (stderr,
                       "# Error, p = %lu does not divide at (N,x) = (%u,%d)\n",
                       (unsigned long) prime.p, N, x);
              pthread_mutex_unlock(&io_mutex);
              abort();
          }
          do {
              factor_list_add(fl, p);
              mpz_divexact_ui (norm, norm, p);
          } while (mpz_divisible_ui_p (norm, p));
      }
  }
}


NOPROFILE_STATIC void
trial_div (factor_list_t *fl, mpz_t norm, const unsigned int N, int x,
           factorbase_degn_t *fb, bucket_primes_t *primes,
	   trialdiv_divisor_t *trialdiv_data, const unsigned long fbb,
           int64_t a MAYBE_UNUSED, uint64_t b MAYBE_UNUSED)
{
    const int trial_div_very_verbose = 0;
    int nr_factors;
    fl->n = 0; /* reset factor list */

    if (trial_div_very_verbose) {
        pthread_mutex_lock(&io_mutex);
        gmp_fprintf (stderr, "# trial_div() entry, x = %d, norm = %Zd\n",
                x, norm);
        pthread_mutex_unlock(&io_mutex);
    }

    // handle 2 separately, if it is in fb
    if (fb->p == 2) {
        int bit = mpz_scan1(norm, 0);
        int i;
        for (i = 0; i < bit; ++i) {
            fl->fac[fl->n] = 2;
            fl->n++;
        }
        if (trial_div_very_verbose) {
            pthread_mutex_lock(&io_mutex);
            gmp_fprintf (stderr, "# x = %d, dividing out 2^%d, norm = %Zd\n",
                    x, bit, norm);
            pthread_mutex_unlock(&io_mutex);
        }
        mpz_tdiv_q_2exp(norm, norm, bit);
        fb = fb_next (fb); // cannot do fb++, due to variable size !
    }

    // remove primes in "primes" that map to x
    divide_primes_from_bucket (fl, norm, N, x, primes, fbb);
#ifdef TRACE_K /* {{{ */
    if (trace_on_spot_ab(a,b) && fl->n) {
        fprintf(stderr, "# divided by 2 + primes from bucket that map to %u: ", x);
        if (!factor_list_fprint(stderr, *fl)) fprintf(stderr, "(none)");
        gmp_fprintf(stderr, ", remaining norm is %Zd\n", norm);
    }
#endif /* }}} */
    if (trial_div_very_verbose) {
        pthread_mutex_lock(&io_mutex);
        gmp_fprintf (stderr, "# x = %d, after dividing out bucket/resieved norm = %Zd\n",
                x, norm);
        pthread_mutex_unlock(&io_mutex);
    }

    do {
      /* Trial divide primes with precomputed tables */
#define TRIALDIV_MAX_FACTORS 32
      int i;
      unsigned long factors[TRIALDIV_MAX_FACTORS];
      if (trial_div_very_verbose)
      {
          pthread_mutex_lock(&io_mutex);
          fprintf (stderr, "# Trial division by ");
          for (i = 0; trialdiv_data[i].p != 1; i++)
              fprintf (stderr, " %lu", trialdiv_data[i].p);
          fprintf (stderr, "\n");
          pthread_mutex_unlock(&io_mutex);
      }

      nr_factors = trialdiv (factors, norm, trialdiv_data, TRIALDIV_MAX_FACTORS);

      for (i = 0; i < MIN(nr_factors, TRIALDIV_MAX_FACTORS); i++)
      {
          if (trial_div_very_verbose) {
              pthread_mutex_lock(&io_mutex);
              fprintf (stderr, " %lu", factors[i]);
              pthread_mutex_unlock(&io_mutex);
          }
          factor_list_add (fl, factors[i]);
      }
      if (trial_div_very_verbose) {
          pthread_mutex_lock(&io_mutex);
          gmp_fprintf (stderr, "\n# After trialdiv(): norm = %Zd\n", norm);
          pthread_mutex_unlock(&io_mutex);
      }
    } while (nr_factors == TRIALDIV_MAX_FACTORS + 1);
}
/* }}} */

/* {{{ cofactoring area */

/* Return 0 if the leftover norm n cannot yield a relation:
   (a) if n > 2^mfb
   (b) if L < n < B^2
   (c) if L^2 < n < B^3

   FIXME: need to check L^k < n < B^(k+1) too.
*/
static int
check_leftover_norm (mpz_t n, size_t lpb, mpz_t BB, mpz_t BBB, size_t mfb)
{
  size_t s = mpz_sizeinbase (n, 2);

  if (s > mfb)
    return 0;
  if (lpb < s && mpz_cmp (n, BB) < 0)
    return 0;
  if (2 * lpb < s && mpz_cmp (n, BBB) < 0)
    return 0;
  if (lpb < s && mpz_probab_prime_p (n, 1))
    return 0;
  return 1;
}

#ifdef UNSIEVE_NOT_COPRIME
static void
unsieve_one_prime (unsigned char *line_start, const unsigned int p, 
                   const unsigned int y, const sieve_info_t *si)
{
  unsigned int x, np = p; /* if 2|y, np=2p, else np=p */

  x = (si->I / 2U) % p;
  if (y % 2U == 0U)
    {
      np += p;
      if (x % 2U == 0U)
        x += p;
    }
  for ( ; x < si->I; x += np)
    line_start[x] = 255;
}


/* Set locations where gcd(i,j) != 1 to 255*/
void 
unsieve_not_coprime (unsigned char *S, const int N, const sieve_info_t *si)
{
  unsigned int y; /* Line coordinate within bucket region */
  for (y = 0U + (N == 0U ? 1U : 0U); 
       y < 1U << (LOG_BUCKET_REGION - si->logI); y++)
    {
      unsigned int c = y + (N << (LOG_BUCKET_REGION - si->logI));
      unsigned int p;
      unsigned char *line_start = S + (y << si->logI);

      p = si->lpf[c]; /* set p to largest prime factor of c */
      if (p > 3U)
        {
          ASSERT (c % p == 0U);
          unsieve_one_prime (line_start, p, y, si);
          do {c /= p;} while (c % p == 0U);
        }
      
      while (c % 2U == 0U) 
        c >>= 1;
      
      if (c % 3U == 0U)
        {
          const unsigned int I_ul  = si->I / sizeof (unsigned long);
          unsigned int i, x;
          unsigned long s[3] = {0UL, 0UL, 0UL};
          unsigned long * restrict ul_line_start = (unsigned long *) line_start;
          
          /* Sieve only the small pattern */
          for (x = (si->I / 2U) % 3U; x < 3U * sizeof(unsigned long); x += 3U)
            ((unsigned char *) s)[x] = 255;
          
          /* Then apply pattern to array */
          for (i = 0U; i < I_ul - 2U; i += 3U)
            {
              ul_line_start[i] |= s[0];
              ul_line_start[i + 1] |= s[1];
              ul_line_start[i + 2] |= s[2];
            }
          if (i < I_ul - 1U)
            line_start[i] |= s[0];
          if (i < I_ul)
            line_start[i + 1] |= s[1];
          
          do {c /= 3U;} while (c % 3U == 0U);
        }

      for (p = 5U; p * p <= c; p += 2U)
        if (c % p == 0U)
          {
            unsieve_one_prime (line_start, p, y, si);
            do {c /= p;} while (c % p == 0U);
          }

      /* Now c == 1 or c is a prime */
      if (c != 1U)
        {
          ASSERT(c > 3U && si->lpf[c] == c);
          unsieve_one_prime (line_start, c, y, si);
        }
    }
}
#endif /* ifdef UNSIEVE_NOT_COPRIME */

struct local_sieve_data_s {
    unsigned char * S;      /* local sieve array */
    small_sieve_data_t lsrsd[1];
    /* extra copies */
    small_sieve_data_t *lssd;
    small_sieve_data_t *rssd;
};
typedef struct local_sieve_data_s local_sieve_data[1];
typedef struct local_sieve_data_s * local_sieve_data_ptr;
typedef const struct local_sieve_data_s * local_sieve_data_srcptr;


/* Adds the number of sieve reports to *survivors,
   number of survivors with coprime a, b to *coprimes */

NOPROFILE_STATIC int
factor_survivors (thread_data_ptr th, int N, local_sieve_data * loc)
{
    sieve_info_t * si = th->si;
    cado_poly_ptr cpoly = si->cpoly;
    sieve_side_info_ptr rat = si->sides[RATIONAL_SIDE];
    sieve_side_info_ptr alg = si->sides[ALGEBRAIC_SIDE];

    int cpt = 0;
    int surv = 0, copr = 0;
    mpz_t norm[2], BB[2], BBB[2];
    factor_list_t factors[2];
    mpz_array_t *f[2] = { NULL, };
    uint32_array_t *m[2] = { NULL, }; /* corresponding multiplicities */
    bucket_primes_t primes[2];

    mpz_t BLPrat;       /* alone ? */

    for(int side = 0 ; side < 2 ; side++) {
        f[side] = alloc_mpz_array (8);
        m[side] = alloc_uint32_array (8);

        factor_list_init(&factors[side]);
        mpz_init (norm[side]);
        mpz_init (BB[side]);
        mpz_init (BBB[side]);

        unsigned long lim = (side == RATIONAL_SIDE) ? cpoly->rlim : cpoly->alim;
        mpz_ui_pow_ui (BB[side], lim, 2);
        mpz_mul_ui (BBB[side], BB[side], lim);
    }

    mpz_init (BLPrat);
    mpz_set_ui (BLPrat, cpoly->rlim);
    mpz_mul_2exp (BLPrat, BLPrat, cpoly->lpbr); /* fb bound * lp bound */

    unsigned char * alg_S = loc[ALGEBRAIC_SIDE]->S;
    unsigned char * rat_S = loc[RATIONAL_SIDE]->S;
#ifdef TRACE_K /* {{{ */
    if (trace_on_spot_Nx(N, trace_Nx.x)) {
        fprintf(stderr, "# When entering factor_survivors for bucket %u, alg_S[%u]=%u, rat_S[%u]=%u\n",
                trace_Nx.N, trace_Nx.x, alg_S[trace_Nx.x], trace_Nx.x, rat_S[trace_Nx.x]);
    }
#endif  /* }}} */

#ifdef UNSIEVE_NOT_COPRIME
    unsieve_not_coprime (alg_S, N, si);
#endif
    
    for (int x = 0; x < bucket_region; ++x)
      {
#ifdef TRACE_K /* {{{ */
          if (trace_on_spot_Nx(N, x)) {
              fprintf(stderr, "# alg->Bound[%u]=%u, rat->Bound[%u]=%u\n",
                      alg_S[trace_Nx.x], alg->Bound[alg_S[x]],
                      rat_S[trace_Nx.x], rat->Bound[rat_S[x]]);
          }
#endif /* }}} */
        unsigned int X;
        unsigned int i, j;

        if (alg->Bound[alg_S[x]] + rat->Bound[rat_S[x]] >= 127)
          {
            alg_S[x] = 255;
            continue;
          }
        surv++;

        X = x + (N << LOG_BUCKET_REGION);
        i = abs ((int) (X & (si->I - 1)) - si->I / 2);
        j = X >> si->logI;
#ifndef UNSIEVE_NOT_COPRIME
        if (bin_gcd_safe (i, j) != 1)
          {
#ifdef TRACE_K
            if (trace_on_spot_Nx(N, x)) {
                fprintf(stderr, "# Slot [%u] in bucket %u has non coprime (i,j)=(%d,%u)\n",
                        trace_Nx.x, trace_Nx.N, i, j);
            }
#endif
            alg_S[x] = 255;
            continue;
          }
#endif
      }

    /* Copy those bucket entries that belong to sieving survivors and
       store them with the complete prime */
    /* FIXME: choose a sensible size here */

    /* XXX: Don't believe that resieve_start is easily changeable... */
    const int resieve_start = ALGEBRAIC_SIDE;
    unsigned char * S = loc[resieve_start]->S;

    for(int z = 0 ; z < 2 ; z++) {
        int side = resieve_start ^ z;
        primes[side] = init_bucket_primes (bucket_region);

        for (int i = 0; i < si->nb_threads; ++i) {
            thread_data_ptr other = th + i - th->id;
            purge_bucket (&primes[side], other->sides[side]->BA, N, S);
        }

        /* Resieve small primes for this bucket region and store them 
           together with the primes recovered from the bucket updates */
        resieve_small_bucket_region (&primes[side], N, S, loc[side]->lsrsd, si);

        /* Sort the entries to avoid O(n^2) complexity when looking for
           primes during trial division */
        bucket_sortbucket (&primes[side]);
    }

    /* Scan array one long word at a time. If any byte is <255, i.e. if
       the long word is != 0xFFFF...FF, examine the bytes */
    for (int xul = 0; xul < bucket_region; xul += sizeof (unsigned long)) {
#ifdef TRACE_K
        if ((unsigned int) N == trace_Nx.N && (unsigned int) xul <= trace_Nx.x && (unsigned int) xul + sizeof (unsigned long) > trace_Nx.x) {
            fprintf(stderr, "# Slot [%u] in bucket %u has value %u\n",
                    trace_Nx.x, trace_Nx.N, S[trace_Nx.x]);
        }
#endif
        if (*(unsigned long *)(S + xul) == (unsigned long)(-1L)) 
            continue;
        for (int x = xul; x < xul + (int) sizeof (unsigned long); ++x) {
            if (S[x] == 255) continue;

            int64_t a;
            uint64_t b;

            // Compute algebraic and rational norms.
            NxToAB (&a, &b, N, x, si);

#ifdef TRACE_K
            if (trace_on_spot_ab(a, b)) {
                fprintf(stderr, "# about to print relation for (%"PRId64",%"PRIu64")\n",a,b);
            }
#endif
            /* since a,b both even were not sieved, either a or b should be odd */
            // ASSERT((a | b) & 1);
            if (UNLIKELY(((a | b) & 1) == 0))
            {
                pthread_mutex_lock(&io_mutex);
                fprintf (stderr, "# Error: a and b both even for N = %d, x = %d,\n"
                        "i = %d, j = %d, a = %ld, b = %lu\n",
                        N, x, ((x + N*bucket_region) & (si->I - 1))
                        - (si->I >> 1),
                        (x + N*bucket_region) >> si->logI,
                        (long) a, (unsigned long) b);
                pthread_mutex_unlock(&io_mutex);
                continue;
            }

            /* Since the q-lattice is exactly those (a, b) with
               a == rho*b (mod q), q|b  ==>  q|a  ==>  q | gcd(a,b) */
            if (b == 0 || (b >= si->q && b % si->q == 0))
                continue;

            copr++;

            /* For hunting missed relations */
#if 0
            if (a == -6537753 && b == 1264)
                fprintf (stderr, "# Have relation %ld,%lu at bucket nr %d, "
                        "x = %d, K = %lu\n", 
                        a, b, N, x, (unsigned long) N * bucket_region + x);
#endif

            int pass = 1;

            for(int z = 0 ; pass && z < 2 ; z++) {
                int side = RATIONAL_SIDE ^ z;   /* start with rational */
                int rat = (side == RATIONAL_SIDE);
                const mpz_t * f = (const mpz_t *) (rat ? cpoly->g : cpoly->f);
                int deg = rat ? cpoly->degreeg : cpoly->degree;
                int lim = rat ? cpoly->rlim : cpoly->alim;
                int lpb = rat ? cpoly->lpbr : cpoly->lpba;
                int mfb = rat ? cpoly->mfbr : cpoly->mfba;

                // Trial divide rational norm
                eval_fij (norm[side], f, deg, a, b);
                if (si->ratq == (side == RATIONAL_SIDE))
                    mpz_divexact_ui (norm[side], norm[side], si->q);
#ifdef TRACE_K
                if (trace_on_spot_ab(a, b)) {
                    gmp_fprintf(stderr, "# start trial division for norm=%Zd on %s side for (%"PRId64",%"PRIu64")\n",norm[side],sidenames[side],a,b);
                }
#endif
                trial_div (&factors[side], norm[side], N, x,
                        si->sides[side]->fb,
                        &primes[side], si->sides[side]->trialdiv_data,
                        lim, a, b);

                pass = check_leftover_norm (norm[side], lpb,
                            BB[side], BBB[side], mfb);
#ifdef TRACE_K
                if (trace_on_spot_ab(a, b)) {
                    gmp_fprintf(stderr, "# checked leftover norm=%Zd on %s side for (%"PRId64",%"PRIu64"): %d\n",norm[side],sidenames[side],a,b,pass);
                }
#endif
            }
            if (!pass) continue;

            /* if norm[RATIONAL_SIDE] is above BLPrat, then it might not
             * be smooth. We factor it first. Otherwise we factor it
             * last.
             */
            int first = mpz_cmp(norm[RATIONAL_SIDE], BLPrat) > 0 ? RATIONAL_SIDE : ALGEBRAIC_SIDE;

            for(int z = 0 ; pass && z < 2 ; z++) {
                int side = first ^ z;
                int rat = (side == RATIONAL_SIDE);
                int lpb = rat ? cpoly->lpbr : cpoly->lpba;
                pass = factor_leftover_norm(norm[side], lpb, f[side], m[side], si->strategy);
            }
            if (!pass) continue;

#ifdef UNSIEVE_NOT_COPRIME
            ASSERT (bin_gcd_safe (a, b) == 1);
#endif

#ifdef TRACE_K
            if (trace_on_spot_ab(a, b)) {
                fprintf(stderr, "# Relation for (%"PRId64",%"PRIu64") printed\n", a, b);
            }
#endif
            if (!si->bench) {
                pthread_mutex_lock(&io_mutex);
                fprintf (si->output, "%" PRId64 ",%" PRIu64, a, b);
                for(int z = 0 ; z < 2 ; z++) {
                    int side = RATIONAL_SIDE ^ z;
                    fprintf (si->output, ":");
                    int comma = factor_list_fprint (si->output, factors[side]);
                    for (unsigned int i = 0; i < f[side]->length; ++i) {
                        for (unsigned int j = 0; j < m[side]->data[i]; j++) {
                            if (comma++) fprintf (si->output, ",");
                            gmp_fprintf (si->output, "%Zx", f[side]->data[i]);
                        }
                    }
                    if (si->ratq == (side == RATIONAL_SIDE)) {
                        if (comma++) fprintf (si->output, ",");
                        fprintf (si->output, "%" PRIx64 "", si->q);
                    }
                }
                fprintf (si->output, "\n");
                fflush (si->output);
                pthread_mutex_unlock(&io_mutex);
            }
            cpt++;
            /* Build histogram of lucky S[x] values */
            for(int side = 0 ; side < 2 ; side++)
                th->rep->report_sizes[side][loc[side]->S[x]]++;
        }
    }

    th->rep->survivors1 += surv;
    th->rep->survivors2 += copr;

    mpz_clear (BLPrat);

    for(int side = 0 ; side < 2 ; side++) {
        clear_bucket_primes (&primes[side]);
        mpz_clear (BBB[side]);
        mpz_clear (BB[side]);
        mpz_clear(norm[side]);
        factor_list_clear(&factors[side]);
        clear_uint32_array (m[side]);
        clear_mpz_array (f[side]);
    }

    return cpt;
}

/* }}} */

/****************************************************************************/

/* {{{ Conversions between different representations for sieve locations:
 *
 * Coordinate systems for identifying an (a,b) pair.
 *
 * (a, b): This is the one from textbooks. We always have b>=0 (only free
 *         relations have b=0, and we don't see them here).
 * (i, j): For a give special q, this is a point in the q-lattice. Given
 *         the lattice basis given by (a0 b0 a1 b1), this corresponds to
 *         the (a,b) pair equal to i*(a0,b0)+j*(a1,b1). By construction
 *         this should lead to one of the norms having si->q as a factor.
 *         i is within [-I/2, I/2[, and j is within [1, J[
 * (N, x): bucket number N, location x. N is within [0,si->nb_buckets[
 *         and x within [0,bucket_region[ ; we have:
 *         N*bucket_region+x == (I/2+i)+j*I
 *
 * There are some change of coordinate functions at the end of this file.
 * The coordinate system (N, x) is almost always referred to as just x,
 * where x is a 32-bit value equal to N*bucket_region+x. Thus from this
 * wide x, it is possible te recover both N and (the short) x.
 */
static void xToIJ(int *i, unsigned int *j, const unsigned int X, const sieve_info_t * si)
{
    *i = (X % (si->I)) - (si->I >> 1);
    *j = X / si->I;
}

static void NxToIJ(int *i, unsigned int *j, const unsigned int N, const unsigned int x, const sieve_info_t * si)
{
    unsigned int X = x + N * bucket_region;
    return xToIJ(i, j, X, si);
}

static void IJTox(unsigned int * x, int i, unsigned int j, const sieve_info_t * si)
{
    *x = i + (si->I)*j + (si->I>>1);
}

static void IJToNx(unsigned int *N, unsigned int * x, int i, unsigned int j, const sieve_info_t * si)
{
    IJTox(x, i, j, si);
    *N = *x >> LOG_BUCKET_REGION;
    *x &= (bucket_region - 1);
}

static void IJToAB(int64_t *a, uint64_t *b, const int i, const unsigned int j, 
       const sieve_info_t * si)
{
    int64_t s, t;
    s = (int64_t)i * (int64_t) si->a0 + (int64_t)j * (int64_t)si->a1;
    t = (int64_t)i * (int64_t) si->b0 + (int64_t)j * (int64_t)si->b1;
    if (t >= 0)
      {
        *a = s;
        *b = t;
      }
    else
      {
        *a = -s;
        *b = -t;
      }
}

/* Warning: b might be negative, in which case we return (-a,-b) */
static void xToAB(int64_t *a, uint64_t *b, const unsigned int x, const sieve_info_t *si)
{
    int i, j;
    int64_t c;
    uint32_t I = si->I;

    i = (x & (I - 1)) - (I >> 1);
    j = x >> si->logI;
    *a = (int64_t) i * (int64_t) si->a0 + (int64_t) j * (int64_t) si->a1;
    c =  (int64_t) i * (int64_t) si->b0 + (int64_t) j * (int64_t) si->b1;
    if (c >= 0)
      *b = c;
    else
      {
        *a = -*a;
        *b = -c;
      }
}
static void NxToAB(int64_t *a, uint64_t *b, const unsigned int N, const unsigned int x, const sieve_info_t *si)
{
    xToAB(a, b, N * bucket_region + x, si);
}

static int ABToIJ(int *i, unsigned int *j, const int64_t a, const uint64_t b, const sieve_info_t * si)
{
    int64_t ii =   a * (int64_t) si->b1 - b * (int64_t)si->a1;
    int64_t jj = - a * (int64_t) si->b0 + b * (int64_t)si->a0;
    if (ii % (int64_t) si->q) return 0; ii /= (int64_t) si->q;
    if (jj % (int64_t) si->q) return 0; jj /= (int64_t) si->q;
    if (jj < 0) { ii = -ii; jj = -jj; }
    *i = ii;
    *j = jj;
    return 1;
}
static int ABTox(unsigned int *x, const int64_t a, const uint64_t b, const sieve_info_t * si)
{
    int i;
    unsigned int j;
    if (!ABToIJ(&i, &j, a, b, si)) return 0;
    IJTox(x, a, b, si);
    return 1;
}

MAYBE_UNUSED static int ABToNx(unsigned int * N, unsigned int *x, const int64_t a, const uint64_t b, const sieve_info_t * si)
{
    int i;
    unsigned int j;
    if (!ABToIJ(&i, &j, a, b, si)) return 0;
    IJToNx(N, x, a, b, si);
    return 1;
}
/* }}} */

void test_divisible_x (const fbprime_t p, const unsigned long x, 
                       const int n, const sieve_info_t *si, const char side)
{
  const unsigned long X = x + n * bucket_region;
  const long i = (long) (X & ((unsigned long) si->I - 1UL)) - (long) si->I / 2;
  const unsigned long j = X >> si->logI;
  mpz_t v;

  mpz_init (v);
  if (side == 'a')
    eval_fij (v, (const mpz_t *) si->fij, si->degree, i, j);
  else if (side == 'r')
    eval_fij (v, (const mpz_t *) si->gij, 1, i, j);
  else
    abort();
  
  if (!mpz_divisible_ui_p (v, (unsigned long) p))
   gmp_fprintf (stderr, "# test_divisible_x (" FBPRIME_FORMAT 
                ", %lu, %d, %c): i = %ld, j = %lu, v = %Zd\n", 
                p, x, n, side, i, j, v);
  
  mpz_clear (v);
}


/*********************** norm computation ************************************/

/* Put in fij[] the coefficients of f'(i) = F(a0*i+a1, b0*i+b1).
   Assumes the coefficients of fij[] are initialized.
   Put in fijd[] a double-precision approximation of fij[]/q.
*/
static void
fij_from_f (mpz_t *fij, const sieve_info_t *si, mpz_t *f, const int d)
{
  int k, l;
  mpz_t *g; /* will contain the coefficients of (b0*i+b1)^l */
  mpz_t f0;

  for (k = 0; k <= d; k++)
    mpz_set (fij[k], f[k]);

  g = malloc ((d + 1) * sizeof (mpz_t));
  for (k = 0; k <= d; k++)
    mpz_init (g[k]);
  mpz_init (f0);

  /* Let h(x) = quo(f(x), x), then F(x,y) = H(x,y)*x + f0*y^d, thus
     F(a0*i+a1, b0*i+b1) = H(a0*i+a1, b0*i+b1)*(a0*i+a1) + f0*(b0*i+b1)^d.
     We use that formula recursively. */

  mpz_set_ui (g[0], 1); /* g = 1 */

  for (k = d - 1; k >= 0; k--)
    {
      /* invariant: we have already translated coefficients of degree > k,
         in f[k+1..d], and g = (b0*i+b1)^(d - (k+1)), with coefficients in
         g[0..d - (k+1)]:
         f[k]   <- f[k] + a1*f[k+1]
         ...
         f[l] <- a0*f[l]+a1*f[l+1] for k < l < d
         ...
         f[d] <- a0*f[d] */
      mpz_swap (f0, fij[k]); /* save the new constant coefficient */
      mpz_mul_si (fij[k], fij[k + 1], si->a1);
      for (l = k + 1; l < d; l++)
        {
          mpz_mul_si (fij[l], fij[l], si->a0);
          mpz_addmul_si (fij[l], fij[l + 1], si->a1);
        }
      mpz_mul_si (fij[d], fij[d], si->a0);

      /* now compute (b0*i+b1)^(d-k) from the previous (b0*i+b1)^(d-k-1):
         g[d-k] = b0*g[d-k-1]
         ...
         g[l] = b1*g[l]+b0*g[l-1] for 0 < l < d-k
         ...
         g[0] = b1*g[0]
      */
      mpz_mul_si (g[d - k], g[d - k - 1], si->b0);
      for (l = d - k - 1; l > 0; l--)
        {
          mpz_mul_si (g[l], g[l], si->b1);
          mpz_addmul_si (g[l], g[l-1], si->b0);
        }
      mpz_mul_si (g[0], g[0], si->b1);

      /* now g has degree d-k, and we add f0*g */
      for (l = k; l <= d; l++)
        mpz_addmul (fij[l], g[l - k], f0);
    }

  mpz_clear (f0);
  for (k = 0; k <= d; k++)
    mpz_clear (g[k]);
  free (g);
}

/* return max |g(x)| for 0 <= x <= s,
   where g(x) = g[d]*x^d + ... + g[1]*x + g[0] */
static double
get_maxnorm_aux (double *g, const unsigned int d, double s)
{
  unsigned int k, l, sign_change, new_sign_change;
  double **dg;    /* derivatives of g */
  double a, va, b, vb;
  double *roots, gmax;

  dg = (double**) malloc (d * sizeof (double*));
  dg[0] = g;
  for (k = 1; k < d; k++) /* dg[k] is the k-th derivative, thus has
                             degree d-k, i.e., d-k+1 coefficients */
    dg[k] = (double*) malloc ((d - k + 1) * sizeof (double));
  roots = (double*) malloc (d * sizeof (double));
  for (k = 1; k < d; k++)
    for (l = 0; l <= d - k; l++)
      dg[k][l] = (l + 1) * dg[k - 1][l + 1];
  /* now dg[d-1][0]+x*dg[d-1][1] is the (d-1)-th derivative: it can have at
     most one sign change, iff dg[d-1][0] and dg[d-1][0]+dg[d-1][1] have
     different signs */
  if (dg[d-1][0] * (dg[d-1][0] + dg[d-1][1]) < 0)
    {
      sign_change = 1;
      roots[0] = - dg[d-1][0] / dg[d-1][1]; /* root of (d-1)-th derivative */
    }
  else
    sign_change = 0;
  roots[sign_change] = s; /* end of interval */
  for (k = d - 1; k-- > 1;)
    {
      /* invariant: sign_change is the number of sign changes of the
         (k+1)-th derivative, with corresponding roots in roots[0]...
         roots[sign_change-1], and roots[sign_change] = s. */
      a = 0.0;
      va = dg[k][0]; /* value of dg[k] at x=0 */
      new_sign_change = 0;
      for (l = 0; l <= sign_change; l++)
        {
          b = roots[l]; /* root of dg[k+1], or end of interval */
          vb = fpoly_eval (dg[k], d - k, b);
          if (va * vb < 0) /* root in interval */
            roots[new_sign_change++] = fpoly_dichotomy (dg[k], d - k,
                                                        a, b, va, 20);
          a = b;
          va = vb;
        }
      roots[new_sign_change] = s; /* end of interval */
      sign_change = new_sign_change;
    }
  /* now all extrema of g are 0, roots[0], ..., roots[sign_change] = s */
  gmax = fabs (g[0]);
  for (k = 0; k <= sign_change; k++)
    {
      va = fabs (fpoly_eval (g, d, roots[k]));
      if (va > gmax)
        gmax = va;
    }
  free (roots);
  for (k = 1; k < d; k++)
    free (dg[k]);
  free (dg);
  return gmax;
}

/* returns the maximal value of log2 |F(a,b)/q| for
   a = a0 * i + a1 * j, b = b0 * i + b1 * j and q >= q0,
   -I/2 <= i <= I/2, 0 <= j <= I/2*min(s*B/|a1|,B/|b1|)
   where B >= sqrt(2*q/s/sqrt(3)) for all special-q in the current range
   (s is the skewness, and B = si.B, see lattice.tex).

   Since |a0| <= s*B and |b0| <= B, then
   |a0 * i + a1 * j| <= s*B*I and |b0 * i + b1 * j| <= B*I,
   thus it suffices to compute M = max |F(x,y)| in the rectangle
   -s <= x <= s, 0 <= y <= 1, and to multiply M by (B*I)^deg(F).

   Since F is homogeneous, we know M = max |F(x,y)| is attained on the border
   of the rectangle, i.e.:
   (a) either on F(s,y) for 0 <= y <= 1
   (b) either on F(x,1) for -s <= x <= s
   (c) either on F(-s,y) for 0 <= y <= 1
   (d) or on F(x,0) for -s <= x <= s, but this maximum is f[d]*s^d,
       and is attained in (a) or (c).
*/
static double
get_maxnorm (cado_poly cpoly, sieve_info_t *si, uint64_t q0)
{
  unsigned int d = cpoly->degree, k;
  double *fd; /* double-precision coefficients of f */
  double norm, max_norm, pows, tmp;

  fd = (double*) malloc ((d + 1) * sizeof (double));
  for (k = 0; k <= d; k++)
    fd[k] = mpz_get_d (cpoly->f[k]);

  /* (b1) determine the maximum of |f(x)| for 0 <= x <= s */
  max_norm = get_maxnorm_aux (fd, d, cpoly->skew);

  /* (b2) determine the maximum of |f(-x)| for 0 <= x <= s */
  norm = get_maxnorm_aux (fd, d, -cpoly->skew);
  if (norm > max_norm)
    max_norm = norm;

  for (pows = 1.0, k = 0; k <= d; k++)
    {
      fd[k] *= pows;
      pows *= cpoly->skew;
    }
  /* swap coefficients; if d is odd, we need to go up to k = floor(d/2) */
  for (k = 0; k <= d / 2; k++)
    {
      tmp = fd[k];
      fd[k] = fd[d - k];
      fd[d - k] = tmp;
    }

  /* (a) determine the maximum of |g(y)| for 0 <= y <= 1 */
  norm = get_maxnorm_aux (fd, d, 1.0);
  if (norm > max_norm)
    max_norm = norm;

  /* (c) determine the maximum of |g(-y)| for 0 <= y <= 1 */
  norm = get_maxnorm_aux (fd, d, -1.0);
  if (norm > max_norm)
    max_norm = norm;

  free (fd);

  /* multiply by (B*I)^d and divide by q0 if sieving on alg side */
  tmp = max_norm * pow (si->B * (double) si->I, (double) d);
  if (!si->ratq)
      tmp /= (double) q0;
  return log2(tmp);
}

/* v <- |f(i,j)|, where f is of degree d */
NOPROFILE_STATIC void
eval_fij (mpz_t v, const mpz_t *f, const unsigned int d, const long i,
	  const unsigned long j)
{
  unsigned int k;
  mpz_t jpow;

  mpz_init_set_ui (jpow, 1);
  mpz_set (v, f[d]);
  for (k = d; k-- > 0;)
    {
      mpz_mul_si (v, v, i);
      mpz_mul_ui (jpow, jpow, j);
      mpz_addmul (v, f[k], jpow);
    }
  mpz_abs (v, v); /* avoids problems with negative norms */
  mpz_clear (jpow);
}


/* check that the double x fits into an int32_t */
#define fits_int32_t(x) \
  ((double) INT32_MIN <= (x)) && ((x) <= (double) INT32_MAX)

/* return non-zero when the reduced lattice has entries that do not
   fit into int32_t, otherwise return 0 */
static int
SkewGauss (sieve_info_t *si, double skewness)
{
  double a[2], b[2], q, maxab0, maxab1;

  a[0] = (double) si->q;
  ASSERT_ALWAYS(a[0] < 9007199254740992.0); /* si.q should be less than 2^53
                                               so that a[0] is exact */
  b[0] = 0.0;
  a[1] = (double) si->rho;
  b[1] = skewness;
  ASSERT(b[1] != 0);
  while (1)
    {
      /* reduce vector (a[0], b[0]) with respect to (a[1], b[1]) */
      q = (a[0] * a[1] + b[0] * b[1]) / (a[1] * a[1] + b[1] * b[1]);
      q = rint (q);
      if (q == 0.0)
        break;
      a[0] -= q * a[1];
      b[0] -= q * b[1];

      /* reduce vector (a[1], b[1]) with respect to (a[0], b[0]) */
      q = (a[0] * a[1] + b[0] * b[1]) / (a[0] * a[0] + b[0] * b[0]);
      q = rint (q);
      if (q == 0.0)
        break;
      a[1] -= q * a[0];
      b[1] -= q * b[0];
    }
  if (!(fits_int32_t(a[0]) && fits_int32_t(b[0] / skewness) &&
	fits_int32_t(a[1]) && fits_int32_t(b[1] / skewness)))
    return 1;
  /* now b[0], b[1] should be of the form i*skewness, but this might not be
     exact due to rounding errors, thus we round them to the nearest integer */
  maxab0 = fabs (a[0]) > fabs (b[0]) ? fabs (a[0]) : fabs (b[0]);
  maxab1 = fabs (a[1]) > fabs (b[1]) ? fabs (a[1]) : fabs (b[1]);
  if (maxab0 <= maxab1)
    {
      si->a0 = (int32_t) a[0];
      si->b0 = (int32_t) rint (b[0] / skewness);
      si->a1 = (int32_t) a[1];
      si->b1 = (int32_t) rint (b[1] / skewness);
    }
  else /* swap (a0,b0) and (a1,b1) */
    {
      si->a1 = (int32_t) a[0];
      si->b1 = (int32_t) rint (b[0] / skewness);
      si->a0 = (int32_t) a[1];
      si->b0 = (int32_t) rint (b[1] / skewness);
      maxab1 = maxab0;
    }

  /* make sure J does not exceed I/2 */
  if (maxab1 >= si->B)
    si->J = (uint32_t) (si->B * skewness / maxab1 * (double) (si->I >> 1));
  else
    si->J = si->I >> 1;

  /* Make sure the bucket region size divides the sieve region size, 
     partly covered bucket regions may lead to problems when 
     reconstructing p from half-empty buckets. */
  /* Compute number of i-lines per bucket region, must be integer */
  ASSERT_ALWAYS(LOG_BUCKET_REGION >= si->logI);
  uint32_t i = 1U << (LOG_BUCKET_REGION - si->logI);
  i *= si->nb_threads;  /* ensures nb of bucket regions divisible by nb_threads */
  si->J = ((si->J - 1U) / i + 1U) * i; /* Round up to multiple of i */

  return 0;
}

/* return max(|a0|,|a1|)/min(|a0|,|a1|) */
double
skewness (sieve_info_t *si)
{
  double a0 = fabs ((double) si->a0);
  double a1 = fabs ((double) si->a1);

  return (a0 > a1) ? a0 / a1 : a1 / a0;
}

/************************ cofactorization ********************************/

/* FIXME: the value of 20 seems large. Normally, a few Miller-Rabin passes
   should be enough. See also http://www.trnicely.net/misc/mpzspsp.html */
#define NMILLER_RABIN 1 /* in the worst case, what can happen is that a
                           composite number is declared as prime, thus
                           a relation might be missed, but this will not
                           affect correctness */
#define IS_PROBAB_PRIME(X) (0 != mpz_probab_prime_p((X), NMILLER_RABIN))
#define BITSIZE(X)      (mpz_sizeinbase((X), 2))
#define NFACTORS        8 /* maximal number of large primes */

/* This function was contributed by Jerome Milan (and bugs were introduced
   by Paul Zimmermann :-).
   Input: n - the number to be factored (leftover norm). Must be composite!
          l - (large) prime bit size bound is L=2^l
   Assumes n > 0.
   Return value:
          0 if n has a prime factor larger than 2^l
          1 if all prime factors of n are < 2^l
   Output:
          the prime factors of n are factors->data[0..factors->length-1],
          with corresponding multiplicities multis[0..factors->length-1].
*/
int
factor_leftover_norm (mpz_t n, unsigned int l,
                      mpz_array_t* const factors, uint32_array_t* const multis,
		      facul_strategy_t *strategy)
{
  uint32_t i, nr_factors;
  unsigned long ul_factors[16];
  int facul_code;

  factors->length = 0;
  multis->length = 0;

  /* factoring programs do not like 1 */
  if (mpz_cmp_ui (n, 1) == 0)
    return 1;

  /* If n < L, we know that n is prime, since all primes < B have been
     removed, and L < B^2 in general, where B is the factor base bound,
     thus we only need a primality test when n > L. */
  if (BITSIZE(n) <= l)
    {
      append_mpz_to_array (factors, n);
      append_uint32_to_array (multis, 1);
      return 1;
    }
/* Input is required to be composite!
  else if (IS_PROBAB_PRIME(n))
    {
      return 0;
    }
*/

  /* use the facul library */
  // gmp_printf ("facul: %Zd\n", n);
  facul_code = facul (ul_factors, n, strategy);

  if (facul_code == FACUL_NOT_SMOOTH)
    return 0;

  ASSERT (facul_code == 0 || mpz_cmp_ui (n, ul_factors[0]) != 0);

  if (facul_code > 0)
    {
      nr_factors = facul_code;
      for (i = 0; i < nr_factors; i++)
	{
	  unsigned long r;
	  mpz_t t;
	  if (ul_factors[i] > (1UL << l)) /* Larger than large prime bound? */
	    return 0;
	  r = mpz_tdiv_q_ui (n, n, ul_factors[i]);
	  ASSERT_ALWAYS (r == 0UL);
	  mpz_init (t);
	  mpz_set_ui (t, ul_factors[i]);
	  append_mpz_to_array (factors, t);
	  mpz_clear (t);
	  append_uint32_to_array (multis, 1); /* FIXME, deal with repeated
						 factors correctly */
	}

      if (mpz_cmp_ui (n, 1UL) == 0)
	return 1;
      unsigned int s = BITSIZE(n);
      if (s <= l)
        {
          append_mpz_to_array (factors, n);
          append_uint32_to_array (multis, 1);
          return 1;
        }
      /* If we still have more than two primes (or something non-smooth),
         bail out */
      if (s > 2*l)
        return 0;
      /* We always abort below, so let's skip the prp test
      if (IS_PROBAB_PRIME(n))
        return 0; */
    }
  /* When sieving for 3 large primes, here are so many left over, non-smooth
     numbers here that factoring them all takes a long time, for few
     additional relations */
  return 0;
}

/* th->id gives the number of the thread: it is supposed to deal with the set
 * of bucket_regions corresponding to that number, ie those that are
 * congruent to id mod nb_thread.
 *
 * The other threads are accessed by combining the thread pointer th and
 * the thread id: the i-th thread is at th - id + i
 */
void *
process_bucket_region(thread_data_ptr th)
{
    sieve_info_t *si            = th->si;

    las_report_ptr rep = th->rep;

    local_sieve_data loc[2];
    memset(loc, 0, sizeof(loc));

    local_sieve_data_ptr lrat = loc[RATIONAL_SIDE];
    local_sieve_data_ptr lalg = loc[ALGEBRAIC_SIDE];

    for(int side = 0 ; side < 2 ; side++) {
        local_sieve_data_ptr lo = loc[side];
        sieve_side_info_ptr s = si->sides[side];
        thread_side_data_ptr ts = th->sides[side];

        /* make copies of small sieve data: the "next_position" field is
         * specific to each thread.  */
        if (si->nb_threads > 1) {
            lo->lssd= (small_sieve_data_t *)malloc(sizeof(small_sieve_data_t));
            clone_small_sieve (lo->lssd, &ts->ssd);
        } else {
            lo->lssd = &ts->ssd;
        }

        /* Yet another copy: used in factor_survivors for resieving small
         * primes */
        copy_small_sieve (lo->lsrsd, lo->lssd, s->trialdiv_primes);

        /* A third copy? 
         * TODO: come on! we should be able to do it with less copies 
         */
        if (si->nb_threads > 1) {
            lo->rssd= (small_sieve_data_t *)malloc(sizeof(small_sieve_data_t));
            clone_small_sieve (lo->rssd, lo->lsrsd);
        } else {
            lo->rssd = lo->lsrsd;
        }

        /* local sieve region */
        lo->S = (unsigned char *) malloc(bucket_region);
        ASSERT_ALWAYS (lo->S != NULL);
    }

    /* loop over appropriate set of sieve regions */
    for (int i = th->id; i < si->nb_buckets; i += si->nb_threads) 
      {
        /* update the positions */
        if (si->nb_threads > 1) {
            const int nl = (bucket_region >> si->logI)*i;
            int use_offset = i > th->id;
            for(int side = 0 ; side < 2 ; side++) {
                local_sieve_data_ptr lo = loc[side];
                thread_side_data_ptr ts = th->sides[side];

                ssd_update_positions(lo->lssd, &(ts->ssd), si, nl, use_offset);
                ssd_update_positions(lo->lsrsd, lo->rssd, si, nl, use_offset);
            }
        }

        /* Init rational norms */
        rep->tn[RATIONAL_SIDE] -= seconds ();
        init_rat_norms_bucket_region(lrat->S, i, si);
        rep->tn[RATIONAL_SIDE] += seconds ();

        /* Apply rational buckets */
        rep->ttsm -= seconds();
        for (int j = 0; j < si->nb_threads; ++j)  {
            thread_data_ptr ot = th + j - th->id;
            apply_one_bucket(lrat->S, ot->sides[RATIONAL_SIDE]->BA, i, si);
        }
        rep->ttsm += seconds();

        /* Sieve small rational primes */
        sieve_small_bucket_region(lrat->S, i, *lrat->lssd, si, 'r');
	

        /* Init algebraic norms */
        rep->tn[ALGEBRAIC_SIDE] -= seconds ();
        /* XXX Only the survivors of the rational sieve are initialized */
        rep->survivors0 += init_alg_norms_bucket_region(lalg->S, lrat->S, i, si);
        rep->tn[ALGEBRAIC_SIDE] += seconds ();

        /* Apply algebraic buckets */
        rep->ttsm -= seconds();
        for (int j = 0; j < si->nb_threads; ++j) {
            thread_data_ptr ot = th + j - th->id;
            apply_one_bucket(lalg->S, ot->sides[ALGEBRAIC_SIDE]->BA, i, si);
        }
        rep->ttsm += seconds();

        /* Sieve small algebraic primes */
        sieve_small_bucket_region(lalg->S, i, *lalg->lssd, si, 'a');

        /* Factor survivors */
        rep->ttf -= seconds ();
        rep->reports += factor_survivors (th, i, loc);
        rep->ttf += seconds ();
      }

    /* clear */
    for(int side = 0 ; side < 2 ; side++) {
        local_sieve_data_ptr lo = loc[side];
        free(lo->S);
        clear_small_sieve(*lo->lsrsd);
        if (si->nb_threads > 1) {
            clear_small_sieve(*lo->lssd);
            free(lo->lssd);
            clear_small_sieve(*lo->rssd);
            free(lo->rssd);
        }
    }
    return NULL;
}

static thread_data * thread_data_alloc(sieve_info_t * si)
{
    thread_data * thrs = (thread_data *) malloc(si->nb_threads * sizeof(thread_data));
    memset(thrs, 0, si->nb_threads * sizeof(thread_data));

    for(int i = 0 ; i < si->nb_threads ; i++) {
        thrs[i]->id = i;
        thrs[i]->si = si;
    }

    for(int z = 0 ; z < 2 ; z++) {
        int side = ALGEBRAIC_SIDE ^ z;
        sieve_side_info_ptr s = si->sides[side];
        /* This serves as a pointer */
        factorbase_degn_t *fb = s->fb;

        /* skip over small primes */
        while (fb->p != FB_END && fb->p < (fbprime_t) si->bucket_thresh)
            fb = fb_next (fb); 
#ifdef SPLIT_BUCKET_FB_PER_THREAD
        dispatch_fb(thrs, &fb_alg, fb, si->nb_threads);
#else
        ASSERT (fb->p != FB_END);
        for (int i = 0; i < si->nb_threads; ++i) {
            thrs[i]->sides[side]->fb_bucket = fb;
            fb = fb_next(fb);
            ASSERT (fb->p != FB_END);
            thrs[i]->sides[side]->pmax = FBPRIME_MAX;
        }
#endif
        fprintf (si->output, "# Number of primes in %s factor base = %zu\n", sidenames[side], fb_nroots_total(s->fb));

        /* Counting the bucket-sieved primes per thread.  */
        unsigned long * nn = (unsigned long *) malloc(si->nb_threads * sizeof(unsigned long));
        double * dd = (double *) malloc(si->nb_threads * sizeof(double));
        memset(nn, 0, si->nb_threads * sizeof(unsigned long));
        memset(dd, 0, si->nb_threads * sizeof(double));
        fb = thrs[0]->sides[side]->fb_bucket;
        for(int i = 0 ; fb->p != FB_END ; fb = fb_next (fb)) {
            nn[i] += fb->nr_roots;
            dd[i] += fb->nr_roots / (double) fb->p;
            i++;
            i %= si->nb_threads;
        }
        fprintf (si->output, "# Number of bucket-sieved primes in %s factor base per thread =", sidenames[side]);
        for(int i = 0 ; i < si->nb_threads ; i++)
            fprintf (si->output, " %lu", nn[i]);
        fprintf(si->output, "\n");
        fprintf (si->output, "# Inverse sum of bucket-sieved primes in %s factor base per thread =", sidenames[side]);
        for(int i = 0 ; i < si->nb_threads ; i++)
            fprintf (si->output, " %.5f", dd[i]);
        fprintf(si->output, " [hit jitter %.2f%%]\n", 100*(dd[0]/dd[si->nb_threads-1]-1));
        free(nn);
        free(dd);
    }
    return thrs;
}

static void thread_data_free(thread_data * thrs)
{
    free(thrs); /* nothing to do ! */
}

static void thread_buckets_alloc(thread_data * thrs)
{
    sieve_info_t * si = thrs[0]->si;
    for(int side = 0 ; side < 2 ; side++) {
        for (int i = 0; i < si->nb_threads; ++i) {
            thrs[i]->sides[side]->BA = init_bucket_array(si->nb_buckets, si->bucket_limit / si->nb_threads);
        }
    }
}

static void thread_buckets_free(thread_data * thrs)
{
    sieve_info_t * si = thrs[0]->si;
    for(int side = 0 ; side < 2 ; side++) {
        for (int i = 0; i < si->nb_threads; ++i) {
            clear_bucket_array(thrs[i]->sides[side]->BA);
        }
    }
}

static double thread_buckets_max_full(thread_data * thrs)
{
    sieve_info_t * si = thrs[0]->si;
    double mf, mf0 = 0;
    for (int i = 0; i < si->nb_threads; ++i) {
        mf = buckets_max_full (thrs[i]->sides[0]->BA);
        if (mf > mf0) mf0 = mf;
        mf = buckets_max_full (thrs[i]->sides[1]->BA);
        if (mf > mf0) mf0 = mf;
    }
    return mf0;
}


/*************************** main program ************************************/

static void
usage (const char *argv0, const char * missing)
{
  fprintf (stderr, "Usage: %s [-I I] -poly xxx.poly -fb xxx.roots -q0 q0 [-q1 q1] [-rho rho]\n",
           argv0);
  fprintf (stderr, "          -I i            sieving region has side 2^i [default %u]\n", DEFAULT_I);
  fprintf (stderr, "          -poly xxx.poly  use polynomial xxx.poly\n");
  fprintf (stderr, "          -fb xxx.roots   use factor base xxx.roots\n");
  fprintf (stderr, "          -q0 nnn         left bound of special-q range\n");
  fprintf (stderr, "          -q1 nnn         right bound of special-q range\n");
  fprintf (stderr, "          -rho r          sieve only algebraic root r mod q0\n");
  fprintf (stderr, "          -tdthresh nnn   trial-divide primes p/r <= nnn (r=number of roots)\n");
  fprintf (stderr, "          -bkthresh nnn   bucket-sieve primes p >= nnn\n");
  fprintf (stderr, "          -rlim     nnn   rational factor base bound nnn\n");
  fprintf (stderr, "          -alim     nnn   algebraic factor base bound nnn\n");
  fprintf (stderr, "          -lpbr     nnn   rational large prime bound 2^nnn\n");
  fprintf (stderr, "          -lpba     nnn   algebraic large prime bound 2^nnn\n");
  fprintf (stderr, "          -mfbr     nnn   rational cofactor bound 2^nnn\n");
  fprintf (stderr, "          -mfba     nnn   algebraic cofactor bound 2^nnn\n");
  fprintf (stderr, "          -rlambda  nnn   rational lambda value is nnn\n");
  fprintf (stderr, "          -alambda  nnn   algebraic lambda value is nnn\n");
  fprintf (stderr, "          -S        xxx   skewness value is xxx\n");
  fprintf (stderr, "          -v              be verbose (print some sieving statistics)\n");
  fprintf (stderr, "          -out filename   write relations to filename instead of stdout\n");
  fprintf (stderr, "          -mt nnn   use nnn threads\n");
  fprintf (stderr, "          -ratq           use rational special-q\n");
  fprintf (stderr, "          The following are for benchs:\n");
  fprintf (stderr, "          -bench          activate bench mode\n");
  fprintf (stderr, "          -skfact   xxx   skip factor, default=1.01\n");
  fprintf (stderr, "          -bench2         activate alternate bench mode\n");
  fprintf (stderr, "          -percent   xxx  percentage of sieving, default=1e-3\n");
  if (missing) {
      fprintf(stderr, "\nError: missing parameter %s\n", missing);
  }
  exit (EXIT_FAILURE);
}

int
main (int argc0, char *argv0[])
{
    sieve_info_t si;
    const char *fbfilename = NULL;
    const char *polyfilename = NULL;
    cado_poly cpoly;
    double t0, tfb, tts;
    uint64_t q0 = 0, q1 = 0, rho = 0;
    uint64_t *roots;
    unsigned long nroots;
    int td_thresh = 1024; /* cost threshold trialdiv/resieving */
    int bucket_thresh = 0;
    int rpow_lim = 0, apow_lim = 0;
    int I = DEFAULT_I, i;
    unsigned long sq = 0;
    double totJ = 0.0;
    /* following command-line values override those in the polynomial file */
    const char *outputname = NULL;
    int argc = argc0;
    char **argv = argv0;
    double max_full = 0.;
    int nb_threads = 1;
    int bench = 0;
    int bench2 = 0;
    double skip_factor = 1.01;  /* next_q = q*skip_factor in bench mode */
    double bench_percent = 1e-3; 
    long bench_tot_rep = 0;
    double bench_tot_time = 0.0;

    memset(&si, 0, sizeof(sieve_info_t));

    param_list pl;
    param_list_init(pl);
    cado_poly_init (cpoly);
    param_list_configure_knob(pl, "-v", &si.verbose);
    param_list_configure_knob(pl, "-ratq", &si.ratq);
    param_list_configure_knob(pl, "-bench", &bench);
    param_list_configure_knob(pl, "-bench2", &bench2);
    argv++, argc--;
    for( ; argc ; ) {
        if (param_list_update_cmdline(pl, &argc, &argv)) { continue; }
        /* Could also be a file */
        FILE * f;
        if ((f = fopen(argv[0], "r")) != NULL) {
            param_list_read_stream(pl, f);
            fclose(f);
            argv++,argc--;
            continue;
        }
        fprintf(stderr, "Unhandled parameter %s\n", argv[0]);
        usage(argv0[0],NULL);
    }

    polyfilename = param_list_lookup_string(pl, "poly");
    if (polyfilename) param_list_read_file(pl, polyfilename);
    fbfilename = param_list_lookup_string(pl, "fb");

    if (!cado_poly_set_plist (cpoly, pl)) {
        fprintf (stderr, "Error reading polynomial file\n");
        exit (EXIT_FAILURE);
    }

    param_list_parse_int(pl, "mt", &nb_threads);
    param_list_parse_int(pl, "I", &I);
    param_list_parse_uint64(pl, "q0", &q0);
    param_list_parse_uint64(pl, "q1", &q1);
    param_list_parse_uint64(pl, "rho", &rho);
    param_list_parse_int(pl, "tdthresh", &td_thresh);
    param_list_parse_int(pl, "bkthresh", &bucket_thresh);
    param_list_parse_int(pl, "rpowlim", &rpow_lim);
    param_list_parse_int(pl, "apowlim", &apow_lim);
    param_list_parse_double(pl, "S", &cpoly->skew);
    param_list_parse_double(pl, "skfact", &skip_factor);
    param_list_parse_double(pl, "percent", &bench_percent);
    int ok = 1;
    ok = ok && param_list_parse_ulong(pl, "rlim", &cpoly->rlim);
    ok = ok && param_list_parse_ulong(pl, "alim", &cpoly->alim);
    ok = ok && param_list_parse_int(pl, "lpbr", &cpoly->lpbr);
    ok = ok && param_list_parse_int(pl, "lpba", &cpoly->lpba);
    ok = ok && param_list_parse_int(pl, "mfbr", &cpoly->mfbr);
    ok = ok && param_list_parse_int(pl, "mfba", &cpoly->mfba);
    ok = ok && param_list_parse_double(pl, "rlambda", &cpoly->rlambda);
    ok = ok && param_list_parse_double(pl, "alambda", &cpoly->alambda);

    if (!ok) {
        fprintf(stderr, "Some parameters are missing among *lim lpb* mfb* *lambda\n");
        usage(argv0[0],NULL);
    }

    outputname = param_list_lookup_string(pl, "out");
    /* Init output file */
    si.output = stdout;
    if (outputname) {
        if (!(si.output = gzip_open (outputname, "w"))) {
            fprintf (stderr, "Could not open %s for writing\n", outputname);
            exit (EXIT_FAILURE);
        }
    }

    param_list_print_command_line(si.output, pl);

#ifdef SSE_NORM_INIT
    fprintf (si.output, "# SSE_NORM_INIT is activated\n");
#endif

    if (fbfilename == NULL) usage(argv0[0], "fb");
    if (q0 == 0) usage(argv0[0], "q0");

    /* if -rho is given, we sieve only for q0, thus -q1 is not allowed */
    if (rho != 0 && q1 != 0)
      {
        fprintf (stderr, "Error, -q1 and -rho are mutually exclusive\n");
        exit (EXIT_FAILURE);
      }

    /* if -q1 is not given, sieve only for q0 */
    if (q1 == 0)
      q1 = q0 + 1;

    /* check that q1 fits into an unsigned long */
    if (q1 > (uint64_t) ULONG_MAX)
      {
        fprintf (stderr, "Error, q1=%" PRIu64 " exceeds ULONG_MAX\n", q1);
        exit (EXIT_FAILURE);
      }

    fprintf (si.output, "# Sieving parameters: rlim=%lu alim=%lu lpbr=%d lpba=%d\n",
             cpoly->rlim, cpoly->alim, cpoly->lpbr, cpoly->lpba);
    fprintf (si.output, "#                     mfbr=%d mfba=%d rlambda=%1.1f alambda=%1.1f\n",
             cpoly->mfbr, cpoly->mfba, cpoly->rlambda, cpoly->alambda);
    fprintf (si.output, "#                     skewness=%1.1f\n",
             cpoly->skew);

    if (cpoly->skew <= 0.0)
      {
        fprintf (stderr, "Error, please provide a positive skewness\n");
        exit (EXIT_FAILURE);
      }

    /* check that nb_threads (-mt nnn) is positive */
    if (nb_threads <= 0)
      {
        fprintf (stderr, "Error, please provide a positive number of threads\n");
        exit (EXIT_FAILURE);
      }

    si.bench=bench + bench2;

    /* this does not depend on the special-q */
    sieve_info_init (&si, cpoly, I, q0, bucket_thresh, si.output, nb_threads);

    sieve_side_info_ptr rat = si.sides[RATIONAL_SIDE];
    sieve_side_info_ptr alg = si.sides[ALGEBRAIC_SIDE];

    /* {{{ Read algebraic factor base */
    {
      fbprime_t *leading_div;
      tfb = seconds ();
      leading_div = factor_small (cpoly->f[cpoly->degree], cpoly->alim);
      alg->fb = fb_read_addproj (fbfilename, alg->scale * LOG_SCALE, 0,
				leading_div);
      ASSERT_ALWAYS(alg->fb != NULL);
      tfb = seconds () - tfb;
      fprintf (si.output, 
               "# Reading algebraic factor base of %zuMb took %1.1fs\n", 
               fb_size (alg->fb) >> 20, tfb);
      free (leading_div);
    }
    /* }}} */
    /* {{{ Prepare rational factor base */
    {
        tfb = seconds ();
        if (rpow_lim >= si.bucket_thresh)
          {
            rpow_lim = si.bucket_thresh - 1;
            printf ("# rpowthresh reduced to %d\n", rpow_lim);
          }
        rat->fb = fb_make_linear ((const mpz_t *) cpoly->g, (fbprime_t) cpoly->rlim,
                                 rpow_lim, rat->scale * LOG_SCALE, 
                                 si.verbose, 1, si.output);
        tfb = seconds () - tfb;
        fprintf (si.output, "# Creating rational factor base of %zuMb took %1.1fs\n",
                 fb_size (rat->fb) >> 20, tfb);
    }
    /* }}} */

    thread_data * thrs = thread_data_alloc(&si);

    init_rat_norms (&si);
    init_alg_norms (&si);

    sieve_info_init_refactoring(&si, td_thresh); /* Init refactoring stuff */
    si.strategy = facul_make_strategy (15, MIN(cpoly->rlim, cpoly->alim),
                                       1UL << MIN(cpoly->lpbr, cpoly->lpba));

    las_report report;
    las_report_init(report);

    /* special q (and root rho) */
    roots = (uint64_t *) malloc (cpoly->degree * sizeof (uint64_t));
    q0 --; /* so that nextprime gives q0 if q0 is prime */
    nroots = 0;

    t0 = seconds ();
    fprintf (si.output, "#\n");
    int rep_bench = 0;
    int nbq_bench = 0;
    double t_bench = seconds();
    while (q0 < q1)
      {
        while (nroots == 0) /* {{{ go to next prime and generate roots */
          {
            q0 = uint64_nextprime (q0);
            if (q0 >= q1)
              goto end;  // breaks two whiles.
            si.q = q0;
            if (si.ratq)
                nroots = poly_roots_uint64 (roots, cpoly->g, 1, q0);
            else
                nroots = poly_roots_uint64 (roots, cpoly->f, cpoly->degree, q0);
            if (nroots > 0)
              {
                fprintf (si.output, "### q=%" PRIu64 ": root%s", q0,
                         (nroots == 1) ? "" : "s");
                for (i = 1; i <= (int) nroots; i++)
                  fprintf (si.output, " %" PRIu64, roots[nroots-i]);
                fprintf (si.output, "\n");
              }
          }
        /* }}} */

        /* computes a0, b0, a1, b1 from q, rho, and the skewness */
        si.rho = roots[--nroots];
        if (rho != 0 && si.rho != rho) /* if -rho, wait for wanted root */
          continue;
        if (SkewGauss (&si, cpoly->skew) != 0)
	  continue;
        /* FIXME: maybe we can discard some special q's if a1/a0 is too large,
           see http://www.mersenneforum.org/showthread.php?p=130478 */

        fprintf (si.output, "# Sieving q=%" PRIu64 "; rho=%" PRIu64
                 "; a0=%d; b0=%d; a1=%d; b1=%d\n",
                 si.q, si.rho, si.a0, si.b0, si.a1, si.b1);
        sq ++;

        /* precompute the skewed polynomials of f(x) and g(x) */
        fij_from_f (si.fij, &si, cpoly->f, cpoly->degree);
        fij_from_f (si.gij, &si, cpoly->g, 1);

        /* checks the value of J, updates floating-point fij(x) */
        sieve_info_update (&si);
        totJ += (double) si.J;

#ifdef TRACE_K
        if (trace_ab.a || trace_ab.b) {
            if (!ABToIJ(&trace_ij.i, &trace_ij.j, trace_ab.a, trace_ab.b, &si)) {
                fprintf(stderr, "# Relation (%"PRId64",%"PRIu64") to be traced is outside of the current q-lattice\n", trace_ab.a, trace_ab.b);
                trace_ij.i=0;
                trace_ij.j=UINT_MAX;
                trace_Nx.N=0;
                trace_Nx.x=UINT_MAX;
            } else {
                IJToNx(&trace_Nx.N, &trace_Nx.x, trace_ij.i, trace_ij.j, &si);
            }
        } else if (trace_ij.i || trace_ij.j < UINT_MAX) {
            IJToAB(&trace_ab.a, &trace_ab.b, trace_ij.i, trace_ij.j, &si);
            IJToNx(&trace_Nx.N, &trace_Nx.x, trace_ij.i, trace_ij.j, &si);
        } else if (trace_Nx.N || trace_Nx.x < UINT_MAX) {
            NxToIJ(&trace_ij.i, &trace_ij.j, trace_Nx.N, trace_Nx.x, &si);
            IJToAB(&trace_ab.a, &trace_ab.b, trace_ij.i, trace_ij.j, &si);
        }
        if (trace_ij.j < UINT_MAX && trace_ij.j >= si.J) {
            fprintf(stderr, "# Relation (%"PRId64",%"PRIu64") to be traced is outside of the current (i,j)-rectangle (j=%u)\n", trace_ab.a, trace_ab.b, trace_ij.j);
            trace_ij.i=0;
            trace_ij.j=UINT_MAX;
            trace_Nx.N=0;
            trace_Nx.x=UINT_MAX;
        }
        if (trace_ij.i || trace_ij.j < UINT_MAX) {
            fprintf(stderr, "# Tracing relation (a,b)=(%"PRId64",%"PRIu64") (i,j)=(%d,%u), (N,x)=(%u,%u)\n",
                    trace_ab.a, trace_ab.b, trace_ij.i, trace_ij.j, trace_Nx.N, trace_Nx.x);
        }
#endif

            report->ttsm -= seconds();

            /* Allocate buckets */
            thread_buckets_alloc(thrs);

            /* Fill in rat and alg buckets */
            thread_do(thrs, &fill_in_buckets_both);

            max_full = thread_buckets_max_full(thrs);
            if (max_full >= 1.0) {
                fprintf(stderr, "maxfull=%f\n", max_full);
                for (i = 0; i < si.nb_threads; ++i) {
                    fprintf(stderr, "intend to free [%d] max_full=%f %f\n",
                            i,
                            buckets_max_full (thrs[i]->sides[0]->BA),
                            buckets_max_full (thrs[i]->sides[1]->BA));
                }
                thread_buckets_free(thrs); /* may crash. See below */

                int old_bl = si.bucket_limit;
                si.bucket_limit *= old_bl;
                si.bucket_limit += si.bucket_limit / 10;
                max_full *= (double) old_bl / si.bucket_limit;
                nroots++;   // ugly: redo the same class
                // when doing one big malloc, there's some chance that the
                // bucket overrun actually stepped over the next bucket. In
                // this case, the freeing of buckets in the code above might
                // have succeeded, so we can hope to resume with this special
                // q. On the other hand, if we have one malloc per bucket,
                // the free() calls above are guaranteed to crash.
                // Thus it's okay to proceed, if we're lucky enough to reach
                // here. Note that increasing bucket_limit will have a
                // permanent effect on the rest of this run.
                // abort();
                continue;
            }

            report->ttsm += seconds();

            /* Initialize data for sieving small primes */
            thread_data_init_small_sieve(thrs);

            /* Process bucket regions in parallel */
            thread_do(thrs, &process_bucket_region);

            /* Display results for this special q */
            {
                las_report rep;
                las_report_init(rep);
                for (int i = 0; i < si.nb_threads; ++i) {
                    las_report_accumulate(rep, thrs[i]->rep);
                }
                if (si.verbose) {
                    fprintf (si.output, "# %lu survivors after rational sieve,", rep->survivors0);
                    fprintf (si.output, " %lu survivors after algebraic sieve, ", rep->survivors1);
                    fprintf (si.output, "coprime: %lu\n", rep->survivors2);
                }
                fprintf (si.output, "# %lu relation(s) for (%" PRIu64 ",%" PRIu64")\n", rep->reports, si.q, si.rho);
                rep_bench += rep->reports;
                las_report_accumulate(report, rep);
                las_report_clear(rep);
            }
            
            thread_data_clear_small_sieve(thrs);

            thread_buckets_free(thrs);

        /* {{{ bench stats */
        if (bench) {
            uint64_t newq0 = (uint64_t) (skip_factor*((double) q0));
            uint64_t savq0 = q0;
            // print some estimates for special-q's between q0 and the next
            int nb_q = 1;
            do {
                q0 = uint64_nextprime (q0);
                nb_q ++;
            } while (q0 < newq0);
            q0 = newq0;
            nroots=0;
            t_bench = seconds() - t_bench;
            fprintf(si.output,
              "# Stats for q=%" PRIu64 ": %d reports in %1.1f s\n",
              savq0, rep_bench, t0);
            fprintf(si.output,
              "# Estimates for next %d q's: %d reports in %1.0f s, %1.2f s/r\n",
              nb_q, nb_q*rep_bench, t0*nb_q, t0/((double)rep_bench));
            bench_tot_time += t0*nb_q;
            bench_tot_rep += nb_q*rep_bench;
            rep_bench = 0;
            fprintf(si.output, "# Cumulative (estimated): %lu reports in %1.0f s, %1.2f s/r\n",
                    bench_tot_rep, bench_tot_time,
		    (double) bench_tot_time / (double) bench_tot_rep);
            t_bench = seconds();
        }
        /* }}} */
        /* {{{ bench stats */
        if (bench2) {
            nbq_bench++;
            const int BENCH2 = 50;
            if (rep_bench >= BENCH2) {
                t_bench = seconds() - t_bench;
                fprintf(si.output,
                  "# Got %d reports in %1.1f s using %d specialQ\n",
                  rep_bench, t_bench, nbq_bench);
                double relperq = (double)rep_bench / (double)nbq_bench;
                double est_rep = (double)rep_bench;
                do {
                    q0 = uint64_nextprime (q0);
                    est_rep += relperq;
                } while (est_rep <= BENCH2 / bench_percent);
                fprintf(si.output,
                  "# Extrapolate to %ld reports up to q = %" PRIu64 "\n",
                  (long) est_rep, q0);
                bench_tot_time += t_bench / bench_percent;
                bench_tot_rep += BENCH2 / bench_percent;
                fprintf(si.output,
                  "# Cumulative (estimated): %lu reports in %1.0f s, %1.2f s/r\n",
                  bench_tot_rep, bench_tot_time,
                  (double) bench_tot_time / (double) bench_tot_rep);
                // reinit for next slice of bench:
                t_bench = seconds();
                nbq_bench = 0;
                rep_bench = 0;
                nroots=0;
            }
        }
        /* }}} */
      } // end of loop over special q ideals.

 end:
    /* {{{ stats */
    t0 = seconds () - t0;
    fprintf (si.output, "# Average J=%1.0f for %lu special-q's, max bucket fill %f\n",
             totJ / (double) sq, sq, max_full);
    tts = t0;
    tts -= report->tn[0];
    tts -= report->tn[1];
    tts -= report->ttf;
    if (si.verbose)
      facul_print_stats (si.output);
    if (si.verbose)
    {
        fprintf (si.output, "# Histogram of sieve report values that led to "
                "relations:\n");
        for(int side = 0 ; side < 2 ; side++) {
            fprintf (si.output, "# %s side: ", sidenames[side]);
            for (i = 0; i < 256; i++) {
                unsigned long r = report->report_sizes[side][i];
                if (r>0)
                    fprintf (si.output, "%d:%lu ", i, r);
            }
            fprintf (si.output, "\n");
        }
      }
    if (si.nb_threads > 1) 
        fprintf (si.output, "# Total wct time %1.1fs [precise timings available only for mono-thread]\n", t0);
    else
        fprintf (si.output, "# Total time %1.1fs [norm %1.2f+%1.1f, sieving %1.1f"
            " (%1.1f + %1.1f),"
             " factor %1.1f]\n", t0,
             report->tn[RATIONAL_SIDE],
             report->tn[ALGEBRAIC_SIDE],
             tts, report->ttsm, tts-report->ttsm, report->ttf);
    fprintf (si.output, "# Total %lu reports [%1.3fs/r, %1.1fr/sq]\n",
             report->reports, t0 / (double) report->reports,
             (double) report->reports / (double) sq);
    if (bench || bench2) {
        fprintf(si.output, "# Total (estimated): %lu reports in %1.1f s\n",
                bench_tot_rep, bench_tot_time);
    }
    /* }}} */

    /* {{{ stats */
    if (bucket_prime_stats) 
      {
        printf ("# Number of bucket primes: %ld\n", nr_bucket_primes);
        printf ("# Number of divisibility tests of bucket primes: %ld\n", 
                nr_div_tests);
        printf ("# Number of compositeness tests of bucket primes: %ld\n", 
                nr_composite_tests);
        printf ("# Number of wrapped composite values while dividing out "
                "bucket primes: %ld\n", nr_wrap_was_composite);
      }
    /* }}} */

    sieve_info_clear_refactoring(&si);
    facul_clear_strategy (si.strategy);
    si.strategy = NULL;

    thread_data_free(thrs);

    free(si.sides[0]->fb);
    free(si.sides[1]->fb);
    sieve_info_clear (&si);
    cado_poly_clear (cpoly);
    free (roots);
    las_report_clear(report);

    if (outputname)
        gzip_close (si.output, "");

    param_list_clear(pl);

    return 0;
}
