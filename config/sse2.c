/* This source file is our test case for sse-2 support. If this can be
 * compiled, then we're happy because sse-2 will be usable in cantor/ ;
 * it also means that SSE_NORM_INIT will be usable in sieve/las.
 */
#include <stdint.h>
#include <emmintrin.h>

int main(int argc, char *argv[])
{
    __m128i foo = _mm_setr_epi32(argc, argc + 1, argc + 2, argc + 3);
    __m128i bar = _mm_setr_epi32(argc + 3, argc + 2, argc + 1, argc);
    __m128i x = _mm_setr_epi32(42, 0, 17, 0);
    __m128d g = _mm_set_pd(42.0, 17.0);
    x = _mm_srl_epi64(x, x);
    foo = _mm_mullo_epi16(foo, bar);
    foo = _mm_slli_epi64(foo, 1);
    foo = _mm_xor_si128(bar, _mm_unpacklo_epi32(foo, bar));
    foo = _mm_srli_epi64(foo, 1);
    foo = _mm_mullo_epi16(foo, bar);
    foo = _mm_shuffle_epi32(foo, 78);
    foo = _mm_xor_si128(bar, _mm_unpacklo_epi32(foo, bar));
    foo = _mm_srli_si128(foo, 1);
    foo = _mm_xor_si128(foo, x);

    return _mm_extract_epi16(foo, 0) & (argc - 1);
}
