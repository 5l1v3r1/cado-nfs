#ifndef LAS_THREADS_H_
#define LAS_THREADS_H_

#include <pthread.h>
#include "las-forwardtypes.h"
#include "bucket.h"
#include "fb.h"
#include "las-report-stats.h"
#include "las-base.hpp"

/* {{{ thread-related defines */
/* All of this exists _for each thread_ */
struct thread_side_data : private NonCopyable {
  // m_bucket_array_t mBA; /* Not used if not fill_in_m_buckets (3 passes sort) */
  // k_bucket_array_t kBA; /* Ditto for fill_in_k_buckets (2 passes sort) */
  bucket_array_t<bucket_update_t<1, shorthint_t> > BA;    /* Always used */
  const fb_part *fb;

  /* For small sieve */
  int * ssdpos;
  int * rsdpos;

  unsigned char *bucket_region;
  sieve_checksum checksum_post_sieve;

  thread_side_data();
  ~thread_side_data();
  /* Allocate enough memory to be able to store at least n_bucket buckets,
     each of size at least fill_ratio * bucket region size. */
  void allocate_bucket_array(uint32_t n_bucket, double fill_ratio);
  void set_fb(const fb_part *_fb) {fb = _fb;}
  void update_checksum(){checksum_post_sieve.update(bucket_region, BUCKET_REGION);}
};

struct thread_data : private NonCopyable {
  int id;
  thread_side_data sides[2];
  las_info_srcptr las;
  sieve_info_ptr si;
  las_report rep;
  unsigned char *SS;
  bool is_initialized;
  thread_data();
  ~thread_data();
  void init(int id, las_info_srcptr las);
  void pickup_si(sieve_info_ptr si);
  void update_checksums();
};

class thread_workspaces : private NonCopyable {
  thread_data *thrs;
  bool *used;
  const size_t nr_workspaces;
  pthread_mutex_t mutex;
  sieve_info_ptr si;
public:
  thread_workspaces(size_t n, las_info_ptr _las);
  ~thread_workspaces();
  void pickup_si(sieve_info_ptr si);
  void thread_do(void * (*) (thread_data *));
  void buckets_alloc();
  void buckets_free();
  double buckets_max_full();
  void accumulate(las_report_ptr, sieve_checksum *);
  thread_data &reserve_workspace();
  void release_workspace(thread_data &);
};

#endif
