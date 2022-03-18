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

#ifndef HEVC_PLAYER_VIEW_H_
#define HEVC_PLAYER_VIEW_H_

#import <UIKit/UIKit.h>

@class HEVCPlayerView;

@protocol HEVCPlayerViewDelegate <NSObject>

/**
 * Called when an error occurred during playing a file.
 * @param {HEVCPlayerView *} hevcPlayerView
 * @param {NSObject *} error
 */
- (void) playerView:(HEVCPlayerView* _Nonnull)hevcPlayerView didFail:(NSObject* _Nonnull)error;

/**
 * Called when the HEVCPlayerView object finishes playing a file.
 * @param {HEVCPlayerView *} hevcPlayerView
 * @param {NSObject*} dummy
 */
- (void) playerView:(HEVCPlayerView* _Nonnull)hevcPlayerView didFinish:(NSObject* _Nullable)dummy;

/**
 * Called when the HEVCPlayerView object finishes rendering a frame. This
 * method is called by the decoder thread, i.e. the decoder thread cannot
 * decode frames until this method exits. So, applications should dispatch
 * time-consuming tasks to other threads.
 * @param {HEVCPlayerView *} hevcPlayerView
 * @param {NSInteger} index
 */
- (void) playerView:(HEVCPlayerView* _Nonnull)hevcPlayerView didUpdateFrame:(NSInteger)index;

@end

@interface HEVCPlayerView: UIView {
}

/**
 * The delegate.
 * @type {id<HEVCPlayerViewDelegate>}
 * @weak
 */
@property (nonatomic, weak) id<HEVCPlayerViewDelegate> _Nullable delegate;

// UIView methods
#if TARGET_OS_IPHONE
+ (Class _Nonnull) layerClass;
#endif
- (id _Nullable) initWithCoder:(NSCoder* _Nonnull)aDecoder;
- (id _Nullable) initWithFrame:(CGRect)frame device:(id<MTLDevice> _Nonnull)device;
- (void)dealloc;

/**
 * Starts playing an HEVC file referred by the specified URL.
 * @param {NSURL*} url
 * @param {NSInteger} fps
 * @param {BOOL} loop
 */
- (void) playFileFromURL:(NSURL* _Nonnull)url fps:(NSInteger)fps loop:(BOOL)loop;

/**
 * Finishes playing the HEVC file being played.
 */
- (void) finish;

/**
 * Invalidates this player. An NSThread object retained by this player causes a
 * cycling dependency. This method detaches the NSThread object from this player
 * so ARC can delete this player.
 */
-(void) invalidate;

@end

#endif  // HEVC_PLAYER_VIEW_H_
