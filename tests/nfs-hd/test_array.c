#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include "sieving_bound.h"
#include "array.h"
#include "cado.h"
#include "utils.h"
#include "macros.h"
#include "int64_vector.h"

int main()
{
  sieving_bound_t H;
  unsigned int t = 4;

  sieving_bound_init(H, t);
  for (unsigned int i = 0; i < t; i++) {
    sieving_bound_set_hi(H, i, 2);
  }

  uint64_t nb = sieving_bound_number_element(H);

  int64_vector_t vector;
  int64_vector_init(vector, t);

  mpz_vector_t v;
  mpz_vector_init(v, t);

  for (unsigned int i = 0; i < t - 1; i++) {
    int64_vector_setcoordinate(vector, i, -(int64_t)H->h[i]);
  }

  uint64_t index;
  uint64_t index1;

  index = array_int64_vector_index(vector, H, nb);
  ASSERT_ALWAYS(index == 0);
  for (unsigned int i = 1; i < nb; i++) {
    int64_vector_add_one(vector, H);
    index = array_int64_vector_index(vector, H, nb);
    ASSERT_ALWAYS(index == i);
    array_index_mpz_vector(v, index, H, nb);
    for (unsigned int j = 0; j < t; j++) {
      ASSERT_ALWAYS(mpz_cmp_si(v->c[j], vector->c[j] == 0));
    }
    index1 = array_mpz_vector_index(v, H, nb);
    ASSERT_ALWAYS(index == index1);
  }

  mpz_vector_clear(v);
  int64_vector_clear(vector);
  sieving_bound_clear(H);

  return 0;
}
