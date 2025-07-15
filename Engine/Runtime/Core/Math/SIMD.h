#pragma once

#include "Base/Base.h"

#if defined(_WIN32) || defined(__linux__)
#define TYR_USE_AVX2
#endif

#ifdef TYR_USE_AVX2
#include <immintrin.h>
#else
#error "AVX2 not supported on this platform yet"
#endif

namespace tyr
{
#ifdef TYR_USE_AVX2
    // Type aliases for AVX2 registers
    using RegI = __m256i;   // Integer SIMD register (8 x int32)
    using Reg = __m256;     // Float SIMD register (8 x float)
    using RegD = __m256d;   // Double SIMD register (4 x double)

#endif

    class SIMD
    {
    public:
#ifdef TYR_USE_AVX2
        // --- Integer operations ---

        static inline RegI LoadI(const int* ptr)
        {
            return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
        }

        static inline void StoreI(int* ptr, RegI v)
        {
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(ptr), v);
        }

        static inline RegI AddI(RegI a, RegI b)
        {
            return _mm256_add_epi32(a, b);
        }

        static inline RegI SubI(RegI a, RegI b)
        {
            return _mm256_sub_epi32(a, b);
        }

        static inline RegI MulI(RegI a, RegI b)
        {
            return _mm256_mullo_epi32(a, b);
        }

        static inline RegI AndI(RegI a, RegI b)
        {
            return _mm256_and_si256(a, b);
        }

        static inline RegI OrI(RegI a, RegI b)
        {
            return _mm256_or_si256(a, b);
        }

        static inline RegI XorI(RegI a, RegI b)
        {
            return _mm256_xor_si256(a, b);
        }

        static inline RegI SetI(int v)
        {
            return _mm256_set1_epi32(v);
        }

        // --- Float operations ---

        static inline Reg Load(const float* ptr)
        {
            return _mm256_loadu_ps(ptr);
        }

        static inline void Store(float* ptr, Reg v)
        {
            _mm256_storeu_ps(ptr, v);
        }

        static inline Reg Add(Reg a, Reg b)
        {
            return _mm256_add_ps(a, b);
        }

        static inline Reg Sub(Reg a, Reg b)
        {
            return _mm256_sub_ps(a, b);
        }

        static inline Reg Mul(Reg a, Reg b)
        {
            return _mm256_mul_ps(a, b);
        }

        static inline Reg Div(Reg a, Reg b)
        {
            return _mm256_div_ps(a, b);
        }

        static inline Reg Set(float v)
        {
            return _mm256_set1_ps(v);
        }

        static inline Reg Sqrt(Reg a)
        {
            return _mm256_sqrt_ps(a);
        }

        static inline Reg Rcp(Reg a)
        {
            return _mm256_rcp_ps(a);
        }

        // --- Double operations ---

        static inline RegD LoadD(const double* ptr)
        {
            return _mm256_loadu_pd(ptr);
        }

        static inline void StoreD(double* ptr, RegD v)
        {
            _mm256_storeu_pd(ptr, v);
        }

        static inline RegD AddD(RegD a, RegD b)
        {
            return _mm256_add_pd(a, b);
        }

        static inline RegD SubD(RegD a, RegD b)
        {
            return _mm256_sub_pd(a, b);
        }

        static inline RegD MulD(RegD a, RegD b)
        {
            return _mm256_mul_pd(a, b);
        }

        static inline RegD DivD(RegD a, RegD b)
        {
            return _mm256_div_pd(a, b);
        }

        static inline RegD SetD(double v)
        {
            return _mm256_set1_pd(v);
        }

        static inline RegD SqrtD(RegD a)
        {
            return _mm256_sqrt_pd(a);
        }
    };
#endif
} 
