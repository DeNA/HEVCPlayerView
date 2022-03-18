/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 DeNA Co., Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef BASE_INTRIN_H_
#define BASE_INTRIN_H_

#if _MSC_VER
#include <intrin.h>
#elif __i386__ || __x86_64__
#include <immintrin.h>
#elif __arm__ || __aarch64__
#include <arm_neon.h>
#endif

// Emulate some GNU built-in functions on Visual C++.
#if _MSC_VER
/**
 * Whether or not the compilation target is IA-32 CPUs.
 * @define {int}
 */
#if _M_IX86
#ifndef __i386__
#define __i386__ 1
#endif
#endif

/**
 * Whether or not the compilation target is x64 CPUs.
 * @define {int}
 */
#if _M_X64
#ifndef __x86_64__
#define __x86_64__ 1
#endif
#endif

/**
 * Whether or not the compilation target is AArch64 CPUs.
 * @define {int}
 */
#if _M_ARM64
#ifndef __aarch64__
#define __aarch64__ 1
#endif
#endif

/**
 * Whether or not the compilation target is AArch32 CPUs.
 * @define {int}
 */
#if _M_ARM
#ifndef __arm__
#define __arm__ 1
#endif
#endif

/**
 * The number of bytes of the `size_t` type.
 * @define {int}
 */
#ifndef __SIZEOF_SIZE_T__
#if _M_X64 || _M_ARM64
#define __SIZEOF_SIZE_T__ 8
#else
#define __SIZEOF_SIZE_T__ 4
#endif
#endif

/**
 * The number of bytes of the largest register.
 * @define {int}
 */
#ifndef __BIGGEST_ALIGNMENT__
#if _M_IX86 || _M_X64
#define __BIGGEST_ALIGNMENT__ 32
#else
#define __BIGGEST_ALIGNMENT__ 16
#endif
#endif

#if _M_IX86 || _M_X64
/**
 * Whether or not the compilation target is Haswell (fourth-generation Intel
 * Core) CPUs or newer.
 * @define {int}
 */
#ifndef __haswell__
#define __haswell__ 0
#endif
 
/**
 * Whether or not the host compiler allows using SSE2 intrinsics.
 * @define {int}
 */
#ifndef __SSE__
#define __SSE__ 1
#endif

/**
 * Whether or not the host compiler allows using SSE2 intrinsics.
 * @define {int}
 */
#ifndef __SSE2__
#define __SSE2__ 1
#endif

/**
 * Whether or not the host compiler allows using SSE3 intrinsics.
 * @define {int}
 */
#ifndef __SSE3__
#define __SSE3__ 1
#endif

/**
 * Whether or not the host compiler allows using SSSE3 intrinsics.
 * @define {int}
 */
#ifndef __SSSE3__
#define __SSSE3__ 1
#endif

/**
 * Whether or not the host compiler allows using SSE4.2 intrinsics.
 * @define {int}
 */
#ifndef __SSE4_1__
#define __SSE4_1__ 1
#endif

/**
 * Whether or not the host compiler allows using SSE4.2 intrinsics.
 * @define {int}
 */
#ifndef __SSE4_2__
#define __SSE4_2__ 1
#endif

/**
 * Whether or not the host compiler allows using POPCNT intrinsics.
 * @define {int}
 */
#ifndef __LZCNT__
#define __LZCNT__ 1
#endif

/**
 * Whether or not the host compiler allows using POPCNT intrinsics.
 * @define {int}
 */
#ifndef __POPCNT__
#define __POPCNT__ 1
#endif

/**
 * Whether or not the host compiler allows using AVX intrinsics.
 * @define {int}
 */
#ifndef __AVX__
#define __AVX__ 1
#endif

/**
 * Whether or not the host compiler allows using AVX2 intrinsics.
 * @define {int}
 */
#ifndef __AVX2__
#define __AVX2__ 1
#endif

/**
 * Whether or not the host compiler allows using AESNI intrinsics.
 * @define {int}
 */
#ifndef __AES__
#define __AES__ 1
#endif

/**
 * Whether or not the host compiler allows using BMI intrinsics.
 * @define {int}
 */
#ifndef __BMI__
#define __BMI__ 1
#endif
  
/**
 * Whether or not the host compiler allows using BMI intrinsics.
 * @define {int}
 */
#ifndef __BMI2__
#define __BMI2__ 1
#endif

/**
 * Whether or not the host compiler allows using FMA3 intrinsics.
 * @define {int}
 */
#ifndef __FMA3__
#define __FMA3__ 1
#endif
#endif

/**
 * Swaps the byte order of a 16-bit unsigned integer.
 * @param {uint16_t} n
 * @return {uint16_t}
 */
__forceinline uint16_t __builtin_bswap16(uint16_t n) {
  return _byteswap_ushort(n);
}

/**
 * Swaps the byte order of a 32-bit unsigned integer.
 * @param {uint32_t} n
 * @return {uint32_t}
 */
__forceinline uint32_t __builtin_bswap32(uint32_t n) {
  return _byteswap_ulong(n);
}

#if __SIZEOF_SIZE_T__ == 8
/**
 * Swaps the byte order of a 64-bit unsigned integer.
 * @param {uint64_t} n
 * @return {uint64_t}
 */
__forceinline uint64_t __builtin_bswap64(uint64_t n) {
  return _byteswap_uint64(n);
}
#endif

/**
 * Returns the number of leading zeros.
 * @param {uint32_t} mask
 * @return {unsigned int}
 */
__forceinline unsigned int __builtin_clz(uint32_t mask) {
  unsigned long bits;
  _BitScanReverse(&bits, mask);
  return static_cast<unsigned int>(31 - bits);
}

/**
 * Returns the number of trailing zeros.
 * @param {uint32_t} mask
 * @return {unsigned int}
 */
__forceinline unsigned int __builtin_ctz(uint32_t mask) {
  unsigned long bits;
  _BitScanForward(&bits, mask);
  return static_cast<unsigned int>(bits);
}

#if __SIZEOF_SIZE_T__ == 8
/**
 * Returns the number of leading zeros.
 * @param {uint64_t} mask
 * @return {unsigned int}
 */
__forceinline unsigned int __builtin_clzll(uint64_t mask) {
  unsigned long bits;
  _BitScanReverse64(&bits, mask);
  return static_cast<unsigned int>(63 - bits);
}

/**
 * Returns the number of trailing zeros.
 * @param {uint64_t} mask
 * @return {unsigned int}
 */
__forceinline unsigned int __builtin_ctzll(uint64_t mask) {
  unsigned long bits;
  _BitScanForward64(&bits, mask);
  return static_cast<unsigned int>(bits);
}
#endif
#endif

#if __SSE2__
inline void _mm_storeu_epu8(uint8_t* p, __m128i a) {
  _mm_storeu_si128(reinterpret_cast<__m128i*>(p), a);
}
#endif

#if __AVX2__
inline __m256i _mm256_load_epi8(const int8_t* p) {
  return _mm256_load_si256(reinterpret_cast<const __m256i*>(p));
}

inline void _mm256_store_epi8(int8_t* p, __m256i a) {
  _mm256_store_si256(reinterpret_cast<__m256i*>(p), a);
}

inline __m256i _mm256_load_epu8(const uint8_t* p) {
  return _mm256_load_si256(reinterpret_cast<const __m256i*>(p));
}

inline void _mm256_store_epu8(uint8_t* p, __m256i a) {
  _mm256_store_si256(reinterpret_cast<__m256i*>(p), a);
}

inline __m256i _mm256_load_epi16(const int16_t* p) {
  return _mm256_load_si256(reinterpret_cast<const __m256i*>(p));
}

inline void _mm256_store_epi16(int16_t* p, __m256i a) {
  _mm256_store_si256(reinterpret_cast<__m256i*>(p), a);
}

inline __m256i _mm256_load_epu16(const uint16_t* p) {
  return _mm256_load_si256(reinterpret_cast<const __m256i*>(p));
}

inline void _mm256_store_epu16(uint16_t* p, __m256i a) {
  _mm256_store_si256(reinterpret_cast<__m256i*>(p), a);
}

inline __m256i _mm256_load_epi32(const int32_t* p) {
  return _mm256_load_si256(reinterpret_cast<const __m256i*>(p));
}

inline void _mm256_store_epi32(int32_t* p, __m256i a) {
  _mm256_store_si256(reinterpret_cast<__m256i*>(p), a);
}

inline __m256i _mm256_load_epu32(const uint32_t* p) {
  return _mm256_load_si256(reinterpret_cast<const __m256i*>(p));
}

inline void _mm256_store_epu32(uint32_t* p, __m256i a) {
  _mm256_store_si256(reinterpret_cast<__m256i*>(p), a);
}

inline __m256i _mm256_loadu_epi8(const int8_t* p) {
  return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
}

inline void _mm256_storeu_epi8(int8_t* p, __m256i a) {
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), a);
}

inline __m256i _mm256_loadu_epu8(const uint8_t* p) {
  return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
}

inline void _mm256_storeu_epu8(uint8_t* p, __m256i a) {
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), a);
}

inline __m256i _mm256_set2_epi16(int16_t a, int16_t b) {
  return _mm256_set_epi16(a, b, a, b, a, b, a, b, a, b, a, b, a, b, a, b);
}

inline uintptr_t _mm256_movemask_epu8(__m256i a) {
  return (uintptr_t)(unsigned int)_mm256_movemask_epi8(a);
}
#endif

#endif  // BASE_INTRIN_H_
