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

#ifndef HEVC_BITSTREAM_H_
#define HEVC_BITSTREAM_H_

#include <stdint.h>
#include "../base/cpu.h"
#include "../base/intrin.h"
#include "../base/log.h"

namespace hevc {

/**
 * The class that reads data from a bit stream.
 */
struct BitStreamReader {
  enum {
    CACHE_BITS = __SIZEOF_SIZE_T__ * 8,
    CACHE_BITS2 = CACHE_BITS / 2,
  };

  /**
   * Initializes this bit-stream reader.
   * @param {const uint8_t*} top
   * @param {const uint8_t*} end
   */
  void Initialize(const uint8_t* top, const uint8_t* end) {
    top_ = top;
    end_ = end;
    LoadCache();
  }

  /**
   * Reads one bit from this bit-stream reader.
   * @return {T}
   */
  template <typename T>
  T GetBit() {
    if (length_ == 0) {
      LoadCache();
    }
    --length_;
    uintptr_t code = cache_ >> length_;
    cache_ ^= code << length_;
    return static_cast<T>(code);
  }

  /**
   * Reads the specified number of bits from this bit-stream reader.
   * @param {uintptr_t} length
   * @return {T}
   */
  template <typename T>
  T GetBits(uintptr_t length) {
    HEVC_ASSERT(length <= 16);
    if (length_ < length) {
      FillCache();
    }
    length_ -= length;
    uintptr_t code = cache_ >> length_;
    cache_ ^= code << length_;
    return static_cast<T>(code);
  }

  /**
   * Reads the specified number of bits from this bit-stream reader.
   * @param {uintptr_t} length
   * @return {T}
   */
  template <typename T>
  T GetBitsLong(uintptr_t length) {
    HEVC_ASSERT(length <= 32);
#if __SIZEOF_SIZE_T__ == 8
    if (length_ < length) {
      FillCache();
    }
    length_ -= length;
    uintptr_t code = cache_ >> length_;
    cache_ ^= code << length_;
    return static_cast<T>(code);
#else
    uint32_t code = 0;
    if (length > length_) {
      length -= length_;
      code = cache_ << length;
      cache_ = CPU::LoadUINT32BE(top_);
      length_ = 32;
    }
    length_ -= length;
    code |= cache >> length_;
    cache_ ^= code << length_;
    return static_cast<T>(code);
#endif
  }

  /**
   * Reads an unsigned Exp-Golomb code in the range 0 to 65534. An Exp-Golomb
   * code is a variable-length code listed in the following table.
   *  +-------+---------------------------------+
   *  | value | Exp-Golomb code                 |
   *  +-------+---------------------------------+
   *  | 0     | 1                               |
   *  | 1     | 010                             |
   *  | 2     | 011                             |
   *  | 3     | 00100                           |
   *    ...
   *  | 65534 | 0000000000000001111111111111111 |
   *  +-------+---------------------------------+
   * @return {T}
   */
  template <typename T>
  T GetGolomb() {
    return static_cast<T>(ReadGolomb());
  }

  /**
   * Skips an unsigned Exp-Golomb code.
   */
  void SkipGolomb() {
    ReadGolomb();
  }

  /**
   * Moves the read position of this bit-stream reader.
   * @param {uintptr_t} length
   */
  void SkipBits(uintptr_t length) {
    if (length >= length_) {
      length -= length_;
      top_ += length >> 3;
      length &= 0x07;
      LoadCache();
    }
    length_ -= length;
    cache_ ^= (cache_ >> length_) << length_;
  }

  /**
   * Moves the read position to the next byte boundary.
   */
  void SkipToByteBoundary() {
    length_ &= ~7;
    cache_ ^= (cache_ >> length_) << length_;
  }

  /**
   * Loads data to the cache of this bit-stream reader.
   * @private
   */
  void LoadCache() {
#if __SIZEOF_SIZE_T__ == 8
    cache_ = CPU::LoadUINT64BE(top_);
#else
    cache_ = CPU::LoadUINT32BE(top_);
#endif
    top_ += CACHE_BITS / 8;
    length_ = CACHE_BITS;
  }

  /**
   * Fills the cache of this bit-stream reader.
   * @private
   */
  void FillCache() {
    cache_ <<= CACHE_BITS2;
#if __SIZEOF_SIZE_T__ == 8
    cache_ |= CPU::LoadUINT32BE(top_);
#else
    cache_ |= CPU::LoadUINT16BE(top_);
#endif
    top_ += CACHE_BITS2 / 8;
    length_ += CACHE_BITS2;
  }

  /**
   * Reads an unsigned Exp-Golomb code in the range 0 to 65534.
   * @return {uintptr_t}
   * @private
   */
  uintptr_t ReadGolomb();

  /**
   * The current position of the input data.
   * @type {const uint8_t*}
   * @private
   */
  const uint8_t* top_;

  /**
   * The end of the input data.
   * @type {const uint8_t*}
   * @private
   */
  const uint8_t* end_;

  /**
   * The cached data.
   * @type {uintptr_t}
   * @private
   */
  uintptr_t cache_;

  /**
   * The number of bits in the cache.
   * @type {uintptr_t}
   * @private
   */
  uintptr_t length_;
};

}  // namespace hevc

#endif  // HEVC_BITSTREAM_H_
