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

#include "../mov/atom.h"

namespace mov {

int FileTypeAtom::IsValid() const {
  const uint32_t size = base_.GetSize();
#if 1
  const FourCC type = GetType();
  if (type != mov::TYPE_FTYP || size < sizeof(FileTypeAtom)) {
    return 0;
  }
#endif
  const uint8_t* data = base_.GetData();
  const FourCC major_brand = LoadBrand(&data[8]);
  if (major_brand == mov::BRAND_QUICKTIME) {
    for (uint32_t i = 16; i < size; i += 4) {
      const FourCC compatible_brand = LoadBrand(&data[i]);
      if (compatible_brand == major_brand) {
        return 1;
      }
    }
  }
  return 0;
}

}  // namespace mov
