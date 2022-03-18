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

#ifndef HEVC_DECODER_H_
#define HEVC_DECODER_H_

#include <stddef.h>
#include <stdint.h>
#if __APPLE__
#include <VideoToolbox/VideoToolbox.h>
#endif
#include "../base/intrin.h"

namespace mov {
struct VideoSampleDescriptionExtension;
struct AtomCollection;
}  // namespace mov

namespace hevc {

/**
 * H.265 NAL (Network Abstract Layer) unit types defined in Section 7.4.2.2. 
 * @enum {int}
 */
enum NALUnitType {
  NAL_TRAIL_N        = 0,
  NAL_TRAIL_R        = 1,
  NAL_TSA_N          = 2,
  NAL_TSA_R          = 3,
  NAL_STSA_N         = 4,
  NAL_STSA_R         = 5,
  NAL_RADL_N         = 6,
  NAL_RADL_R         = 7,
  NAL_RASL_N         = 8,
  NAL_RASL_R         = 9,
  NAL_VCL_N10        = 10,
  NAL_VCL_R11        = 11,
  NAL_VCL_N12        = 12,
  NAL_VCL_R13        = 13,
  NAL_VCL_N14        = 14,
  NAL_VCL_R15        = 15,
  NAL_BLA_W_LP       = 16,
  NAL_BLA_W_RADL     = 17,
  NAL_BLA_N_LP       = 18,
  NAL_IDR_W_RADL     = 19,
  NAL_IDR_N_LP       = 20,
  NAL_CRA_NUT        = 21,
  NAL_RSV_IRAP_VCL22 = 22,
  NAL_RSV_IRAP_VCL23 = 23,
  NAL_RSV_VCL24      = 24,
  NAL_RSV_VCL25      = 25,
  NAL_RSV_VCL26      = 26,
  NAL_RSV_VCL27      = 27,
  NAL_RSV_VCL28      = 28,
  NAL_RSV_VCL29      = 29,
  NAL_RSV_VCL30      = 30,
  NAL_RSV_VCL31      = 31,
  NAL_VPS            = 32,
  NAL_SPS            = 33,
  NAL_PPS            = 34,
  NAL_AUD            = 35,
  NAL_EOS_NUT        = 36,
  NAL_EOB_NUT        = 37,
  NAL_FD_NUT         = 38,
  NAL_SEI_PREFIX     = 39,
  NAL_SEI_SUFFIX     = 40,
  NAL_RSV_NVCL41     = 41,
  NAL_RSV_NVCL42     = 42,
  NAL_RSV_NVCL43     = 43,
  NAL_RSV_NVCL44     = 44,
  NAL_RSV_NVCL45     = 45,
  NAL_RSV_NVCL46     = 46,
  NAL_RSV_NVCL47     = 47,
  NAL_UNSPEC48       = 48,
  NAL_UNSPEC49       = 49,
  NAL_UNSPEC50       = 50,
  NAL_UNSPEC51       = 51,
  NAL_UNSPEC52       = 52,
  NAL_UNSPEC53       = 53,
  NAL_UNSPEC54       = 54,
  NAL_UNSPEC55       = 55,
  NAL_UNSPEC56       = 56,
  NAL_UNSPEC57       = 57,
  NAL_UNSPEC58       = 58,
  NAL_UNSPEC59       = 59,
  NAL_UNSPEC60       = 60,
  NAL_UNSPEC61       = 61,
  NAL_UNSPEC62       = 62,
  NAL_UNSPEC63       = 63,
};

/**
 * H.265 slice types defined in Section 7.4.7.1.
 * @enum {int}
 */
enum SliceType {
  SLICE_B = 0,
  SLICE_P = 1,
  SLICE_I = 2,
};

/**
 * H.265 scalability-mask indices defined in Annex F.7.4.3.1.1. (These indices
 * use most-significant-bit first order so this decoder can read scalability
 * masks at once.)
 * @enum {int}
 */
enum ScalabilityMaskIndex {
  DEPTH_LAYER_FLAG = 15 - 0,
  VIEW_ORDER_IDX   = 15 - 1,
  DEPENDENCY_ID    = 15 - 2,
  AUX_ID           = 15 - 3,
};

/**
 * H.265 auxiliary IDs defined in Annex F.7.4.3.1.1.
 * @enum {int}
 */
enum AuxId {
  AUX_ALPHA = 1,
  AUX_DEPTH = 2,
};

enum {
  // Section 7.4.3.1: `vps_max_layers_minus1` is in `[0,62]`.
  MAX_LAYERS = 63,
  // Section 7.4.3.1: `vps_max_sub_layers_minus1` is in `[0,6]`.
  MAX_SUB_LAYERS = 7,
};

/**
 * H.265 SEI (Supplemental Enhancement Information) message types defined in
 * Annex D.2.1.
 * @enum {int}
 */
enum SEIMessageType {
  SEI_TYPE_BUFFERING_PERIOD                            = 0,
  SEI_TYPE_PIC_TIMING                                  = 1,
  SEI_TYPE_PAN_SCAN_RECT                               = 2,
  SEI_TYPE_FILLER_PAYLOAD                              = 3,
  SEI_TYPE_USER_DATA_REGISTERED_ITU_T_T35              = 4,
  SEI_TYPE_USER_DATA_UNREGISTERED                      = 5,
  SEI_TYPE_RECOVERY_POINT                              = 6,
  SEI_TYPE_DEC_REF_PIC_MARKING_REPETITION              = 7,
  SEI_TYPE_SPARE_PIC                                   = 8,
  SEI_TYPE_SCENE_INFO                                  = 9,
  SEI_TYPE_SUB_SEQ_INFO                                = 10,
  SEI_TYPE_SUB_SEQ_LAYER_CHARACTERISTICS               = 11,
  SEI_TYPE_SUB_SEQ_CHARACTERISTICS                     = 12,
  SEI_TYPE_FULL_FRAME_FREEZE                           = 13,
  SEI_TYPE_FULL_FRAME_FREEZE_RELEASE                   = 14,
  SEI_TYPE_FULL_FRAME_SNAPSHOT                         = 15,
  SEI_TYPE_PROGRESSIVE_REFINEMENT_SEGMENT_START        = 16,
  SEI_TYPE_PROGRESSIVE_REFINEMENT_SEGMENT_END          = 17,
  SEI_TYPE_MOTION_CONSTRAINED_SLICE_GROUP_SET          = 18,
  SEI_TYPE_FILM_GRAIN_CHARACTERISTICS                  = 19,
  SEI_TYPE_DEBLOCKING_FILTER_DISPLAY_PREFERENCE        = 20,
  SEI_TYPE_STEREO_VIDEO_INFO                           = 21,
  SEI_TYPE_POST_FILTER_HINT                            = 22,
  SEI_TYPE_TONE_MAPPING_INFO                           = 23,
  SEI_TYPE_SCALABILITY_INFO                            = 24,
  SEI_TYPE_SUB_PIC_SCALABLE_LAYER                      = 25,
  SEI_TYPE_NON_REQUIRED_LAYER_REP                      = 26,
  SEI_TYPE_PRIORITY_LAYER_INFO                         = 27,
  SEI_TYPE_LAYERS_NOT_PRESENT_4                        = 28,
  SEI_TYPE_LAYER_DEPENDENCY_CHANGE                     = 29,
  SEI_TYPE_SCALABLE_NESTING_4                          = 30,
  SEI_TYPE_BASE_LAYER_TEMPORAL_HRD                     = 31,
  SEI_TYPE_QUALITY_LAYER_INTEGRITY_CHECK               = 32,
  SEI_TYPE_REDUNDANT_PIC_PROPERTY                      = 33,
  SEI_TYPE_TL0_DEP_REP_INDEX                           = 34,
  SEI_TYPE_TL_SWITCHING_POINT                          = 35,
  SEI_TYPE_PARALLEL_DECODING_INFO                      = 36,
  SEI_TYPE_MVC_SCALABLE_NESTING                        = 37,
  SEI_TYPE_VIEW_SCALABILITY_INFO                       = 38,
  SEI_TYPE_MULTIVIEW_SCENE_INFO_4                      = 39,
  SEI_TYPE_MULTIVIEW_ACQUISITION_INFO_4                = 40,
  SEI_TYPE_NON_REQUIRED_VIEW_COMPONENT                 = 41,
  SEI_TYPE_VIEW_DEPENDENCY_CHANGE                      = 42,
  SEI_TYPE_OPERATION_POINTS_NOT_PRESENT                = 43,
  SEI_TYPE_BASE_VIEW_TEMPORAL_HRD                      = 44,
  SEI_TYPE_FRAME_PACKING_ARRANGEMENT                   = 45,
  SEI_TYPE_MULTIVIEW_VIEW_POSITION_4                   = 46,
  SEI_TYPE_DISPLAY_ORIENTATION                         = 47,
  SEI_TYPE_MVCD_SCALABLE_NESTING                       = 48,
  SEI_TYPE_MVCD_VIEW_SCALABILITY_INFO                  = 49,
  SEI_TYPE_DEPTH_REPRESENTATION_INFO_4                 = 50,
  SEI_TYPE_THREE_DIMENSIONAL_REFERENCE_DISPLAYS_INFO_4 = 51,
  SEI_TYPE_DEPTH_TIMING                                = 52,
  SEI_TYPE_DEPTH_SAMPLING_INFO                         = 53,
  SEI_TYPE_CONSTRAINED_DEPTH_PARAMETER_SET_IDENTIFIER  = 54,
  SEI_TYPE_GREEN_METADATA                              = 56,
  SEI_TYPE_STRUCTURE_OF_PICTURES_INFO                  = 128,
  SEI_TYPE_ACTIVE_PARAMETER_SETS                       = 129,
  SEI_TYPE_PARAMETER_SETS_INCLUSION_INDICATION         = SEI_TYPE_ACTIVE_PARAMETER_SETS,
  SEI_TYPE_DECODING_UNIT_INFO                          = 130,
  SEI_TYPE_TEMPORAL_SUB_LAYER_ZERO_IDX                 = 131,
  SEI_TYPE_DECODED_PICTURE_HASH                        = 132,
  SEI_TYPE_SCALABLE_NESTING_5                          = 133,
  SEI_TYPE_REGION_REFRESH_INFO                         = 134,
  SEI_TYPE_NO_DISPLAY                                  = 135,
  SEI_TYPE_TIME_CODE                                   = 136,
  SEI_TYPE_MASTERING_DISPLAY_COLOUR_VOLUME             = 137,
  SEI_TYPE_SEGMENTED_RECT_FRAME_PACKING_ARRANGEMENT    = 138,
  SEI_TYPE_TEMPORAL_MOTION_CONSTRAINED_TILE_SETS       = 139,
  SEI_TYPE_CHROMA_RESAMPLING_FILTER_HINT               = 140,
  SEI_TYPE_KNEE_FUNCTION_INFO                          = 141,
  SEI_TYPE_COLOUR_REMAPPING_INFO                       = 142,
  SEI_TYPE_DEINTERLACED_FIELD_IDENTIFICATION           = 143,
  SEI_TYPE_CONTENT_LIGHT_LEVEL_INFO                    = 144,
  SEI_TYPE_DEPENDENT_RAP_INDICATION                    = 145,
  SEI_TYPE_CODED_REGION_COMPLETION                     = 146,
  SEI_TYPE_ALTERNATIVE_TRANSFER_CHARACTERISTICS        = 147,
  SEI_TYPE_AMBIENT_VIEWING_ENVIRONMENT                 = 148,
  SEI_TYPE_CONTENT_COLOUR_VOLUME                       = 149,
  SEI_TYPE_EQUIRECTANGULAR_PROJECTION                  = 150,
  SEI_TYPE_CUBEMAP_PROJECTION                          = 151,
  SEI_TYPE_FISHEYE_VIDEO_INFO                          = 152,
  SEI_TYPE_SPHERE_ROTATION                             = 154,
  SEI_TYPE_REGIONWISE_PACKING                          = 155,
  SEI_TYPE_OMNI_VIEWPORT                               = 156,
  SEI_TYPE_REGIONAL_NESTING                            = 157,
  SEI_TYPE_MCTS_EXTRACTION_INFO_SETS                   = 158,
  SEI_TYPE_MCTS_EXTRACTION_INFO_NESTING                = 159,
  SEI_TYPE_LAYERS_NOT_PRESENT_5                        = 160,
  SEI_TYPE_INTER_LAYER_CONSTRAINED_TILE_SETS           = 161,
  SEI_TYPE_BSP_NESTING                                 = 162,
  SEI_TYPE_BSP_INITIAL_ARRIVAL_TIME                    = 163,
  SEI_TYPE_SUB_BITSTREAM_PROPERTY                      = 164,
  SEI_TYPE_ALPHA_CHANNEL_INFO                          = 165,
  SEI_TYPE_OVERLAY_INFO                                = 166,
  SEI_TYPE_TEMPORAL_MV_PREDICTION_CONSTRAINTS          = 167,
  SEI_TYPE_FRAME_FIELD_INFO                            = 168,
  SEI_TYPE_THREE_DIMENSIONAL_REFERENCE_DISPLAYS_INFO   = 176,
  SEI_TYPE_DEPTH_REPRESENTATION_INFO_5                 = 177,
  SEI_TYPE_MULTIVIEW_SCENE_INFO_5                      = 178,
  SEI_TYPE_MULTIVIEW_ACQUISITION_INFO_5                = 179,
  SEI_TYPE_MULTIVIEW_VIEW_POSITION_5                   = 180,
  SEI_TYPE_ALTERNATIVE_DEPTH_INFO                      = 181,
  SEI_TYPE_SEI_MANIFEST                                = 200,
  SEI_TYPE_SEI_PREFIX_INDICATION                       = 201,
  SEI_TYPE_ANNOTATED_REGIONS                           = 202,
  SEI_TYPE_SUBPIC_LEVEL_INFO                           = 203,
  SEI_TYPE_SAMPLE_ASPECT_RATIO_INFO                    = 204,
};

/**
 * The class that encapsulates a subset of an H.265 PTL (Profile, Tier, and
 * Level) defined in Section 7.3.3.
 */
struct ProfileTierLevel {
  /**
   * The bit-mask representing an array of profile compatibility flags. This
   * variable stores profile compatibilities in the most-significant-bit first
   * order, i.e. the 0-th bit of this variable represents the 31-th bit of the
   * `profile_compatibility_flag[]` array.
   *   +-------+--------------------------------+
   *   | index | field                          |
   *   +-------+--------------------------------+
   *   | 0     | profile_compatibility_flag[31] |
   *   | 1     | profile_compatibility_flag[30] |
   *     ...
   *   | 31    | profile_compatibility_flag[0]  |
   *   +-------+--------------------------------+
   * @type {uint32_t}
   */
  uint32_t general_profile_compatibility_flags;

  /**
   * The profile space.
   * @type {uint8_t}
   */
  uint8_t general_profile_space;

  /**
   * The tier.
   * @type {uint8_t}
   */
  uint8_t general_tier_flag;

  /**
   * The profile.
   *   +-------+--------------------+
   *   | value | profile            |
   *   +-------+--------------------+
   *   | 0     | Main               |
   *   | 1     | Main 10            |
   *   | 2     | Main Still Picture |
   *   | 3     | Range Extension    |
   *   +-------+--------------------+
   * @type {uint8_t}
   */
  uint8_t general_profile_idc;

  /**
   * Whether or not this H.265 stream is a progressive stream.
   * @type {uint8_t}
   */
  uint8_t general_progressive_source_flag;

  /**
   * Whether or not this H.265 stream is an interlaced stream.
   * @type {uint8_t}
   */
  uint8_t general_interlaced_source_flag;

  /**
   * Whether or not this H.265 stream does not have frame-packing-arrangement
   * SEI messages.
   * @type {uint8_t}
   */
  uint8_t general_non_packed_constraint_flag;

  /**
   * Whether or not `frame_seq_flag` is 0.
   * @type {uint8_t}
   */
  uint8_t general_frame_only_constraint_flag;

  /**
   * Whether or not the `INBLD` capability is required for decoding this H.265
   * stream.
   * @type {uint8_t}
   */
  uint8_t general_inbld_flag;

  /**
   * The 30 times of the level number. The following table enumerates possible
   * values defined by the current H.265 specification.
   *   +-------+-------+
   *   | value | level |
   *   +-------+-------+
   *   | 30    | 1     |
   *   | 60    | 2     |
   *   | 63    | 2.1   |
   *   | 90    | 3     |
   *   | 93    | 3.1   |
   *   | 120   | 4     |
   *   | 123   | 4.1   |
   *   | 150   | 5     |
   *   | 153   | 5.1   |
   *   | 156   | 5.2   |
   *   | 180   | 6     |
   *   | 183   | 6.1   |
   *   | 186   | 6.2   |
   *   +-------+-------+
   * @type {uint8_t}
   */
  uint8_t general_level_idc;
};

/**
 * The class that encapsulates a subset of an H.265 VPS (Video Parameter Set)
 * defined in Section 7.3.2.1 and Annex F.7.3.2.1.
 */
struct VideoParameterSet {
  /**
   * The inner class that encapsulates a subset of an H.265 VPS (Video
   * Parameter Set) extension defined in Annex F.7.3.2.1.
   */
  struct Extension {
    /**
     * Layer IDs.
     * @type {uint8[63][16]}
     */
    uint8_t layer_id_in_nuh[MAX_LAYERS];

    /**
     * Dimension IDs.
     * @type {uint8[63][16]}
     */
    uint8_t dimension_id[MAX_LAYERS][16];

    /**
     * Conformance level. (This value is multiplied by 30, e.g. 120 represents
     * Conformance Level 4.0.)
     * @type {uint8_t}
     */
    uint8_t general_level_idc;
  };

  /**
   * The H.265 PTL of the main stream.
   * @type {hevc::ProfileTierLevel}
   */
  ProfileTierLevel profile_tier_level;

  /**
   * The H.265 VPS extension.
   * @type {hevc::VideoParameterSet::Extension}
   */
  Extension extension;

  /**
   * The ID of this VPS.
   * @type {uint8_t}
   */
  uint8_t vps_video_parameter_set_id;

  /**
   * The maximum allowed number of layers.
   * @type {uint8_t}
   */
  uint8_t vps_max_layers_minus1;

  /**
   * The maximum allowed number of sub-layers.
   * @type {uint8_t}
   */
  uint8_t vps_max_sub_layers_minus1;
  //uint8_t vps_temporal_id_nesting_flag;

  /**
   * Whether or not this VPS has `vps_max_dec_pic_buffering_minus1[]`,
   * 'vps_num_reorder_pics[]`, `vps_max_latency_increase_plus1[]` for its
   * sub-layers.
   * @type {uint8_t}
   */
  uint8_t vps_sub_layer_ordering_info_present_flag;

  //uint8_t vps_max_dec_pic_buffering_minus1[MAX_SUB_LAYERS];
  //uint8_t vps_num_reorder_pics[MAX_SUB_LAYERS];
  //uint8_t vps_max_latency_increase_plus1[MAX_SUB_LAYERS];

  /**
   * The maximum allowed value for `nuh_layer_id`.
   * @type {uint8_t}
   */
  uint8_t vps_max_layer_id;

  /**
   * The number of layer sets specified by this VPS.
   * @type {uint8_t}
   */
  uint8_t vps_num_layer_sets_minus1;

  // uint8_t layer_id_included_flag[][];

  /**
   * Whether or not this VPS has `vps_num_units_in_tick`, `vps_time_scale`,
   * `vps_poc_proportional_to_timing_flag`, and `vps_num_hrd_parameters`.
   * @type {uint8_t}
   */
  uint8_t vps_timing_info_present_flag;
  // uint32_t vps_num_units_in_tick;
  // uint8_t vps_poc_proportional_to_timing_flag;
  // uint8_t vps_num_ticks_poc_diff_one_minus1;
  // uint8_t vps_num_hrd_parameters;

  /**
   * Whether or not this VPS has an H.265 VPS extension.
   * @type {uint8_t}
   */
  uint8_t vps_extension_flag;
};

/**
 * The class that encapsulates a subset of an H.265 SPS (Sequence Parameter Set)
 * defined in Section 7.3.2.2.
 */
struct SequenceParameterSet {
  /**
   * The H.265 PTL of this SPS.
   * @type {hevc::ProfileTierLevel}
   */
  ProfileTierLevel profile_tier_level;

  /**
   * The width of a decoded picture.
   * @type {uint16_t}
   */
  uint16_t pic_width_in_luma_samples;

  /**
   * The height of a decoded picture.
   * @type {uint16_t}
   */
  uint16_t pic_height_in_luma_samples;

  /**
   * The ID of the active VPS.
   * @type {uint8_t}
   */
  uint8_t sps_video_parameter_set_id;

  /**
   * The maximum number of temporal sub-layers.
   * @type {uint8_t}
   */
  uint8_t sps_max_sub_layers_minus1;

  /**
   * The ID of this SPS.
   * @type {uint8_t}
   */
  uint8_t sps_seq_parameter_set_id;

  /**
   * The chroma sampling relative to the luma sampling.
   * @type {uint8_t}
   */
  uint8_t chroma_format_idc;

  /**
   * Whether or not the three color components (Y, U, and V) are coded
   * separately.
   * @type {uint8_t}
   */
  uint8_t separate_colour_plane_flag;

  /**
   * The bit depth of the luma samples, i.e. `bit_depth_luma_minus8` + 8.
   * @type {uint8_t}
   */
  uint8_t bit_depth_luma;

  /**
   * The bit depth of the chroma samples, i.e. `bit_depth_chroma_minus8` + 8.
   * @type {uint8_t}
   */
  uint8_t bit_depth_chroma;

  /**
   * The bit length of a `MaxPicOrderCntLsb` value in a slice header, i.e.
   * `log2_max_pic_order_cnt_lsb_minus4` + 4.
   * @type {uint8_t}
   */
  uint8_t log2_max_pic_order_cnt_lsb;

  // uint8_t scaling_list_enable_flag;
  // uint8_t amp_enabled_flag;
  // uint8_t sample_adaptive_offset_enabled_flag;
  // uint8_t pcm_enabled_flag;
};

/**
 * The class that encapsulates an H.265 PPS (Picture Parameter Set) defined in
 * Section 7.3.2.3.
 */
struct PictureParameterSet {
  /**
   * The ID of this PPS.
   * @type {uint8_t}
   */
  uint8_t pps_pic_parameter_set_id;

  /**
   * The ID of the active SPS.
   * @type {uint8_t}
   */
  uint8_t pps_seq_parameter_set_id;

  /**
   * Whether or not the slice headers associated with this PPS has
   * `dependent_slice_segment_flag`.
   * @type {uint8_t}
   */
  uint8_t dependent_slice_segments_enabled_flag;

  /**
   * Whether or not the slice headers associated with this PPS has
   * `pic_output_flag`.
   * @type {uint8_t}
   */
  uint8_t output_flag_present_flag;

  /**
   * The length of `slice_reserved_flag[]` in the slice headers associated with
   * this PPS.
   * @type {uint8_t}
   */
  uint8_t num_extra_slice_header_bits;
};

/**
 * SEI (Supplemental Enhancement Information) messages.
 */
namespace sei {

/**
 * The class that encapsulates an alpha-channel-information SEI (Supplemental
 * Enhancement Information) defined in Annex F.14.2.8.
 */
struct AlphaChannelInformation {
  uint8_t alpha_channel_cancel_flag;
  uint8_t alpha_channel_use_idc;
  uint8_t alpha_channel_bit_depth_minus8;
  uint8_t alpha_transparent_value;
  uint8_t alpha_opaque_value;
  uint8_t alpha_channel_incr_flag;
  uint8_t alpha_channel_clip_flag;
};

}  // namespace sei

/**
 * The class that decodes an HEVC-with-Alpha stream
 * <https://developer.apple.com/videos/play/wwdc2019/506/> An HEVC-with-Alpha
 * stream is a QuickTime stream consisting of two H.265 layers (a YUV layer and
 * an alpha layer).
 */
struct Decoder {
#if __APPLE__
  /**
   * The callback function called when this decoder finishes decoding a frame.
   * ```
   * void HandleDecodeFrame(void* object,
   *                        void* source_frame_refcon,
   *                        OSStatus status,
   *                        VTDecodeInfoFlags info_flags,
   *                        CVImageBufferRef image_buffer,
   *                        CMTime time_stamp,
   *                        CMTime duration) {
   *   ...
   * }
   * ```
   * @public
   */
  typedef VTDecompressionOutputCallback OutputCallback;

  /**
   * The block called when this decoder finishes decoding a frame.
   * DecodeFrame(0, ^(OSStatus status,
   *                  VTDecodeInfoFlags info_flags,
   *                  CVImageBufferRef image_buffer,
   *                  CMTime time_stamp,
   *                  CMTime duration) {
   * });
   * @public
   */
  typedef VTDecompressionOutputHandler OutputHandler;
#else
  /**
   * The callback function called when this decoder finishes decoding a frame.
   * ```
   * void HandleDecodeFrame(void* object) {
   *   ...
   * }
   * ```
   * @public
   */
  typedef void (*OutputCallback)(void* object);
#endif

  /**
   * The inner class encapsulating the information of a sample (or frame) in a
   * QuickTime stream.
   * @public
   */
  struct Sample {
    /**
     * The offset from the beginning of the QuickTime stream.
     * @type {uint32_t}
     * @public
     */
    uint32_t offset;

    /**
     * The sample size.
     * @type {uint32_t}
     * @public
     */
    uint32_t size;

    /**
     * The sample duration, in the time coordinate system declared in the `mdhd`
     * atom.
     * @type {uint32_t}
     * @public
     */
    uint32_t duration;

    /**
     * Whether of not this sample is a key frame.
     * @type {uint32_t}
     * @public
     */
    uint32_t picture_order_count;
  };

  /**
   * Initializes this decoder.
   */
  void Initialize();

  /**
   * Creates resources used by this decoder.
   * @param {const void*} data
   * @param {size_t} size
   * @param {hevc::Decoder::OutputCallback} callback
   * @param {void*} object
   * @return {int}
   */
  int Create(const void* data,
             size_t size,
             OutputCallback callback,
             void* object);

  /**
   * Deletes all resources owned by this decoder.
   */
  void Destroy();

  /**
   * Resets this decoder. IOS invalidates Video Toolbox sessions when an
   * application becomes inactive. This function deletes all Video Toolbox
   * objects and re-creates them to prevent using invalidated sessions.
   * @return {int}
   */
  int Reset();

  /**
   * Returns the total number of frames in the HEVC-with-Alpha stream.
   * @return {int}
   */
  int GetNumberOfFrames() const {
    return static_cast<int>(number_of_samples_);
  }

  /**
   * Returns the total number of samples in the HEVC-with-Alpha stream.
   * @return {int}
   */
  int GetNumberOfSamples() const {
    return static_cast<int>(number_of_samples_);
  }

  /**
   * Returns the maximum picture-order count in the HEVC-with-Alpha stream.
   * @return {int}
   */
  int GetMaxPictureOrderCount() const {
    return static_cast<int>(max_picture_order_count_);
  }

  /**
   * Returns the picture order count for the specified sample.
   * @param {int} sample
   * @return {int}
   */
  int GetPictureOrderCount(int sample) const {
    return samples_[sample].picture_order_count;
  }

  /**
   * Returns whether this HEVC-with-Alpha stream premultiplies alpha pixels.
   * @return {int}
   */
  int IsPremultipliedAlpha(int sample) const {
    return alpha_.alpha_channel_use_idc == 1;
  }

  /**
   * Returns the frame number of the QuickTime stream. (The returned frame
   * number may not represent a key frame.)
   * @param {float} presentation_time
   * @return {int}
   */
  int GetFrame(float presentation_time) const;

  /**
   * Decodes the specified sample synchronously.
   * @param {int} sample_number
   * @return {int}
   */
  int DecodeSample(int sample_number);

#if __APPLE__
  /**
   * Decodes the specified sample asynchronously.
   * @param {int} sample_number
   * @param {VTDecompressionOutputHandler} handler
   * @return {int}
   */
  int DecodeSample(int frame_number, VTDecompressionOutputHandler handler);
#endif

  /**
   * Returns whether the specified NAL packet is an H.265 IDR (Instantaneous
   * Decoding Refresh) packet.
   * @param {hevc::NALUnitType} nal_unit_type
   * @return {int}
   * @private
   */
  static int IsIDR(NALUnitType nal_unit_type) {
    uintptr_t n = static_cast<uintptr_t>(nal_unit_type);
    return n - NAL_IDR_W_RADL <= NAL_IDR_N_LP - NAL_IDR_W_RADL;
  }

  /**
   * Returns whether the specified NAL packet is an H.265 BLA (Broken Link
   * Access) packet.
   * @param {hevc::NALUnitType} nal_unit_type
   * @return {int}
   * @private
   */
  static int IsBLA(NALUnitType nal_unit_type) {
    uintptr_t n = static_cast<uintptr_t>(nal_unit_type);
    return n - NAL_BLA_W_LP <= NAL_BLA_W_RADL - NAL_BLA_W_LP;
  }

  /**
   * Returns whether the specified NAL packet is an H.265 IRAP (Intra Random
   * Access Point) packet.
   * @param {hevc::NALUnitType} nal_unit_type
   * @return {int}
   * @private
   */
  static int IsIRAP(NALUnitType nal_unit_type) {
    uintptr_t n = static_cast<uintptr_t>(nal_unit_type);
    return n - NAL_BLA_W_LP <= NAL_RSV_IRAP_VCL23 - NAL_BLA_W_LP;
  }

  /**
   * Initializes the `frames_[]` array so this decoder can decode them.
   * @param {const mov::AtomCollection*} map
   * @return {int}
   * @private
   */
  int InitializeSamples(const mov::AtomCollection* map);

  /**
   * Decodes an HEVC Decoder (`hvcC`) configuration.
   * @param {const mov::VideoSampleDescriptionExtension*} extension
   * @return {int}
   * @private
   */
  int DecodeHEVCDecoderConfiguration(
      const mov::VideoSampleDescriptionExtension* extension);

  /**
   * Decodes an H.265 VPS (Video Parameter Set), including VPS extensions. This
   * function decodes H.265 VPS parameters required for decoding an
   * HEVC-with-Alpha stream with Video Toolbox as written in Section 7.3.2.1 and
   * Annex F.7.3.2.1.
   * @param {const uint8_t*} rbsp_data
   * @param {uintptr_t} rbsp_size
   * @param {uintptr_t} index
   * @private
   */
  int DecodeVideoParameterSet(const uint8_t* rbsp_data, uintptr_t rbsp_size);

  /**
   * Decodes an H.265 SPS (Sequence parameter set. This function decodes H.265
   * SPS parameters required for decoding an HEVC-with-Alpha stream with Video
   * Toolbox as written in Section 7.3.2.2.
   * @param {const uint8_t*} rbsp_data
   * @param {uintptr_t} rbsp_size
   * @param {uintptr_t} index
   * @private
   */
  int DecodeSequenceParameterSet(const uint8_t* rbsp_data,
                                 uintptr_t rbsp_size,
                                 uintptr_t index);

  /**
   * Decodes an H.265 PPS (Picture Parameter Set). This function decodes H.265
   * PPS parameters required for decoding an HEVC-with-Alpha stream with Video
   * Toolbox as written in Section 7.3.2.3.
   * @param {const uint8_t*} rbsp_data
   * @param {uintptr_t} rbsp_size
   * @param {uintptr_t} index
   * @private
   */
  int DecodePictureParameterSet(const uint8_t* rbsp_data,
                                uintptr_t rbsp_size,
                                uintptr_t index);

  /**
   * Decodes an H.265 SEI (Supplemental Enhancement Information) messages. This
   * function decodes H.265 alpha-channel information SEI messages required for
   * decoding an HEVC with Alpha stream with Video Toolbox as written in Annex
   * D.2 and Annex F.14.
   * @param {const uint8_t*} rbsp_data
   * @param {uintptr_t} rbsp_size
   * @private
   */
  int DecodeSupplementalEnhancementInformation(const uint8_t* rbsp_data,
                                               uintptr_t rbsp_size);

  /**
   * Decodes an H.265 slice header and retrieves its picture-order count as
   * written in Section 7.3.6.
   * @param {const uint8_t*} packet_data
   * @param {uintptr_t} packet_size
   * @return {uint32_t}
   * @private
   */
  uint32_t DecodeSliceHeader(const uint8_t* packet_data,
                             uintptr_t packet_size) const;

  /**
   * Parses a Profile Tier Level.
   * @param {const uint8_t*} top
   * @param {const uint8_t*} end
   * @param {uintptr_t} max_sub_layers
   * @param {hevc::ProfileTierLevel*} ptl
   * @return {const uint8_t*}
   * @private
   */
  const uint8_t* ParseProfileTierLevel(const uint8_t* top,
                                       const uint8_t* end,
                                       uintptr_t max_sub_layers,
                                       ProfileTierLevel* ptl) const;

  /**
   * Extracts an RBSP (Raw Byte Sequence Payload) from the specified data.
   * @param {const uint8_t*} data
   * @param {uintptr_t} size
   * @param {uint8_t*} rbsp
   * @private
   * @const
   */
  uintptr_t ExtractRBSP(const uint8_t* data,
                        uintptr_t size,
                        uint8_t* rbsp) const;

#if __APPLE__
  /**
   * Initializes the Video Toolbox decoder.
   * @private
   */
  void InitializeVideoToolbox();

  /**
   * Creates the Video Toolbox decoder.
   * @param {const mov::VideoSampleDescriptionExtension*} extension
   * @param {int} farme_width
   * @param {int} farme_height
   * @return {int}
   * @private
   */
  int CreateVideoToolbox(
      const mov::VideoSampleDescriptionExtension* extension,
      int frame_width,
      int frame_height,
      OutputCallback callback,
      void* object);

  /**
   * Destroys the Video Toolbox decoder.
   * @private
   */
  void DestroyVideoToolbox();

  /**
   * Resets the Video Toolbox decoder.
   * @return {int}
   * @private
   */
  int ResetVideoToolbox();

  /**
   * Decodes the specified sample synchronously using the Video Toolbox decoder.
   * @param {int} sample_number
   * @return {int}
   * @private
   */
  int DecodeSampleVideoToolbox(int sample_number);
#endif

#if __APPLE__
  /**
   * The format of the output `CMSampleBuffer` objects.
   * @type {CMFormatDescriptionRef}
   * @private
   */
  CMFormatDescriptionRef format_description_;

  /**
   * The decompression session.
   * @type {VTDecompressionSessionRef}
   * @private
   */
  VTDecompressionSessionRef decoder_session_;

  /**
   * The `hvcC` extension of the QuickTime stream.
   * @type {const mov::VideoSampleDescriptionExtension*}
   * @private
   */
  const mov::VideoSampleDescriptionExtension* hvcc_extension_;

  /**
   * The callback function called when this decoder finishes decoding a frame.
   * @type {hevc::Decoder::OutputCallback}
   * @private
   */
  OutputCallback decoder_callback_;

  /**
   * The parameter for the decoder callback.
   * @type {void*}
   * @private
   */
  void* decoder_object_;
#endif

  /**
   * The data of the QuickTime stream.
   * @type {uint8_t*}
   * @private
   */
  uint8_t* data_;

  /**
   * The size of the QuickTime stream.
   * @type {size_t}
   * @private
   */
  size_t size_;

  /**
   * The information of the samples of this QuickTime stream.
   * @type {hevc::Decoder::Sample*}
   * @private
   */
  Sample* samples_;

  /**
   * The number of `Sample` objects in the `samples_` array.
   * @type {uint32_t}
   * @private
   */
  uint32_t number_of_samples_;

  /**
   * The maximum picture-order count.
   * @type {uint32_t}
   * @private
   */
  uint32_t max_picture_order_count_;

  /**
   * The time scale for this QuickTime stream, the number of time units. (This
   * value is the denominator for sample durations.)
   * @type {uint32_t}
   * @private
   */
  uint32_t time_scale_;

  /**
   * The frame width of the QuickTime stream.
   * @type {int}
   * @private
   */
  int frame_width_;

  /**
   * The frame height of the QuickTime stream.
   * @type {int}
   * @private
   */
  int frame_height_;

  /**
   * The VPS (Video Parameter Set) of this QuickTime stream.
   * @type {hevc::VideoParameterSet}
   * @private
   */
  VideoParameterSet vps_;

  /**
   * The SPSs (Sequence Parameter Sets) of this QuickTime stream. (An HEVC-with-
   * Alpha video stream has two layers, i.e. it has two SPSs.)
   * @type {hevc::SequenceParameterSet}
   * @private
   */
  SequenceParameterSet sps_[2];

  /**
   * The PPSs (Picture Parameter Sets) of this QuickTime stream. (An HEVC-with-
   * Alpha video stream has two layers, i.e. it has two PPSs.)
   * @type {hevc::PictureParameterSet}
   * @private
   */
  PictureParameterSet pps_[2];

  /**
   * The Alpha-Channel SEI (Supplemental Enhancement Information) message.
   * @type {hevc::sei::AlphaChannelInformation}
   * @private
   */
  sei::AlphaChannelInformation alpha_;

  /**
   * The cache that stores an RBSP (Raw-Byte Sequence Payload) fragment.
   * @type {uint8_t[256]}
   * @private
   */
  alignas(__BIGGEST_ALIGNMENT__) uint8_t rbsp_data_[256];
};

}  // namespace hevc

#endif // HEVC_DECODER_H_
