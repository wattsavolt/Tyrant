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
    using Reg128I = __m128i;   // Integer SIMD register (4 x int32)
    using Reg128 = __m128;     // Float SIMD register (4 x float)
    using Reg128D = __m128d;   // Double SIMD register (2 x double)
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

        // Copies 4 floats into both halves, such as one matrix row for two rows at once.
        static inline Reg Broadcast4(const float* ptr)
        {
            return _mm256_broadcast_ps(reinterpret_cast<const __m128*>(ptr));
        }

        // Fills each 4-float half with that half's own element at Lane.
        template<int Lane>
        static inline Reg SplatHalves(Reg v)
        {
            return _mm256_shuffle_ps(v, v, _MM_SHUFFLE(Lane, Lane, Lane, Lane));
        }

        // --- 4-wide float operations, such as one matrix row, a quaternion or a Vector4 ---

        static inline Reg128 Load4(const float* ptr)
        {
            return _mm_loadu_ps(ptr);
        }

        static inline void Store4(float* ptr, Reg128 v)
        {
            _mm_storeu_ps(ptr, v);
        }

        static inline Reg128 Set4(float v)
        {
            return _mm_set1_ps(v);
        }

        // In lane order, x first.
        static inline Reg128 Set4(float x, float y, float z, float w)
        {
            return _mm_setr_ps(x, y, z, w);
        }

        static inline Reg128 Add4(Reg128 a, Reg128 b)
        {
            return _mm_add_ps(a, b);
        }

        static inline Reg128 Sub4(Reg128 a, Reg128 b)
        {
            return _mm_sub_ps(a, b);
        }

        static inline Reg128 Mul4(Reg128 a, Reg128 b)
        {
            return _mm_mul_ps(a, b);
        }

        // Every lane set to lane Lane of v.
        template<int Lane>
        static inline Reg128 Splat4(Reg128 v)
        {
            return _mm_shuffle_ps(v, v, _MM_SHUFFLE(Lane, Lane, Lane, Lane));
        }

        // Lanes picked from v in output order, so Shuffle4<3, 2, 1, 0> reverses them.
        template<int X, int Y, int Z, int W>
        static inline Reg128 Shuffle4(Reg128 v)
        {
            return _mm_shuffle_ps(v, v, _MM_SHUFFLE(W, Z, Y, X));
        }

        static inline float Dot4(Reg128 a, Reg128 b)
        {
            return _mm_cvtss_f32(_mm_dp_ps(a, b, 0xFF));
        }

        // Swaps rows and columns of the 4x4 matrix held in the four registers.
        static inline void Transpose4(Reg128& row0, Reg128& row1, Reg128& row2, Reg128& row3)
        {
            _MM_TRANSPOSE4_PS(row0, row1, row2, row3);
        }

        static inline Reg128I FloatToHalf(Reg f)
        {
            return _mm256_cvtps_ph(f, 0); // F16C intrinsic, returns 128-bit packed half
        }

        static inline uint ExtractLowI(Reg128I v)
        {
            return static_cast<uint>(_mm_cvtsi128_si32(v));
        }

    };
#endif
} 
