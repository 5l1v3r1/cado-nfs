#ifndef MPZ_POLY_H_
#define MPZ_POLY_H_

#include <stdint.h>
#include <gmp.h>

#ifdef __cplusplus
extern "C" {
#endif

int lift_root(mpz_t * f, int d, unsigned long pk, unsigned long * r);
int lift_rootz(mpz_t * f, int d, mpz_t pk, mpz_t r);
int mp_poly_cmp (mpz_t *f, mpz_t *g, int d);
void mp_poly_homogeneous_eval_siui (mpz_t v, mpz_t *f, const unsigned int d, const long i, const unsigned long j);
void
mp_poly_homography (mpz_t *fij, mpz_t *f, const int d, int32_t H[4]);

#ifdef __cplusplus
}
#endif

#endif	/* MPZ_POLY_H_ */
