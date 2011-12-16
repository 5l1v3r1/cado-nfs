#ifndef POLYSELECT_AUXILIARY_H_
#define POLYSELECT_AUXILIARY_H_

/* header file for auxiliary routines for polyselect

Copyright 2008, 2009, 2010 Emmanuel Thome, Paul Zimmermann

This file is part of CADO-NFS.

CADO-NFS is free software; you can redistribute it and/or modify it under the
terms of the GNU Lesser General Public License as published by the Free
Software Foundation; either version 2.1 of the License, or (at your option)
any later version.

CADO-NFS is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
details.

You should have received a copy of the GNU Lesser General Public License
along with CADO-NFS; see the file COPYING.  If not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include <float.h>  /* for DBL_MAX */
#include <string.h>

#define MAX_DEGREE 6

#define DEFAULT_INCR 60 /* we want a positive integer with many divisors,
                           other values are 210, 2310, 30030, 510510, 9699690,
                           223092870 */

/* differents methods for the L2 norm */
#define RECTANGULAR 0
#define CIRCULAR 1
#define DEFAULT_L2_METHOD CIRCULAR

#define SKEWNESS_DEFAULT_PREC 10

/* prime bounds for the computation of alpha */
#define ALPHA_BOUND_SMALL  100
#define ALPHA_BOUND       2000

/* parameters for Murphy's E-value */
#define BOUND_F 1e7
#define BOUND_G 5e6
#define AREA    1e16
#define MURPHY_K 1000

#define mpz_add_si(a,b,c)                       \
  if (c >= 0) mpz_add_ui (a, b, c);             \
  else mpz_sub_ui (a, b, -(c))

#define mpz_submul_si(a,b,c)                    \
  if (c >= 0) mpz_submul_ui (a, b, c);          \
  else mpz_addmul_ui (a, b, -(c))
  
#include "cado_poly.h"

#ifdef __cplusplus
extern "C" {
#endif

double L2_lognorm (mpz_t*, unsigned long, double, int);
double L2_skewness (mpz_t*, int, int, int);
double L2_skewness_old (mpz_t*, int, int, int);
double L2_skewness_Newton (mpz_t*, int, int, int);
double L2_skewness_derivative (mpz_t*, int, int, int);

/* alpha */
double special_val0 (mpz_t*, int, unsigned long);
double special_valuation (mpz_t * f, int d, unsigned long p, mpz_t disc);
double special_valuation_affine ( mpz_t * f, int d, unsigned long p, mpz_t disc );
double get_alpha (mpz_t*, const int, unsigned long);
double get_biased_alpha_projective ( mpz_t *f, const int d, unsigned long B );

/* poly info, being called in order */
void print_poly_fg_bare (FILE*, mpz_t*, mpz_t*, int, mpz_t);
double print_cadopoly (FILE*, cado_poly, int);
void print_cadopoly_extra (FILE*, cado_poly, int, char**, double, int);
double print_poly_fg ( mpz_t *f, mpz_t *g, int d, mpz_t N, int verbose );

void discriminant (mpz_t, mpz_t*, const int);
long rotate_aux (mpz_t *f, mpz_t b, mpz_t m, long k0, long k, unsigned int t);
double rotate (mpz_t*, int, unsigned long, mpz_t, mpz_t, long*, long*, int,
               int, int);
long translate (mpz_t*, int, mpz_t*, mpz_t, mpz_t, int, int);
void optimize (mpz_t*, int, mpz_t*, int, int);
void optimize_aux (mpz_t *f, int d, mpz_t *g, int verbose, int use_rotation, int method);
void optimize_dir_aux (mpz_t *f, int d, mpz_t *g, int verbose, int method);
void rotate_bounds (mpz_t *f, int d, mpz_t b, mpz_t m, long *K0, long *K1, long *J0, long *J1, int verbose, int);
void do_translate_z (mpz_t *f, int d, mpz_t *g, mpz_t k);

/* changed for rootsieve5.c */
void eval_poly_ui (mpz_t v, mpz_t *f, int d, unsigned long r);
void eval_poly_diff_ui (mpz_t v, mpz_t *f, int d, unsigned long r);

#ifdef __cplusplus
}
#endif

/********************* data structures for first phase ***********************/

typedef struct {
  /* the linear polynomial is b*x-m, with lognorm logmu */
  mpz_t b;
  mpz_t m;
  double logmu;
} m_logmu_t;

#ifdef __cplusplus
extern "C" {
#endif

m_logmu_t* m_logmu_init (unsigned long);
void m_logmu_clear (m_logmu_t*, unsigned long);
int m_logmu_insert (m_logmu_t*, unsigned long, unsigned long*, mpz_t, mpz_t,
                    double, char*);

#ifdef __cplusplus
}
#endif


#endif	/* POLYSELECT_AUXILIARY_H_ */

