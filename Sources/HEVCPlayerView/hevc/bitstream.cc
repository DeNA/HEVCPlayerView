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

#include "../hevc/bitstream.h"

namespace hevc {

uintptr_t BitStreamReader::ReadGolomb() {
  // Peek 32 bits from the bit-stream cache and decode a 16-bit Exp-Golomb code
  // in it. (`0000000000000001111111111111111` (65534) is the longest a 16-bit
  // Exp-Golomb code, i.e. the length of the longest 16-bit Exp-Golomb code is
  // `15 + 16 = 31` bits.)
#if __SIZEOF_SIZE_T__ == 8
  // Fill the cache so it contains at least 32 bits of data, the length of the
  // longest 16-bit Exp-Golomb code.
  if (length_ < 32) {
    FillCache();
  }
  uintptr_t scan = cache_ << (64 - length_);
  uintptr_t code_length = __builtin_clzll(scan) * 2 + 1;
  HEVC_ASSERT(code_length <= 32);
  length_ -= code_length;
  uintptr_t code = cache_ >> length_;
  cache_ ^= code << length_;
  return code - 1;
#else
  if (length_ == 32) {
    uintptr_t code_length = __builtin_clz(cache_) * 2 + 1;
    length_ -= code_length;
    uintptr_t code = cache_ >> length_;
    cache_ ^= code << length_;
    return code - 1;
  }
  // This bit-stream does not have 32-bits of data in its cache. Read the next
  // 32 bits of data from this bit-stream and create a 32-bit word.
  uintptr_t next = CPU::LoadUINT32BE(top_);
  uintptr_t scan = (cache_ << (32 - length_)) | (next >> length_);

  // Retrieve an Exp-Golomb code from the above 32-bit word and return it.
  uintptr_t code_length = __builtin_clz(scan) * 2 + 1;
  HEVC_ASSERT(code_length <= 32);
  uintptr_t code = scan >> (32 - code_length);
  if (code_length > length_) {
    code_length -= length_;
    cache_ = next;
    length_ = 32;
    top_ += 4;
  }
  length_ -= code_length;
  cache_ ^= code << length_;
  return code - 1;
#endif
}

}  // namespace hevc
