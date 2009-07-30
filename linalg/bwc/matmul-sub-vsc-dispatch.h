#ifndef MATMUL_SUB_VSC_DISPATCH_H_
#define MATMUL_SUB_VSC_DISPATCH_H_

#include "abase.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void matmul_sub_vsc_dispatch(abobj_ptr x, abt * dst, abt const * src, const uint16_t * q, unsigned long count);


#ifdef __cplusplus
}
#endif

#endif	/* MATMUL_SUB_VSC_DISPATCH_H_ */
