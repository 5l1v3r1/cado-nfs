/* Contains functions used when doing filter step for FFS instead of NFS */

#include "cado.h"
#include "fppol.h"
#include "portability.h"
#include "utils.h"
#include "filter_matrix.h"
#include "sparse.h"
#include "utils_ffs.h"

unsigned int weight_ffs (int e)
{
  if (e == 0)
      return 0;
  else
      return 1; /* Should depend on e, for now jsut constant*/
}

unsigned int weight_rel_ffs (relation_t rel)
{
  int i;
  unsigned int w = 0;

  for (i = 0; i < rel.nb_rp; i++)
    w += weight_ffs (rel.rp[i].e);

  for (i = 0; i < rel.nb_ap; i++)
    w += weight_ffs (rel.ap[i].e);

  return w;
}

/* return a/b mod p, and p when gcd(b,p) <> 1: this corresponds to a
   projective root */
HT_T
findroot_ffs (int64_t a, uint64_t b, HT_T p)
{
  fppol64_t pol_a, pol_b, pol_p;
  fppol64_t pol_r;

  fppol64_set_ui_sparse (pol_a, (uint64_t) a);
  fppol64_set_ui_sparse (pol_b, b);
  fppol64_set_ui_sparse (pol_p, (uint64_t) p);

#if DEBUG
  if (UNLIKELY(fppol64_deg(pol_p) == -1))
    {
      fprintf(stderr, "p: ul=%lu lx=%lx str=%s\n", p, p, s);
    }
#endif

  fppol64_rem (pol_a, pol_a, pol_p);
  fppol64_rem (pol_b, pol_b, pol_p);
  if (!fppol64_invmod (pol_b, pol_b, pol_p))
      return (HT_T) p;

  fppol64_mulmod (pol_r, pol_a, pol_b, pol_p);

  return (HT_T) fppol64_get_ui_sparse(pol_r);
}

int ffs_poly_set_plist(cado_poly poly, param_list pl)
{
  param_list_parse_ulong(pl, "fbb0", &(poly->rat->lim));
  param_list_parse_int(pl, "lpb0", &(poly->rat->lpb));
  param_list_parse_ulong(pl, "fbb1", &(poly->alg->lim));
  param_list_parse_int(pl, "lpb1", &(poly->alg->lpb));

  return 1;
}

// returns 0 on failure, 1 on success.
int ffs_poly_read(cado_poly poly, const char *filename)
{
    FILE *file;
    int r;
    param_list pl;

    file = fopen(filename, "r");
    if (file == NULL) 
      {
	      fprintf(stderr, "read_polynomial: could not open %s\n", filename);
	      return 0;
      }
    
    param_list_init(pl);
    param_list_read_stream(pl, file);
    r = ffs_poly_set_plist(poly, pl);

    param_list_clear(pl);
    fclose(file);
    return r;
}
