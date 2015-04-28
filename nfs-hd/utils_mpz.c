#include "utils_mpz.h"
#include <stdint.h>
#include "gmp-ecm.h"
#include <stdlib.h>
#include "macros.h"

void factor_init(factor_ptr factor, unsigned int number)
{
  ASSERT(number > 0);

  factor->number = number;
  factor->factorization = (mpz_t * ) malloc(sizeof(mpz_t) * number);
  for (unsigned int i = 0; i < number; i++) {
    mpz_init(factor->factorization[i]);
  }
}

void factor_clear(factor_ptr factor)
{
  for (unsigned int i = 0; i < factor->number; i++) {
    mpz_clear(factor->factorization[i]);
  }
  free(factor->factorization);
  factor->number = 0;
}

void factor_realloc(factor_ptr factor, unsigned int number)
{
  ASSERT(factor->number > number);

  for (unsigned int i = number; i < factor->number; i++) {
    mpz_clear(factor->factorization[i]);
  }
  factor->factorization = realloc(factor->factorization, sizeof(mpz_t) *
				  number);
  factor->number = number;
}

static void factorize(factor_ptr factor, mpz_t z, unsigned int * number)
{
  int ret = 0;
  mpz_t res;
  mpz_init(res);
  for (unsigned int i = 0; i < 10; i++) {
    ret = ecm_factor(res, z, 200, NULL);
    if (ret) {
      if (mpz_cmp_ui(res, 0) != 0 && mpz_cmp_ui(res, 1) != 0) {
        if (mpz_perfect_power_p(res)){
          unsigned int j = 2;
          mpz_t root;
          mpz_t rem;
          mpz_init(root);
          mpz_init(rem);
          do {
            mpz_rootrem(root, rem, res, j);
            j++;
          } while (mpz_cmp_ui(rem, 0) != 0);
          if (mpz_probab_prime_p (root, 25)) {
            for (unsigned int k = 1; k < j; k++) {
              mpz_set(factor->factorization[* number], root);
              * number = * number + 1;
            }
          } else {
            for (unsigned int k = 1; k < j; k++) {
              factorize(factor, root, number);
            }
          }
          mpz_clear(root);
          mpz_clear(rem);
        } else if (mpz_probab_prime_p (res, 25)) {
          mpz_set(factor->factorization[* number], res);
          * number = * number + 1;
        } else {
          factorize(factor, res, number);
        }
        mpz_divexact(res, z, res);
        if (mpz_probab_prime_p (res, 25)) {
          mpz_set(factor->factorization[* number], res);
          * number = * number + 1;
        } else if (mpz_cmp_ui(res, 1) != 0) {
          factorize(factor, res, number);
        }
        break;
      }
    }
  }
  mpz_clear(res);
}

static void brute_force_factorize(factor_ptr factor, mpz_srcptr z_root,
    unsigned int * number)
{
  mpz_t z;
  mpz_init(z);
  mpz_set(z, z_root);
  mpz_t prime;
  mpz_init(prime);
  mpz_set_ui(prime, 1);
  while(mpz_cmp_ui(z, 1) != 0) {
    mpz_nextprime(prime, prime);
    mpz_t q;
    mpz_init(q);
    mpz_t r;
    mpz_init(r);
    mpz_fdiv_qr(q, r, z, prime);
    while (mpz_cmp_ui(r, 0) == 0) {
      mpz_set(z, q);
      mpz_set(factor->factorization[* number], prime);
      * number = * number + 1;
      mpz_fdiv_qr(q, r, z, prime);
    }
    mpz_clear(q);
    mpz_clear(r);
  }
  mpz_clear(prime);
  mpz_clear(z);
}

static int compare(const void * p1, const void * p2)
{
  return(mpz_cmp((mpz_srcptr) p1, (mpz_srcptr) p2));
}

void sort_factor(factor_ptr factor)
{
  qsort(factor->factorization, factor->number, sizeof(factor->factorization[0]),
        compare);
}

unsigned int gmp_brute_force_factorize(factor_ptr factor, mpz_srcptr z)
{
  unsigned int number = mpz_sizeinbase(z, 2);
  factor_init(factor, number);
  unsigned int nb = 0;
  if (mpz_probab_prime_p (z, 25)) {
    mpz_set(factor->factorization[nb], z);
    nb = 1;
  } else {
    brute_force_factorize(factor, z, &nb);
  }
  factor_realloc(factor, nb);
  sort_factor(factor);

  mpz_t tmp;
  mpz_init(tmp);
  mpz_set_ui(tmp, 1);
  for (unsigned int i = 0; i < factor->number - 1; i++) {
    mpz_mul(tmp, tmp, factor->factorization[i]);
  }
  unsigned int assert_facto = 1;
  if (mpz_cmp(tmp, z)) {
    assert_facto = 0;
  }
  mpz_clear(tmp);

  return assert_facto;
}

unsigned int gmp_factorize(factor_ptr factor, mpz_t z)
{
  unsigned int number = mpz_sizeinbase(z, 2);
  factor_init(factor, number);
  unsigned int nb = 0;
  if (mpz_probab_prime_p (z, 25)) {
    mpz_set(factor->factorization[nb], z);
    nb = 1;
  } else {
    factorize(factor, z, &nb);
  }
  factor_realloc(factor, nb);
  sort_factor(factor);

  mpz_t tmp;
  mpz_init(tmp);
  mpz_set_ui(tmp, 1);
  for (unsigned int i = 0; i < factor->number - 1; i++) {
    mpz_mul(tmp, tmp, factor->factorization[i]);
  }
  unsigned int assert_facto = 1;
  if (mpz_cmp(tmp, z)) {
    assert_facto = 0;
  }
  mpz_clear(tmp);

  return assert_facto;
}

void factor_fprintf(FILE * file, factor_srcptr factor)
{
  fprintf(file, "[");
  for (unsigned int i = 0; i < factor->number - 1; i++) {
    gmp_fprintf(file, "%Zd, ", factor->factorization[i]);
  }
  gmp_fprintf(file, "%Zd]\n", factor->factorization[factor->number - 1]);
}

unsigned int factor_is_smooth(factor_srcptr factor, mpz_t B, unsigned int sort)
{
  if (sort) {
    ASSERT(sort == 1);

    if (mpz_cmp(factor->factorization[factor->number - 1], B) > 0) {
      return 0;
    }
  } else {
    ASSERT(sort == 0);

    for (unsigned int i = 0; i < factor->number; i++) {
      if (mpz_cmp(factor->factorization[i], B) > 0) {
        return 0;
      }
    }
  }

  return 1;
}

int mpz_invert_ui(mpz_ptr rop, mpz_srcptr op1, const uint64_t op2)
{
  mpz_t op3;
  mpz_init(op3);
  mpz_set_ui(op3, op2);
  int res = mpz_invert(rop, op1, op3);
  mpz_clear(op3);

  return res;
}
