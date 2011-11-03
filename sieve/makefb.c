#include "cado.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for strcmp() */
#include <math.h> /* for sqrt and floor and log and ceil */
#include "utils.h"


/*
 * Compute g(x) = f(a*x+b), with deg f = d, and a and b are longs.
 * Must have a != 0 and d >= 1
 */
void mp_poly_linear_comp(mpz_t *g, mpz_t *f, int d, long a, long b) {
    ASSERT (a != 0  &&  d >= 1);
    // lazy: use the poly_t interface of utils/poly.h 
    poly_t aXpb, aXpbi, G, Aux;
    poly_alloc(aXpb, 1);  // alloc sets to zero
    poly_alloc(aXpbi, d);  
    poly_alloc(G, d);
    poly_alloc(Aux, d);
    { 
        mpz_t aux;
        mpz_init(aux);
        mpz_set_si(aux, a);
        poly_setcoeff(aXpb, 1, aux);
        mpz_set_si(aux, b);
        poly_setcoeff(aXpb, 0, aux);
        mpz_clear(aux);
    }
    poly_copy(aXpbi, aXpb);
    poly_setcoeff(G, 0, f[0]);
    for (int i = 1; i <= d; ++i) {
        poly_mul_mpz(Aux, aXpbi, f[i]);
        poly_add(G, G, Aux);
        if (i < d)
            poly_mul(aXpbi, aXpbi, aXpb);
    }
    for (int i = 0; i < d; ++i)
        poly_getcoeff(g[i], i, G);
    poly_free(aXpb);
    poly_free(aXpbi);
    poly_free(G);
    poly_free(Aux);
}

int mpz_p_val(mpz_t z, unsigned long p) {
    int v = 0;
    if (mpz_cmp_ui(z, 0) == 0)
        return INT_MAX;
    mpz_t zz;
    mpz_init(zz);
    mpz_abs(zz, z);
    unsigned long r;
    do {
        r = mpz_tdiv_q_ui(zz, zz, p);
        if (r == 0) 
            v++;
    } while (r == 0 && mpz_cmp_ui(zz, 1)!=0);
    mpz_clear(zz);
    return v;
}

int mp_poly_p_val_of_content(mpz_t *f, int d, unsigned long p) {
    int val = INT_MAX;
    for (int i = 0; i <= d; ++i) {
        int v = mpz_p_val(f[i], p);
        if (v == 0)
            return 0;
        val = MIN(val, v);
    }
    return val;
}


// TODO:
// Stolen from sieve.c. Should be shared, at some point.
void
mp_poly_eval (mpz_t r, mpz_t *poly, int deg, long a)
{
  int i;

  mpz_set (r, poly[deg]);
  for (i = deg - 1; i >= 0; i--)
    {
      mpz_mul_si (r, r, a);
      mpz_add (r, r, poly[i]);
    }
}

// Evaluate the derivative of poly at a.
void
mp_poly_eval_diff (mpz_t r, mpz_t *poly, int deg, long a)
{
  int i;

  mpz_mul_ui (r, poly[deg], (unsigned long)deg);
  for (i = deg - 1; i >= 1; i--)
    {
      mpz_mul_si (r, r, a);
      mpz_addmul_ui (r, poly[i], (unsigned long)i);
    }
}

// Same function as above, with a slightly different interface.
unsigned long
lift_root_unramified(mpz_t *f, int d, unsigned long r, 
        unsigned long p,int kmax) {
    mpz_t aux, aux2, mp_p;
    int k = 1;
    mpz_init(aux);
    mpz_init(aux2);
    mpz_init_set_ui(mp_p, p);
    while (k < kmax) {
        mpz_mul(mp_p, mp_p, mp_p); // p^2k
        // Everything should always fit in a long, at least on a 64-bit
        // machine.
        ASSERT(mpz_sizeinbase(mp_p, 2) < 8*sizeof(unsigned long));
        mp_poly_eval(aux, f, d, r);
        mp_poly_eval_diff(aux2, f, d, r);
        if (!mpz_invert(aux2, aux2, mp_p)) {
            fprintf(stderr, "Error in lift_root_unramified: multiple root mod %lu\n", p);
            exit(EXIT_FAILURE);
        }
        mpz_mul(aux, aux, aux2);
        mpz_ui_sub(aux, r, aux);
        mpz_mod(aux, aux, mp_p);
        r = mpz_get_ui(aux);
        k *= 2;
    }
    mpz_clear(aux);
    mpz_clear(aux2);
    mpz_clear(mp_p);
    return r;
}

typedef struct {
    unsigned long q;
    unsigned long r; 
    int n1;
    int n0;
} entry;

typedef struct {
    entry *list;
    int len;
    int alloc;
} entry_list;

void entry_list_init(entry_list *L) {
    L->list = (entry*) malloc(10*sizeof(entry));
    L->alloc = 10;
    L->len = 0;
}

void entry_list_clear(entry_list *L) {
    free(L->list);
}

void push_entry(entry_list *L, entry E) {
    if (L->len == L->alloc) {
        L->alloc += 10;
        L->list = (entry *)realloc(L->list, (L->alloc)*sizeof(entry));
    }
    L->list[L->len++] = E;
}

int cmp_entry(const void *A, const void *B) {
    entry a, b;
    a = ((entry *)A)[0];
    b = ((entry *)B)[0];
    if (a.q < b.q) 
        return -1;
    if (a.q > b.q)
        return 1;
    if (a.n1 < b.n1)
        return -1;
    if (a.n1 > b.n1)
        return 1;
    if (a.n0 < b.n0)
        return -1;
    if (a.n0 > b.n0)
        return 1;
    if (a.r < b.r)
        return -1;
    if (a.r > b.r)
        return 1;
    return 0;
}

/* see makefb.sage for the meaning of this function and its parameters */
void all_roots_affine(entry_list *L, mpz_t *f, int d, unsigned long p,
        int kmax, int k0, int m, unsigned long phi1, unsigned long phi0) {
    int nroots;
    unsigned long *roots;
    mpz_t aux;

    if (k0 >= kmax) { 
        return;
    }
    roots = (unsigned long*) malloc(d * sizeof(unsigned long));
    mpz_init(aux);

    nroots = poly_roots_ulong(roots, f, d, p);
    for (int i = 0; i < nroots; ++i) {
        unsigned long r = roots[i];
        mp_poly_eval_diff(aux, f, d, r);
        unsigned long dfr = mpz_mod_ui(aux, aux, p);
        if (dfr != 0) {
            unsigned long rr = lift_root_unramified(f, d, r, p, kmax-k0);
            unsigned long phir = phi1 * rr + phi0;
            unsigned long pml = 1;
            for (int j = 0; j < m; ++j)
                pml *= p;
            for (int l = 1; l < kmax-k0; ++l) {
                pml *= p;
                unsigned long phirr = phir % pml;
                entry E;
                E.q = pml;
                E.r = phirr;
                E.n1 = k0+l;
                E.n0 = k0+l-1;
                push_entry(L, E);
            }
        } else { 
            mpz_t *ff;
            ff = (mpz_t *) malloc((d+1)*sizeof(mpz_t));
            for (int j = 0; j <= d; ++j)
                mpz_init(ff[j]);
            mp_poly_linear_comp(ff, f, d, p, r);
            int val = mp_poly_p_val_of_content(ff, d, p);
            unsigned long pmp1 = 1;
            for (int j = 0; j < m+1; ++j)
                pmp1 *= p;
            unsigned long phir = (phi1 * r + phi0) % pmp1;
            entry E;
            E.q = pmp1;
            E.r = phir;
            E.n1 = k0+val;
            E.n0 = k0;
            push_entry(L, E);
            unsigned long nphi1 = phi1*p;
            unsigned long nphi0 = phi0 + phi1*r;
            {
                mpz_t pv;
                mpz_init(pv);
                mpz_set_ui(pv, 1);
                for (int j = 0; j < val; ++j)
                    mpz_mul_ui(pv, pv, p);
                for (int j = 0; j <= d; ++j)
                    mpz_tdiv_q(ff[j], ff[j], pv);
                mpz_clear(pv);
            }
            all_roots_affine(L, ff, d, p, kmax, k0+val, m+1, nphi1, nphi0);
            for (int j = 0; j <= d; ++j)
                mpz_clear(ff[j]);
            free(ff);
        }
    }
    free(roots);
    mpz_clear(aux);
}

/* Compute roots mod powers of p, up to maxbits.
 * TODO:
 * Maybe the number of bits of the power is not really the right measure.
 * We could take into account not only the cost of the updates (which is
 * indeed in k*log(p)), but also the amount of information we gain for
 * each update, which is in log(p). So what would be the right measure
 * ???
 */

/*
 * See makefb.sage for details on this function
 */

entry_list all_roots(mpz_t *f, int d, unsigned long p, int maxbits) {
    int kmax;
    entry_list L;
    entry_list_init(&L);
    kmax = (int)ceil(maxbits*log(2)/log(p));
    // FIXME: Hum... this is inelegant, to say the least...
    // Furthermore, it costs something. Should be fixed.
    if (kmax < 2)
        kmax = 2;

    {
        mpz_t *fh;
        mpz_t pk;
        fh = (mpz_t *)malloc ((d+1)*sizeof(mpz_t));
        mpz_init(pk);
        mpz_set_ui(pk, 1);
        for (int i = 0; i <= d; ++i) {
            mpz_init(fh[i]);
            mpz_mul(fh[i], f[d-i], pk);
            if (i < d)
                mpz_mul_ui(pk, pk, p);
        }
        int v = mp_poly_p_val_of_content(fh, d, p);
        if (v > 0) { // We have projective roots only in that case
            { 
                entry E;
                E.q = p;
                E.r = p;
                E.n1 = v;
                E.n0 = 0;
                push_entry(&L, E);
            }
            mpz_set_ui(pk, p);
            for (int i = 1; i < v; ++i)
                mpz_mul_ui(pk, pk, p);
            for (int i = 0; i <= d; ++i)
                mpz_tdiv_q(fh[i], fh[i], pk);

            all_roots_affine(&L, fh, d, p, kmax-1, 0, 0, 1, 0);
            // convert back the roots
            for (int i = 1; i < L.len; ++i) {
                entry E = L.list[i];
                E.q *= p;
                E.n1 += v;
                E.n0 += v;
                E.r = E.r*p + E.q;
                L.list[i] = E;
            }
        }
        for (int i = 0; i <= d; ++i) 
            mpz_clear(fh[i]);
        mpz_clear(pk);
        free(fh);
    }
    all_roots_affine(&L, f, d, p, kmax, 0, 0, 1, 0);

    qsort((void *)(&L.list[0]), L.len, sizeof(entry), cmp_entry);
    return L;
}


void makefb_with_powers(mpz_t *f, int d, unsigned long alim, 
        int maxbits) {
    printf("# Roots for polynomial ");
    fprint_polynomial(stdout, f, d);
    printf("# DEGREE: %d\n", d);
    printf("# alim = %lu\n", alim);
    printf("# maxbits = %d\n", maxbits);

    entry_list L;
    unsigned long p;
    for (p = 2; p <= alim; p = getprime (p)) {
        L = all_roots(f, d, p, maxbits);
        // print in a compactified way
        int oldn0=-1, oldn1=-1;
        unsigned long oldq = 0;
        for (int i = 0; i < L.len; ++i) {
            unsigned long q = L.list[i].q;
            int n1 = L.list[i].n1;
            int n0 = L.list[i].n0;
            unsigned long r =  L.list[i].r;
            if (q == oldq && n1 == oldn1 && n0 == oldn0)
                printf(",%lu", r);
            else {
                if (i > 0)
                    printf("\n");
                oldq = q; oldn1 = n1; oldn0 = n0;
                if (n1 == 1 && n0 == 0) 
                    printf("%lu: %lu", q, r);
                else
                    printf("%lu:%d,%d: %lu", q, n1, n0, r);
            }
        }
        if (L.len > 0) 
            printf("\n");
        entry_list_clear(&L);
    }
}


void
makefb (FILE *fp, cado_poly cpoly)
{
  unsigned long p;
  int d = cpoly->alg->degree;
  unsigned long *roots;
  int nroots, i;

  cado_poly_check (cpoly);

  fprintf (fp, "# Roots for polynomial ");
  fprint_polynomial (fp, cpoly->alg->f, d);

  fprintf (fp, "# DEGREE: %d\n", d);

  roots = (unsigned long*) malloc (d * sizeof (unsigned long));

  for (p = 2; p <= cpoly->alg->lim; p = getprime (p))
    {
        nroots = poly_roots_ulong(roots, cpoly->alg->f, d, p);
        // TODO: poly_roots_ulong returns 0 if f mod p is 0.
        // This corresponds to complicated roots that should maybe go to
        // the factor base (hum... maybe not!)
        // DOTO: this is now fixed in the -powers version!
      if (nroots != 0)
        {
          fprintf (fp, "%lu: %lld", p, (long long int) roots[0]);
          for (i = 1; i < nroots; i++)
            fprintf (fp, ",%lld", (long long int) roots[i]);
          fprintf (fp, "\n");
        }
    }

  getprime (0); /* free the memory used by getprime() */
  free (roots);
}

static void usage()
{
    fprintf (stderr, "Usage: makefb [-v] [-powers] [-poly file]\n");
    exit (1);
}

int
main (int argc, char *argv[])
{
  int verbose = 0;
  int use_powers = 0;
  param_list pl;
  cado_poly cpoly;
  FILE * f;

  param_list_init(pl);
  cado_poly_init(cpoly);

  param_list_configure_knob(pl, "-v", &verbose);
  param_list_configure_knob(pl, "-powers", &use_powers);

  argv++, argc--;
  for( ; argc ; ) {
      if (param_list_update_cmdline(pl, &argc, &argv)) { continue; }

      /* Could also be a file */
      if ((f = fopen(argv[0], "r")) != NULL) {
          param_list_read_stream(pl, f);
          fclose(f);
          argv++,argc--;
          continue;
      }

      fprintf(stderr, "Unhandled parameter %s\n", argv[0]);
      usage();
  }

  const char * filename;
  if ((filename = param_list_lookup_string(pl, "poly")) != NULL) {
      param_list_read_file(pl, filename);
  }

  if (!cado_poly_set_plist (cpoly, pl))
    {
      fprintf (stderr, "Error reading polynomial file\n");
      exit (EXIT_FAILURE);
    }
  param_list_clear(pl);

  if (use_powers) {
      makefb_with_powers(cpoly->alg->f, cpoly->alg->degree, 
              cpoly->alg->lim, 12);
  } else {
      makefb (stdout, cpoly);
  }

  cado_poly_clear (cpoly);

  return 0;
}
