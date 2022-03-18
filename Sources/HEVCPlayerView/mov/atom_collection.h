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

#ifndef MOV_ATOM_COLLECTION_H_
#define MOV_ATOM_COLLECTION_H_

#include <stddef.h>
#include <stdint.h>

namespace mov {

struct Atom;
struct FileTypeAtom;
struct MediaHeaderAtom;
struct SampleDescriptionAtom;
struct SampleDescription;
struct VideoSampleDescription;
struct VideoSampleDescriptionExtension;
struct TimeToSampleAtom;
struct SyncSampleAtom;
struct SampleToChunkAtom;
struct SampleSizeAtom;
struct ChunkOffsetAtom;

/**
 * The class implementing a mapping table from an atom ID to an atom object.
 */
struct AtomCollection {
  /**
   * Atom IDs.
   * @enum
   */
  enum AtomID {
    ID_FTYP = 0,
    // ID_WIDE,
    ID_MDAT,
    // ID_MOOV,
    // ID_MVHD,
    // ID_TRAK,
    // ID_TKHD,
    // ID_EDTS,
    // ID_MDIA,
    ID_MDHD,
    // ID_HDLR,
    // ID_MINF,
    // ID_VMHD,
    // ID_DINF,
    // ID_STBL,
    ID_STSD,
    ID_STTS,
    // ID_CTTS,
    // ID_CSLG,
    // ID_STSS,
    // ID_STPS,
    // ID_STSC,
    // ID_STSZ,
    // ID_STCO,
    // ID_SDTP,
    // ID_STSH,
    ID_STSS,
    ID_STSC,
    ID_STSZ,
    ID_STCO,
    // ID_UDTA,
    ID_ERROR,
  };

  /**
   * Initializes this mapping table.
   * @public
   */
  void Initialize();

  /**
   * Enumerates atoms in a QuickTime stream.
   * @param {const uint8_t* __restrict} data
   * @param {size_t} size
   * @return {int}
   * @public
   */
  int Enumerate(const uint8_t* __restrict data, size_t size);

  /**
   * Returns whether or not this map contains atoms required for calculating
   * sample durations.
   * @return {int}
   * @public
   */
  int HasSampleDurations() const {
    enum {
      REQUIRED_ATOMS = (1 << ID_STTS) | (1 << ID_MDHD),
    };
    return (mask_ & REQUIRED_ATOMS) == REQUIRED_ATOMS;
  }

  /**
   * Retrieves the file-type atom found by this enumerator.
   * @return {const mov::FileTypeAtom*}
   * @public
   */
  const mov::FileTypeAtom* GetFileTypeAtom() const {
    return GetAtom<mov::FileTypeAtom>(ID_FTYP);
  }

  /**
   * Retrieves the media-header atom found by this enumerator.
   * @return {const mov::MediaHeaderAtom*}
   * @public
   */
  const mov::MediaHeaderAtom* GetMediaHeaderAtom() const {
    return GetAtom<mov::MediaHeaderAtom>(ID_MDHD);
  }

  /**
   * Retrieves the sample-description atom found by this enumerator.
   * @return {const mov::SampleDescriptionAtom*}
   * @public
   */
  const mov::SampleDescriptionAtom* GetSampleDescriptionAtom() const {
    return GetAtom<mov::SampleDescriptionAtom>(ID_STSD);
  }

  /**
   * Retrieves the time-to-sample atom found by this enumerator.
   * @return {const mov::TimeToSampleAtom*}
   * @public
   */
  const mov::TimeToSampleAtom* GetTimeToSampleAtom() const {
    return GetAtom<mov::TimeToSampleAtom>(ID_STTS);
  }

  /**
   * Retrieves the sync-sample atom found by this enumerator.
   * @return {const mov::SyncSampleAtom*}
   * @public
   */
  const mov::SyncSampleAtom* GetSyncSampleAtom() const {
    return GetAtom<mov::SyncSampleAtom>(ID_STSS);
  }

  /**
   * Retrieves the sample-to-chunk atom found by this enumerator.
   * @return {const mov::SampleToChunkAtom*}
   * @public
   */
  const mov::SampleToChunkAtom* GetSampleToChunkAtom() const {
    return GetAtom<mov::SampleToChunkAtom>(ID_STSC);
  }

  /**
   * Retrieves the sample-size atom found by this enumerator.
   * @return {const mov::SampleSizeAtom*}
   * @public
   */
  const mov::SampleSizeAtom* GetSampleSizeAtom() const {
    return GetAtom<mov::SampleSizeAtom>(ID_STSZ);
  }

  /**
   * Retrieves the chunk-offset atom found by this enumerator.
   * @return {const mov::ChunkOffsetAtom*}
   * @public
   */
  const mov::ChunkOffsetAtom* GetChunkOffsetAtom() const {
    return GetAtom<mov::ChunkOffsetAtom>(ID_STCO);
  }

  /**
   * Retrieves the media-data atom found by this enumerator.
   * @return {const mov::Atom*}
   * @public
   */
  const mov::Atom* GetMediaData() const {
    return GetAtom<mov::Atom>(ID_MDAT);
  }

  /**
   * Enumerates child atoms in a QuickTime atom.
   * @param {const uint8_t* __restrict} data
   * @param {size_t} offset
   * @param {size_t} size
   * @return {uint32_t}
   * @private
   */
  uint32_t EnumerateChildAtoms(const uint8_t* __restrict data,
    size_t offset,
    size_t size);

  /**
   * Returns the specified atom.
   * @param {int} id
   * @return {const T*}
   * @private
   */
  template <typename T>
  const T* GetAtom(int id) const {
    return reinterpret_cast<const T*>(atoms_[id]);
  }

  /**
   * The atoms enumerated by this enumerator.
   * @type {const mov::Atom*[]}
   * @private
   */
  const mov::Atom* atoms_[ID_ERROR];

  /**
   * The bit-mask representing the atoms enumerated by this object.
   * @type {uint32_t}
   * @private
   */
  uint32_t mask_;
};

}  // namespace mov

#endif  // MOV_ATOM_COLLECTION_H_
