#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "ularith.h"
#include "modredc_ul.h"
#include "trialdiv.h"
#ifdef TESTDRIVE
#include "getprime.h"
#include <sys/time.h>
#include <sys/resource.h>
#endif

static const int verbose = 0;

static void
trialdiv_init_divisor (trialdiv_divisor_t *d, const unsigned long p)
{
#if TRIALDIV_MAXLEN > 1
  int i;
#endif
  ASSERT (p % 2UL == 1UL);
  /* Test that p^2 does not overflow an unsigned long */
  ASSERT (TRIALDIV_MAXLEN == 1 || p < (1UL << (LONG_BIT / 2)));
  /* Test that p < sqrt (word_base / (TRIALDIV_MAXLEN - 1)) */
  ASSERT (TRIALDIV_MAXLEN == 1 || 
	  p * p < ULONG_MAX / (TRIALDIV_MAXLEN - 1));
  d->p = p;
#if TRIALDIV_MAXLEN > 1
  if (p == 1UL)
    d->w[0] = 0UL; /* DIV would cause quotient overflow */
  else
    ularith_div_2ul_ul_ul_r (&(d->w[0]), 0UL, 1UL, p);
  for (i = 1; i < TRIALDIV_MAXLEN; i++)
    ularith_div_2ul_ul_ul_r (&(d->w[i]), 0UL, d->w[i - 1], p);
#endif
  d->pinv = ularith_invmod (p);
  d->plim = ULONG_MAX / p;
}

/* Trial division for integers with 1 unsigned long */
static inline int
trialdiv_div1 (const unsigned long *n, const trialdiv_divisor_t *d)
{
#ifdef __GNUC__
  __asm__ ("# trialdiv_div1");
#endif
  return n[0] * d->pinv <= d->plim;
}

/* Trial division for integers with 2 unsigned long */
static inline int
trialdiv_div2 (const unsigned long *n, const trialdiv_divisor_t *d)
{
  unsigned long r0, r1, x0, x1;
#ifdef __GNUC__
  __asm__ ("# trialdiv_div2");
#endif
  ularith_mul_ul_ul_2ul (&x0, &x1, n[1], d->w[0]); /* n_1 * (w^1 % p) */
  r0 = n[0];
  r1 = x1;
  ularith_add_ul_2ul (&r0, &r1, x0);
  x0 = r1 * d->w[0]; /* TODO: optimal? */
  x0 += r0;
  if (x0 < r0)
    x0 += d->w[0];
  x0 *= d->pinv;
  return x0 <= d->plim;
}

/* Trial division for integers with 3 unsigned long */
static inline int
trialdiv_div3 (const unsigned long *n, const trialdiv_divisor_t *d)
{
  unsigned long r0, r1, x0, x1;
#ifdef __GNUC__
  __asm__ ("# trialdiv_div3");
#endif
  if (verbose)
    printf ("p = %lu, n2:n1:n0 = %lu * w^2 + %lu * w + %lu", 
            d->p, n[2], n[1], n[0]);
  ularith_mul_ul_ul_2ul (&x0, &x1, n[1], d->w[0]); /* n_1 * (w^1 % p) */
  r0 = n[0];
  r1 = x1;
  ularith_add_ul_2ul (&r0, &r1, x0);
  ularith_mul_ul_ul_2ul (&x0, &x1, n[2], d->w[1]); /* n_2 * (w^2 % p) */
  ularith_add_2ul_2ul (&r0, &r1, x0, x1);
  if (verbose)
    printf ("  r1:r0 = %lu * w + %lu", r1, r0);
  x0 = r1 * d->w[0];
  x0 += r0;
  if (x0 < r0)
    x0 += d->w[0];
  if (verbose)
    printf ("  x0 = %lu", x0);
  x0 *= d->pinv;
  return x0 <= d->plim;
}

/* Trial division for integers with 4 unsigned long */
static inline int
trialdiv_div4 (const unsigned long *n, const trialdiv_divisor_t *d)
{
  unsigned long r0, r1, x0, x1;
#ifdef __GNUC__
  __asm__ ("# trialdiv_div4");
#endif
  ularith_mul_ul_ul_2ul (&x0, &x1, n[1], d->w[0]); /* n_1 * (w^1 % p) */
  r0 = n[0];
  r1 = x1;
  ularith_add_ul_2ul (&r0, &r1, x0);
  ularith_mul_ul_ul_2ul (&x0, &x1, n[2], d->w[1]); /* n_2 * (w^2 % p) */
  ularith_add_2ul_2ul (&r0, &r1, x0, x1);
  ularith_mul_ul_ul_2ul (&x0, &x1, n[3], d->w[2]); /* n_3 * (w^3 % p) */
  ularith_add_2ul_2ul (&r0, &r1, x0, x1);
  x0 = r1 * d->w[0];
  x0 += r0;
  if (x0 < r0)
    x0 += d->w[0];
  x0 *= d->pinv;
  return x0 <= d->plim;
}

/* Trial division for integers with 5 unsigned long */
static inline int
trialdiv_div5 (const unsigned long *n, const trialdiv_divisor_t *d)
{
  unsigned long r0, r1, x0, x1;
#ifdef __GNUC__
  __asm__ ("# trialdiv_div5");
#endif
  ularith_mul_ul_ul_2ul (&x0, &x1, n[1], d->w[0]); /* n_1 * (w^1 % p) */
  r0 = n[0];
  r1 = x1;
  ularith_add_ul_2ul (&r0, &r1, x0);
  ularith_mul_ul_ul_2ul (&x0, &x1, n[2], d->w[1]); /* n_2 * (w^2 % p) */
  ularith_add_2ul_2ul (&r0, &r1, x0, x1);
  ularith_mul_ul_ul_2ul (&x0, &x1, n[3], d->w[2]); /* n_3 * (w^3 % p) */
  ularith_add_2ul_2ul (&r0, &r1, x0, x1);
  ularith_mul_ul_ul_2ul (&x0, &x1, n[4], d->w[3]); /* n_4 * (w^4 % p) */
  ularith_add_2ul_2ul (&r0, &r1, x0, x1);
  x0 = r1 * d->w[0];
  x0 += r0;
  if (x0 < r0)
    x0 += d->w[0];
  x0 *= d->pinv;
  return x0 <= d->plim;
}

/* Trial division for integers with 6 unsigned long */
static inline int
trialdiv_div6 (const unsigned long *n, const trialdiv_divisor_t *d)
{
  unsigned long r0, r1, x0, x1;
#ifdef __GNUC__
  __asm__ ("# trialdiv_div6");
#endif
  ularith_mul_ul_ul_2ul (&x0, &x1, n[1], d->w[0]); /* n_1 * (w^1 % p) */
  r0 = n[0];
  r1 = x1;
  ularith_add_ul_2ul (&r0, &r1, x0);
  ularith_mul_ul_ul_2ul (&x0, &x1, n[2], d->w[1]); /* n_2 * (w^2 % p) */
  ularith_add_2ul_2ul (&r0, &r1, x0, x1);
  ularith_mul_ul_ul_2ul (&x0, &x1, n[3], d->w[2]); /* n_3 * (w^3 % p) */
  ularith_add_2ul_2ul (&r0, &r1, x0, x1);
  ularith_mul_ul_ul_2ul (&x0, &x1, n[4], d->w[3]); /* n_4 * (w^4 % p) */
  ularith_add_2ul_2ul (&r0, &r1, x0, x1);
  ularith_mul_ul_ul_2ul (&x0, &x1, n[5], d->w[4]); /* n_5 * (w^5 % p) */
  ularith_add_2ul_2ul (&r0, &r1, x0, x1);
  x0 = r1 * d->w[0];
  x0 += r0;
  if (x0 < r0)
    x0 += d->w[0];
  x0 *= d->pinv;
  return x0 <= d->plim;
}


/* Divides primes in d out of N and stores them (with multiplicity) in f.
   Never stores more than max_div primes in f. Returns the number of
   primes found, or max_div+1 if the number of primes exceeds max_div. 
   Primes not stored in f (because max_div was exceeded) are not divided out.
   This way, trial division of highly composite values can be processed
   by repeated calls of trialdiv(). */

size_t 
trialdiv (unsigned long *f, mpz_t N, const trialdiv_divisor_t *d,
	  const size_t max_div)
{
  size_t n = 0;
  
#if TRIALDIV_MAXLEN > 6
#error trialdiv not implemented for input sizes of more than 6 words
#endif

  while (mpz_cmp_ui (N, 1UL) > 0)
    {
      size_t s = mpz_size(N);
      if (verbose) gmp_printf ("s = %d, N = %Zd, ", s, N);
      if (s > TRIALDIV_MAXLEN)
        abort ();
      if (s == 1)
        {
	  mp_limb_t t = mpz_getlimbn (N, 0);
	  while (t * d->pinv > d->plim)
	    d++;
        }
#if TRIALDIV_MAXLEN >= 2
      else if (s == 2)
        {
          while (!trialdiv_div2 (N[0]._mp_d, d))
            d++;
        }
#endif
#if TRIALDIV_MAXLEN >= 3
      else if (s == 3)
        {
          while (!trialdiv_div3 (N[0]._mp_d, d))
            d++;
        }
#endif
#if TRIALDIV_MAXLEN >= 4
      else if (s == 4)
        {
          while (!trialdiv_div4 (N[0]._mp_d, d))
            d++;
        }
#endif
#if TRIALDIV_MAXLEN >= 5
      else if (s == 5)
        {
          while (!trialdiv_div5 (N[0]._mp_d, d))
            d++;
        }
#endif
#if TRIALDIV_MAXLEN >= 6
      else if (s == 6)
        {
          while (!trialdiv_div6 (N[0]._mp_d, d))
            d++;
        }
#endif
      if (verbose) printf ("\n");

      if (d->p == 1UL)
        break;

      ASSERT (mpz_divisible_ui_p (N, d->p));

      if (n == max_div)
	return max_div + 1;

      f[n++] = d->p;
      mpz_divexact_ui (N, N, d->p); /* TODO, we can use pinv here */
    }
  
  return n;
}


/* Initialise a trialdiv_divisor_t array with the nr primes stored in *f.
   This function allcates memory for the array, inits each entry, and puts
   a sentinel at the end. */
trialdiv_divisor_t *
trialdiv_init (const fbprime_t *f, const unsigned int nr)
{
  trialdiv_divisor_t *d;
  unsigned int i;
  
  d = (trialdiv_divisor_t *) malloc ((nr + 1) * sizeof (trialdiv_divisor_t));
  ASSERT (d != NULL);
  for (i = 0; i < nr; i++)
    trialdiv_init_divisor (&(d[i]), (unsigned long) (f[i]));
  trialdiv_init_divisor (&(d[nr]), 1UL);

  return d;
}

void
trialdiv_clear (trialdiv_divisor_t *d)
{
  free (d);
}

#ifdef TESTDRIVE
int main (int argc, char **argv)
{
  fbprime_t *primes;
  trialdiv_divisor_t *d;
  size_t max_div = 16;
  unsigned long *factors;
  int i, r = 0, expect = 0, len = 1, nr_primes = 1000, nr_N = 1000000;
  mpz_t M, N;
  struct rusage usage;
  double usrtime;
  
  if (argc > 1)
    len = atoi (argv[1]);
  
  if (argc > 2)
    nr_N = atoi (argv[2]);

  if (argc > 3)
    nr_primes = atoi (argv[3]);
  
  if (len > TRIALDIV_MAXLEN)
    {
      printf ("Error, trialdiv not implemented for input sizes greater than "
	      "%d words\n", TRIALDIV_MAXLEN);
      exit (EXIT_FAILURE);
    }

  mpz_init (M);
  mpz_init (N);
  mpz_set_ui (N, 1UL);
  mpz_mul_2exp (N, N, 8 * sizeof(unsigned long) * len);
  mpz_tdiv_q_ui (N, N, 3UL);
  mpz_nextprime (N, N);

  for (i = 0; i < 0; i++)
    getprime(1UL);
  primes = malloc (nr_primes * sizeof (fbprime_t));
  for (i = 0; i < nr_primes; i++)
    {
      unsigned long ppow;
      primes[i] = getprime(1UL);
      ppow = 1UL;
      /* FIXME powers greater than ULONG_MAX may appear in N */
      while (ULONG_MAX / primes[i] >= ppow)
	{
	  ppow *= primes[i];
	  expect += nr_N / ppow;
	  if (mpz_tdiv_ui (N, ppow) + nr_N % ppow > ppow)
	    expect++;
	}
    }
  printf ("Primes in [%lu, %lu]\n", 
	  (unsigned long) primes[0], (unsigned long) primes[nr_primes - 1]);
  getprime (0);

  d = trialdiv_init (primes, nr_primes);
  free (primes);
  factors = (unsigned long *) malloc (max_div * sizeof (unsigned long));
  
  for (i = 0; i < nr_N; i++)
    {
      size_t t;
      mpz_set (M, N);
      do {
	t = trialdiv (factors, M, d, max_div);
	r += (t > max_div) ? max_div : t;
      } while (t > max_div);
      mpz_add_ui (N, N, 1UL);
    }
  
  getrusage(RUSAGE_SELF, &usage);
  usrtime = (double) usage.ru_utime.tv_sec * 1000000. +
      (double) usage.ru_utime.tv_usec;

  mpz_clear (M);
  mpz_clear (N);
  trialdiv_clear (d);
  free (factors);
  printf ("Found %d divisors, expected %d\n", r, expect);
  if (r != expect)
    printf ("Error: did not find the expected number of divisors!\n");
  printf ("Time: %f s, per N: %f mus, per prime: %f ns\n", 
	  usrtime / 1e6, usrtime / nr_N, usrtime / nr_N / nr_primes * 1e3);
  return 0;
}
#endif
