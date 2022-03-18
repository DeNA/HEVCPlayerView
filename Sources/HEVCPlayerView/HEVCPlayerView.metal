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

#include <metal_stdlib>

struct HEVCPlayerColor {
  float4 position [[ position ]];
  float2 texture;
};

vertex HEVCPlayerColor HEVCPlayerVertex(uint vid [[ vertex_id ]]) {
  const HEVCPlayerColor vertices[4] = {
    { float4(-1.0f, -1.0f, 0.0f, 1.0f), float2(0.0f, 1.0f) },
    { float4(1.0f, -1.0f, 0.0f, 1.0f), float2(1.0f, 1.0f) },
    { float4(-1.0f, 1.0f, 0.0f, 1.0f), float2(0.0f, 0.0f) },
    { float4(1.0f, 1.0f, 0.0f, 1.0f), float2(1.0f, 0.0f) },
  };
  return vertices[vid];
}

fragment float4 HEVCPlayerFragment(HEVCPlayerColor in [[ stage_in ]],
                                   metal::texture2d<float> textureY [[ texture(0) ]],
                                   metal::texture2d<float> textureUV [[ texture(1) ]],
                                   metal::texture2d<float> textureA [[ texture(2) ]]) {
  // Apply the `420v`->`f420` conversion as written in Stack Overflow
  // <https://stackoverflow.com/a/61219470>.
  constexpr metal::sampler sampler(metal::filter::linear);
  const float video_y = textureY.sample(sampler, in.texture).r;
  const float2 video_uv = textureUV.sample(sampler, in.texture).rg;
  const float full_y = (video_y - 16.0f / 255.0f) * (255.0f / (235.0f - 16.0f));
  const float2 full_uv = (video_uv - 16.0f / 255.0f) * (255.0f / (240.0f - 16.0f)) - 0.5f;

  // Apply the BT.709 YUV->RGB conversion. (Video Toolbox uses BT.709 to encode
  // HEVC video clips by default. That said, the color attachment of input
  // `CVPixelBuffer` objects is `kCVImageBufferYCbCrMatrix_ITU_R_709_2` by
  // default.
  const metal::float3x3 bt709 = metal::float3x3(float3(1.0f, 1.0f, 1.0f), float3(0.0f, -0.21482f, 2.12798f), float3(1.28033f, -0.38059f, 0.0f));
  // Convert the `kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange`
  const float3 rgb = bt709 * float3(full_y, full_uv);
  const float alpha = textureA.sample(sampler, in.texture).r;
  return float4(rgb, alpha);
}
