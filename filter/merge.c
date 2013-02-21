/* merge --- main program to merge relations into relation-sets (cycles)

Copyright 2008-2009 Francois Morain.
Reviewed by Paul Zimmermann, February 2009.

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

/* Algorithm: digest and interpolation from Cavallar02.

  Stefania Cavallar, On the Number Field Sieve Integer Factorisation Algorithm,
  PhD thesis, University of Leiden, 2002, 108 pages.
 */

#include "cado.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for strcmp */

#include "portability.h"
#include "utils.h" /* for gzip_open */

#include "merge_opts.h"
#include "filter_matrix.h" /* for filter_matrix_t */
#include "report.h"     /* for report_t */
#include "markowitz.h" /* for MkzInit */
#include "merge_mono.h" /* for mergeOneByOne */

#ifdef USE_MPI
#include "mpi.h"
#include "merge_mpi.h"
#endif

#ifdef FOR_FFS
#include "utils_ffs.h"
#endif

#define MAXLEVEL_DEFAULT 10
#define KEEP_DEFAULT 160
#define SKIP_DEFAULT 32
#define FORBW_DEFAULT 0
#define RATIO_DEFAULT 1.1
#define COVERNMAX_DEFAULT 100.0
#define MKZTYPE_DEFAULT 1 /* pure Markowitz */
#define WMSTMAX_DEFAULT 7 /* relevant only if mkztype == 2 */

static void
usage (void)
{
  fprintf (stderr, "Usage: merge -mat xxx -out xxx [options]\n");
  fprintf (stderr, "\n");
  fprintf (stderr, "   -mat   xxx     - input (purged) file is xxx\n");
  fprintf (stderr, "   -out   xxx     - output (history) file is xxx\n");
  fprintf (stderr, "\n");
  fprintf (stderr, "   -maxlevel nnn  - merge up to nnn rows (default %u)\n",
	   MAXLEVEL_DEFAULT);
  fprintf (stderr, "   -keep nnn      - keep an excess of nnn (default %u)\n",
	   KEEP_DEFAULT);
  fprintf (stderr, "   -skip nnn      - bury the nnn heaviest columns (default %u)\n",
	   SKIP_DEFAULT);
  fprintf (stderr, "   -forbw nnn     - controls the optimization function (default %u, see below)\n",
	   FORBW_DEFAULT);
  fprintf (stderr, "   -ratio rrr     - maximal ratio cN(final)/cN(min) with forbw=0 (default %1.1f)\n",
	   RATIO_DEFAULT);
  fprintf (stderr, "   -coverNmax nnn - with forbw=3, stop when c/N exceeds nnn (default %1.2f)\n", COVERNMAX_DEFAULT);
  fprintf (stderr, "   -itermax nnn   - if non-zero, stop when nnn columns have been removed (cf -resume)\n");
  fprintf (stderr, "   -resume xxx    - resume from history file xxx (cf -itermax)\n");
  fprintf (stderr, "   -mkztype nnn   - controls how the weight of a merge is approximated (default %d)\n", MKZTYPE_DEFAULT);
  fprintf (stderr, "   -wmstmax nnn   - if mkztype = 2, controls until when a mst is used (default %d)\n", WMSTMAX_DEFAULT);
  fprintf (stderr, "\nThe different optimization functions are, where c is the total matrix weight\n");
  fprintf (stderr, "and N the number of rows (relation-sets):\n");
  fprintf (stderr, "   -forbw 0 - optimize the matrix size N (cf -ratio)\n");
  fprintf (stderr, "   -forbw 1 - stop when the product cN is minimal\n");
  fprintf (stderr, "   -forbw 3 - stop when the ratio c/N exceeds coverNmax\n");
  exit (1);
}

int
main (int argc, char *argv[])
{
    filter_matrix_t mat[1];
    report_t rep[1];
    int maxlevel = MAXLEVEL_DEFAULT, keep = KEEP_DEFAULT, skip = SKIP_DEFAULT;
    double tt;
    double ratio = RATIO_DEFAULT; /* bound on cN_new/cN to stop the merge */
    int forbw = FORBW_DEFAULT;
    double coverNmax = COVERNMAX_DEFAULT;
    int mkztype = MKZTYPE_DEFAULT;
    int wmstmax = WMSTMAX_DEFAULT; /* use real MST minimum for wt[j] <= wmstmax*/
    int itermax = 0;
    double wct0 = wct_seconds ();
    param_list pl;
    param_list_init (pl);

#ifdef USE_MPI
    MPI_Init(&argc, &argv);
#endif

    argv++, argc--;

    if (argc == 0) /* To avoid a seg fault is no command line arg is given */
      usage ();

    for( ; argc ; ) {
      if (param_list_update_cmdline(pl, &argc, &argv)) continue;
      fprintf (stderr, "Unknown option: %s\n", argv[0]);
      usage ();
    }

    /* print command-line arguments */
    param_list_print_command_line (stdout, pl);
    fflush(stdout);

    const char * purgedname = param_list_lookup_string (pl, "mat");
    const char * outname = param_list_lookup_string (pl, "out");
    /* -resume can be useful to continue a merge stopped due  */
    /* to a too small value of -maxlevel                      */
    const char * resumename = param_list_lookup_string (pl, "resume");

    param_list_parse_int (pl, "maxlevel", &maxlevel);
    param_list_parse_int (pl, "keep", &keep);
    param_list_parse_int (pl, "skip", &skip);
    param_list_parse_int (pl, "forbw", &forbw);

    param_list_parse_double (pl, "ratio", &ratio);
    param_list_parse_double (pl, "coverNmax", &coverNmax);

    param_list_parse_int (pl, "mkztype", &mkztype);
    param_list_parse_int (pl, "wmstmax", &wmstmax);

    param_list_parse_int (pl, "itermax", &itermax);

    purgedfile_stream ps;
    purgedfile_stream_init(ps);
    purgedfile_stream_openfile (ps, purgedname);

    mat->nrows = ps->nrows;
#ifdef FOR_FFS
    mat->ncols = ps->ncols + 1; /*for FFS, we add a column of 1*/
#else
    mat->ncols = ps->ncols;
#endif
    mat->keep  = keep;
    mat->cwmax = 2 * maxlevel;
    mat->rwmax = INT_MAX;
    mat->mergelevelmax = maxlevel;
    mat->itermax = itermax;

#ifdef USE_MPI
    mpi_start_proc(outname,mat,purgedfile,purgedname,forbw,ratio,coverNmax,
		   resumename);
    /* TODO: clean the mat data structure (?) */
    MPI_Finalize();
    return 0;
#endif
    initMat (mat, 0, mat->ncols);

    tt = seconds ();
    filter_matrix_read_weights (mat, ps);
    printf ("Getting column weights took %2.2lf\n", seconds () - tt);
    fflush (stdout);
    /* note: we can't use purgedfile_stream_rewind on a compressed file,
       thus we close and reopen */
    purgedfile_stream_closefile (ps);
    purgedfile_stream_openfile (ps, purgedname);

    /* print weight counts */
    {
      unsigned long j, *nbm, total_weight = 0;
      int w;

      nbm = (unsigned long*) malloc ((maxlevel + 1) * sizeof (unsigned long));
      memset (nbm, 0, (maxlevel + 1) * sizeof (unsigned long));
      for (j = 0; j < (unsigned long) mat->ncols; j++)
        {
          w = mat->wt[GETJ(mat, j)];
          total_weight += w;
          if (w <= maxlevel)
            nbm[w] ++;
        }
      printf ("Total matrix weight: %lu\n", total_weight);
      for (j = 0; j <= (unsigned long) maxlevel; j++)
        if (nbm[j] != 0)
          printf ("There are %lu column(s) of weight %lu\n", nbm[j], j);
      free (nbm);
    }
    fflush (stdout);

    fillmat (mat);

    tt = wct_seconds ();
    filter_matrix_read (mat, ps, skip);
    printf ("Time for filter_matrix_read: %2.2lf\n", wct_seconds () - tt);

    /* initialize rep, i.e., mostly opens outname */
    init_rep (rep, outname, mat, 0, MERGE_LEVEL_MAX);
    /* output the matrix dimensions in the history file */
#ifdef FOR_FFS
    report2 (rep, mat->nrows, mat->ncols-1, -1);
#else
    report2 (rep, mat->nrows, mat->ncols, -1);
#endif

    /* resume from given history file if needed */
    if (resumename != NULL)
      resume (rep, mat, resumename);

    mat->wmstmax = wmstmax;
    mat->mkztype = mkztype;
    tt = seconds();
    MkzInit (mat);
    printf ("Time for MkzInit: %2.2lf\n", seconds()-tt);

    mergeOneByOne (rep, mat, maxlevel, forbw, ratio, coverNmax);

    gzip_close (rep->outfile, outname);
    printf ("Final matrix has N=%d nc=%d (%d) w(M)=%lu N*w(M)=%"
            PRIu64"\n", mat->rem_nrows, mat->rem_ncols,
            mat->rem_nrows - mat->rem_ncols, mat->weight,
	    (uint64_t) mat->rem_nrows * (uint64_t) mat->weight);
    fflush (stdout);
    MkzClose (mat);
    clearMat (mat);
    purgedfile_stream_closefile (ps);
    purgedfile_stream_clear (ps);

    param_list_clear (pl);

    printf ("Total merge time: %1.0f seconds\n", seconds ());

    print_timing_and_memory (wct0);

    return 0;
}
