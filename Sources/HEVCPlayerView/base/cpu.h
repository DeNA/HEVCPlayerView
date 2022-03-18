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

#ifndef BASE_CPU_H_
#define BASE_CPU_H_

#include <stdint.h>
#include "../base/endian.h"
#include "../base/intrin.h"

/**
 * The class that encapsulates CPU-specific operations.
 */
struct CPU {
  /**
   * Initializes static variables used by this class.
   * @static
   */
  static void Initialize();

#if __i386__ || __x86_64__
  /**
   * Returns whether or not the host CPU supports SSE2, SSE, and MMX.
   * @return {int}
   */
  static int HaveSSE2() {
    return 1;
  }

  /**
   * Returns whether or not the host CPU supports SSE4.
   * @return {int}
   */
  static int HaveSSE4() {
#if __ANDROID__
    return 1;
#else
    return sse4_;
#endif
  }

  /**
   * Returns whether or not the host CPU supports AVX2 (and FMA3).
   * @return {int}
   */
  static int HaveAVX2() {
    return avx2_;
  }
#elif __arm__ || __aarch64__
  /**
   * Returns whether or not the host CPU supports NEON.
   * @return {int}
   */
  static int HaveNEON() {
#if __aarch64__
    return 1;
#else
    return neon_;
#endif
  }
#endif

  /**
   * Reads an unsigned 16-bit integer from the specified location in the CPU byte
   * order.
   * @param {const void*} p
   * @return {uint16_t}
   */
  static uint16_t LoadUINT16(const void* p) {
#if __i386__ || __x86_64__ || __arm__ || __aarch64__
    return static_cast<const uint16_t*>(p)[0];
#else
    uint16_t n;
    memcpy(&n, p, sizeof(uint16_t));
    return n;
#endif
  }

  /**
   * Reads an unsigned 16-bit integer from the specified location in the
   * least-significant-byte first order  (a.k.a. little endian).
   * @param {const void*} p
   * @return {uint16_t}
   */
  static uint16_t LoadUINT16LE(const void* p) {
#if _BYTE_ORDER == _LITTLE_ENDIAN
    return LoadUINT16(p);
#else
    return __builtin_bswap16(LoadUINT16(p));
#endif
  }

  /**
   * Reads an unsigned 16-bit integer from the specified location in the
   * most-significant-byte first order (a.k.a. big endian).
   * @param {const void*} p
   * @return {uint16_t}
   */
  static uint16_t LoadUINT16BE(const void* p) {
#if _BYTE_ORDER == _BIG_ENDIAN
    return LoadUINT16(p);
#else
    return __builtin_bswap16(LoadUINT16(p));
#endif
  }

  /**
   * Writes an unsigned 16-bit integer to the specified location (which may be
   * unaligned to a 2-byte boundary) in the CPU byte order.
   * @param {void*} p
   * @param {uint16_t} n
   */
  static void StoreUINT16(void* p, uint16_t n) {
#if __i386__ || __x86_64__ || __arm__ || __aarch64__
    static_cast<uint16_t*>(p)[0] = n;
#else
    memcpy(p, &n, sizeof(uint16_t));
#endif
  }

  /**
   * Writes an unsigned 16-bit integer to the specified location in the
   * least-significant-byte first order  (a.k.a. little endian).
   * @param {void*} p
   * @param {uint16_t} n
   */
  static void StoreUINT16LE(void* p, uint16_t n) {
#if _BYTE_ORDER == _LITTLE_ENDIAN
    StoreUINT16(p, n);
#else
    StoreUINT16(p, __builtin_bswap16(n));
#endif
  }

  /**
   * Writes an unsigned 16-bit integer to the specified location in the
   * least-significant-byte first order  (a.k.a. little endian).
   * @param {void*} p
   * @param {uint16_t} n
   */
  static void StoreUINT16BE(void* p, uint16_t n) {
#if _BYTE_ORDER == _BIG_ENDIAN
    StoreUINT16(p, n);
#else
    StoreUINT16(p, __builtin_bswap16(n));
#endif
  }

  /**
   * Reads an unsigned 32-bit integer from the specified location (which may be
   * unaligned to a 4-byte boundary) in the CPU byte order.
   * @param {const void*} p
   * @return {uint32_t}
   */
  static uint32_t LoadUINT32(const void* p) {
#if __i386__ || __x86_64__ || __arm__ || __aarch64__
    return static_cast<const uint32_t*>(p)[0];
#else
    uint32_t n;
    memcpy(&n, p, sizeof(uint32_t));
    return n;
#endif
  }

  /**
   * Reads an unsigned 32-bit integer from the specified location in the
   * least-significant-byte first order  (a.k.a. little endian).
   * @param {const void*} p
   * @return {uint32_t}
   */
  static uint32_t LoadUINT32LE(const void* p) {
#if _BYTE_ORDER == _LITTLE_ENDIAN
    return LoadUINT32(p);
#else
    return __builtin_bswap32(LoadUINT32(p));
#endif
  }

  /**
   * Reads an unsigned 32-bit integer from the specified location in the
   * most-significant-byte first order (a.k.a. big endian).
   * @param {const void*} p
   * @return {uint32_t}
   */
  static uint32_t LoadUINT32BE(const void* p) {
#if _BYTE_ORDER == _BIG_ENDIAN
    return LoadUINT32(p);
#else
    return __builtin_bswap32(LoadUINT32(p));
#endif
  }

  /**
   * Writes an unsigned 32-bit integer to the specified location (which may be
   * unaligned to a 4-byte boundary) in the CPU byte order.
   * @param {void*} p
   * @param {uint32_t} n
   */
  static void StoreUINT32(void* p, uint32_t n) {
#if __i386__ || __x86_64__ || __arm__ || __aarch64__
    static_cast<uint32_t*>(p)[0] = n;
#else
    memcpy(p, &n, sizeof(uint32_t));
#endif
  }

  /**
   * Writes an unsigned 32-bit integer to the specified location in the
   * least-significant-byte first order  (a.k.a. little endian).
   * @param {void*} p
   * @param {uint32_t} n
   */
  static void StoreUINT32LE(void* p, uint32_t n) {
#if _BYTE_ORDER == _LITTLE_ENDIAN
    StoreUINT32(p, n);
#else
    StoreUINT32(p, __builtin_bswap32(n));
#endif
  }

  /**
   * Writes an unsigned 32-bit integer to the specified location in the
   * most-significant-byte first order  (a.k.a. big endian).
   * @param {void*} p
   * @param {uint32_t} n
   */
  static void StoreUINT32BE(void* p, uint32_t n) {
#if _BYTE_ORDER == _BIG_ENDIAN
    StoreUINT32(p, n);
#else
    StoreUINT32(p, __builtin_bswap32(n));
#endif
  }

  /**
   * Extracts continuous bits from an unsigned 32-bit integer.
   * @param {uint32_t} n
   * @param {uint32_t} start
   * @param {uint32_t} length
   */
  static uint32_t BitExtractUINT32(uint32_t n,
                                   uint32_t start,
                                   uint32_t length) {
#if __haswell__
    return _bextr_u32(n, start, length);
#else
    return (n >> start) & ((1 << length) - 1);
#endif
  }

#if __i386__ || __x86_64__
  /**
   * Counts the number of set bits in a variable.
   * @param {uint32_t} p
   * @return {uint32_t}
   */
  static uint32_t PopCountUINT32(uint32_t p) {
#if __ANDROID__
    // Use inline assembly on Android, where clang prohibits using SSE4
    // intrinsics in a file built for non-SSE4 CPUs.
    __asm__("popcnt %1, %0" : "=r" (p) : "r" (p));
    return static_cast<uint32_t>(p);
#else
    // Use the `_mm_popcnt_u32()` intrinsic on Visual C++ and on Intel C++.
    return static_cast<uint32_t>(_mm_popcnt_u32(p));
#endif
  }
#endif

  /**
   * Counts the number of set bits in a variable.
   * @param {uint32_t} p
   * @return {uint32_t}
   */
  static uint32_t BitCountUINT32(uint32_t p) {
#if __i386__ || __x86_64__
    // Use the POPCNT instruction on Intel CPUs if possible.
    if (HaveSSE4()) {
      return PopCountUINT32(p);
    }
#endif
    // Count the number of set bits in parallel as written in
    // [Bit Twiddling Hacks](http://graphics.stanford.edu/~seander/bithacks.html)
    p = (p & 0x55555555) + ((p >> 1) & 0x55555555);
    p = (p & 0x33333333) + ((p >> 2) & 0x33333333);
    p = (p & 0x0f0f0f0f) + ((p >> 4) & 0x0f0f0f0f);
    p = (p * 0x01010101) >> 24;
    return p;
  }

  /**
   * Sets the specified bit of a variable.
   * @param {uint32_t*} p
   * @param {uint32_t} bit
   */
  static void BitSet(uint32_t* p, uint32_t bit) {
#if _MSC_VER
    // Use the `_bittestandset()` intrinsic on Visual C++, which provides the
    // intrinsic on all CPUs. (Visual C++ optimizes this intrinsic pretty well.)
    _bittestandset(reinterpret_cast<long*>(p), bit);
#elif __i386__ || __x86_64__
    // Use the inline assembly on GCC or on clang, which do not have the
    // `_bittestandset()` intrinsic. This function must use either the
    // `BTS reg, reg` instruction or the `BTS reg, imm` one. (The `BTS mem, reg`
    // instruction is VERY SLOW.)
    __asm__("btsl %1, %0" : "+r" (*p) : "ri" (bit));
#else
    // Use C++. (This is the last resort.)
    * p |= 1 << bit;
#endif
  }

  /**
   * Clears the specified bit of a variable.
   * @param {uint32_t*} p
   * @param {uint32_t} bit
   */
  static void BitClear(uint32_t* p, uint32_t bit) {
#if _MSC_VER
    _bittestandreset(reinterpret_cast<long*>(p), bit);
#elif __i386__ || __x86_64__
    __asm__("btrl %1, %0" : "+r" (*p) : "ri" (bit));
#else
    * p &= ~(1 << bit);
#endif
  }

  /**
   * Returns a bit-mask of the specified length.
   * @param {uint32_t} length
   */
  static uint32_t BitMask(uint32_t length) {
    return static_cast<uint32_t>(~(~0UL << length));
  }

#if __SIZEOF_SIZE_T__ == 8
  /**
   * Reads an unsigned 64-bit integer from the specified location (which may be
   * unaligned to a 8-byte boundary) in the CPU byte order.
   * @param {const void*} p
   * @return {uint64_t}
   */
  static uint64_t LoadUINT64(const void* p) {
#if __x86_64__ || __aarch64__
    return static_cast<const uint64_t*>(p)[0];
#else
    uint64_t n;
    memcpy(&n, p, sizeof(uint64_t));
    return n;
#endif
  }

  /**
   * Reads an unsigned 64-bit integer from the specified location in the
   * least-significant-byte first order  (a.k.a. little endian).
   * @param {const void*} p
   * @return {uint64_t}
   */
  static uint64_t LoadUINT64LE(const void* p) {
#if _BYTE_ORDER == _LITTLE_ENDIAN
    return LoadUINT64(p);
#else
    return __builtin_bswap64(LoadUINT64(p));
#endif
  }

  /**
   * Reads an unsigned 64-bit integer from the specified location in the
   * most-significant-byte first order (a.k.a. big endian).
   * @param {const void*} p
   * @return {uint64_t}
   */
  static uint64_t LoadUINT64BE(const void* p) {
#if _BYTE_ORDER == _BIG_ENDIAN
    return LoadUINT64(p);
#else
    return __builtin_bswap64(LoadUINT64(p));
#endif
  }

  /**
   * Writes an unsigned 64-bit integer to the specified location (which may be
   * unaligned to a 8-byte boundary) in the CPU byte order.
   * @param {void*} p
   * @param {uint64_t} n
   */
  static void StoreUINT64(void* p, uint64_t n) {
#if __x86_64__ || __aarch64__
    static_cast<uint64_t*>(p)[0] = n;
#else
    memcpy(p, &n, sizeof(uint64_t));
#endif
  }

  /**
   * Writes an unsigned 64-bit integer to the specified location in the
   * least-significant-byte first order  (a.k.a. little endian).
   * @param {void*} p
   * @param {uint64_t} n
   */
  static void StoreUINT64LE(void* p, uint64_t n) {
#if _BYTE_ORDER == _LITTLE_ENDIAN
    StoreUINT64(p, n);
#else
    StoreUINT64(p, __builtin_bswap64(n));
#endif
  }

  /**
   * Writes an unsigned 64-bit integer to the specified location in the
   * most-significant-byte first order  (a.k.a. big endian).
   * @param {void*} p
   * @param {uint64_t} n
   */
  static void StoreUINT64BE(void* p, uint64_t n) {
#if _BYTE_ORDER == _BIG_ENDIAN
    StoreUINT64(p, n);
#else
    StoreUINT64(p, __builtin_bswap64(n));
#endif
  }

  /**
   * Extracts continuous bits from an unsigned 64-bit integer.
   * @param {uint64_t} n
   * @param {uint64_t} start
   * @param {uint64_t} length
   */
  static uint64_t BitExtractUINT64(uint64_t n,
                                   uint64_t start,
                                   uint64_t length) {
#if __haswell__
    return _bextr_u64(n, start, length);
#else
    return (n >> start) & ((1ULL << length) - 1ULL);
#endif
  }

#if __x86_64__
  /**
   * Counts the number of set bits in a 64-bit variable.
   * @param {uint64_t} p
   * @return {uint64_t}
   */
  static uint64_t PopCountUINT64(uint64_t p) {
#if __ANDROID__
    // Use inline assembly on Android, where clang prohibits using SSE4
    // intrinsics in a file built for non-SSE4 CPUs.
    __asm__("popcnt %1, %0" : "=r" (p) : "r" (p));
    return static_cast<uint64_t>(p);
#else
    // Use the `_mm_popcnt_u64()` intrinsic on Visual C++ and on Intel C++.
    return static_cast<uint64_t>(_mm_popcnt_u64(p));
#endif
  }
#endif

  /**
   * Counts the number of set bits in a variable.
   * @param {uint64_t} p
   * @return {uint64_t}
   */
  static uint64_t BitCountUINT64(uint64_t p) {
#if __i386__ || __x86_64__
    // Use the POPCNT instruction on Intel CPUs if possible.
    if (HaveSSE4()) {
      return PopCountUINT64(p);
    }
#endif
    // Count the number of set bits in parallel as written in
    // [Bit Twiddling Hacks](http://graphics.stanford.edu/~seander/bithacks.html)
    p = (p & 0x5555555555555555ULL) + ((p >> 1) & 0x5555555555555555ULL);
    p = (p & 0x3333333333333333ULL) + ((p >> 2) & 0x3333333333333333ULL);
    p = (p & 0x0f0f0f0f0f0f0f0fULL) + ((p >> 4) & 0x0f0f0f0f0f0f0f0fULL);
    p = (p * 0x0101010101010101ULL) >> 56;
    return p;
  }

  /**
   * Sets the specified bit of a variable.
   * @param {uint64_t*} p
   * @param {uint64_t} bit
   */
  static void BitSetUINT64(uint64_t* p, uint64_t bit) {
#if _MSC_VER
    _bittestandset64(reinterpret_cast<__int64*>(p), bit);
#elif __i386__ || __x86_64__
    __asm__("btsq %1, %0" : "+r" (*p) : "ri" (bit));
#else
    *p |= 1ULL << bit;
#endif
  }

  /**
   * Clears the specified bit of a variable.
   * @param {uint64_t*} p
   * @param {uint64_t} bit
   */
  static void BitClearUINT64(uint64_t* p, uint64_t bit) {
#if _MSC_VER
    _bittestandreset64(reinterpret_cast<__int64*>(p), bit);
#elif __i386__ || __x86_64__
    __asm__("btrq %1, %0" : "+r" (*p) : "ri" (bit));
#else
    *p &= ~(1ULL << bit);
#endif
  }

  /**
   * Returns a bit-mask of the specified length.
   * @param {uint64_t} length
   */
  static uint64_t BitMaskUINT64(uint64_t length) {
    return ~(~0ULL << length);
  }
#endif

  /**
   * Reads an unsigned pointer from the specified location (which may be
   * unaligned to a 8-byte boundary) in the CPU byte order.
   * @param {const void*} p
   * @return {uintptr_t}
   */
  static uintptr_t LoadUPTR(const void* p) {
#if __SIZEOF_SIZE_T__ == 8
    return LoadUINT64(p);
#else
    return LoadUINT32(p);
#endif
  }

  /**
   * Reads an unsigned pointer from the specified location in the
   * least-significant-byte first order  (a.k.a. little endian).
   * @param {const void*} p
   * @return {uintptr_t}
   */
  static uintptr_t LoadUPTRLE(const void* p) {
#if __SIZEOF_SIZE_T__ == 8
    return LoadUINT64LE(p);
#else
    return LoadUINT32LE(p);
#endif
  }

  /**
   * Reads an unsigned pointer from the specified location in the
   * most-significant-byte first order (a.k.a. big endian).
   * @param {const void*} p
   * @return {uintptr_t}
   */
  static uint64_t LoadUPTRBE(const void* p) {
#if __SIZEOF_SIZE_T__ == 8
    return LoadUINT64BE(p);
#else
    return LoadUINT32BE(p);
#endif
  }

  /**
   * Writes an unsigned pointer to the specified location (which may be
   * unaligned to a 8-byte boundary) in the CPU byte order.
   * @param {void*} p
   * @param {uintptr_t} n
   */
  static void StoreUPTR(void* p, uintptr_t n) {
#if __SIZEOF_SIZE_T__ == 8
    StoreUINT64(p, n);
#else
    StoreUINT32(p, n);
#endif
  }

  /**
   * Writes an unsigned pointer to the specified location in the
   * least-significant-byte first order  (a.k.a. little endian).
   * @param {void*} p
   * @param {uintptr_t} n
   */
  static void StoreUPTRLE(void* p, uintptr_t n) {
#if __SIZEOF_SIZE_T__ == 8
    StoreUINT64LE(p, n);
#else
    StoreUINT32LE(p, n);
#endif
  }

  /**
   * Writes an unsigned pointer to the specified location in the
   * most-significant-byte first order  (a.k.a. big endian).
   * @param {void*} p
   * @param {uintptr_t} n
   */
  static void StoreUPTRBE(void* p, uintptr_t n) {
#if __SIZEOF_SIZE_T__ == 8
    StoreUINT64BE(p, n);
#else
    StoreUINT32BE(p, n);
#endif
  }

  /**
   * Sets the specified bit of a variable.
   * @param {uintptr_t*} p
   * @param {uintptr_t} bit
   */
  static void BitSetUPTR(uintptr_t* p, uintptr_t bit) {
#if __SIZEOF_SIZE_T__ == 8
    BitSetUINT64(reinterpret_cast<uint64_t*>(p), bit);
#else
    BitSet(p, bit);
#endif
  }

  /**
   * Clears the specified bit of a variable.
   * @param {uintptr_t*} p
   * @param {uintptr_t} bit
   */
  static void BitClearUPTR(uintptr_t* p, uintptr_t bit) {
#if __SIZEOF_SIZE_T__ == 8
    BitClearUINT64(reinterpret_cast<uint64_t*>(p), bit);
#else
    BitClear(p, bit);
#endif
  }

  /**
   * Returns a bit-mask of the specified length.
   * @param {uintptr_t} length
   * @return {uintptr_t}
   */
  static uintptr_t BitMaskUPTR(uintptr_t length) {
#if __SIZEOF_SIZE_T__ == 8
    return BitMaskUINT64(length);
#else
    return BitMask(length);
#endif
  }

#if __i386__ || __x86_64__
  /**
   * Whether the host CPU supports SSE4, SSSE3, SSE3, SSE2, SSE, and MMX.
   * @type {uint8_t}
   */
  static uint8_t sse4_;

  /**
   * Whether the host CPU supports AVX2.
   * @type {uint8_t}
   */
  static uint8_t avx2_;
#elif __arm__
  /**
   * Whether the host CPU supports NEON
   * @type {uint8_t}
   */
  static uint8_t neon_;
#endif
};

#endif // BASE_CPU_H_
