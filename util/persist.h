#ifndef INCLUDE_PERSIST_H_
#define INCLUDE_PERSIST_H_

#include <cstdint>
#include <cstdio>
#include <emmintrin.h>
#include <cstring>

#define asm_clwb(addr)                      \
    ({                                      \
        __asm__ __volatile__("clwb %0"      \
                             :              \
                             : "m"(*addr)); \
    })

#define asm_clflush(addr)                   \
    ({                                      \
        __asm__ __volatile__("clflush %0"   \
                             :              \
                             : "m"(*addr)); \
    })

#define asm_clflushopt(addr)                 \
    ({                                       \
        __asm__ __volatile__("clflushopt %0" \
                             :               \
                             : "m"(*addr));  \
    })

/*  Memory fence:  
    mfence can be replaced with sfence, if the CPU supports sfence.
*/
#define asm_mfence()                          \
    ({                                        \
        __asm__ __volatile__("mfence" ::      \
                                 : "memory"); \
    })

#define asm_lfence()                          \
    ({                                        \
        __asm__ __volatile__("lfence" ::      \
                                 : "memory"); \
    })

#define asm_sfence()                          \
    ({                                        \
        __asm__ __volatile__("sfence" ::      \
                                 : "memory"); \
    })

inline void pflush(char* p, size_t size)
{
    uint64_t ptr = (uint64_t)p & ~(64 - 1);
    uint64_t uptr = ptr + size;
    for (; ptr < uptr; ptr += 64) {
        asm_clflushopt((char*)ptr);
    }
    asm_sfence();
}

inline void pflush_clwb(char* p, size_t size)
{
    uint64_t ptr = (uint64_t)p & ~(64 - 1);
    uint64_t uptr = ptr + size;
    for (; ptr < uptr; ptr += 64) {
        asm_clwb((char*)ptr);
    }
    asm_sfence();
}

inline void pflush_wo_fence(char* p, size_t size)
{
    uint64_t ptr = (uint64_t)p & ~(64 - 1);
    uint64_t uptr = ptr + size;
    for (; ptr < uptr; ptr += 64) {
        asm_clflushopt((char*)ptr);
    }
    asm_sfence();
}

inline void pflush_no_fence (char *p, size_t size)
{
  uint64_t ptr = (uint64_t)p & ~(64 - 1);
  uint64_t uptr = ptr + size;
  for (; ptr < uptr; ptr += 64) {
    asm_clflushopt((char*)ptr);
  }
}

// SSE2 intrinsics support
static inline void sse2_movnt4x64b(char* dest, const char* src)
{
    __m128i xmm0 = _mm_loadu_si128((__m128i*)src + 0); // 16
    __m128i xmm1 = _mm_loadu_si128((__m128i*)src + 1); // 32
    __m128i xmm2 = _mm_loadu_si128((__m128i*)src + 2); // 48
    __m128i xmm3 = _mm_loadu_si128((__m128i*)src + 3); // 64
    __m128i xmm4 = _mm_loadu_si128((__m128i*)src + 4);
    __m128i xmm5 = _mm_loadu_si128((__m128i*)src + 5);
    __m128i xmm6 = _mm_loadu_si128((__m128i*)src + 6);
    __m128i xmm7 = _mm_loadu_si128((__m128i*)src + 7); // 128B
    __m128i xmm8 = _mm_loadu_si128((__m128i*)src + 8);
    __m128i xmm9 = _mm_loadu_si128((__m128i*)src + 9);
    __m128i xmm10 = _mm_loadu_si128((__m128i*)src + 10);
    __m128i xmm11 = _mm_loadu_si128((__m128i*)src + 11);
    __m128i xmm12 = _mm_loadu_si128((__m128i*)src + 12);
    __m128i xmm13 = _mm_loadu_si128((__m128i*)src + 13);
    __m128i xmm14 = _mm_loadu_si128((__m128i*)src + 14);
    __m128i xmm15 = _mm_loadu_si128((__m128i*)src + 15); // 256B
    _mm_stream_si128((__m128i*)dest + 0, xmm0);
    _mm_stream_si128((__m128i*)dest + 1, xmm1);
    _mm_stream_si128((__m128i*)dest + 2, xmm2);
    _mm_stream_si128((__m128i*)dest + 3, xmm3);
    _mm_stream_si128((__m128i*)dest + 4, xmm4);
    _mm_stream_si128((__m128i*)dest + 5, xmm5);
    _mm_stream_si128((__m128i*)dest + 6, xmm6);
    _mm_stream_si128((__m128i*)dest + 7, xmm7);
    _mm_stream_si128((__m128i*)dest + 8, xmm8);
    _mm_stream_si128((__m128i*)dest + 9, xmm9);
    _mm_stream_si128((__m128i*)dest + 10, xmm10);
    _mm_stream_si128((__m128i*)dest + 11, xmm11);
    _mm_stream_si128((__m128i*)dest + 12, xmm12);
    _mm_stream_si128((__m128i*)dest + 13, xmm13);
    _mm_stream_si128((__m128i*)dest + 14, xmm14);
    _mm_stream_si128((__m128i*)dest + 15, xmm15);
}

static inline void sse2_movnt2x64b(char* dest, const char* src)
{
    __m128i xmm0 = _mm_loadu_si128((__m128i*)src + 0);
    __m128i xmm1 = _mm_loadu_si128((__m128i*)src + 1);
    __m128i xmm2 = _mm_loadu_si128((__m128i*)src + 2);
    __m128i xmm3 = _mm_loadu_si128((__m128i*)src + 3);
    __m128i xmm4 = _mm_loadu_si128((__m128i*)src + 4);
    __m128i xmm5 = _mm_loadu_si128((__m128i*)src + 5);
    __m128i xmm6 = _mm_loadu_si128((__m128i*)src + 6);
    __m128i xmm7 = _mm_loadu_si128((__m128i*)src + 7);
    _mm_stream_si128((__m128i*)dest + 0, xmm0);
    _mm_stream_si128((__m128i*)dest + 1, xmm1);
    _mm_stream_si128((__m128i*)dest + 2, xmm2);
    _mm_stream_si128((__m128i*)dest + 3, xmm3);
    _mm_stream_si128((__m128i*)dest + 4, xmm4);
    _mm_stream_si128((__m128i*)dest + 5, xmm5);
    _mm_stream_si128((__m128i*)dest + 6, xmm6);
    _mm_stream_si128((__m128i*)dest + 7, xmm7);
}

static inline void sse2_movnt1x64b(char* dest, const char* src)
{
    __m128i xmm0 = _mm_loadu_si128((__m128i*)src + 0);
    __m128i xmm1 = _mm_loadu_si128((__m128i*)src + 1);
    __m128i xmm2 = _mm_loadu_si128((__m128i*)src + 2);
    __m128i xmm3 = _mm_loadu_si128((__m128i*)src + 3);
    _mm_stream_si128((__m128i*)dest + 0, xmm0);
    _mm_stream_si128((__m128i*)dest + 1, xmm1);
    _mm_stream_si128((__m128i*)dest + 2, xmm2);
    _mm_stream_si128((__m128i*)dest + 3, xmm3);
}

static inline void sse2_movnt1x32b(char* dest, const char* src)
{
    __m128i xmm0 = _mm_loadu_si128((__m128i*)src + 0);
    __m128i xmm1 = _mm_loadu_si128((__m128i*)src + 1);
    _mm_stream_si128((__m128i*)dest + 0, xmm0);
    _mm_stream_si128((__m128i*)dest + 1, xmm1);
}

static inline void sse2_movnt1x16b(char* dest, const char* src)
{
    __m128i xmm0 = _mm_loadu_si128((__m128i*)src);
    _mm_stream_si128((__m128i*)dest, xmm0);
}

static inline void sse2_movnt1x8b(char* dest, const char* src)
{
    _mm_stream_si64((long long*)dest, *(long long*)src);
}

static inline void sse2_movnt1x4b(char* dest, const char* src)
{
    _mm_stream_si32((int*)dest, *(int*)src);
}

static inline void sse2_small_mov(char* dest, const char* src, size_t size)
{
    memcpy(dest, src, size);
    pflush_wo_fence(dest, size);
}

inline void persist_store(char* dest, char* src, size_t size)
{
    memcpy(dest, src, size);
    pflush(dest, size);
}

inline void nontemporal_store(char* dest, char* src, size_t size)
{
    size_t cnt = (uint64_t)dest & 63;
    if (cnt > 0) {
        cnt = 64 - cnt;
        if (cnt > size) {
            cnt = size;
        }
        sse2_small_mov(dest, src, cnt);
        dest += cnt;
        src += cnt;
        size -= cnt;
    }
    while (size >= 256) {
        sse2_movnt4x64b(dest, src);
        dest += 256;
        src += 256;
        size -= 256;
    }
    if (size >= 128) {
        sse2_movnt2x64b(dest, src);
        dest += 128;
        src += 128;
        size -= 128;
    }
    if (size >= 64) {
        sse2_movnt1x64b(dest, src);
        dest += 64;
        src += 64;
        size -= 64;
    }
    if (size >= 32) {
        sse2_movnt1x32b(dest, src);
        dest += 32;
        src += 32;
        size -= 32;
    }
    if (size >= 16) {
        sse2_movnt1x16b(dest, src);
        dest += 16;
        src += 16;
        size -= 16;
    }
    if (size >= 8) {
        sse2_movnt1x8b(dest, src);
        dest += 8;
        src += 8;
        size -= 8;
    }
    if (size >= 4) {
        sse2_movnt1x4b(dest, src);
        dest += 4;
        src += 4;
        size -= 4;
    }
    if (size > 0) {
        sse2_small_mov(dest, src, size);
    }
    asm_sfence();
}

inline static void persist_data(char* src, size_t size)
{
    pflush(src, size);
}

inline static void persist_data_clwb(char* src, size_t size)
{
    pflush_clwb(src, size);
}

#endif