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

#include "../mov/atom_collection.h"

#include <memory.h>
#include "../mov/atom.h"

namespace mov {

void AtomCollection::Initialize() {
  memset(&atoms_[0], 0, sizeof(atoms_));
  mask_ = 0;
}
  
int AtomCollection::Enumerate(const uint8_t* __restrict data, size_t size) {
  enum {
    REQUIRED_ATOMS = (1 << ID_FTYP) | (1 << ID_MDAT) | (1 << ID_STSD) |
        (1 << ID_STSC) | (1 << ID_STSZ) | (1 << ID_STCO),
  };
  mask_ = EnumerateChildAtoms(data, 0, size);
  return (mask_ & (REQUIRED_ATOMS | (1 << ID_ERROR))) == REQUIRED_ATOMS;
}

uint32_t AtomCollection::EnumerateChildAtoms(
    const uint8_t* __restrict data,
    size_t offset,
    size_t size) {
  uint32_t atom_mask = 0;
  const uint8_t* start = &data[offset];
  const uint8_t* end = &data[size];
  while (start < end) {
    const mov::Atom* atom = reinterpret_cast<const mov::Atom*>(start);
    const uint32_t atom_size = atom->GetSize();
    if (start + atom_size > end) {
      return 1 << ID_ERROR;
    }
    const mov::FourCC atom_type = atom->GetType();
    switch (atom_type) {
    case mov::TYPE_FTYP:
      atoms_[ID_FTYP] = atom;
      atom_mask |= 1 << ID_FTYP;
      break;
    case mov::TYPE_MDAT:
      atoms_[ID_MDAT] = atom;
      atom_mask |= 1 << ID_MDAT;
      break;
    case mov::TYPE_MOOV:
    case mov::TYPE_TRAK:
    case mov::TYPE_MDIA:
    case mov::TYPE_MINF:
    case mov::TYPE_STBL:
      atom_mask |= EnumerateChildAtoms(start, 8, atom_size);
      break;
    case mov::TYPE_MDHD:
      if (!atoms_[ID_MDHD]) {
        atoms_[ID_MDHD] = atom;
        atom_mask |= 1 << ID_MDHD;
      }
      break;
    case mov::TYPE_STSD:
      if (!atoms_[ID_STSD]) {
        atoms_[ID_STSD] = atom;
        atom_mask |= 1 << ID_STSD;
      }
      break;
    case mov::TYPE_STTS:
      if (!atoms_[ID_STTS]) {
        atoms_[ID_STTS] = atom;
        atom_mask |= 1 << ID_STTS;
      }
      break;
    case mov::TYPE_STSS:
      if (!atoms_[ID_STSS]) {
        atoms_[ID_STSS] = atom;
        atom_mask |= 1 << ID_STSS;
      }
      break;
    case mov::TYPE_STSC:
      if (!atoms_[ID_STSC]) {
        atoms_[ID_STSC] = atom;
        atom_mask |= 1 << ID_STSC;
      }
      break;
    case mov::TYPE_STSZ:
      if (!atoms_[ID_STSZ]) {
        atoms_[ID_STSZ] = atom;
        atom_mask |= 1 << ID_STSZ;
      }
      break;
    case mov::TYPE_STCO:
      if (!atoms_[ID_STCO]) {
        atoms_[ID_STCO] = atom;
        atom_mask |= 1 << ID_STCO;
      }
      break;
    default:
      break;
    }
    start += atom_size;
  }
  return atom_mask;
}

}  // namespace mov
