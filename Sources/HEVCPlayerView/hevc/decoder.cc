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

#include "../hevc/decoder.h"

#include <stdio.h>
#include <memory.h>
#include "../base/cpu.h"
#include "../base/log.h"
#include "../hevc/bitstream.h"
#include "../mov/atom.h"
#include "../mov/atom_collection.h"

#ifndef __APPLE__
namespace {

enum {
  kVTPropertyNotSupportedErr = -12900,
  kVTPropertyReadOnlyErr = -12901,
  kVTParameterErr = -12902,
  kVTInvalidSessionErr = -12903,
  kVTAllocationFailedErr = -12904,
  kVTPixelTransferNotSupportedErr = -12905,
  kVTCouldNotFindVideoDecoderErr = -12906,
  kVTCouldNotCreateInstanceErr = -12907,
  kVTCouldNotFindVideoEncoderErr = -12908,
  kVTVideoDecoderBadDataErr = -12909,
  kVTVideoDecoderUnsupportedDataFormatErr = -12910,
  kVTVideoDecoderMalfunctionErr = -12911, // c.f. -8960
  kVTVideoEncoderMalfunctionErr = -12912,
  kVTVideoDecoderNotAvailableNowErr = -12913,
  kVTImageRotationNotSupportedErr = -12914,
  kVTVideoEncoderNotAvailableNowErr = -12915,
  kVTFormatDescriptionChangeNotSupportedErr = -12916,
  kVTInsufficientSourceColorDataErr = -12917,
  kVTCouldNotCreateColorCorrectionDataErr = -12918,
  kVTColorSyncTransformConvertFailedErr = -12919,
  kVTVideoDecoderAuthorizationErr = -12210,
  kVTVideoEncoderAuthorizationErr = -12211,
  kVTColorCorrectionPixelTransferFailedErr = -12212,
  kVTMultiPassStorageIdentifierMismatchErr = -12213,
  kVTMultiPassStorageInvalidErr = -12214,
  kVTFrameSiloInvalidTimeStampErr = -12215,
  kVTFrameSiloInvalidTimeRangeErr = -12216,
  kVTCouldNotFindTemporalFilterErr = -12217,
  kVTPixelTransferNotPermittedErr = -12218,
};

}  // namespace
#endif

namespace hevc {

void Decoder::Initialize() {
  // Set zero to the all members except `rbsp_data_[]`.
  memset(this, 0, offsetof(Decoder, rbsp_data_));
#if __APPLE__
  InitializeVideoToolbox();
#endif
}

int Decoder::Create(const void* data,
                    size_t size,
                    OutputCallback callback,
                    void* object) {
  // Create a copy of the given QuickTime stream so Video Toolbox can write its
  // data. (This clone has a 16-byte (32-byte) padding at its end so this
  // decoder can safely read 16 (32) bytes from anywhere in it.)
  data_ = static_cast<uint8_t*>(malloc(size + __BIGGEST_ALIGNMENT__));
  if (!data_) {
    return kVTAllocationFailedErr;
  }
  memcpy(data_, data, size);

  // Parse QuickTime atoms in the stream required for decoding it with Video
  // Toolbox.
  mov::AtomCollection map;
  map.Initialize();
  if (!map.Enumerate(&data_[0], size)) {
    return kVTVideoDecoderUnsupportedDataFormatErr;
  }
  const mov::FileTypeAtom* file_type_atom = map.GetFileTypeAtom();
  if (!file_type_atom->IsValid()) {
    return kVTVideoDecoderUnsupportedDataFormatErr;
  }
  const mov::SampleDescriptionAtom* sample_description_atom =
      map.GetSampleDescriptionAtom();
  uint32_t number_of_descriptions = sample_description_atom->GetCount();
  if (!number_of_descriptions) {
    return kVTVideoDecoderUnsupportedDataFormatErr;
  }
  const mov::SampleDescription* sample_description =
      sample_description_atom->GetFirstDescription();
  do {
    const mov::FourCC format = sample_description->GetType();
    if (format == mov::FORMAT_HVC1) {
      const mov::VideoSampleDescription* video_sample_description =
          sample_description->GetVideoSampleDescription();
      const int frame_width = video_sample_description->GetWidth();
      const int frame_height = video_sample_description->GetHeight();
      frame_width_ = frame_width;
      frame_height_ = frame_height;
      HEVC_LOG_D("%s(): width=%d, height=%d\n",
          __FUNCTION__, frame_width, frame_height);

      // Decode extensions only if it has sufficient space to contain an
      // extension header (8 bytes). (Apple encoders sometimes add a 4-byte
      // padding to the end of a video-sample-description extension. This
      // decoder has to ignore such paddings.)
      const uint8_t* extra_start = video_sample_description->GetExtraData();
      const uint8_t* extra_end = video_sample_description->GetNext();
      while (extra_start + 8 <= extra_end) {
        const mov::VideoSampleDescriptionExtension* extension =
            video_sample_description->GetExtension(extra_start);
        if (extension->GetType() == mov::EXTENSION_HVCC) {
          // Read this `hvcC` extension to determine whether or not this
          // QuickTime stream is an HEVC-with-Alpha stream.
          // TODO(hbono): Video Toolbox decodes `hvcC` extensions by itself and
          // this decoder does not have to do it for now. But it is definitely
          // better for this decoder to decode `hvcC` extensions in the future.
          if (!DecodeHEVCDecoderConfiguration(extension)) {
            return kVTVideoDecoderUnsupportedDataFormatErr;
          }

          // Initialize the `samples_[]` array so this decoder can seek frames
          // in the QuickTime stream.
          if (!InitializeSamples(&map)) {
            return kVTAllocationFailedErr;
          }

          // Set sample durations using a `stts` atom and a `mdhd` atom.
          if (map.HasSampleDurations()) {
            const mov::TimeToSampleAtom* time_to_sample_atom =
                map.GetTimeToSampleAtom();
            const mov::MediaHeaderAtom* media_header_atom =
                map.GetMediaHeaderAtom();
            time_scale_ = media_header_atom->GetTimeScale();
            uint32_t entry_start = 0;
            const uint32_t number_of_entries = time_to_sample_atom->GetCount();
            for (uint32_t i = 0; i < number_of_entries; ++i) {
              const mov::TimeToSampleAtom::Entry* entry =
                  time_to_sample_atom->GetEntry(i);
              const uint32_t entry_end = entry_start + entry->GetCount();
              if (entry_end > number_of_samples_) {
                return kVTVideoDecoderBadDataErr;
              }
              if (entry_start < entry_end) {
                const uint32_t entry_duration = entry->GetDuration();
                do {
                  samples_[entry_start].duration = entry_duration;
                } while (++entry_start < entry_end);
              }
            }
          }
#if __APPLE__
          // This initializer can get all information required for initializing
          // a Video Toolbox session.
          return CreateVideoToolbox(
              extension, frame_width, frame_height, callback, object);
#endif
        }
        extra_start += extension->GetSize();
      }
    }
    sample_description = sample_description->GetNextDescription();
  } while (--number_of_descriptions > 0);

  return kVTVideoDecoderUnsupportedDataFormatErr;
}

void Decoder::Destroy() {
#if __APPLE__
  DestroyVideoToolbox();
  hvcc_extension_ = NULL;
  decoder_callback_ = NULL;
  decoder_object_ = NULL;
#endif
  if (samples_) {
    free(samples_);
    samples_ = NULL;
  }
  if (data_) {
    free(data_);
    data_ = NULL;
  }
}

int Decoder::Reset() {
#if __APPLE__
  return ResetVideoToolbox();
#else
  return -1;
#endif
}

int Decoder::DecodeSample(int sample_number) {
#if __APPLE__
  return DecodeSampleVideoToolbox(sample_number);
#else
  return 0;
#endif
}

int Decoder::InitializeSamples(const mov::AtomCollection* map) {
  // Merge the sample counts in the `stsc` atom of the input QuickTime stream
  // and the offsets in its `stco` atom into an array of chunks.
  const mov::SampleToChunkAtom* sample_to_chunk_atom =
      map->GetSampleToChunkAtom();
  const mov::ChunkOffsetAtom* chunk_offset_atom =
      map->GetChunkOffsetAtom();
  const mov::SampleSizeAtom* sample_size_atom =
      map->GetSampleSizeAtom();
  const uint32_t number_of_entries = sample_to_chunk_atom->GetCount();
  const uint32_t number_of_chunks = chunk_offset_atom->GetCount();
  const uint32_t sample_size = sample_size_atom->GetSampleSize();
  if (number_of_entries == 0 || number_of_chunks == 0) {
    return 0;
  }
  struct Chunk {
    uint32_t first_sample;
    uint32_t number_of_samples;
    uint32_t offset;
  };
  Chunk* chunks =
      static_cast<Chunk*>(malloc(number_of_chunks * sizeof(Chunk)));
  if (!chunks) {
    return 0;
  }
  int result = 0;
  {
    // Retrieve the sample counts in the `stsc` atom and fill the sample counts.
    // For each chunk, this loop copies the sample count of a `stsc` entry to
    // which it belongs.
    //   +-------+-------------+--------------+-----------------------+
    //   | entry | first chunk | sample count | sample description ID |
    //   +-------+-------------+--------------+-----------------------+
    //   | 0     | 1           | 30           | 1                     |
    //   | 1     | 3           | 15           | 2                     |
    //   +-------+-------------+--------------+-----------------------+
    number_of_samples_ = 0;
    const mov::SampleToChunkAtom::Entry* first_entry =
        sample_to_chunk_atom->GetEntry(0);
    const mov::SampleToChunkAtom::Entry* entry =
        sample_to_chunk_atom->GetEntry(number_of_entries - 1);
    uint32_t first_chunk = entry->GetFirst();
    uint32_t first_sample = 1;
    for (uint32_t i = number_of_chunks; i > 0; --i) {
      while (i < first_chunk) {
        if (entry <= first_entry) {
          goto free_chunks;
        }
        --entry;
        first_chunk = entry->GetFirst();
      }
      {
        const uint32_t number_of_samples = entry->GetSamples();
        number_of_samples_ += number_of_samples;
        chunks[i - 1].number_of_samples = number_of_samples;
      }
    }
    HEVC_LOG_D("%s(): number_of_samples=%d\n",
        __FUNCTION__, number_of_samples_);

    // Read the number of samples in the `stsz` atom and verify that it is same
    // as the calculated one if this QuickTime stream is a VBR stream.
    if (!sample_size && number_of_samples_ != sample_size_atom->GetCount()) {
      goto free_chunks;
    }

    // Retrieve the chunk offsets of the `stco` atom and fill the chunk offsets.
    for (uint32_t i = 0; i < number_of_chunks; ++i) {
      Chunk* chunk = &chunks[i];
      chunk->first_sample = first_sample;
      chunk->offset = chunk_offset_atom->GetOffset(i);
      first_sample += chunk->number_of_samples;
    }
  }

  // Create a `samples_` array and initialize it with the above `chunks[]`
  // array.
  samples_ = static_cast<Sample*>(
      malloc(number_of_samples_ * sizeof(Sample)));
  if (!samples_) {
    goto free_chunks;
  }
  {
    const Chunk* chunk = &chunks[0];
    const Chunk* last_chunk = &chunks[number_of_chunks - 1];
    uint32_t sample_offset = 0;
    max_picture_order_count_ = 0;
    for (uint32_t i = 1; i <= number_of_samples_; ++i) {
      while (i >= chunk->first_sample + chunk->number_of_samples) {
        if (chunk >= last_chunk) {
          goto free_chunks;
        }
        ++chunk;
        sample_offset = 0;
      }
      {
        const uint32_t sample_index = i - 1;
        Sample* sample = &samples_[sample_index];
        sample->offset = chunk->offset + sample_offset;
        sample->size = sample_size ? sample_size :
            sample_size_atom->GetSampleSize(sample_index);
        sample->duration = 0;
        sample->picture_order_count =
            DecodeSliceHeader(&data_[sample->offset], sample->size);
        max_picture_order_count_ =
            max_picture_order_count_ >= sample->picture_order_count ?
            max_picture_order_count_ : sample->picture_order_count;
        sample_offset += sample->size;
        HEVC_LOG_D("%s(): samples[%d] = { offset: %x, size: %d, order: %d }\n",
            __FUNCTION__, sample_index, sample->offset, sample->size,
            sample->picture_order_count);
      }
    }
  }
  result = 1;

free_chunks:
  free(chunks);

  return result;
}

int Decoder::DecodeHEVCDecoderConfiguration(
    const mov::VideoSampleDescriptionExtension* extension) {
  // Decode the given `hvcC` configuration and retrieve whether or not this
  // QuickTime stream is an HEVC with Alpha stream. An `hvcC` configuration
  // consists of a 21-byte header and an array of NAL-unit arrays as listed
  // below.
  //   +-------+------+-------------------------------------+
  //   | index | size | field                               |
  //   +-------+------+-------------------------------------+
  //   | 0     | 8    | configuration_version               |
  //   +-------+------+-------------------------------------+
  //   | 8     | 2    | general_profile_space               |
  //   |       | 1    | general_tier_flag                   |
  //   |       | 5    | general_profile_idc                 |
  //   +-------+------+-------------------------------------+
  //   | 16    | 32   | general_profile_compatibility_flags |
  //   +-------+------+-------------------------------------+
  //   | 48    | 48   | general_constraint_indicator_flags  |
  //   +-------+------+-------------------------------------+
  //   | 96    | 8    | general_level_idc                   |
  //   +-------+------+-------------------------------------+
  //   | 104   | 4    | reserved = '1111'                   |
  //   |       | 12   | min_spatial_segmentation_idc        |
  //   +-------+------+-------------------------------------+
  //   | 120   | 6    | reserved = '111111'                 |
  //   |       | 2    | parallelism_type                    |
  //   +-------+------+-------------------------------------+
  //   | 128   | 6    | reserved = '111111'                 |
  //   |       | 2    | chroma_format                       |
  //   +-------+------+-------------------------------------+
  //   | 136   | 5    | reserved = '11111'                  |
  //   |       | 3    | bit_depth_luma_minus_8              |
  //   +-------+------+-------------------------------------+
  //   | 144   | 5    | reserved = '11111'                  |
  //   |       | 3    | bit_depth_chroma_minus_8            |
  //   +-------+------+-------------------------------------+
  //   | 152   | 16   | averaget_frame_rate                 |
  //   +-------+------+-------------------------------------+
  //   | 168   | 2    | constant_frame_rate                 |
  //   |       | 3    | num_temporal_layers                 |
  //   |       | 1    | temporal_id_nested                  |
  //   +-------+------+-------------------------------------+
  //   | 176   | 8    | number_of_arrays                    |
  //   +-------+------+-------------------------------------+
  //   |       |      | nal_unit_array #1 (VPS[0])          |
  //   +-------+------+-------------------------------------+
  //   |       |      | nal_unit_array #2 (SPS[0], SPS[1])  |
  //   +-------+------+-------------------------------------+
  //   |       |      | nal_unit_array #3 (PPS[0], PPS[1])  |
  //   +-------+------+-------------------------------------+
  //   |       |      | nal_unit_array #4 (SEI_PREFIX[0])   |
  //   +-------+------+-------------------------------------+
  // This function finds a VPS in this `hvcC` extension and parses its VPS
  // extension as written in the "HEVC with Alpha compatibility profile".
  const uint8_t* hvcc_start = extension->GetExtraData();
  const uint8_t* hvcc_end = extension->GetNext();
  if (hvcc_end - hvcc_start < 21 + 1) {
    return 0;
  }
  int number_of_arrays = static_cast<int>(hvcc_start[21 + 1]);
  if (number_of_arrays < 3) {
    return 0;
  }
  const uint8_t* array_start = &hvcc_start[21 + 1 + 1];
  const uint8_t* array_end = hvcc_end;
  do {

    // Read the array header. An array header starts with a 3-byte header listed
    // below.
    //   +-------+------+--------------------+
    //   | index | size | field              |
    //   +-------+------+--------------------+
    //   | 0     | 1    | array_completeness |
    //   |       | 1    | reserved = '0'     |
    //   |       | 6    | nal_unit_type      |
    //   +-------+------+--------------------+
    //   | 8     | 16   | num_nal_units      |
    //   +-------+------+--------------------+
    if (array_end - array_start < 1 + 2) {
      return 0;
    }
    const NALUnitType nal_unit_type =
        static_cast<NALUnitType>(array_start[0] & 0x3f);
    const uintptr_t number_of_nal_units = CPU::LoadUINT16BE(&array_start[1]);
    const uint8_t* nal_unit_start = &array_start[1 + 2];
    const uint8_t* nal_unit_end = array_end;
#if __SIZEOF_SIZE_T__ == 8
    static const uint64_t nal_unit_types = (1ULL << NAL_VPS) |
        (1ULL << NAL_SPS) | (1ULL << NAL_PPS) | (1ULL << NAL_SEI_PREFIX);
    const uint64_t decode_nal_units = (nal_unit_types >> nal_unit_type) & 1ULL;
#else
    static const uint32_t nal_unit_types[2] = {
      0x00000000,
      (1UL << (NAL_VPS - 32)) | (1UL << (NAL_SPS - 32)) |
          (1UL << (NAL_PPS - 32)) | (1UL << (NAL_SEI_PREFIX - 32)),
    }
    const uint32_t decode_nal_units =
        (nal_unit_types[nal_unit_type >> 5] >> (nal_unit_type & 31)) & 1UL;
#endif
    for (uintptr_t i = 0; i < number_of_nal_units; ++i) {
      if (nal_unit_end - nal_unit_start < 2) {
        return 0;
      }
      const uintptr_t nal_unit_size = CPU::LoadUINT16BE(&nal_unit_start[0]);
      if (nal_unit_end - nal_unit_start < static_cast<intptr_t>(nal_unit_size)) {
        return 0;
      }
      if (decode_nal_units) {
        // Extract an RBSP (Raw Byte Sequence Payload) required for decoding an
        // HEVC-with-Alpha stream with Video Toolbox, i.e. VPS RBSPs, SPS RBSPs,
        // PPS RBSPs, and SEI_PREFIX RBSPs. These RBSPs should be smaller than
        // 256 bytes (the size of the RBSP spool of this decoder).
        if (nal_unit_size >= sizeof(rbsp_data_)) {
          return 0;
        }
        uint8_t* rbsp_data = &rbsp_data_[0];
        uintptr_t rbsp_size =
            ExtractRBSP(&nal_unit_start[2], nal_unit_size, rbsp_data);
        int result = 1;
        if (nal_unit_type == NAL_VPS) {
          result = DecodeVideoParameterSet(rbsp_data, rbsp_size);
        } else if (nal_unit_type == NAL_SPS) {
          result = DecodeSequenceParameterSet(rbsp_data, rbsp_size, i);
        } else if (nal_unit_type == NAL_PPS) {
          result = DecodePictureParameterSet(rbsp_data, rbsp_size, i);
        } else { // nal_unit_type == NAL_SEI_PREFIX
          DecodeSupplementalEnhancementInformation(rbsp_data, rbsp_size);
        }
        if (!result) {
          return 0;
        }
      }
      nal_unit_start += 2 + nal_unit_size;
    }
    array_start = nal_unit_start;
  } while (--number_of_arrays > 0);
  return 1;
}

int Decoder::DecodeVideoParameterSet(const uint8_t* rbsp_data,
                                     uintptr_t rbsp_size) {
  if (rbsp_size < 2 + 4 + 12) {
    return 0;
  }
  // Decode a NAL header and an H.265 VPS (including a PTL) in the extracted
  // RBSP. An H.265 VPS starts with a 18-byte header listed below.
  //   +-------+------+-------------------------------+
  //   | index | size | field                         |
  //   +-------+------+-------------------------------+
  //   | 0     | 1    | 0                             |
  //   |       | 6    | nal_unit_type                 |
  //   |       | 6    | nuh_layer_id                  |
  //   |       | 3    | nuh_temporary_id_plus1        |
  //   +-------+------+-------------------------------+
  //   | 16    | 4    | vps_video_parameter_set_id    |
  //   |       | 1    | vps_base_layer_internal_flag  |
  //   |       | 1    | vps_base_layer_available_flag |
  //   |       | 6    | vps_max_layers_minus1         |
  //   |       | 3    | vps_max_sub_layers_minus1     |
  //   |       | 1    | vps_temporal_id_nesting_flag  |
  //   |       | 16   | vps_reserved_0xffff_16bits    |
  //   +-------+------+-------------------------------+
  //   | 48    | 96   | profile_tier_level[0]         |
  //   +-------+------+-------------------------------+
  VideoParameterSet* vps = &vps_;
  const uint32_t rbsp_data2 = CPU::LoadUINT16BE(&rbsp_data[2]);
  vps->vps_video_parameter_set_id = rbsp_data2 >> 12;
  // vps->vps_base_layer_internal_flag = CPU::BitExtractUINT32(rbsp_data2, 11, 1);
  // vps->vps_base_layer_available_flag = CPU::BitExtractUINT32(rbsp_data2, 10, 1);
  vps->vps_max_layers_minus1 = CPU::BitExtractUINT32(rbsp_data2, 4, 6);
  vps->vps_max_sub_layers_minus1 = CPU::BitExtractUINT32(rbsp_data2, 1, 3);
  // vps->vps_temporal_id_nesting_flag = CPU::BitExtractUINT32(rbsp_data2, 0, 1);
  if (vps->vps_max_sub_layers_minus1 > 0) {
    HEVC_NOT_IMPLEMENTED();
    return 0;
  }
  const uint8_t* rbsp_top = &rbsp_data[6];
  const uint8_t* rbsp_end = &rbsp_data[rbsp_size];
  rbsp_top = ParseProfileTierLevel(rbsp_top, rbsp_end,
      vps->vps_max_sub_layers_minus1, &vps->profile_tier_level);
  if (!rbsp_top) {
    HEVC_LOG_E("unsupported PTL.");
    return 0;
  }
  // Decode variable-length parameters of this VPS and VPS extensions as written
  // in Section 7.3.2.1 and Annex F.7.3.2.1. For example, a byte sequence
  // `11 C0 BF 78 08 00 08 30 28 52 00 00 00 00 65 20` represents the following
  // VPS parameters.
  //   +------------------------------------------+------------------+-------+
  //   | field                                    | code             | value |
  //   +------------------------------------------+------------------+-------+
  //   | vps_sub_layer_ordering_info_present_flag | 0                | false |
  //   | vps_max_dec_pic_buffering_minus1[0]      | 00100            | 3     |
  //   | vps_num_reorder_pics[0]                  | 011              | 2     |
  //   | vps_max_latency_increase_plus1[0]        | 1                | 0     |
  //   | vps_max_layer_id                         | 000000           | 0     |
  //   | vps_num_layer_sets_minus1                | 1                | 0     |
  //   | vps_timing_info_present_flag             | 0                | false |
  //   | vps_extension_flag                       | 1                | 1     |
  //   +------------------------------------------+------------------+-------+
  //   | vps_extension_alignment_bit_equal_to_one | 11111            |       |
  //   +------------------------------------------+------------------+-------+
  //   | profile_tier_level                       |                  |       |
  //   |   general_level_idc                      | 01111000         | 120   |
  //   +------------------------------------------+------------------+-------+
  //   | splitting_flag                           | 0                | false |
  //   | scalability_mask_flag[]                  | 0,0,0,1,0,0,0,0, |       |
  //   |                                          | 0,0,0,0,0,0,0,0  |       |
  //   | dimension_id_len_minus1[AuxId]           | 000              | 0     |
  //   | vps_nuh_layer_id_present_flag            | 1                | true  |
  //   | layer_id_in_nuh[1]                       | 000001           | 1     |
  //   | dimenstion_id[1][AuxId]                  | 1                | 1     |
  //     ...
  //   +------------------------------------------+------------------+-------+
  BitStreamReader reader;
  reader.Initialize(rbsp_top, rbsp_end);
  vps->vps_sub_layer_ordering_info_present_flag = reader.GetBit<uint8_t>();
  if (vps->vps_sub_layer_ordering_info_present_flag) {
    HEVC_NOT_IMPLEMENTED();
    return 0;
  }
  reader.SkipGolomb();  // vps_max_dec_pic_buffering_minus1[0]
  reader.SkipGolomb();  // vps_num_reorder_pics[0]
  reader.SkipGolomb();  // vps_max_latency_increase_plus1[0]

  // Skip `layer_id_included_flags`.
  vps->vps_max_layer_id = reader.GetBits<uint8_t>(6);
  vps->vps_num_layer_sets_minus1 = reader.GetGolomb<uint8_t>();
  uint8_t skip_bits = vps->vps_num_layer_sets_minus1 * vps->vps_max_layer_id;
  if (skip_bits > 0) {
    reader.SkipBits(skip_bits);
  }
  vps->vps_timing_info_present_flag = reader.GetBit<uint8_t>();
  if (vps->vps_timing_info_present_flag) {
    reader.SkipBits(32);  // vps_num_units_in_tick
    reader.SkipBits(32);  // vps_time_scale
    uint8_t vps_poc_proportional_to_timing_flag = reader.GetBit<uint8_t>();
    if (vps_poc_proportional_to_timing_flag) {
      reader.SkipGolomb();  // vps_num_ticks_poc_diff_one_minus1
    }
    uint32_t vps_num_hrd_parameters = reader.GetGolomb<uint32_t>();
    if (vps_num_hrd_parameters) {
      HEVC_NOT_IMPLEMENTED();
      return 0;
    }
  }
  vps->vps_extension_flag = reader.GetBit<uint8_t>();
  if (vps->vps_extension_flag) {
    reader.SkipToByteBoundary();  // vps_extension_alignment_bit_equal_to_one

    // Read a PTL of the layers. (An extension PTL consists only of a
    // `general_level_idc` field if `vps_max_sub_layers_minus1` is zero.)
    VideoParameterSet::Extension* extension = &vps->extension;
    extension->general_level_idc = reader.GetBits<uint8_t>(8);

    // Retrieve values of the `dimension_id_length[]` array. (The
    // `scalability_mask_flags` variable stores scalability masks in the
    // most-significant-bit first order, e.g. its bit 15 represents the value
    // of `scalability_mask_flag[0]`.)
    uint8_t splitting_flag = reader.GetBit<uint8_t>();
    if (splitting_flag) {
      HEVC_NOT_IMPLEMENTED();
      return 0;
    }
    uint8_t dimension_id_len[16] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    uint32_t scalability_mask_flags = reader.GetBits<uint32_t>(16);
    if (scalability_mask_flags) {
      uint32_t mask_flags = scalability_mask_flags;
      do {
        uintptr_t index = __builtin_ctz(mask_flags);
        dimension_id_len[index] = reader.GetBits<uint8_t>(3) + 1;
        mask_flags ^= 1 << index;
      } while (mask_flags);
    }
    // Read the value of `dimension_id[][]`.
    uint8_t vps_nuh_layer_id_present_flag = reader.GetBit<uint8_t>();
    for (uintptr_t i = 1; i <= vps->vps_max_layers_minus1; ++i) {
      if (vps_nuh_layer_id_present_flag) {
        extension->layer_id_in_nuh[i] = reader.GetBits<uint8_t>(6);
      } else {
        extension->layer_id_in_nuh[i] = static_cast<uint8_t>(i);
      }
      if (scalability_mask_flags) {
        uint32_t mask_flags = scalability_mask_flags;
        do {
          uintptr_t index = __builtin_ctz(mask_flags);
          extension->dimension_id[i][index] =
              reader.GetBits<uint8_t>(dimension_id_len[index]);
          mask_flags ^= 1 << index;
        } while (mask_flags);
      }
    }
    // Check whether or not this stream has an alpha layer.
    const uint8_t alpha_layer_id_in_nuh = extension->layer_id_in_nuh[1];
    if (alpha_layer_id_in_nuh > 1 ||
        extension->dimension_id[alpha_layer_id_in_nuh][AUX_ID] != AUX_ALPHA) {
      HEVC_LOG_E("no alpha layers.");
      return 0;
    }
    // TODO(hbono): decode all VPS extension fields if necessary.
  }
  return 1;
}

int Decoder::DecodeSequenceParameterSet(const uint8_t* rbsp_data,
                                        uintptr_t rbsp_size,
                                        uintptr_t index) {
  if (rbsp_size < 2 + 1 + 12) {
    return 0;
  }
  // Decode a NAL header and an H.265 SPS (including a PTL) in the extracted
  // RBSP. An H.265 SPS starts with a 15-byte header listed below.
  //   +-------+------+-------------------------------+
  //   | index | size | field                         |
  //   +-------+------+-------------------------------+
  //   | 0     | 1    | 0                             |
  //   |       | 6    | nal_unit_type                 |
  //   |       | 6    | nuh_layer_id                  |
  //   |       | 3    | nuh_temporary_id_plus1        |
  //   +-------+------+-------------------------------+
  //   | 16    | 4    | sps_video_parameter_set_id    |
  //   |       | 3    | sps_max_sub_layers_minus1     |
  //   |       | 1    | sps_temporal_id_nesting_flag  |
  //   +-------+------+-------------------------------+
  //   | 24    | 96   | profile_tier_level            |
  //   +-------+------+-------------------------------+
  const uint8_t rbsp_data2 = rbsp_data[2];
  uint8_t sps_video_parameter_set_id = rbsp_data2 >> 4;
  uint8_t sps_max_sub_layers_minus1 = CPU::BitExtractUINT32(rbsp_data2, 1, 3);
  // uint8_t sps_temporal_id_nesting_flag = CPU::BitExtractUINT32(rbsp_data2, 0, 1);
  ProfileTierLevel profile_tier_level;

  const uint8_t* rbsp_top = &rbsp_data[3];
  const uint8_t* rbsp_end = &rbsp_data[rbsp_size];
  rbsp_top = ParseProfileTierLevel(rbsp_top, rbsp_end,
      sps_max_sub_layers_minus1, &profile_tier_level);
  if (!rbsp_top) {
    return 0;
  }
  // Parse variable-length parameters of this SPS as written in Section 7.3.2.2.
  // For example, a byte sequence `a0 0b 48 02 81 67 11 e4 91 22 ...` represents
  // the following SPS parameters.
  //   +---------------------------------------------+---------------------+-------+
  //   | field                                       | code                | value |
  //   +---------------------------------------------+---------------------+-------+
  //   | sps_seq_parameter_set_id                    | 1                   | 0     |
  //   | chroma_format_idc                           | 010                 | 1     |
  //   | pic_width_in_luma_samples                   | 00000000101101001   | 360   |
  //   | pic_height_in_luma_samples                  | 0000000001010000001 | 640   |
  //   | pic_conformance_flag                        | 0                   | 0     |
  //   | bit_depth_luma_minus8                       | 1                   | 0     |
  //   | bit_depth_chroma_minus8                     | 1                   | 0     |
  //   | log2_max_pic_order_cnt_lsb_minus4           | 00111               | 6     |
  //   | sps_sub_layer_ordering_info_present_flag    | 0                   | 0     |
  //   | sps_max_dec_pic_buffering_minus1[0]         | 00100               | 3     |
  //   | sps_num_reorder_pics[0]                     | 011                 | 2     |
  //   | sps_max_latency_increase_plus1[0]           | 1                   | 0     |
  //   | log2_min_luma_coding_block_size_minus3      | 1                   | 0     |
  //   | log2_diff_max_min_luma_coding_block_size    | 00100               | 3     |
  //   | log2_min_luma_transform_block_size_minus2   | 1                   | 0     |
  //   | log2_diff_max_min_luma_transform_block_size | 00100               | 3     |
  //   | max_transform_hierarchy_depth_inter         | 010                 | 1     |
  //   | max_transform_hierarchy_depth_intra         | 010                 | 1     |
  //   | scaling_list_enable_flag                    | 0                   | 0     |
  //   | amp_enabled_flag                            | 0                   | 0     |
  //   | sample_adaptive_offset_enabled_flag         | 1                   | 1     |
  //   | pcm_enabled_flag                            | 0                   | 0     |
  //     ...
  //   +---------------------------------------------+---------------------+-------+
  BitStreamReader reader;
  reader.Initialize(rbsp_top, rbsp_end);
  uint8_t sps_seq_parameter_set_id = reader.GetGolomb<uint8_t>();
  if (sps_seq_parameter_set_id >= sizeof(sps_) / sizeof(sps_[0])) {
    HEVC_NOT_IMPLEMENTED();
    return 0;
  }
  SequenceParameterSet* sps = &sps_[sps_seq_parameter_set_id];
  sps->sps_video_parameter_set_id = sps_video_parameter_set_id;
  sps->sps_max_sub_layers_minus1 = sps_max_sub_layers_minus1;
  // sps->sps_temporal_id_nesting_flag = sps_temporal_id_nesting_flag;
  memcpy(&sps->profile_tier_level, &profile_tier_level,
      sizeof(profile_tier_level));
  sps->sps_seq_parameter_set_id = sps_seq_parameter_set_id;

  sps->chroma_format_idc = reader.GetGolomb<uint8_t>();
  if (sps->chroma_format_idc == 3) {
    sps->separate_colour_plane_flag = reader.GetBit<uint8_t>();
    if (sps->separate_colour_plane_flag) {
      sps->chroma_format_idc = 0;
    }
  }
  sps->pic_width_in_luma_samples = reader.GetGolomb<uint16_t>();
  sps->pic_height_in_luma_samples = reader.GetGolomb<uint16_t>();
  uint8_t conformance_window_flag = reader.GetBit<uint8_t>();
  if (conformance_window_flag) {
    reader.SkipGolomb();  // conf_win_left_offset
    reader.SkipGolomb();  // conf_win_right_offset
    reader.SkipGolomb();  // conf_win_top_offset
    reader.SkipGolomb();  // conf_win_bottom_offset
  }
  sps->bit_depth_luma = reader.GetGolomb<uint8_t>() + 8;
  sps->bit_depth_chroma = reader.GetGolomb<uint8_t>() + 8;
  sps->log2_max_pic_order_cnt_lsb = reader.GetGolomb<uint8_t>() + 4;

  // TODO(hbono): decode all SPS fields if necessary.

  return 1;
}

int Decoder::DecodePictureParameterSet(const uint8_t* rbsp_data,
                                       uintptr_t rbsp_size,
                                       uintptr_t index) {
  const uint8_t* rbsp_top = &rbsp_data[2];
  const uint8_t* rbsp_end = &rbsp_data[rbsp_size];
  // Decode H.265 PPS parameters in the extracted RBSP. An H.265 PPS is a
  // variable-length header, e.g. a byte sequence `C0 25` represents the
  // following PPS parameters.
  //   +----------------------------------------+---------------------+-------+
  //   | field                                  | code                | value |
  //   +----------------------------------------+---------------------+-------+
  //   | pps_pic_parameter_set_id               | 1                   | 0     |
  //   | pps_seq_parameter_set_id               | 1                   | 0     |
  //   | dependent_slice_segments_enabled_flag  | 0                   | false |
  //   | output_flag_present_flag               | 0                   | false |
  //   | num_extra_slice_header_bits            | 000                 | 0     |
  //   | sign_data_hiding_enabled_flag          | 0                   | false |
  //   | cabac_init_present_flag                | 0                   | false |
  //   | num_ref_idx_l0_default_active_minus1   | 010                 | 1     |
  //   | num_ref_idx_l1_default_active_minus1   | 010                 | 1     |
  //   | init_qp_minus26                        | 1                   | 0     |
  //     ...
  //   +----------------------------------------+---------------------+-------+
  BitStreamReader reader;
  reader.Initialize(rbsp_top, rbsp_end);
  uint8_t pps_pic_parameter_set_id = reader.GetGolomb<uint8_t>();
  if (pps_pic_parameter_set_id >= sizeof(pps_) / sizeof(pps_[0])) {
    HEVC_NOT_IMPLEMENTED();
    return 0;
  }
  PictureParameterSet* pps = &pps_[pps_pic_parameter_set_id];
  pps->pps_pic_parameter_set_id = pps_pic_parameter_set_id;
  pps->pps_seq_parameter_set_id = reader.GetGolomb<uint8_t>();
  pps->dependent_slice_segments_enabled_flag = reader.GetBit<uint8_t>();
  pps->output_flag_present_flag = reader.GetBit<uint8_t>();
  pps->num_extra_slice_header_bits = reader.GetBits<uint8_t>(3);

  // TODO(hbono): decode all PPS fields if necessary.

  return 1;
}

int Decoder::DecodeSupplementalEnhancementInformation(const uint8_t* rbsp_data,
                                                      uintptr_t rbsp_size) {
  // Scan all SEI messages in the RBSP to decode an alpha-channel SEI message.
  // A SEI message consists of three fields: `payload_type`, `payload_size`, and
  // `payload_data` as written in Section 7.3.5. For example, a byte sequence
  // `A5 04 10 00 7F 90` represents the following alpha-channel SEI message.
  //   +--------------------------------+-----------+-------+
  //   | field                          | code      | value |
  //   +--------------------------------+-----------+-------+
  //   | payload_type                   | 10100101  | 165   |
  //   | payload_size                   | 00000100  | 4     |
  //   +--------------------------------+-----------+-------+
  //   | alpha_channel_cancel_flag      | 0         | false |
  //   | alpha_channel_use_idc          | 001       | 1     |
  //   | alpha_channel_bit_depth_minus8 | 000       | 0     |
  //   | alpha_transparent_value        | 000000000 | 0     |
  //   | alpha_opaque_value             | 011111111 | 255   |
  //   | alpha_channel_incr_flag        | 0         | false |
  //   | alpha_channel_clip_flag        | 0         | false |
  //   +--------------------------------+-----------+-------+
  // NOTE: This implementation treats a `payload_type` field and a
  // `payload_size` field as a 1-byte field. (This is sufficient for QuickTime
  // video streams generated by Video Toolbox.)
  // TODO(hbono): decode variable-length payload types and sizes as written in
  // Section 7.3.5.
  const uint8_t* rbsp_top = &rbsp_data[2];
  const uint8_t* rbsp_end = &rbsp_data[rbsp_size];
  while (rbsp_end - rbsp_top >= 2) {
    uintptr_t payload_type = rbsp_top[0];
    if (payload_type == 0xff) {
      HEVC_NOT_IMPLEMENTED();
      return 0;
    }
    uintptr_t payload_size = rbsp_top[1];
    const uint8_t* payload_data = &rbsp_top[2];
    if (payload_size == 0xff) {
      // Load the remaining bytes of this payload-size field and calculate the
      // payload size. (This code partially unrolls the loop to process 4 bytes
      // (8 bytes) at once.)
      uintptr_t payload_word;
      uintptr_t payload_index;
      while (rbsp_end - payload_data >= sizeof(uintptr_t)) {
        payload_word = ~CPU::LoadUPTRLE(payload_data);
        if (payload_word) {
          goto payload_read_last_word;
        }
        payload_size += 0xff * sizeof(uintptr_t);
        payload_data += sizeof(uintptr_t);
      }
      payload_word = ~CPU::LoadUPTRLE(payload_data);
      payload_word &= CPU::BitMaskUPTR((rbsp_end - payload_data) << 3);
      if (!payload_word) {
        return 0;
      }
 payload_read_last_word:
#if __SIZEOF_SIZE_T__ == 8
      payload_index = __builtin_ctzll(payload_word) >> 3;
#else
      payload_index = __builtin_ctz(payload_word) >> 3;
#endif
      payload_size += 0xff * payload_index;
      payload_size += (~payload_word >> (payload_index << 3)) & 0xff;
      payload_data += payload_index + 1;
    }
    rbsp_top = &payload_data[payload_size];
    if (rbsp_top >= rbsp_end) {
      return 1;
    }
    if (payload_type == SEI_TYPE_ALPHA_CHANNEL_INFO) {
      sei::AlphaChannelInformation* alpha = &alpha_;
      const uint32_t payload_data0 = CPU::LoadUINT32BE(&payload_data[0]);
      alpha->alpha_channel_cancel_flag = payload_data0 >> 31;
      alpha->alpha_channel_use_idc =
          CPU::BitExtractUINT32(payload_data0, 28, 3);
      alpha->alpha_channel_bit_depth_minus8 =
          CPU::BitExtractUINT32(payload_data0, 25, 3);
      if (alpha->alpha_channel_bit_depth_minus8 != 0) {
        HEVC_LOG_E("unsupported alpha format.");
        return 0;
      }
      alpha->alpha_transparent_value =
          CPU::BitExtractUINT32(payload_data0, 25 - 9, 9);
      alpha->alpha_opaque_value =
          CPU::BitExtractUINT32(payload_data0, 25 - 9 * 2, 9);
      alpha->alpha_channel_incr_flag =
          CPU::BitExtractUINT32(payload_data0, 25 - 9 * 2 - 1, 1);
      alpha->alpha_channel_clip_flag =
          CPU::BitExtractUINT32(payload_data0, 25 - 9 * 2 - 2, 1);
    }
  }
  return 1;
}

const uint8_t* Decoder::ParseProfileTierLevel(const uint8_t* top,
                                              const uint8_t* end,
                                              uintptr_t max_sub_layers_minus1,
                                              ProfileTierLevel* ptl) const {
  // Parse the common profile. When the `max_sub_layers_minus1` value is 0, a
  // PTL is a 12-byte header listed below.
  //   +-------+------+-------------------------------------+
  //   | index | size | field                               |
  //   +-------+------+-------------------------------------+
  //   | 0     | 2    | general_profile_space               |
  //   |       | 1    | general_tier_flag                   |
  //   |       | 5    | general_profile_idc                 |
  //   +-------+------+-------------------------------------+
  //   | 8     | 32   | general_profile_compatibility_flags |
  //   +-------+------+-------------------------------------+
  //   | 40    | 1    | general_progressive_source_flag     |
  //   |       | 1    | general_interlaced_source_flag      |
  //   |       | 1    | general_non_packed_constraint_flag  |
  //   |       | 1    | general_frame_only_constraint_flag  |
  //   +-------+------+-------------------------------------+
  //   | 44    | 43   | general_reserved_zero_43bits        |
  //   +-------+------+-------------------------------------+
  //   | 87    | 1    | general_inbld_flag                  |
  //   +-------+------+-------------------------------------+
  //   | 88    | 8    | general_level_idc                   |
  //   +-------+------+-------------------------------------+
  if (end - top < (2 + 1 + 5 + 32 + 4 + 43 + 1 + 8) / 8) {
    return NULL;
  }
  const uint32_t data0 = top[0];
  ptl->general_profile_space = CPU::BitExtractUINT32(data0, 6, 2);
  ptl->general_tier_flag = CPU::BitExtractUINT32(data0, 5, 1);
  ptl->general_profile_idc = CPU::BitExtractUINT32(data0, 0, 5);

  const uint32_t data1 = CPU::LoadUINT32BE(&top[1]);
  ptl->general_profile_compatibility_flags = data1;
  if (ptl->general_profile_idc == 0) {
    ptl->general_profile_idc = __builtin_clz(data1);
  }

  const uint32_t data5 = top[5];
  ptl->general_progressive_source_flag = CPU::BitExtractUINT32(data5, 7, 1);
  ptl->general_interlaced_source_flag = CPU::BitExtractUINT32(data5, 6, 1);
  ptl->general_non_packed_constraint_flag =
      CPU::BitExtractUINT32(data5, 5, 1);
  ptl->general_frame_only_constraint_flag =
      CPU::BitExtractUINT32(data5, 4, 1);

  const uint32_t data10 = top[10];
  ptl->general_inbld_flag = CPU::BitExtractUINT32(data10, 0, 1);
  ptl->general_level_idc = top[11];

  return top + 12;
}

uint32_t Decoder::DecodeSliceHeader(const uint8_t* packet_data,
                                    uintptr_t packet_size) const {
  // Retrieve the NAL unit type of this packet, which are necessary to parse an
  // H.265 slice header. (A NAL packet starts with a 5-byte header listed
  // below.)
  //   +-------+------+-------------------------------------+
  //   | index | size | field                               |
  //   +-------+------+-------------------------------------+
  //   | 0     | 32   | size                                |
  //   +-------+------+-------------------------------------+
  //   | 32    | 1    | 0                                   |
  //   |       | 6    | nal_unit_type                       |
  //   |       | 6    | nal_layer_id                        |
  //   |       | 3    | nuh_temporary_id + 1                |
  //   +-------+------+-------------------------------------+
  const uint32_t data2 = CPU::LoadUINT16BE(&packet_data[4]);
  NALUnitType nal_unit_type =
      static_cast<NALUnitType>(CPU::BitExtractUINT32(data2, 9, 6));
  // uintptr_t nal_layer_id = CPU::BitExtractUINT32(data2, 3, 4);
  // uintptr_t nuh_temporary_id_plus1 = CPU::BitExtractUINT32(data2, 0, 3);

  // Retrieve the picture order count from the H.265 slice header. An H.265
  // slice header is a variable-length header written in Section 7.3.6. For
  // example, an H.265-slice-header fragment `e0 26 ...` represents the
  // following parameters.
  //   +-------------------------+------------+-------------+
  //   | field                   | code       | value       |
  //   +-------------------------+------------+-------------+
  //   | first_slice_in_pic_flag | 1          | 1           |
  //   | pps_id                  | 1          | 0           |
  //   | slice_type              | 1          | 0 (SLICE_B) |
  //   | picture_order_count_lsb | 0000000100 | 4           |
  //     ...
  //   +-------------------------+------------+-------------+
  const uint8_t* slice_top = &packet_data[4 + 2];
  const uint8_t* slice_end = &packet_data[packet_size];
  BitStreamReader reader;
  reader.Initialize(slice_top, slice_end);
  uint8_t first_slice_segment_in_pic_flag = reader.GetBit<uint8_t>();
  if (IsIRAP(nal_unit_type)) {
    reader.SkipBits(1);  // no_output_of_prior_pics_flag
  }
  uint8_t slice_pic_parameter_set_id = reader.GetGolomb<uint8_t>();
  const PictureParameterSet* pps = &pps_[slice_pic_parameter_set_id];
  const SequenceParameterSet* sps = &sps_[pps->pps_seq_parameter_set_id];

  uint8_t dependent_slice_segment_flag = 0;
  if (!first_slice_segment_in_pic_flag) {
    HEVC_NOT_IMPLEMENTED();
    return 0;
  }
  uint32_t picture_order_count = 0;
  if (!dependent_slice_segment_flag) {
    reader.SkipBits(pps->num_extra_slice_header_bits);  // slice_reserved_flag[i]
    reader.SkipGolomb();  // slice_type
    if (pps->output_flag_present_flag) {
      reader.SkipBits(1);  // pic_output_flag
    }
    if (sps->separate_colour_plane_flag) {
      reader.SkipBits(2);  // colour_plane_id
    }
    if (!IsIDR(nal_unit_type)) {
      // Read the `picture_order_count_lsb` field to calculate the picture order
      // count of this slice. (HEVC with Alpha video clips generated by Video
      // Toolbox always sets zero to their `picture_order_count_msb` values,
      // i.e. the picture order count is equal to the `picture_order_count_lsb`
      // value.
      // TODO(hbono): calculate the picture order count as written in the
      // specification.
      uint32_t picture_order_count_lsb =
          reader.GetBits<uint32_t>(sps->log2_max_pic_order_cnt_lsb);
      picture_order_count = picture_order_count_lsb;
   }
  }
  return picture_order_count;
}

uintptr_t Decoder::ExtractRBSP(const uint8_t* data,
                               uintptr_t size,
                               uint8_t* rbsp) const {
  uint8_t* rbsp_top = rbsp;
  uint8_t* rbsp_end = rbsp;
#if __arm__ || __aarch64__
  if (CPU::HaveNEON()) {
    // Remove padding bytes with NEON. (This code uses NEON just for comparing
    // eight bytes at once.)
    uint64_t last_mask_eq_00 = 0;
    do {
      uint64_t data_word = CPU::LoadUINT64LE(data);
      uintptr_t data_size = sizeof(uint64_t) < size ? sizeof(uint64_t) : size;

   	   // Create a bit-mask representing the third byte of `0x00 0x00 0x03`.
      uint8x8_t d0 = vcreate_u8(data_word);
      uint8x8_t d1 = vceq_u8(d0, vdup_n_u8(0));
      uint8x8_t d2 = vcle_u8(d0, vdup_n_u8(3));
      uint64_t mask_eq_00 = vget_lane_u64(vreinterpret_u8_u64(d1), 0);
      uint64_t mask_le_03 = vget_lane_u64(vreinterpret_u8_u64(d2), 0);
      uint64_t mask_eq_00xxxx =
          (mask_eq_00 << 16) | (last_mask_eq_00 >> (64 - 16));
      uint64_t mask_eq_00xx =
          (mask_eq_00 << 8) | (last_mask_eq_00 >> (64 - 8));
      uint64_t mask_le_000003 = mask_eq_00xxxx & mask_eq_00xx & mask_le_03;
      if (data_size < sizeof(uint64_t)) {
        mask_le_000003 &= (1ULL << (data_size << 3)) - 1ULL;
      }
      data += data_size;
      size -= data_size;
      last_mask_eq_00 = mask_eq_00;
      if (!mask_le_000003) {
        CPU::StoreUINT64LE(rbsp_end, data_word);
        rbsp_end += data_size;
      } else {
        // Write the word except the third byte of `0x00 0x00 0x03`.
        data_size = sizeof(uint64_t);
        do {
          const unsigned int index = __builtin_ctzll(mask_le_000003) >> 3;
          CPU::StoreUINT64LE(rbsp_end, data_word);
          rbsp_end += index;
          const unsigned int shift = index << 3;
          mask_le_000003 >>= shift;
          mask_le_000003 >>= 8;
          data_word >>= shift;
          data_word >>= 8;
          data_size -= index + 1;
        } while (mask_le_000003);
        if (data_size > 0) {
          CPU::StoreUINT64LE(rbsp_end, data_word);
          rbsp_end += data_size;
        }
      }
    } while (size > 0);
    return static_cast<int>(rbsp_end - rbsp_top);
  }
#endif
#if __SIZEOF_SIZE_T__ == 8
  // Remove padding bytes on 64-bit CPUs.
  uint64_t last_mask_eq_00 = 0;
  do {
    uint64_t data_word = CPU::LoadUINT64LE(data);
    uintptr_t data_size = sizeof(uint64_t) < size ? sizeof(uint64_t) : size;

    // Find a byte sequence `0x00 0x00 0x03` in a 64-bit word. This code
    // consists of three parts:
    // 1. Create a bit-mask of non-zero bytes in the 64-bit word;
    // 2. Create a bit-mask of bytes greater than 0x03 in the 64-bit word, and;
    // 3. Find a byte sequence `0x00, 0x00, 0x03` in the 64-bit word.
    //
    // The first step calculates the logical sums of its bits in parallel.
    //
    //    b0                      b1 b2    b3 b4          b5 b6    b7
    //    b1                      b2 b3    b4 b5          b6 b7    0
    //   -------------------------------------------------------------
    //    b0|b1                   *  b2|b3 *  b4|b5       *  b6|b7 *
    //
    //    b0|b1                   *  b2|b3 *  b4|b5       *  b6|b7 *
    //    b2|b3                   *  b4|b5 *  b6|b7       *  0     0
    //   -------------------------------------------------------------
    //    b0|b1|b2|b3             *  *     *  b4|b5|b6|b7 *  *     *
    //
    //    b0|b1|b2|b3             *  *     *  b4|b5|b6|b7 *  *     *
    //    b4|b5|b6|b7             *  *     *  0           0  0     0
    //   -------------------------------------------------------------
    //    b0|b1|b2|b3|b4|b5|b6|b7 *  *     *  *           *  *     *
    uint64_t mask_eq_00 = data_word | (data_word >> 1);
    uint64_t mask_le_03 = mask_eq_00 & ~0x0101010101010101ULL;
    mask_eq_00 = mask_eq_00 | (mask_eq_00 >> 2);
    mask_le_03 = mask_le_03 | (mask_le_03 >> 2);
    mask_eq_00 = mask_eq_00 | (mask_eq_00 >> 4);
    mask_le_03 = mask_le_03 | (mask_le_03 >> 4);
    mask_eq_00 = ~mask_eq_00;
    mask_le_03 = ~mask_le_03;

    // Compose the bit-masks into a bit-mask so each byte represents whether or
    // not it is the third byte of a byte sequence `0x00, 0x00, 0x03`.
    uint64_t mask_eq_00xxxx =
        (mask_eq_00 << 16) | (last_mask_eq_00 >> (64 - 16));
    uint64_t mask_eq_00xx =
        (mask_eq_00 << 8) | (last_mask_eq_00 >> (64 - 8));
    uint64_t mask_le_000003 =
        mask_eq_00xxxx & mask_eq_00xx & mask_le_03 & 0x0101010101010101ULL;
    if (data_size < sizeof(uint64_t)) {
      mask_le_000003 &= (1ULL << (data_size << 3)) - 1ULL;
    }
    data += data_size;
    size -= data_size;
    last_mask_eq_00 = mask_eq_00;
    if (!mask_le_000003) {
      CPU::StoreUINT64LE(rbsp_end, data_word);
      rbsp_end += data_size;
    } else {
      // Write the word except the third byte of `0x00 0x00 0x03`.
      data_size = sizeof(uint64_t);
      do {
        const unsigned int index = __builtin_ctzll(mask_le_000003) >> 3;
        CPU::StoreUINT64LE(rbsp_end, data_word);
        rbsp_end += index;
#if __i386__ || __x86_64__ || __arm__ || __aarch64__
        // Decompose a shift operation into two shift operations to avoid
        // shifting a 64-bit variable by 64 bits. (It is an undefined
        // operation.)
        const unsigned int shift = index << 3;
        mask_le_000003 >>= shift;
        mask_le_000003 >>= 8;
        data_word >>= shift;
        data_word >>= 8;
#else
        const unsigned int shift = (index + 1) << 3;
        mask_le_000003 >>= shift;
        data_word >>= shift;
#endif
        data_size -= index + 1;
      } while (mask_le_000003);
      if (data_size > 0) {
        CPU::StoreUINT64LE(rbsp_end, data_word);
        rbsp_end += data_size;
      }
    }
  } while (size > 0);
  return static_cast<int>(rbsp_end - rbsp_top);
#endif
}

#if __APPLE__
int Decoder::DecodeSample(int sample_number,
                          VTDecompressionOutputHandler handler) {
  // Create a `CMBlockBuffer` object referring to the specified sample. (This
  // decoder retains the whole input QuickTime stream and Core Media does not
  // have to create copies of its samples.)
  const Sample* sample = &samples_[sample_number];
  HEVC_LOG_V("%s(): samples[%d] = { offset: %x, size: %d }\n", __FUNCTION__,
             sample_number, sample->offset, sample->size);
  void* data = static_cast<void*>(&data_[sample->offset]);
  int size = sample->size;
  CMBlockBufferRef block_buffer;
  OSStatus status = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault,
      data, size, kCFAllocatorNull, NULL, 0, size, 0, &block_buffer);
  if (!status) {
    // Create a `CMSampleTimingInfo` object representing the sample duration of
    // the source QuickTime stream so the given handler can use it. (Without
    // attaching a `CMSampleTimingInfo` object to a `CMSampleBuffer` object,
    // the `VTDecompressionSessionDecodeFrameWithOutputHandler()` returns an
    // error on iOS.)
    CMSampleTimingInfo timing_info;
    timing_info.duration = CMTimeMake(sample->duration, time_scale_);
    timing_info.presentationTimeStamp =
        CMTimeMake(sample_number * sample->duration, time_scale_);
    timing_info.decodeTimeStamp = kCMTimeInvalid;
    CMSampleBufferRef sample_buffer;
    status = CMSampleBufferCreate(kCFAllocatorDefault, block_buffer, TRUE, 0, 0,
        format_description_, 1, 1, &timing_info, 0, NULL, &sample_buffer);
    if (!status) {
      status = VTDecompressionSessionDecodeFrameWithOutputHandler(
          decoder_session_,
          sample_buffer,
          kVTDecodeFrame_EnableAsynchronousDecompression,
          NULL,
          handler);
      CFRelease(sample_buffer);
    }
    CFRelease(block_buffer);
  }
  return status;
}

void Decoder::InitializeVideoToolbox() {
#if 0
  // The `Decoder::Initialize()` function fills all variables with 0, i.e. this
  // function does not have to set NULL to variables.
  format_description_ = NULL;
  decoder_session_ = NULL;
  hvcc_extension_ = NULL;
  decoder_callback_ = NULL;
  decoder_object_ = NULL;
#endif
}

int Decoder::CreateVideoToolbox(
    const mov::VideoSampleDescriptionExtension* extension,
    int frame_width,
    int frame_height,
    OutputCallback callback,
    void* object) {
  OSStatus status = kVTParameterErr;
  CFMutableDictionaryRef decoder_config;

  decoder_config = CFDictionaryCreateMutable(
      kCFAllocatorDefault,
      0,
      &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);
  if (decoder_config) {
    CFMutableDictionaryRef hvcc_info;
    CFDataRef extra_data;
    CFMutableDictionaryRef buffer_attributes;

    // Initialize the decoder configuration to decode HEVC samples. This code is
    // equivalent to the following swift code.
    // ```
    // let decoder_configuration = [
    //   String(kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder): true,
    //   String(kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms): [
    //     String("hvcC"): [...]
    //   ]
    // ] as CFDictionary
    // ```
#if TARGET_OS_IPHONE == 0
    CFDictionarySetValue(
        decoder_config,
        kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder,
        kCFBooleanTrue);
#endif
    hvcc_info = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                          1,
                                          &kCFTypeDictionaryKeyCallBacks,
                                          &kCFTypeDictionaryValueCallBacks);
    if (!hvcc_info) {
      goto release_configuration;
    }
    extra_data = CFDataCreate(kCFAllocatorDefault,
                              extension->GetExtraData(),
                              extension->GetExtraSize());
    if (!extra_data) {
      goto release_configuration;
    }
    CFDictionarySetValue(hvcc_info, CFSTR("hvcC"), extra_data);
    CFRelease(extra_data);
    CFDictionarySetValue(
        decoder_config,
        kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
        hvcc_info);
    CFRelease(hvcc_info);

    // Create a format description with the above decoder configuration to
    // decode this QuickTime stream as an HEVC with Alpha stream. (If we do not
    // specify this QuickTime stream is an HEVC with Alpha stream, Video Toolbox
    // overwrites the output alpha components with 255.)
    // TODO(hbono): we need to parse the `hvcC` extra data to identify this
    // QuickTime stream is an HEVC with Alpha stream.)
    status = CMVideoFormatDescriptionCreate(kCFAllocatorDefault,
                                            kCMVideoCodecType_HEVCWithAlpha,
                                            frame_width,
                                            frame_height,
                                            decoder_config,
                                            &format_description_);
    if (status) {
      goto release_configuration;
    }

    // Create the attributes of the output `CVPixelBuffer` objects. The code is
    // equivalent to the following swift code.)
    // ```
    // let buffer_attributes = [
    //   String(kCVPixelBufferOpenGLESCompatibilityKey): true,
    //   String(kCVPixelBufferMetalCompatibilityKey): true,
    //   String(kCVPixelBufferIOSurfacePropertiesKey): [
    //     "IOSurfaceOpenGLESFBOCompatibility": true,
    //     "IOSurfaceOpenGLESTextureCompatibility": true,
    //     "IOSurfaceCoreAnimationCompatibility": true,
    //   ]
    // ] as CFDictionary
    // ```
    // (An output `CVPixelBuffer` object must be an `IOSurface` object
    // compatible both with Metal and with OpenGL ES.)
    buffer_attributes = CFDictionaryCreateMutable(
        kCFAllocatorDefault,
        4,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (buffer_attributes) {
      CFMutableDictionaryRef io_surface_properties;
      VTDecompressionOutputCallbackRecord callback_record;
#if TARGET_OS_IPHONE
      CFDictionarySetValue(buffer_attributes,
                           kCVPixelBufferOpenGLESCompatibilityKey,
                           kCFBooleanTrue);
#else
      CFDictionarySetValue(buffer_attributes,
                           kCVPixelBufferIOSurfaceOpenGLTextureCompatibilityKey,
                           kCFBooleanTrue);
      CFDictionarySetValue(buffer_attributes,
                           kCVPixelBufferIOSurfaceOpenGLFBOCompatibilityKey,
                           kCFBooleanTrue);
      CFDictionarySetValue(buffer_attributes,
                           kCVPixelBufferIOSurfaceCoreAnimationCompatibilityKey,
                           kCFBooleanTrue);
#endif
      CFDictionarySetValue(buffer_attributes,
                           kCVPixelBufferMetalCompatibilityKey,
                           kCFBooleanTrue);

      io_surface_properties = CFDictionaryCreateMutable(
          kCFAllocatorDefault,
          0,
          &kCFTypeDictionaryKeyCallBacks,
          &kCFTypeDictionaryValueCallBacks);
      if (!io_surface_properties) {
        goto release_attributes;
      }
#if TARGET_OS_IPHONE
      CFDictionarySetValue(io_surface_properties,
                           CFSTR("IOSurfaceOpenGLESFBOCompatibility"),
                           kCFBooleanTrue);
      CFDictionarySetValue(io_surface_properties,
                           CFSTR("IOSurfaceOpenGLESTextureCompatibility"),
                           kCFBooleanTrue);
      CFDictionarySetValue(io_surface_properties,
                           CFSTR("IOSurfaceCoreAnimationCompatibility"),
                           kCFBooleanTrue);
#endif
      CFDictionarySetValue(buffer_attributes,
                           kCVPixelBufferIOSurfacePropertiesKey,
                           io_surface_properties);
      CFRelease(io_surface_properties);

      // Create a decoder session.
      callback_record.decompressionOutputCallback = callback;
      callback_record.decompressionOutputRefCon = object;
      status = VTDecompressionSessionCreate(NULL,
                                            format_description_,
                                            decoder_config,
                                            buffer_attributes,
                                            callback ? &callback_record : NULL,
                                            &decoder_session_);
      if (!status) {
        hvcc_extension_ = extension;
        decoder_callback_ = callback;
        decoder_object_ = object;
      }

 release_attributes:
      CFRelease(buffer_attributes);
    }

 release_configuration:
    CFRelease(decoder_config);
  }

  return status;
}

void Decoder::DestroyVideoToolbox() {
  if (decoder_session_) {
    VTDecompressionSessionWaitForAsynchronousFrames(decoder_session_);
    CFRelease(decoder_session_);
    decoder_session_ = NULL;
  }
  if (format_description_) {
    CFRelease(format_description_);
    format_description_ = NULL;
  }
}

int Decoder::ResetVideoToolbox() {
  DestroyVideoToolbox();
  if (hvcc_extension_) {
    return CreateVideoToolbox(hvcc_extension_, frame_width_, frame_height_,
        decoder_callback_, decoder_object_);
  }
  return -1;
}

int Decoder::DecodeSampleVideoToolbox(int sample_number) {
  const Sample* sample = &samples_[sample_number];
  void* data = static_cast<void*>(&data_[sample->offset]);
  int size = sample->size;
  CMBlockBufferRef block_buffer;
  OSStatus status = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault,
      data, size, kCFAllocatorNull, NULL, 0, size, 0, &block_buffer);
  if (!status) {
    CMSampleBufferRef sample_buffer;
    status = CMSampleBufferCreate(kCFAllocatorDefault, block_buffer, TRUE, 0, 0,
        format_description_, 1, 0, NULL, 0, NULL, &sample_buffer);
    if (!status) {
      status = VTDecompressionSessionDecodeFrame(
          decoder_session_, sample_buffer, 0, NULL, NULL);
      if (!status) {
        status =
            VTDecompressionSessionWaitForAsynchronousFrames(decoder_session_);
      }
      CFRelease(sample_buffer);
    }
  }
  return status;
}
#endif

}  // namespace hevc
