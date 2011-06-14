#ifndef ABASE_U64K1_H_
#define ABASE_U64K1_H_

/* MPFQ generated file -- do not edit */

#include "mpfq.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include "assert.h"
#include "select_mpi.h"
#include "random_generation.h"
#include "abase_vbase.h"
#ifdef	MPFQ_LAST_GENERATED_TAG
#undef	MPFQ_LAST_GENERATED_TAG
#endif
#define MPFQ_LAST_GENERATED_TAG      u64k1

/* Active handler: Mpfq::defaults::vec */
/* Active handler: Mpfq::defaults */
/* Active handler: Mpfq::engine::defaults::mpi_flat */
/* Active handler: simd_dotprod */
/* Active handler: io */
/* Active handler: trivialities */
/* Active handler: simd_flat */
/* Active handler: simd_u64k */
/* Automatically generated code  */
/* Options used: k=1 tag=u64k1 choose_by_groupsize=CODE(0x1581cf8) prefix=abase_ family=[u64k1, u64k2] virtual_base={
                  'filebase' => 'abase_vbase',
                  'substitutions' => [
                                       [
                                         qr/(?-xism:abase_u64k1_elt \*)/,
                                         'void *'
                                       ],
                                       [
                                         qr/(?-xism:abase_u64k1_src_elt\b)/,
                                         'const void *'
                                       ],
                                       [
                                         qr/(?-xism:abase_u64k1_elt\b)/,
                                         'void *'
                                       ],
                                       [
                                         qr/(?-xism:abase_u64k1_dst_elt\b)/,
                                         'void *'
                                       ],
                                       [
                                         qr/(?-xism:abase_u64k1_elt_ur \*)/,
                                         'void *'
                                       ],
                                       [
                                         qr/(?-xism:abase_u64k1_src_elt_ur\b)/,
                                         'const void *'
                                       ],
                                       [
                                         qr/(?-xism:abase_u64k1_elt_ur\b)/,
                                         'void *'
                                       ],
                                       [
                                         qr/(?-xism:abase_u64k1_dst_elt_ur\b)/,
                                         'void *'
                                       ],
                                       [
                                         qr/(?-xism:abase_u64k1_vec \*)/,
                                         'void *'
                                       ],
                                       [
                                         qr/(?-xism:abase_u64k1_src_vec\b)/,
                                         'const void *'
                                       ],
                                       [
                                         qr/(?-xism:abase_u64k1_vec\b)/,
                                         'void *'
                                       ],
                                       [
                                         qr/(?-xism:abase_u64k1_dst_vec\b)/,
                                         'void *'
                                       ],
                                       [
                                         qr/(?-xism:abase_u64k1_vec_ur \*)/,
                                         'void *'
                                       ],
                                       [
                                         qr/(?-xism:abase_u64k1_src_vec_ur\b)/,
                                         'const void *'
                                       ],
                                       [
                                         qr/(?-xism:abase_u64k1_vec_ur\b)/,
                                         'void *'
                                       ],
                                       [
                                         qr/(?-xism:abase_u64k1_dst_vec_ur\b)/,
                                         'void *'
                                       ],
                                       [
                                         qr/(?-xism:abase_u64k1_poly \*)/,
                                         'void *'
                                       ],
                                       [
                                         qr/(?-xism:abase_u64k1_src_poly\b)/,
                                         'const void *'
                                       ],
                                       [
                                         qr/(?-xism:abase_u64k1_poly\b)/,
                                         'void *'
                                       ],
                                       [
                                         qr/(?-xism:abase_u64k1_dst_poly\b)/,
                                         'void *'
                                       ]
                                     ],
                  'name' => 'abase_vbase'
                };
 */

typedef void * abase_u64k1_field[1];
typedef void * abase_u64k1_dst_field;

typedef uint64_t abase_u64k1_elt[1];
typedef uint64_t * abase_u64k1_dst_elt;
typedef const uint64_t * abase_u64k1_src_elt;

typedef uint64_t abase_u64k1_elt_ur[1];
typedef uint64_t * abase_u64k1_dst_elt_ur;
typedef const uint64_t * abase_u64k1_src_elt_ur;

typedef abase_u64k1_elt * abase_u64k1_vec;
typedef abase_u64k1_elt * abase_u64k1_dst_vec;
typedef abase_u64k1_elt * abase_u64k1_src_vec;

typedef abase_u64k1_elt_ur * abase_u64k1_vec_ur;
typedef abase_u64k1_elt_ur * abase_u64k1_dst_vec_ur;
typedef abase_u64k1_elt_ur * abase_u64k1_src_vec_ur;

#ifdef  __cplusplus
extern "C" {
#endif

/* Functions operating on the field structure */
/* *simd_char2::code_for_field_characteristic */
#define abase_u64k1_field_characteristic(K, z)	mpz_set_ui(z,2)
/* *simd_char2::code_for_field_degree */
#define abase_u64k1_field_degree(K)	1
/* *Mpfq::defaults::code_for_field_init */
#define abase_u64k1_field_init(K)	/**/
/* *Mpfq::defaults::code_for_field_clear */
#define abase_u64k1_field_clear(K)	/**/
/* *Mpfq::defaults::code_for_field_specify */
#define abase_u64k1_field_specify(k, dummy, vp)	/**/
/* *Mpfq::defaults::code_for_field_setopt */
#define abase_u64k1_field_setopt(f, x, y)	/**/

/* Element allocation functions */
/* *Mpfq::defaults::flatdata::code_for_init */
#define abase_u64k1_init(f, px)	/**/
/* *Mpfq::defaults::flatdata::code_for_clear */
#define abase_u64k1_clear(f, px)	/**/

/* Elementary assignment functions */
static inline
void abase_u64k1_set(abase_u64k1_dst_field, abase_u64k1_dst_elt, abase_u64k1_src_elt);
/* missing set_ui */
static inline
void abase_u64k1_set_zero(abase_u64k1_dst_field, abase_u64k1_dst_elt);
/* missing get_ui */
/* missing set_mpn */
/* missing set_mpz */
/* missing get_mpn */
/* missing get_mpz */

/* Assignment of random values */
static inline
void abase_u64k1_random(abase_u64k1_dst_field, abase_u64k1_dst_elt);

/* Arithmetic operations on elements */
static inline
void abase_u64k1_add(abase_u64k1_dst_field, abase_u64k1_dst_elt, abase_u64k1_src_elt, abase_u64k1_src_elt);
/* *simd_char2::code_for_sub */
#define abase_u64k1_sub(K, r, s1, s2)	abase_u64k1_add(K,r,s1,s2)
/* *simd_char2::code_for_neg */
#define abase_u64k1_neg(K, r, s)	abase_u64k1_set(K,r,s)
/* missing mul */
/* missing sqr */
/* missing is_sqr */
/* missing sqrt */
/* missing pow */
/* missing add_ui */
/* missing sub_ui */
/* missing mul_ui */

/* Operations involving unreduced elements */
/* *Mpfq::defaults::flatdata::code_for_elt_ur_init */
#define abase_u64k1_elt_ur_init(f, px)	/**/
/* *Mpfq::defaults::flatdata::code_for_elt_ur_clear */
#define abase_u64k1_elt_ur_clear(f, px)	/**/
static inline
void abase_u64k1_elt_ur_set(abase_u64k1_dst_field, abase_u64k1_dst_elt_ur, abase_u64k1_src_elt_ur);
/* missing elt_ur_set_ui */
static inline
void abase_u64k1_elt_ur_add(abase_u64k1_dst_field, abase_u64k1_dst_elt_ur, abase_u64k1_src_elt_ur, abase_u64k1_src_elt_ur);
/* *simd_char2::code_for_elt_ur_neg */
#define abase_u64k1_elt_ur_neg(K, r, s)	abase_u64k1_elt_ur_set(K,r,s)
/* *simd_char2::code_for_elt_ur_sub */
#define abase_u64k1_elt_ur_sub(K, r, s1, s2)	abase_u64k1_elt_ur_add(K,r,s1,s2)
/* missing mul_ur */
/* missing sqr_ur */
/* missing reduce */

/* Comparison functions */
static inline
int abase_u64k1_cmp(abase_u64k1_dst_field, abase_u64k1_src_elt, abase_u64k1_src_elt);
/* missing cmp_ui */
static inline
int abase_u64k1_is_zero(abase_u64k1_dst_field, abase_u64k1_src_elt);

/* Input/output functions */
void abase_u64k1_asprint(abase_u64k1_dst_field, char * *, abase_u64k1_src_elt);
void abase_u64k1_fprint(abase_u64k1_dst_field, FILE *, abase_u64k1_src_elt);
/* *io::code_for_print */
#define abase_u64k1_print(k, x)	abase_u64k1_fprint(k,stdout,x)
int abase_u64k1_sscan(abase_u64k1_dst_field, abase_u64k1_dst_elt, const char *);
int abase_u64k1_fscan(abase_u64k1_dst_field, FILE *, abase_u64k1_dst_elt);
/* *io::code_for_scan */
#define abase_u64k1_scan(k, x)	abase_u64k1_fscan(k,stdin,x)

/* Vector functions */
void abase_u64k1_vec_init(abase_u64k1_dst_field, abase_u64k1_vec *, unsigned int);
void abase_u64k1_vec_reinit(abase_u64k1_dst_field, abase_u64k1_vec *, unsigned int, unsigned int);
void abase_u64k1_vec_clear(abase_u64k1_dst_field, abase_u64k1_vec *, unsigned int);
static inline
void abase_u64k1_vec_set(abase_u64k1_dst_field, abase_u64k1_dst_vec, abase_u64k1_src_vec, unsigned int);
static inline
void abase_u64k1_vec_set_zero(abase_u64k1_dst_field, abase_u64k1_dst_vec, unsigned int);
static inline
void abase_u64k1_vec_setcoef(abase_u64k1_dst_field, abase_u64k1_dst_vec, abase_u64k1_src_elt, unsigned int);
/* missing vec_setcoef_ui */
static inline
void abase_u64k1_vec_getcoef(abase_u64k1_dst_field, abase_u64k1_dst_elt, abase_u64k1_src_vec, unsigned int);
static inline
void abase_u64k1_vec_add(abase_u64k1_dst_field, abase_u64k1_dst_vec, abase_u64k1_src_vec, abase_u64k1_src_vec, unsigned int);
static inline
void abase_u64k1_vec_neg(abase_u64k1_dst_field, abase_u64k1_dst_vec, abase_u64k1_src_vec, unsigned int);
static inline
void abase_u64k1_vec_rev(abase_u64k1_dst_field, abase_u64k1_dst_vec, abase_u64k1_src_vec, unsigned int);
static inline
void abase_u64k1_vec_sub(abase_u64k1_dst_field, abase_u64k1_dst_vec, abase_u64k1_src_vec, abase_u64k1_src_vec, unsigned int);
/* missing vec_scal_mul */
/* missing vec_conv */
static inline
void abase_u64k1_vec_random(abase_u64k1_dst_field, abase_u64k1_dst_vec, unsigned int);
static inline
int abase_u64k1_vec_cmp(abase_u64k1_dst_field, abase_u64k1_src_vec, abase_u64k1_src_vec, unsigned int);
static inline
int abase_u64k1_vec_is_zero(abase_u64k1_dst_field, abase_u64k1_src_vec, unsigned int);
void abase_u64k1_vec_asprint(abase_u64k1_dst_field, char * *, abase_u64k1_src_vec, unsigned int);
void abase_u64k1_vec_fprint(abase_u64k1_dst_field, FILE *, abase_u64k1_src_vec, unsigned int);
void abase_u64k1_vec_print(abase_u64k1_dst_field, abase_u64k1_src_vec, unsigned int);
int abase_u64k1_vec_sscan(abase_u64k1_dst_field, abase_u64k1_vec *, unsigned int *, const char *);
int abase_u64k1_vec_fscan(abase_u64k1_dst_field, FILE *, abase_u64k1_vec *, unsigned int *);
/* *Mpfq::defaults::vec::io::code_for_vec_scan */
#define abase_u64k1_vec_scan(K, w, n)	abase_u64k1_vec_fscan(K,stdout,w,n)
void abase_u64k1_vec_ur_init(abase_u64k1_dst_field, abase_u64k1_vec_ur *, unsigned int);
void abase_u64k1_vec_ur_reinit(abase_u64k1_dst_field, abase_u64k1_vec_ur *, unsigned int, unsigned int);
void abase_u64k1_vec_ur_clear(abase_u64k1_dst_field, abase_u64k1_vec_ur *, unsigned int);
static inline
void abase_u64k1_vec_ur_set(abase_u64k1_dst_field, abase_u64k1_dst_vec_ur, abase_u64k1_src_vec_ur, unsigned int);
static inline
void abase_u64k1_vec_ur_setcoef(abase_u64k1_dst_field, abase_u64k1_dst_vec_ur, abase_u64k1_src_elt_ur, unsigned int);
static inline
void abase_u64k1_vec_ur_getcoef(abase_u64k1_dst_field, abase_u64k1_dst_elt_ur, abase_u64k1_src_vec_ur, unsigned int);
static inline
void abase_u64k1_vec_ur_add(abase_u64k1_dst_field, abase_u64k1_dst_vec_ur, abase_u64k1_src_vec_ur, abase_u64k1_src_vec_ur, unsigned int);
static inline
void abase_u64k1_vec_ur_sub(abase_u64k1_dst_field, abase_u64k1_dst_vec_ur, abase_u64k1_src_vec_ur, abase_u64k1_src_vec_ur, unsigned int);
/* missing vec_scal_mul_ur */
/* missing vec_conv_ur */
/* missing vec_reduce */
/* *Mpfq::defaults::flatdata::code_for_vec_elt_stride */
#define abase_u64k1_vec_elt_stride(K, n)	((n)*sizeof(abase_u64k1_elt))

/* Functions related to SIMD operation */
/* *simd_u64k::code_for_groupsize */
#define abase_u64k1_groupsize(K)	64
/* *simd_u64k::code_for_set_groupsize */
#define abase_u64k1_set_groupsize(K, n)	assert(n==64)
/* *trivialities::code_for_offset */
#define abase_u64k1_offset(K, n)	n /* TO BE DEPRECATED */
/* *trivialities::code_for_stride */
#define abase_u64k1_stride(K)	1 /* TO BE DEPRECATED */
static inline
void abase_u64k1_set_ui_at(abase_u64k1_dst_field, abase_u64k1_dst_elt, int, unsigned long);
static inline
void abase_u64k1_set_ui_all(abase_u64k1_dst_field, abase_u64k1_dst_elt, unsigned long);
static inline
void abase_u64k1_elt_ur_set_ui_at(abase_u64k1_dst_field, abase_u64k1_dst_elt, int, unsigned long);
static inline
void abase_u64k1_elt_ur_set_ui_all(abase_u64k1_dst_field, abase_u64k1_dst_elt, unsigned long);
void abase_u64k1_dotprod(abase_u64k1_dst_field, abase_u64k1_dst_vec, abase_u64k1_src_vec, abase_u64k1_src_vec, unsigned int);

/* Member templates related to SIMD operation */

/* MPI interface */
void abase_u64k1_mpi_ops_init(abase_u64k1_dst_field);
MPI_Datatype abase_u64k1_mpi_datatype(abase_u64k1_dst_field);
MPI_Datatype abase_u64k1_mpi_datatype_ur(abase_u64k1_dst_field);
MPI_Op abase_u64k1_mpi_addition_op(abase_u64k1_dst_field);
MPI_Op abase_u64k1_mpi_addition_op_ur(abase_u64k1_dst_field);
void abase_u64k1_mpi_ops_clear(abase_u64k1_dst_field);

/* Object-oriented interface */
#define abase_u64k1_oo_impl_name(v)	"u64k1"
static inline
void abase_u64k1_oo_field_clear(abase_vbase_ptr);
void abase_u64k1_oo_field_init(abase_vbase_ptr);
#ifdef  __cplusplus
}
#endif

/* Implementations for inlines */
/* *Mpfq::defaults::flatdata::code_for_set */
static inline
void abase_u64k1_set(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_elt r, abase_u64k1_src_elt s)
{
    if (r != s) memcpy(r,s,sizeof(abase_u64k1_elt));
}

/* *simd_flat::code_for_set_zero */
static inline
void abase_u64k1_set_zero(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_elt r)
{
    memset(r, 0, sizeof(abase_u64k1_elt));
}

/* *simd_flat::code_for_random */
static inline
void abase_u64k1_random(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_elt r)
{
    myrand_area(r, sizeof(abase_u64k1_elt));
}

/* *simd_flat::code_for_add */
static inline
void abase_u64k1_add(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_elt r, abase_u64k1_src_elt s1, abase_u64k1_src_elt s2)
{
        for(unsigned int i = 0 ; i < 1 ; i++) {
            r[i] = s1[i] ^ s2[i];
        }
}

/* *Mpfq::defaults::flatdata::code_for_elt_ur_set */
static inline
void abase_u64k1_elt_ur_set(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_elt_ur r, abase_u64k1_src_elt_ur s)
{
    if (r != s) memcpy(r,s,sizeof(abase_u64k1_elt_ur));
}

/* *simd_flat::code_for_elt_ur_add */
static inline
void abase_u64k1_elt_ur_add(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_elt_ur r, abase_u64k1_src_elt_ur s1, abase_u64k1_src_elt_ur s2)
{
        for(unsigned int i = 0 ; i < 1 ; i++) {
            r[i] = s1[i] ^ s2[i];
        }
}

/* *Mpfq::defaults::flatdata::code_for_cmp */
static inline
int abase_u64k1_cmp(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_src_elt r, abase_u64k1_src_elt s)
{
    return memcmp(r,s,sizeof(abase_u64k1_elt));
}

/* *Mpfq::defaults::flatdata::code_for_is_zero */
static inline
int abase_u64k1_is_zero(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_src_elt r)
{
        unsigned int i;
        for(i = 0 ; i < sizeof(abase_u64k1_elt)/sizeof(r[0]) ; i++) {
            if (r[i]) return 0;
        }
        return 1;
}

/* *Mpfq::defaults::vec::flatdata::code_for_vec_set */
static inline
void abase_u64k1_vec_set(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_vec r, abase_u64k1_src_vec s, unsigned int n)
{
    if (r != s) memcpy(r, s, n*sizeof(abase_u64k1_elt));
}

/* *Mpfq::defaults::vec::flatdata::code_for_vec_set_zero */
static inline
void abase_u64k1_vec_set_zero(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_vec r, unsigned int n)
{
    memset(r, 0, n*sizeof(abase_u64k1_elt));
}

/* *Mpfq::defaults::vec::getset::code_for_vec_setcoef */
static inline
void abase_u64k1_vec_setcoef(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_vec w, abase_u64k1_src_elt x, unsigned int i)
{
    abase_u64k1_set(K, w[i], x);
}

/* *Mpfq::defaults::vec::getset::code_for_vec_getcoef */
static inline
void abase_u64k1_vec_getcoef(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_elt x, abase_u64k1_src_vec w, unsigned int i)
{
    abase_u64k1_set(K, x, w[i]);
}

/* *Mpfq::defaults::vec::addsub::code_for_vec_add */
static inline
void abase_u64k1_vec_add(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_vec w, abase_u64k1_src_vec u, abase_u64k1_src_vec v, unsigned int n)
{
        unsigned int i;
    for(i = 0; i < n; i+=1)
        abase_u64k1_add(K, w[i], u[i], v[i]);
}

/* *Mpfq::defaults::vec::addsub::code_for_vec_neg */
static inline
void abase_u64k1_vec_neg(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_vec w, abase_u64k1_src_vec u, unsigned int n)
{
        unsigned int i;
    for(i = 0; i < n; ++i)
        abase_u64k1_neg(K, w[i], u[i]);
}

/* *Mpfq::defaults::vec::addsub::code_for_vec_rev */
static inline
void abase_u64k1_vec_rev(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_vec w, abase_u64k1_src_vec u, unsigned int n)
{
    unsigned int nn = n >> 1;
    abase_u64k1_elt tmp[1];
    abase_u64k1_init(K, tmp);
    unsigned int i;
    for(i = 0; i < nn; ++i) {
        abase_u64k1_set(K, tmp[0], u[i]);
        abase_u64k1_set(K, w[i], u[n-1-i]);
        abase_u64k1_set(K, w[n-1-i], tmp[0]);
    }
    if (n & 1)
        abase_u64k1_set(K, w[nn], u[nn]);
    abase_u64k1_clear(K, tmp);
}

/* *Mpfq::defaults::vec::addsub::code_for_vec_sub */
static inline
void abase_u64k1_vec_sub(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_vec w, abase_u64k1_src_vec u, abase_u64k1_src_vec v, unsigned int n)
{
    unsigned int i;
    for(i = 0; i < n; ++i)
        abase_u64k1_sub(K, w[i], u[i], v[i]);
}

/* *Mpfq::defaults::vec::getset::code_for_vec_random */
static inline
void abase_u64k1_vec_random(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_vec w, unsigned int n)
{
    unsigned int i;
    for(i = 0; i < n; ++i)
        abase_u64k1_random(K, w[i]);
}

/* *Mpfq::defaults::vec::getset::code_for_vec_cmp */
static inline
int abase_u64k1_vec_cmp(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_src_vec u, abase_u64k1_src_vec v, unsigned int n)
{
    unsigned int i;
    for(i = 0; i < n; ++i) {
        int ret = abase_u64k1_cmp(K, u[i], v[i]);
        if (ret != 0)
            return ret;
    }
    return 0;
}

/* *Mpfq::defaults::vec::flatdata::code_for_vec_is_zero */
static inline
int abase_u64k1_vec_is_zero(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_src_vec r, unsigned int n)
{
    unsigned int i;
    for(i = 0 ; i < n ; i+=1) {
        if (!abase_u64k1_is_zero(K,r[i])) return 0;
    }
    return 1;
}

/* *Mpfq::defaults::vec::flatdata::code_for_vec_ur_set */
static inline
void abase_u64k1_vec_ur_set(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_vec_ur r, abase_u64k1_src_vec_ur s, unsigned int n)
{
    if (r != s) memcpy(r, s, n*sizeof(abase_u64k1_elt_ur));
}

/* *Mpfq::defaults::vec::getset::code_for_vec_ur_setcoef */
static inline
void abase_u64k1_vec_ur_setcoef(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_vec_ur w, abase_u64k1_src_elt_ur x, unsigned int i)
{
    abase_u64k1_elt_ur_set(K, w[i], x);
}

/* *Mpfq::defaults::vec::getset::code_for_vec_ur_getcoef */
static inline
void abase_u64k1_vec_ur_getcoef(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_elt_ur x, abase_u64k1_src_vec_ur w, unsigned int i)
{
    abase_u64k1_elt_ur_set(K, x, w[i]);
}

/* *Mpfq::defaults::vec::addsub::code_for_vec_ur_add */
static inline
void abase_u64k1_vec_ur_add(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_vec_ur w, abase_u64k1_src_vec_ur u, abase_u64k1_src_vec_ur v, unsigned int n)
{
    unsigned int i;
    for(i = 0; i < n; i+=1)
        abase_u64k1_elt_ur_add(K, w[i], u[i], v[i]);
}

/* *Mpfq::defaults::vec::addsub::code_for_vec_ur_sub */
static inline
void abase_u64k1_vec_ur_sub(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_vec_ur w, abase_u64k1_src_vec_ur u, abase_u64k1_src_vec_ur v, unsigned int n)
{
    unsigned int i;
    for(i = 0; i < n; i+=1)
        abase_u64k1_elt_ur_sub(K, w[i], u[i], v[i]);
}

/* *simd_flat::code_for_set_ui_at */
static inline
void abase_u64k1_set_ui_at(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_elt p, int k, unsigned long v)
{
        assert(k < abase_u64k1_groupsize(K));
        uint64_t * xp = (uint64_t *) p;
        uint64_t mask = ((uint64_t)1) << (k%64);
        xp[k/64] = (xp[k/64] & ~mask) | ((((uint64_t)v) << (k%64))&mask);
}

/* *simd_flat::code_for_set_ui_all */
static inline
void abase_u64k1_set_ui_all(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_elt r, unsigned long v)
{
        for(unsigned int i = 0 ; i < 1 ; i++) {
            r[i] = ~v;
        }
}

/* *simd_flat::code_for_elt_ur_set_ui_at */
static inline
void abase_u64k1_elt_ur_set_ui_at(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_elt p, int k, unsigned long v)
{
        assert(k < abase_u64k1_groupsize(K));
        uint64_t * xp = (uint64_t *) p;
        uint64_t mask = ((uint64_t)1) << (k%64);
        xp[k/64] = (xp[k/64] & ~mask) | ((((uint64_t)v) << (k%64))&mask);
}

/* *simd_flat::code_for_elt_ur_set_ui_all */
static inline
void abase_u64k1_elt_ur_set_ui_all(abase_u64k1_dst_field K MAYBE_UNUSED, abase_u64k1_dst_elt r, unsigned long v)
{
        for(unsigned int i = 0 ; i < 1 ; i++) {
            r[i] = ~v;
        }
}

static inline
void abase_u64k1_oo_field_clear(abase_vbase_ptr f)
{
    abase_u64k1_field_clear((abase_u64k1_dst_field)(f->obj));
    free(f->obj);
    f->obj = NULL;
}


#endif  /* ABASE_U64K1_H_ */

/* vim:set ft=cpp: */