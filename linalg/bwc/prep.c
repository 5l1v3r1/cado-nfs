#include "cado.h"
#include <stdio.h>
#include <pthread.h>
#include "bwc_config.h"
#include "parallelizing_info.h"
#include "matmul_top.h"
#include "select_mpi.h"
#include "gauss.h"
#include "gauss.h"
#include "params.h"
#include "xvectors.h"
#include "bw-common.h"
#include "filenames.h"
#include "mpfq/mpfq.h"
#include "mpfq/mpfq_vbase.h"
#include "portability.h"

void * prep_prog(parallelizing_info_ptr pi, param_list pl, void * arg MAYBE_UNUSED)
{
    /* Interleaving does not make sense for this program. So the second
     * block of threads just leave immediately */
    if (pi->interleaved && pi->interleaved->idx)
        return NULL;

    // Doing the ``hello world'' test is a very good way of testing the
    // global mpi/pthreads setup. So despite its apparent irrelevance, I
    // suggest leaving it here as a cheap sanity check.
    pi_hello(pi);

    int tcan_print = bw->can_print && pi->m->trank == 0;
    matmul_top_data mmt;

    int flags[2];
    flags[bw->dir] = THREAD_SHARED_VECTOR;
    flags[!bw->dir] = 0;

    mpfq_vbase A;
    mpfq_vbase_oo_field_init_byfeatures(A, 
            MPFQ_PRIME_MPZ, bw->p,
            MPFQ_GROUPSIZE, bw->n,
            MPFQ_DONE);

    pi_datatype_ptr A_pi = pi_alloc_mpfq_datatype(pi, A);

    matmul_top_init(mmt, A, A_pi, pi, flags, pl, bw->dir);
    unsigned int unpadded = MAX(mmt->n0[0], mmt->n0[1]);

    /* Number of copies of m by n matrices to use for trying to obtain a
     * full-rank matrix (rank m).
     *
     * Note that it must be at least m/n, otherwise we stand no chance !
     */
    unsigned int prep_lookahead_iterations;
    prep_lookahead_iterations = iceildiv(bw->m, bw->m) + 1;

    unsigned int my_nx = 1;

    mmt_comm_ptr mcol = mmt->wr[bw->dir];
    mmt_comm_ptr mrow = mmt->wr[!bw->dir];

    uint32_t * xvecs = malloc(my_nx * bw->m * sizeof(uint32_t));

    void * xymats;

    /* We're cheating on the generic init routines */
    A->vec_init(A, &xymats, bw->m * prep_lookahead_iterations);

    gmp_randstate_t rstate;
    gmp_randinit_default(rstate);

    if (pi->m->trank == 0 && !bw->seed) {
        /* note that bw is shared between threads, thus only thread 0 should
         * test and update it here.
         * at pi->m->jrank > 0, we don't care about the seed anyway
         */
        bw->seed = time(NULL);
        MPI_Bcast(&bw->seed, 1, MPI_INT, 0, pi->m->pals);
    }
    serialize_threads(pi->m);

    gmp_randseed_ui(rstate, bw->seed);
    if (tcan_print) {
        printf("// Random generator seeded with %d\n", bw->seed);
    }



    for (unsigned ntri = 0;; ntri++) {
        serialize_threads(pi->m);

        if (tcan_print) {
            printf("// Generating new x,y vector pair (trial # %u)\n", ntri);
        }
        if (ntri >= my_nx * 10) {
            ++my_nx;
            if (tcan_print) {
                printf("// Getting bored. Trying %u x vectors\n", my_nx);
            }
            xvecs = realloc(xvecs, my_nx * bw->m * sizeof(uint32_t));
            ASSERT_ALWAYS(xvecs != NULL);
        }

        // if we're looking for the right nullspace, then x is on the left.
        // Otherwise, it's on the right.

        // generate indices w.r.t *unpadded* dimensions !
        setup_x_random(xvecs, bw->m, my_nx, mmt->n0[bw->dir], pi, rstate);

        /* Random generation + save is better done as writing random data
         * to a file followed by reading it: this way, seeding works
         * better.
         */

        mmt_vec_set_random_through_file(mmt, NULL, Y_FILE_BASE, bw->dir, 0, unpadded, rstate);

        if (tcan_print) {
            printf("// vector generated and dispatched (trial # %u)\n", ntri);
        }

#if 0
        // Compute y.
        matmul_top_fill_random_source(mmt, bw->dir);

        // we need to save this starting vector for later use if it turns out
        // that we need to save it for real.
        mmt_vec_save(mmt, NULL, Y_FILE_BASE, bw->dir, 0, unpadded);
#endif

        // We must compute x^T M y, x^T M^2 y, and so on.
        // XXX Note that x^Ty does not count here, because it does not
        // take part to the sequence computed by lingen !
        matmul_top_twist_vector(mmt, bw->dir);
        matmul_top_mul(mmt, bw->dir);
        matmul_top_untwist_vector(mmt, bw->dir);
        
        // we have indices mmt->wr[1]->i0..i1 available.
        A->vec_set_zero(A, xymats, bw->m * prep_lookahead_iterations);

        for(unsigned int k = 0 ; k < prep_lookahead_iterations ; k++) {
            for(int j = 0 ; j < bw->m ; j++) {
                void * where = A->vec_subvec(A, xymats, j * prep_lookahead_iterations + k);
                for(unsigned int t = 0 ; t < my_nx ; t++) {
                    uint32_t i = xvecs[j*my_nx+t];
                    if (i < mcol->i0 || i >= mcol->i1)
                        continue;
                    /* We want the data to match our bw->interval on both
                     * directions, because otherwise we'll end up
                     * computing rubbish -- recall that no broadcast_down
                     * has occurred yet.
                     */
                    if (i < mrow->i0 || i >= mrow->i1)
                        continue;
                    A->add(A, where, where, SUBVEC(mcol->v, v, i - mcol->i0));
                }
            }
            matmul_top_twist_vector(mmt, bw->dir);
            matmul_top_mul(mmt, bw->dir);
            matmul_top_untwist_vector(mmt, bw->dir);
        }

        /* Make sure computation is over for everyone ! */
        serialize_threads(pi->m);

        /* Now all threads and jobs must collectively reduce the zone
         * pointed to by xymats */
        pi_allreduce(NULL, xymats,
                bw->m * prep_lookahead_iterations,
                A_pi, BWC_PI_SUM, pi->m);

        /* OK -- now everybody has the same data */

        int dimk;
        
        /* the kernel() call is not reentrant */
        if (pi->m->trank == 0) {
            dimk = kernel((mp_limb_t *) xymats, NULL,
                    bw->m, prep_lookahead_iterations * A->groupsize(A),
                    A->vec_elt_stride(A, prep_lookahead_iterations)/sizeof(mp_limb_t),
                    0);
        }
        pi_thread_bcast((void *) &dimk, 1, BWC_PI_INT, 0, pi->m);

        if (tcan_print)
            printf("// Dimension of kernel: %d\n", dimk);

        if (dimk == 0) {
            if (tcan_print)
                printf("// Found good x,y vector pair after %u trials\n",
                        ntri+1);
            break;
        }
    }

    gmp_randclear(rstate);

    save_x(xvecs, bw->m, my_nx, pi);

    matmul_top_clear(mmt);
    pi_free_mpfq_datatype(pi, A_pi);

    /* clean up xy mats stuff */
    A->vec_clear(A, &xymats, bw->m * prep_lookahead_iterations);

    A->oo_field_clear(A);

    free(xvecs);
    return NULL;
}

/* The GF(p) case is significantly different:
 *  - this is *NOT* a parallel program, although we're happy to use the
 *    same tools as everywhere else for some common operations, namely
 *    the matmul_top layer.
 *  - we do *NOT* perform the * consistency check.
 *  - and we possibly inject the RHS in the data produced.
 */
void * prep_prog_gfp(parallelizing_info_ptr pi, param_list pl, void * arg MAYBE_UNUSED)
{
    /* Interleaving does not make sense for this program. So the second
     * block of threads just leave immediately */
    if (pi->interleaved && pi->interleaved->idx)
        return NULL;


    matmul_top_data mmt;

    int flags[2];
    flags[bw->dir] = THREAD_SHARED_VECTOR;
    flags[!bw->dir] = 0;

    unsigned int nrhs = 0;

    mpfq_vbase A;
    mpfq_vbase_oo_field_init_byfeatures(A, 
            MPFQ_PRIME_MPZ, bw->p,
            MPFQ_GROUPSIZE, 1,
            MPFQ_DONE);

    pi_datatype_ptr A_pi = pi_alloc_mpfq_datatype(pi, A);

    matmul_top_init(mmt, A, A_pi, pi, flags, pl, bw->dir);

    if (pi->m->trank || pi->m->jrank) {
        /* as said above, this is *NOT* a parallel program.  */
        goto leave_prep_prog_gfp;
    }

    gmp_randstate_t rstate;
    gmp_randinit_default(rstate);

    if (pi->m->trank == 0 && !bw->seed) {
        /* note that bw is shared between threads, thus only thread 0 should
         * test and update it here.
         * at pi->m->jrank > 0, we don't care about the seed anyway
         */
        bw->seed = time(NULL);
        MPI_Bcast(&bw->seed, 1, MPI_INT, 0, pi->m->pals);
    }

    gmp_randseed_ui(rstate, bw->seed);
    printf("// Random generator seeded with %d\n", bw->seed);

    const char * rhs_name = param_list_lookup_string(pl, "rhs");
    FILE * rhs = NULL;
    if (rhs_name) {
        rhs = fopen(rhs_name, "r");
        get_rhs_file_header_stream(rhs, NULL, &nrhs, NULL);
        ASSERT_ALWAYS(rhs != NULL);
    }

    /* First create all RHS vectors -- these are just splits of the big
     * RHS block. Those files get created together. */
    if (nrhs) {
        char ** vec_names = malloc(nrhs * sizeof(char *));
        FILE ** vec_files = malloc(nrhs * sizeof(FILE *));
        for(unsigned int j = 0 ; j < nrhs ; j++) {
            int rc = asprintf(&vec_names[j], V_FILE_BASE_PATTERN ".0", j, j+1);
            ASSERT_ALWAYS(rc >= 0);
            vec_files[j] = fopen(vec_names[j], "wb");
            ASSERT_ALWAYS(vec_files[j] != NULL);
            printf("// Creating %s (extraction from %s)\n", vec_names[j], rhs_name);
        }
        void * coeff;
        A->vec_init(A, &coeff, 1);
        mpz_t c;
        mpz_init(c);
        for(unsigned int i = 0 ; i < mmt->n0[!bw->dir] ; i++) {
            for(unsigned int j = 0 ; j < nrhs ; j++) {
                int rc;
                memset(coeff, 0, A->vec_elt_stride(A, 1));
                rc = gmp_fscanf(rhs, "%Zd", c);
                ASSERT_ALWAYS(rc == 1);
                A->set_mpz(A, A->vec_coeff_ptr(A, coeff, 0), c);
                rc = fwrite(coeff, A->vec_elt_stride(A,1), 1, vec_files[j]);
                ASSERT_ALWAYS(rc == 1);
            }
        }
        mpz_clear(c);
        A->vec_clear(A, &coeff, 1);
        for(unsigned int j = 0 ; j < nrhs ; j++) {
            fclose(vec_files[j]);
            free(vec_names[j]);
        }
        free(vec_files);
        free(vec_names);
    }
    /* Now create purely random vectors */
    for(int j = (int) nrhs ; j < bw->n ; j++) {
        void * vec;
        char * vec_name;
        FILE * vec_file;
        int rc = asprintf(&vec_name, V_FILE_BASE_PATTERN ".0", j, j+1);
        ASSERT_ALWAYS(rc >= 0);
        vec_file = fopen(vec_name, "wb");
        ASSERT_ALWAYS(vec_file != NULL);
        printf("// Creating %s\n", vec_name);
        A->vec_init(A, &vec, mmt->n0[!bw->dir]);
        A->vec_random(A, vec, mmt->n0[!bw->dir], rstate);
        rc = fwrite(vec, A->vec_elt_stride(A,1), mmt->n0[!bw->dir], vec_file);
        ASSERT_ALWAYS(rc >= 0 && ((unsigned int) rc) == mmt->n0[!bw->dir]);
        A->vec_clear(A, &vec, mmt->n0[!bw->dir]);
        fclose(vec_file);
        free(vec_name);
    }


    gmp_randclear(rstate);

    {
        /* initialize x -- make it completely deterministic. */
        unsigned int my_nx = 1;
        uint32_t * xvecs = malloc(my_nx * bw->m * sizeof(uint32_t));
        /* with rhs, consider the strategy where the matrix is kept with
         * its full size, but the SM block is replaced with zeros. Here
         * we just force the x vectors to have data there, so as to avoid
         * the possibility that a solution vector found by BW still has
         * non-zero coordinates at these locations.
         */
        if (bw->m < (int) nrhs) {
            fprintf(stderr, "m < nrhs is not supported\n");
            exit(EXIT_FAILURE);
        }
        for(unsigned int i = 0 ; i < nrhs ; i++) {
            xvecs[i] = balancing_pre_shuffle(mmt->bal, mmt->n0[!bw->dir]-nrhs+i);
            printf("Forced %d-th x vector to be the %" PRIu32"-th canonical basis vector\n", i, xvecs[i]);
            ASSERT_ALWAYS(xvecs[i] >= (uint32_t) (bw->m - nrhs));
        }
        for(int i = (int) nrhs ; i < bw->m ; i++) {
            xvecs[i] = i - nrhs;
        }
        /* save_x operates only on the leader thread */
        save_x(xvecs, bw->m, my_nx, pi);
        free(xvecs);
    }

leave_prep_prog_gfp:
    matmul_top_clear(mmt);
    pi_free_mpfq_datatype(pi, A_pi);

    A->oo_field_clear(A);
    return NULL;
}

int main(int argc, char * argv[])
{
    param_list pl;

    bw_common_init_new(bw, &argc, &argv);
    param_list_init(pl);
    parallelizing_info_init();

    bw_common_decl_usage(pl);
    parallelizing_info_decl_usage(pl);
    matmul_top_decl_usage(pl);
    /* declare local parameters and switches */
    param_list_decl_usage(pl, "rhs",
            "file with the right-hand side vectors for inhomogeneous systems mod p");

    bw_common_parse_cmdline(bw, pl, &argc, &argv);

    bw_common_interpret_parameters(bw, pl);
    parallelizing_info_lookup_parameters(pl);
    matmul_top_lookup_parameters(pl);
    /* interpret our parameters: none here (so far). */
    if (mpz_cmp_ui(bw->p, 2) != 0)
        param_list_lookup_string(pl, "rhs");

    if (param_list_warn_unused(pl)) {
        param_list_print_usage(pl, argv[0], stderr);
        exit(EXIT_FAILURE);
    }

    if (mpz_cmp_ui(bw->p, 2) == 0) {
        pi_go(prep_prog, pl, 0);
    } else {
        pi_go(prep_prog_gfp, pl, 0);
    }

    parallelizing_info_finish();
    param_list_clear(pl);
    bw_common_clear_new(bw);

    return 0;
}

