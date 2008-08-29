#include "mod_ul.h"
#include "mod_ul_default.h"

#include "mod_ul_common.c"


/* Compute 1/n (mod 2^wordsize).
   Cruft: this belongs in modredc_*.h, not here  */
unsigned long
modul_invmodlong (unsigned long n)
{
  unsigned long r;

  ASSERT (n % 2UL != 0UL);
  
  /* The square of an odd integer is always 1 (mod 8). So by
     initing r = m, the low three bits in the approximate inverse
     are correct. 
     When r = 1/m (mod 16), the 4th bit of r happens to be the
     XOR of bits 2, 3 and 4 of m. This gives us an approximate 
     inverse with the 4 lowest bits correct, so 3 (for 32 bit) or
     4 (for 64 bit) Newton iterations are enough. */
  r = n ^ ((n & 4UL) << 1) ^ ((n & 2UL) << 2);
  r = 2UL * r - r * r * n; /* Newton iteration */
  r = 2UL * r - r * r * n;
  r = 2UL * r - r * r * n;
  if (sizeof (unsigned long) > 4)
    r = 2UL * r - r * r * n;

  ASSERT_EXPENSIVE (r * n == 1UL);

  return r;
}


/* Put 1/s (mod t) in r and return 1 if s is invertible, 
   or set r to 0 and return 0 if s is not invertible */

int
mod_inv (residue_t r, const residue_t sp, const modulus_t m)
{
  long u1, v1;
  unsigned long u2, v2, s, t;
#ifndef NDEBUG
  residue_t tmp;
#endif

#ifndef NDEBUG
  /* Remember input in case r overwrites it */
  mod_init_noset0 (tmp, m);
  mod_set (tmp, sp, m);
#endif

  s = mod_get_ul (sp, m);
  t = mod_getmod_ul (m);

  ASSERT (t > 0UL);
  ASSERT (s < t);

  if (s == 0UL)
    {
      r[0] = 0UL; /* Not invertible */
#ifndef NDEBUG
      mod_clear (tmp, m);
#endif
      return 0;
    }

  if (s == 1UL)
    {
      r[0] = 1UL;
#ifndef NDEBUG
      mod_clear (tmp, m);
#endif
      return 1;
    }

  u1 = 1L;
  u2 = s;
  v1 = - (long) (t / s); /* No overflow, since s >= 2 */
  v2 = t % s;

  if (v2 == 1UL)
    {
       u1 = v1 + t;
    }
  else 
    {
      while (v2 != 0UL)
	{
	  unsigned long q;
	  /* unroll twice and swap u/v */
	  q = u2 / v2;
	  ASSERT_EXPENSIVE (q <= (unsigned long) LONG_MAX);
	  u1 = u1 - (long) q * v1;
	  u2 = u2 - q * v2;
	  
	  if (u2 == 0UL)
	    {
	      u1 = v1;
	      u2 = v2;
	      break;
	    }
	  
	  q = v2 / u2;
	  ASSERT_EXPENSIVE (q <= (unsigned long) LONG_MAX);
	  v1 = v1 - (long) q * u1;
	  v2 = v2 - q * u2;
	}
  
      if (u2 != 1UL)
	{
	  /* printf ("s=%lu t=%lu found %lu\n", s[0], t[0], u2); */
	  r[0] = 0UL; /* non-trivial gcd */
#ifndef NDEBUG
          mod_clear (tmp, m);
#endif
	  return 0;
	}

      if (u1 < 0L)
        u1 = u1 + t;
    }

  ASSERT ((unsigned long) u1 < t);
    
  mod_set_ul (r, (unsigned long) u1, m);

#ifndef NDEBUG
  mod_mul (tmp, tmp, r, m);
  ASSERT(mod_get_ul (tmp, m) == 1UL);
  mod_clear (tmp, m);
#endif

  return 1;
}
