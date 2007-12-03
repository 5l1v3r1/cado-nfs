#include <sys/time.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#include <gmp.h>
#include "master_params.h"
#include "params.h"
#include <assert.h>
#include "types.h"
#include "macros.h"
#include "auxfuncs.h"
#include "bw_scalar.h"
#include "variables.h"
#include "structure.h"
#include "gmp-hacks.h"
#include "modulus_hacks.h"
#include "twisting_polynomials.h"
#include "e_polynomial.h"
#include "ops_poly.h"
#include "fft_on_matrices.h"
#include "field_def.h"
#include "field_prime.h"
#include "field_quad.h"
#include "field_usage.h"
#include "timer.h"

#ifndef NDEBUG
#include <stdio.h>
/* ... */
#endif

/**********************************************************************/
/**********************************************************************/


/**********************************************************************/

/* XXX : locality is certainly important, unless multiplications take a
 * huge time anyway. So it might be better to keep polynomials gathered
 * in a single area.
 */
struct dft_mb *fft_ec_dft(struct e_coeff *ec, ft_order_t order, double *tm)
{
    struct dft_mb *res;
    unsigned int i, j;
    struct timeval tv;

    timer_r(&tv, TIMER_SET);

    res = malloc(sizeof(struct dft_mb));
    if (res == NULL)
	return res;
    res->degree = ec->degree;
    ft_order_copy(res->order, order);
    if (!(ft_mat_mb_alloc(order, res->p))) {
	free(res);
	return NULL;
    }

    /*
     * We want to compute the order n DFT, n==res->order. ec->degree
     * might actually be bigger than 1<<n, so we reduce e(X)
     * transparently modulo X^{1<<n} in order to get the proper DFT
     */
    for (i = 0; i < m_param; i++) {
	for (j = 0; j < bigdim; j++) {
            mp_limb_t * src0, * src1;
            src0 = mbmat_scal(mbpoly_coeff(ec->p, 0), i, j);
            src1 = mbmat_scal(mbpoly_coeff(ec->p, 1), i, j);
            ft_zero(order, ft_mat_mb_get(order, res->p, i, j));
            ft_transform(order, ft_mat_mb_get(order, res->p, i, j),
                    src0, src1 - src0, ec->degree);
	    printf("."); fflush(stdout);
	}
    }
    printf("\n");

    *tm = timer_r(&tv, TIMER_ASK);

    return res;
}


struct dft_bb *fft_tp_dft(struct t_poly *tp, ft_order_t order, double *tm)
{
    struct dft_bb *res;
    unsigned int i, j, jr;
    struct timeval tv;

    timer_r(&tv, TIMER_SET);

    res = malloc(sizeof(struct dft_bb));
    if (res == NULL)
	return res;
    res->degree = tp->degree;
    ft_order_copy(res->order, order);
    if (!(ft_mat_bb_alloc(order, res->p))) {
	free(res);
	return NULL;
    }

    /* Same thing as above */
    for (i = 0; i < bigdim; i++) {
	for (j = 0; j < bigdim; j++) {
            mp_limb_t * src0, * src1;

	    jr = tp->clist[j];

            src0 = bbmat_scal(bbpoly_coeff(tp->p, 0), i, jr);
            src1 = bbmat_scal(bbpoly_coeff(tp->p, 1), i, jr);

            ft_zero(order, ft_mat_bb_get(order, res->p, i, j));
            ft_transform(order, ft_mat_bb_get(order, res->p, i, j),
                    src0, src1-src0, tp->degnom[jr]);

	    printf("."); fflush(stdout);
	}
    }
    printf("\n");

    *tm = timer_r(&tv, TIMER_ASK);

    return res;
}

#ifdef  HAS_CONVOLUTION_SPECIAL
struct dft_mb *fft_mbb_conv_sp(struct dft_mb *p,
			       struct dft_bb *q,
			       unsigned int dg_kill, double *tm)
{
    struct dft_mb *res;
    unsigned int i, j, l;
    struct timeval tv;

    timer_r(&tv, TIMER_SET);

    res = malloc(sizeof(struct dft_mb));
    if (res == NULL)
	return res;
    res->degree = p->degree + q->degree - dg_kill;
    assert(ft_order_eq(p->order,q->order));
    ft_order_copy(res->order, p->order);
    if (!(ft_mat_mb_alloc(res->order, res->p))) {
	free(res);
	return NULL;
    }

    /* TODO: Multiply using Strassen's algorithm instead. */
    for (i = 0; i < m_param; i++) {
	for (j = 0; j < bigdim; j++) {
            ft_zero(res->order, ft_mat_mb_get(res->order, res->p, i, j));
	    for (l = 0; l < bigdim; l++) {
                ft_convolution_special(res->order,
                        ft_mat_mb_get(res->order, res->p, i, j),
                        ft_mat_mb_get(res->order, p->p, i, l),
                        ft_mat_bb_get(res->order, q->p, l, j),
                        dg_kill);
	    }
	    printf("."); fflush(stdout);
	}
    }
    printf("\n");

    *tm = timer_r(&tv, TIMER_ASK);

    return res;
}
#endif

/* This one is the same, without the feature that we divide by X^d
 * directly in the dft. That way, the dft is longer. This function is
 * only relevant for testing.
 */
struct dft_mb *fft_mbb_conv(struct dft_mb *p,
			       struct dft_bb *q,
			       double *tm)
{
    struct dft_mb *res;
    unsigned int i, j, l;
    struct timeval tv;

    timer_r(&tv, TIMER_SET);

    res = malloc(sizeof(struct dft_mb));
    if (res == NULL)
	return res;
    res->degree = p->degree + q->degree;
    assert(ft_order_eq(p->order,q->order));
    ft_order_copy(res->order, p->order);
    if (!(ft_mat_mb_alloc(res->order, res->p))) {
	free(res);
	return NULL;
    }

    /* TODO: Multiply using Strassen's algorithm instead. */
    for (i = 0; i < m_param; i++) {
	for (j = 0; j < bigdim; j++) {
            ft_zero(res->order, ft_mat_mb_get(res->order, res->p, i, j));
	    for (l = 0; l < bigdim; l++) {
                ft_convolution(res->order,
                        ft_mat_mb_get(res->order, res->p, i, j),
                        ft_mat_mb_get(res->order, p->p, i, l),
                        ft_mat_bb_get(res->order, q->p, l, j));
	    }
	    printf("."); fflush(stdout);
	}
    }
    printf("\n");

    *tm = timer_r(&tv, TIMER_ASK);

    return res;
}

struct dft_bb *fft_bbb_conv(struct dft_bb *p, struct dft_bb *q, double *tm)
{
    struct dft_bb *res;
    unsigned int i, j, l;
    struct timeval tv;

    timer_r(&tv, TIMER_SET);

    res = malloc(sizeof(struct dft_bb));
    if (res == NULL)
	return res;
    res->degree = 0;		/* we don't care, this is meaningless anyway */
    assert(ft_order_eq(p->order,q->order));
    ft_order_copy(res->order, p->order);
    if (!(ft_mat_bb_alloc(res->order, res->p))) {
	free(res);
	return NULL;
    }

    /* TODO: Multiply using Strassen's algorithm instead. */
    for (i = 0; i < bigdim; i++) {
	for (j = 0; j < bigdim; j++) {
	    ft_zero(res->order, ft_mat_bb_get(res->order, res->p, i, j));
	    for (l = 0; l < bigdim; l++) {
		ft_convolution(res->order,
			       ft_mat_bb_get(res->order, res->p, i, j),
			       ft_mat_bb_get(res->order, p->p, i, l),
			       ft_mat_bb_get(res->order, q->p, l, j));
	    }
	    printf("."); fflush(stdout);
	}
    }
    printf("\n");

    *tm = timer_r(&tv, TIMER_ASK);

    return res;
}

void fft_mb_invdft(bw_mbpoly dest,
		   struct dft_mb *p, unsigned int deg, double *tm)
{
    unsigned int i, j;
    struct timeval tv;

    timer_r(&tv, TIMER_SET);

    assert(ft_order_fits(p->order, deg + 1));

    for (i = 0; i < m_param; i++) {
	for (j = 0; j < bigdim; j++) {
            mp_limb_t * dst0;
            mp_limb_t * dst1;
            dst0 = mbmat_scal(mbpoly_coeff(dest, 0), i, j);
            dst1 = mbmat_scal(mbpoly_coeff(dest, 1), i, j);
            ft_itransform(p->order, 
                    dst0, dst1-dst0, deg, ft_mat_mb_get(p->order, p->p, i, j));

	    printf("."); fflush(stdout);
	}
    }
    printf("\n");

    *tm = timer_r(&tv, TIMER_ASK);
}

void fft_tp_invdft(struct t_poly *tp, struct dft_bb *p, double *tm)
{
    unsigned int i, j;
    struct timeval tv;

    timer_r(&tv, TIMER_SET);

    /*
     * Les degr�s de destinations sont d�j� dans tp.
     *
     * Les colonnes ont d�j� �t� r�ordonn�es par tp_comp_alloc, donc
     * la permutation associ�e � tp est triviale : clist[j]==j.
     *
     * Par ailleurs, tp_comp_alloc a d�j� tout mis � z�ro.
     */

    for (i = 0; i < bigdim; i++) {
	for (j = 0; j < bigdim; j++) {
            mp_limb_t * dst0;
            mp_limb_t * dst1;
            dst0 = bbmat_scal(bbpoly_coeff(tp->p, 0), i, j);
            dst1 = bbmat_scal(bbpoly_coeff(tp->p, 1), i, j);
            ft_itransform(p->order, dst0, dst1-dst0, tp->degnom[j],
                    ft_mat_bb_get(p->order, p->p, i, j));
	    printf("."); fflush(stdout);
	}
    }
    printf("\n");

    *tm = timer_r(&tv, TIMER_ASK);
}

struct dft_bb *fft_bb_dft_init_one(unsigned int deg)
{
    struct dft_bb *res;
    unsigned int i, j;

    res = malloc(sizeof(struct dft_bb));
    if (res == NULL)
	return res;
    res->degree = 0;
    ft_order(&(res->order), deg + 1);
    if (!(ft_mat_bb_alloc(res->order, res->p))) {
	free(res);
	return NULL;
    }

    for (i = 0; i < bigdim; i++) {
	for (j = 0; j < bigdim; j++) {
            ft_zero(res->order, ft_mat_bb_get(res->order, res->p, i, j));
        }
        ft_one(res->order, ft_mat_bb_get(res->order, res->p, i, i));
    }

    return res;
}

void dft_mb_free(struct dft_mb *p)
{
    ft_mat_mb_free(p->order, p->p);
    free(p);
}

void dft_bb_free(struct dft_bb *p)
{
    ft_mat_bb_free(p->order, p->p);
    free(p);
}

/* vim:set sw=4 sta et: */
