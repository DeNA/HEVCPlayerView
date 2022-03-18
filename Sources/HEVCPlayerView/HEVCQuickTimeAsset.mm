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

#import "HEVCQuickTimeAsset.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreFoundation/CoreFoundation.h>

#include "mov/atom.h"
#include "mov/atom_collection.h"

@implementation HEVCQuickTimeAsset {
}

- (id)initWithURL:(NSURL *)url {
  self = [super init];
  if (self) {
    _width = 0;
    _height = 0;

    // Search for a VideoSampleDescription atom in the QuickTime file to
    // retrieve its sample size.
    NSFileHandle *handle = [NSFileHandle fileHandleForReadingAtPath:[url path]];
    if (handle) {
      NSData *data;
      if (@available(iOS 13.0, *)) {
        data = [handle readDataToEndOfFileAndReturnError:nil];
      } else {
        data = [handle readDataToEndOfFile];
      }
      if (data) {
        mov::AtomCollection map;
        map.Initialize();
        if (map.Enumerate(static_cast<const uint8_t*>(data.bytes), data.length)) {
          const mov::SampleDescriptionAtom *sample_description_atom = map.GetSampleDescriptionAtom();
          uint32_t number_of_descriptions = sample_description_atom->GetCount();
          const mov::SampleDescription *sample_description = sample_description_atom->GetFirstDescription();
          do {
            const mov::FourCC format = sample_description->GetType();
            if (format == mov::FORMAT_HVC1) {
              const mov::VideoSampleDescription *video_sample_description = sample_description->GetVideoSampleDescription();
              _width = video_sample_description->GetWidth();
              _height = video_sample_description->GetHeight();
              break;
            }
            sample_description = sample_description->GetNextDescription();
          } while (--number_of_descriptions > 0);
        }
      }
    }
  }
  return self;
}

@end
