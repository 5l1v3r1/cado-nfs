#include "cado.h"
#include <stdlib.h>
#include <string.h>
#include "bit_vector.h"
#include "macros.h"

#define BV_BITS 64      // since we're using uint64_t's
#define LN2_BV_BITS 6   // 2^^LN2_BV_BITS = BV_BITS
typedef uint64_t bv_t;

void bit_vector_init(bit_vector_ptr b, size_t n)
{
    b->p = malloc(iceildiv(n, BV_BITS) * sizeof(bv_t));
    b->n = n;
}

void bit_vector_init_set(bit_vector_ptr b, size_t n, int s)
{
    bit_vector_init(b,n);
    bit_vector_set(b,s);
}

void bit_vector_set(bit_vector_ptr b, int s)
{
    memset(b->p, -s, iceildiv(b->n, BV_BITS) * sizeof(bv_t));
}

void bit_vector_clear(bit_vector_ptr b)
{
    free(b->p); b->p = NULL; b->n = 0;
}

/* assume b and c have the same size */
void bit_vector_neg(bit_vector_ptr b, bit_vector_srcptr c)
{
  size_t i, n;
  n = b->n;
  ASSERT_ALWAYS(n == c->n);
  for (i = 0; i < iceildiv(n, BV_BITS); i++)
    b->p[i] = ~c->p[i];
}

int bit_vector_getbit(bit_vector_srcptr b, size_t pos)
{
    bv_t val = b->p[pos >> LN2_BV_BITS];
    bv_t mask = ((bv_t)1) << (pos & (BV_BITS - 1));
    return (val & mask) != 0;
}

int bit_vector_setbit(bit_vector_ptr b, size_t pos)
{
    bv_t val = b->p[pos >> LN2_BV_BITS];
    bv_t mask = ((bv_t)1) << (pos & (BV_BITS - 1));
    b->p[pos >> LN2_BV_BITS] |= mask;
    return (val & mask) != 0;
}

int bit_vector_clearbit(bit_vector_ptr b, size_t pos)
{
    bv_t val = b->p[pos >> LN2_BV_BITS];
    bv_t mask = ((bv_t)1) << (pos & (BV_BITS - 1));
    b->p[pos >> LN2_BV_BITS] &= ~mask;
    return (val & mask) != 0;
}

int bit_vector_flipbit(bit_vector_ptr b, size_t pos)
{
    bv_t mask = ((bv_t)1) << (pos & (BV_BITS - 1));
    bv_t val = b->p[pos >> LN2_BV_BITS] ^= mask;
    return (val & mask) != 0;
}

void bit_vector_write_to_file(bit_vector_srcptr b, const char * fname)
{
    FILE * f = fopen(fname, "w");
    ASSERT_ALWAYS(f);
    size_t z = iceildiv(b->n, BV_BITS);
    int rz = fwrite(b->p, sizeof(bv_t), z, f);
    ASSERT_ALWAYS(rz >= 0 && (size_t) rz == z);
    fclose(f);
}

void bit_vector_read_from_file(bit_vector_ptr b, const char * fname)
{
    FILE * f = fopen(fname, "r");
    ASSERT_ALWAYS(f);
    size_t z = iceildiv(b->n, BV_BITS);
    int rz = fread(b->p, sizeof(bv_t), z, f);
    ASSERT_ALWAYS(rz >= 0 && (size_t) rz == z);
    fclose(f);
}
