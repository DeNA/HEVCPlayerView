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

#import "HEVCPlayerView.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <Metal/Metal.h>
#import "HEVCWeakProxy.h"

#include <memory.h>
#include <pthread.h>
#include "hevc/decoder.h"

#import "HEVCBundleHelper.h"

/**
 * The class that encapsulates an array of images decoded by the `hevc::Decoder`
 * object. This class is also used for sorting decoded images with their picture
 * order numbers. (That said, this player sorts decoded images by itself.)
 */
struct HEVCPictureArray {
  /**
   * The inner class that encapsulates an image and its status.
   */
  struct Picture {
    /**
     * The image buffer.
     * @type {uintptr_t}
     */
    CVImageBufferRef image;

    /**
     * The image status.
     *  +-------+-------------+
     *  | value | description |
     *  +-------+-------------+
     *  | 0     | not decoded |
     *  | 1     | decoding    |
     *  | 2     | decoded     |
     *  +-------+-------------+
     * @type {uintptr_t}
     */
    uintptr_t status;
  };

  /**
   * Initializes an empty array.
   */
  void Initialize() {
    pthread_mutex_init(&mutex_, NULL);
    count_ = 0;
    session_ = 0;
    pictures_ = NULL;
  }

  /**
   * Deletes this array.
   * @param {uintptr_t} count
   */
  void Destroy() {
    Reset();
    pthread_mutex_destroy(&mutex_);
  }

  /**
   * Initializes this array.
   * @param {uintptr_t} count
   */
  void Create(uintptr_t count) {
    count_ = count;
    ++session_;
    pictures_ = static_cast<Picture*>(calloc(count, sizeof(Picture)));
  }

  /**
   * Resets this array to its initial state. This function atomically deletes
   * all cached images and deletes the array.
   */
  void Reset() {
    pthread_mutex_lock(&mutex_);
    ++session_;
    if (pictures_) {
      for (uintptr_t i = 0; i < count_; ++i) {
        Picture* picture = &pictures_[i];
        if (picture->image) {
          CFRelease(picture->image);
          picture->image = NULL;
        }
        picture->status = 0;
      }
      free(pictures_);
    }
    pictures_ = NULL;
    count_ = 0;
    pthread_mutex_unlock(&mutex_);
  }

  /**
   * Deletes all images cached in this array without deleting the array. (This
   * function is for re-playing the QuickTime stream being played by the view.)
   */
  void ClearCache() {
    pthread_mutex_lock(&mutex_);
    ++session_;
    if (pictures_) {
      for (uintptr_t i = 0; i < count_; ++i) {
        Picture* picture = &pictures_[i];
        if (picture->image) {
          CFRelease(picture->image);
          picture->image = NULL;
        }
        picture->status = 0;
      }
    }
    pthread_mutex_unlock(&mutex_);
  }

  /**
   * Returns the size of this array.
   * @return {uintptr_t}
   */
  uintptr_t GetCount() const {
    return count_;
  }

  /**
   * Retrieves the image status at the specified index.
   * @param {int} index
   * @return {uintptr_t}
   */
  uintptr_t GetStatus(int index) {
    pthread_mutex_lock(&mutex_);
    const uintptr_t status = pictures_[index].status;
    pthread_mutex_unlock(&mutex_);
    return status;
  }

  /**
   * Updates the image status at the specified index.
   * @param {int} index
   * @param {uintptr_t} status
   * @return {uintptr_t}
   */
  uintptr_t SetStatus(int index, uintptr_t status) {
    pthread_mutex_lock(&mutex_);
    const uintptr_t session = session_;
    Picture* picture = &pictures_[index];
    picture->status = status;
    pthread_mutex_unlock(&mutex_);
    return session;
  }

  /**
   * Retrieves the image at the specified index. This function does not decrease
   * the reference count of the retrieved image, i.e. the caller must call the
   * `CFRelease()` function after it uses the returned image.
   * @param {int} index
   * @return {CVImageBufferRef}
   */
  CVImageBufferRef GetImage(int index) {
    pthread_mutex_lock(&mutex_);
    Picture* picture = &pictures_[index];
    CVImageBufferRef image = picture->image;
    if (image) {
      picture->image = NULL;
    }
    picture->status = 0;
    pthread_mutex_unlock(&mutex_);
    return image;
  }

  /**
   * Attaches the specified image to the specified index. This function does not
   * increases the reference count of the specified image, i.e. the caller must
   * call the `CFRetain()` function to increase its reference count.
   * @param {int} index
   * @param {CVImageBufferRef} image
   * @param {uintptr_t} status
   * @param {uintptr_t} session
   */
  void SetImage(int index,
                CVImageBufferRef image,
                uintptr_t status,
                uintptr_t session) {
    pthread_mutex_lock(&mutex_);
    if (session == session_) {
      Picture* picture = &pictures_[index];
      picture->image = image;
      picture->status = status;
      CFRetain(image);
    }
    pthread_mutex_unlock(&mutex_);
  }

  /**
   * The mutex that allows only one thread to access this object.
   * @type {pthread_mutex_t}
   * @private
   */
  pthread_mutex_t mutex_;

  /**
   * The number of pictures in this array.
   * @type {uintptr_t}
   * @private
   */
  uintptr_t count_;

  /**
   * The session ID of this array.
   * @type {uintptr_t}
   * @private
   */
  uintptr_t session_;

  /**
   * The pictures.
   * @type {pthread_mutex_t}
   * @private
   */
  Picture* pictures_;
};

@implementation HEVCPlayerView {
  /**
   * The Metal layer to which this view draws decoded frames.
   * @type {CAMetalLayer*}
   * @private
   */
  CAMetalLayer *_metalLayer;

  /**
   * The command queue for drawing an HEVC-with-Alpha stream to this view.
   * @type {id<MTLCommandQueue>}
   * @private
   */
  id<MTLCommandQueue> _commandQueue;

  /**
   * The render state that encapsulates the shader programs used by this
   * view.
   * @type {id<MTLRenderPipelineState>}
   * @private
   */
  id<MTLRenderPipelineState> _renderPipelineState;

  /**
   * The texture cache that generates textures (from an HEVC-with-Alpha stream)
   * used by this view.
   * @type {CVMetalTextureCacheRef}
   * @private
   */
  CVMetalTextureCacheRef _textureCache;

  /**
   * The render-pass descriptor that writes textures (generated by the above
   * texture cache) to this view.
   * @type {MTLRenderPassDescriptor}
   * @private
   */
  MTLRenderPassDescriptor *_renderPassDescriptor;

  /**
   * The HEVC-with-Alpha decoder.
   * @type {hevc::Decoder}
   * @private
   */
  hevc::Decoder _decoder;

  /**
   * The display link to synchronize this view to the refresh rate of the host
   * display.
   * @type {CADisplayLink*}
   * @private
   */
  CADisplayLink *_displayLink;

  /**
   * The interval period for decoding an HEVC-with-Alpha stream and rendering
   * its output frames to this view repeatedly.
   * @type {CFTimeInternal}
   * @private
   */
  CFTimeInterval _timeInterval;

  /**
   * The last time when this view decoded an HEVC-with-Alpha stream.
   * @type {CFTimeInternal}
   * @private
   */
  CFTimeInterval _lastTimestamp;

  /**
   * The mutex that allows only one thread to attach it to the display link.
   * @type {pthread_mutex_t}
   * @private
   */
  pthread_mutex_t _mutex;

  /**
   * The path to the HEVC-with-Alpha stream being decoded.
   * @type {NSString*}
   * @private
   */
  NSString *_path;

  /**
   * The thread that decodes the HEVC-with-Alpha stream. This thread receives
   * events from the `CADisplayLink` object.
   * @type {NSString*}
   * @private
   */
  NSThread *_thread;

  /**
   * The array of decoded pictures.
   * @type {HEVCPictureArray}
   * @private
   */
  HEVCPictureArray _pictures;

  /**
   * The sample number being decoded by the HEVC-with-Alpha decoder.
   * @type {int}
   * @private
   */
  int _sample;

  /**
   * The picture number to be rendered by this view. (This value is a key for
   * sorting decoded samples in the playing order.)
   * @type {int}
   * @private
   */
  int _picture;

  /**
   * The frame number rendered by this view. (This view caches decoded samples,
   * i.e. a frame number is not always equal to a sample number.)
   * @type {int}
   * @private
   */
  int _frame;

  /**
   * Whether or not this player loops playing the QuickTime stream.
   * @type {BOOL}
   * @private
   */
  BOOL _loop;

  /**
   * Whether or not this player should rewind its positions on idle. (This
   * variable is used for playing the QuickTime stream being played at its
   * beginning.)
   * @type {BOOL}
   * @private
   */
  BOOL _rewind;

  /**
   * Whether or not this player should reset its HEVC-with-Alpha decoder. (This
   * variable is used for deleting an invalidated decoder and re-creating a new
   * one.)
   * @type {BOOL}
   * @private
   */
  BOOL _reset;

  /**
   * Whether or not this player is being inactive.
   * @type {BOOL}
   * @private
   */
  BOOL _suspend;
    
  /**
    * Whether or not this player is finshed.
    * @type {BOOL}
    * @private
    */
  BOOL _finished;

  /**
   * The parameters provided to this player.
   * @private
   */
  struct {
    /**
     * The interval period for decoding an HEVC-with-Alpha stream and rendering
     * its output frames to this view repeatedly.
     * @type {NSTimeInterval}
     */
    NSTimeInterval interval;

    /**
     * Whether or not this player loops playing the QuickTime stream.
     * @type {NSInteger}
     */
    BOOL loop;
  } _parameters;
}

#if TARGET_OS_IPHONE
+ (Class) layerClass {
  return [CAMetalLayer class];
}
#endif

- (id)initWithCoder:(NSCoder *)coder {
  self = [super initWithCoder:coder];
  if (self) {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    [self initViewWithDevice:device];
  }
  return self;
}

- (id)initWithFrame:(CGRect)frame device:(id<MTLDevice>)device {
  self = [super initWithFrame:frame];
  if (self) {
    [self initViewWithDevice:device];
  }
  return self;
}

- (void)dealloc {
  // TODO(hbono): Should this method wait for the background thread to finish
  // and delete its `pthread_mutex_t` object?
  [_displayLink invalidate];
  _pictures.Destroy();
  _decoder.Destroy();
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)playFileFromURL:(NSURL *)url fps:(NSInteger)fps loop:(BOOL)loop {
  if (![NSThread isMainThread]) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self playFileFromURL:url fps:fps loop:loop];
    });
    return;
  }
  // Calculate the play interval in seconds. (It truncates the interval to
  // milliseconds, e.g. 33.3 ms (0.0333 s) => 33 ms (0.0330 s))
  _parameters.interval = floor(1000.0 / (CFTimeInterval)fps) / 1000.0;
  _parameters.loop = loop;
  _finished = NO;
  NSString *path = [url path];
  if ([_path isEqualToString:path]) {
    // Notify the decoder thread to reset its positions when it becomes idle.
    _rewind = YES;
    _displayLink.paused = NO;
    return;
  }
  // Load the file asynchronously and play it.
  _path = path;
  NSFileHandle *handle = [NSFileHandle fileHandleForReadingAtPath:path];
  if (!handle) {
    [self finishWithError:[NSError errorWithDomain:NSCocoaErrorDomain code:NSFileNoSuchFileError userInfo:nil]];
    return;
  }
  NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self selector:@selector(readToEndOfFile:) name:NSFileHandleReadToEndOfFileCompletionNotification object:handle];
  [handle readToEndOfFileInBackgroundAndNotify];
}

- (void)finish {
  _displayLink.paused = YES;
  _finished = YES;
  if (self.delegate) {
    [self.delegate playerView:self didFinish:0];
  }
}

- (void)finishWithError: (NSError *) error {
  _displayLink.paused = YES;
  _finished = YES;
  if (self.delegate) {
    [self.delegate playerView:self didFail:error];
  }
}

-(void)invalidate {
  [_thread cancel];
  _thread = nil;
}

#pragma mark - internal methods

/**
 * Initializes this view. This method initializes all its internal variables.
 * @param {id<MTLDevice>} device
 */
- (void)initViewWithDevice:(id<MTLDevice>)device {
  // Initialize the `CAMetalLayer` object associated with this view and cache
  // it to avoid retrieving it when this view renders decoded images. That said,
  // this view caches the `CAMetalLayer` object to render images on the decoder
  // thread.
  _metalLayer = (CAMetalLayer *)[self layer];
  _metalLayer.opaque = NO;
  _metalLayer.drawsAsynchronously = YES;
  _metalLayer.contentsGravity = kCAGravityResizeAspectFill;
  _metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  _metalLayer.framebufferOnly = YES;
  _metalLayer.presentsWithTransaction = NO;

  // Create Metal resources.
  _commandQueue = [device newCommandQueue];
  id<MTLLibrary> mtlLibrary = [device newDefaultLibraryWithBundle:[HEVCBundleHelper getBundle] error:nil];
  if (mtlLibrary) {
    MTLRenderPipelineDescriptor *descriptor = [MTLRenderPipelineDescriptor new];
    if (descriptor) {
      descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
      descriptor.vertexFunction = [mtlLibrary newFunctionWithName: @"HEVCPlayerVertex"];
      descriptor.fragmentFunction = [mtlLibrary newFunctionWithName: @"HEVCPlayerFragment"];
      _renderPipelineState = [device newRenderPipelineStateWithDescriptor:descriptor error:nil];
    }
    _renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    _renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionDontCare;
    _renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
  }
  NSDictionary *attributes = [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithInt:MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget], kCVMetalTextureUsage, nil];
  CVMetalTextureCacheCreate(kCFAllocatorDefault, nil, device, (__bridge CFDictionaryRef)attributes, &_textureCache);

  // Observe a couple of application notifications to stop rendering images
  // while the host application is inactive.
  NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self selector:@selector(willResignActive:) name:UIApplicationWillResignActiveNotification object:nil];
  [defaultCenter addObserver:self selector:@selector(didBecomeActive:) name:UIApplicationDidBecomeActiveNotification object:nil];

  // Initialize the HEVC decoder and its resources.
  _decoder.Initialize();
  _path = @"";
  _thread = nil;
  _pictures.Initialize();
  _sample = 0;
  _picture = 0;
  _frame = 0;
  _loop = NO;
  _rewind = NO;
  _reset = NO;
  _suspend = NO;
  _finished = NO;
  _parameters.interval = 0.0;
  _parameters.loop = NO;

  // Create a display link so the caller thread can own it.
  pthread_mutex_init(&_mutex, NULL);
  HEVCWeakProxy *weakProxy = [HEVCWeakProxy weakProxyWithObject:self];
  _displayLink = [CADisplayLink displayLinkWithTarget:weakProxy selector:@selector(updateFrame:)];
  _timeInterval = 0.0;
  _lastTimestamp = -1.0;
}

/**
 * Called when the host application becomes inactive.
 * @param {NSNotification*} notification
 */
- (void)willResignActive:(NSNotification *)notification {
  _displayLink.paused = YES;
  if(!_finished) {
    _suspend = YES;
  }
}

/**
 * Called when the host application becomes active.
 * @param {NSNotification*} notification
 */
- (void)didBecomeActive:(NSNotification *)notification {
  // Read the `_displayLink.paused` value and tell the decoder thread to reset
  // the HEVC decoder only when it is `YES`, i.e. this player is becoming back
  // from an inactive state. (This method should ignore `didBecomeActive` events
  // dispatched when this view is created.)
  if (_displayLink.isPaused && !_finished) {
    _reset = YES;
    _displayLink.paused = NO;
  }
}

/**
 * Called when the `NSFileHandle` object finishes reading all data from the
 * QuickTime file being played.
 * @param {NSNotification*} notification
 */
- (void)readToEndOfFile:(NSNotification *)notification {
  NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter removeObserver:self name:NSFileHandleReadToEndOfFileCompletionNotification object:[notification object]];

  NSDictionary *userInfo = [notification userInfo];
  NSData *data = [userInfo objectForKey:NSFileHandleNotificationDataItem];
  if (!data) {
    [self finishWithError:[NSError errorWithDomain:NSCocoaErrorDomain code:NSFileReadUnknownError userInfo:nil]];
    return;
  }
  // Tell the decoder thread to exit when this decoder is decoding another file
  // and wait for the thread to exit. (This method temporarily acquires the
  // mutex owned by the decoder thread to wait.)
  [_thread cancel];
  pthread_mutex_lock(&_mutex);
  pthread_mutex_unlock(&_mutex);

  // Delete the picture cache and the HEVC decoder after the decoder thread
  // exits.
  _pictures.Reset();
  _decoder.Destroy();

  // Re-initialize the HEVC decoder and create a decoder thread.
  int status = _decoder.Create(data.bytes, data.length, NULL, NULL);
  if (status) {
    [self finishWithError: [NSError errorWithDomain:NSOSStatusErrorDomain code:status userInfo:nil]];
    return;
  }
  // Initialize the picture cache and the player parameters.
  _pictures.Create(_decoder.GetMaxPictureOrderCount() + 1);
  _timeInterval = _parameters.interval;
  _lastTimestamp = -1.0;
  _loop = _parameters.loop;
  _sample = 0;
  _picture = 0;
  _frame = 0;
  _thread = [[NSThread alloc] initWithTarget:self selector:@selector(threadMethod:) object:nil];
  [_thread setName:@"com.dena.pokota.HEVCPlayerView.DecoderThread"];
  [_thread start];
}

/**
 * The entry point function of the rendering thread. (This thread also decodes
 * QuickTime streams.)
 * @param {id} arg
 */
- (void)threadMethod:(id)arg {
  // Acquire the mutex and synchronize this thread to the refresh rate of the
  // display. (This method must be executed atomically to prevent multiple
  // threads from attaching it to one `CADisplayLink` object.)
  pthread_mutex_lock(&_mutex);
  [_displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
  _displayLink.paused = NO;
  if (@available(iOS 10.0, *)) {
    _displayLink.preferredFramesPerSecond = 0;
  }

  // Decode the QuickTime stream and render its pictures.
  NSThread* thread = _thread;
  while (thread.cancelled == NO) {
    // Render a decoded picture when this thread receives a `CADisplayLink` event.
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow: 1.0f / 60.0f]];
      
    if (_finished && _sample != 0) {
      _pictures.ClearCache();
      _sample = 0;
      _picture = 0;
      _frame = 0;
      _loop = _parameters.loop;
      _timeInterval = _parameters.interval;
      _lastTimestamp = -1.0;
      [self clearScreen];
    }

    // Delete all resources used by this thread and clear the output screen when
    // this player is being inactive.
    if (_suspend) {
      _suspend = NO;
      _pictures.ClearCache();
      _sample = 0;
      _picture = 0;
      _frame = 0;
      [self clearScreen];
    }
    // Reset the positions and decode samples on idle. (This thread renders
    // images once in 33 ms, i.e. it is sufficiently idle to decode samples.)
    if (!_displayLink.isPaused) {
      if (_rewind) {
        _rewind = NO;
        _pictures.ClearCache();
        _sample = 0;
        _picture = 0;
        _frame = 0;
        _loop = _parameters.loop;
        _timeInterval = _parameters.interval;
        _lastTimestamp = -1.0;
      }
      [self decodePictureAt:_picture];
    }
  }

  // Detach this thread from the `CADisplayLink` object and release the mutex so
  // another thread can use the `CADisplayLink` object.
  [_displayLink removeFromRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
  pthread_mutex_unlock(&_mutex);
}

/**
 * Decodes HEVC samples to fill the image cache.
 * @param {int} picture
 */
- (void)decodePictureAt:(int)picture {
  // Reset the HEVC decoder to prevent using invalidated objects.
  if (_reset) {
    _reset = NO;
    _decoder.Reset();
  }
  // Decode all samples until the HEVC decoder decodes the specified picture.
  // For an HEVC stream, its samples are not sorted in the playing (frame) order
  // as listed in the following table.
  //   +---------+----------+-----------+
  //   | frame # | sample # | picture # |
  //   +---------+----------+-----------+
  //   | 0       | 0        | 0         |
  //   | 1       | 1        | 4         |
  //   | 2       | 2        | 2         |
  //   | 3       | 3        | 1         |
  //   | 4       | 4        | 3         |
  //   | 5       | 5        | 8         |
  //     ...
  //   | 28      | 28       | 27        |
  //   | 29      | 29       | 29        |
  //   | 30      | 30       | 0         |
  //   | 31      | 31       | 4         |
  //     ...
  //   | 119     | 119      | 29        |
  //   | 120     | 0        | 0         |
  //     ...
  //   +---------+----------+-----------+
  // To render samples in the playing order, this method decodes samples until
  // it decodes a sample with the specified picture # in advance. For example,
  // this method decodes four samples (0, 1, 2, and 3) if `picture` is 1 and
  // `_sample` is 0.
  while (!_pictures.GetStatus(picture)) {
    [self decodeSampleAt:_sample];
    _sample = (_sample + 1) % _decoder.GetNumberOfSamples();
  }
}

/**
 * Decodes an HEVC sample at the specified index.
 * @param {int} sample
 */
- (void)decodeSampleAt:(int)sample {
  const int picture = _decoder.GetPictureOrderCount(sample);
  if (_pictures.GetStatus(picture) == 0) {
    __weak typeof(self) weakView = self;
    const uintptr_t session = _pictures.SetStatus(picture, 1);
    int status = _decoder.DecodeSample(sample, ^(OSStatus status,
                                                 VTDecodeInfoFlags flags,
                                                 CVImageBufferRef imageBuffer,
                                                 CMTime timestamp,
                                                 CMTime duration) {
      typeof(self) view = weakView;
      if (view) {
        HEVCPictureArray* pictures = &view->_pictures;
        if (status) {
          // Just write the error code to the console. (Unfortunately, the
          // `NSError` interface cannot stringify Video Toolbox errors.)
          NSLog(@"%s:error: status=%d\n", __FUNCTION__, status);
          pictures->SetStatus(picture, 0);
          _reset = YES;
          return;
        }
        if (imageBuffer) {
          pictures->SetImage(picture, imageBuffer, 2, session);
        }
      }
    });
    // Reset this player on error. (Video Toolbox returns an error if an
    // inactive application decodes samples. In this case, this player discards
    // all cached images and re-decodes them next time when it becomes active.)
    if (status) {
      _pictures.SetStatus(picture, 0);
      _reset = YES;
      return;
    }
  }
}

/**
 * Called when this player receives a `CADisplayLink` event. This method
 * retrieves the next frame from the cache and draws it. (This player decodes
 * samples on idle or after rendering a frame so it can render frames
 * immediately when it receives `CADisplayLink` events.)
 * @param {CADisplayLink*} sender
 */
- (void)updateFrame:(CADisplayLink*)sender {
  // Measure time elapsed since the last time when this player renders a picture
  // to render pictures at the specified interval.
  const CFTimeInterval timestamp = sender.timestamp;
  if (_lastTimestamp >= 0.0) {
    if (timestamp - _lastTimestamp < _timeInterval) {
      return;
    }
  }
  _lastTimestamp = timestamp;

  // Exit this method if this player is inactive, i.e. if this player cannot use
  // Metal.
  if (_displayLink.isPaused) {
    return;
  }

  // Stop playing the QuickTime stream when this player has rendered its final
  // frame.
  if (_frame >= _decoder.GetNumberOfFrames()) {
    [self finish];
    return;
  }

  @autoreleasepool {
    // Render the `_picture`-th picture in the picture cache. (This view decodes
    // samples of the HEVC-with-Alpha stream on idle so this method can render
    // decoded images immediately.)
    CVImageBufferRef imageBuffer = _pictures.GetImage(_picture);
    if (imageBuffer) {
      const size_t width = CVPixelBufferGetWidth(imageBuffer);
      const size_t height = CVPixelBufferGetHeight(imageBuffer);
#if HEVC_DEBUG
      int is_planar = CVPixelBufferIsPlanar(imageBuffer);
      OSType pixel_format_type = CVPixelBufferGetPixelFormatType(imageBuffer);
      size_t bytes_per_row = CVPixelBufferGetBytesPerRow(imageBuffer);
      NSLog(@"is_planar=%d, pixel_format_type=%d\n",
            is_planar, pixel_format_type);
      NSLog(@"width=%zu, height=%zu, bytes_per_row=%zu\n",
            width, height, bytes_per_row);
#endif
      // Create a `CAMetalDrawable` object for the next display frame and draw the
      // decoded image (consisting of three planes (Y, UV, and alpha)) onto it.
      // (`[CAMetalLayer nextDrawable:]` is a blocking method and it should not be
      // executed on the main thread.)
      CAMetalLayer *metalLayer = _metalLayer;
      metalLayer.drawableSize = CGSizeMake((float)width, (float)height);
      id<CAMetalDrawable> metalDrawable = [metalLayer nextDrawable];
      if (metalDrawable) {
        CVMetalTextureRef textureY;
        CVMetalTextureCacheRef textureCache = _textureCache;
        CVReturn resultY = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault, textureCache, imageBuffer, nil, MTLPixelFormatR8Unorm, width, height, 0, &textureY);
        if (resultY == kCVReturnSuccess) {
          CVMetalTextureRef textureUV;
          CVReturn resultUV = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault, textureCache, imageBuffer, nil, MTLPixelFormatRG8Unorm, width / 2, height / 2, 1, &textureUV);
          if (resultUV == kCVReturnSuccess) {
            CVMetalTextureRef textureA;
            CVReturn resultA = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault, textureCache, imageBuffer, nil, MTLPixelFormatR8Unorm, width, height, 2, &textureA);
            if (resultA == kCVReturnSuccess) {
              MTLRenderPassDescriptor *renderPassDescriptor = _renderPassDescriptor;
              renderPassDescriptor.colorAttachments[0].texture = metalDrawable.texture;
              id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
              if (commandBuffer) {
                id<MTLRenderCommandEncoder> commandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
                if (commandEncoder) {
#if HEVC_DEBUG
                  commandEncoder.label = @"Render an YUVA420 image";
#endif
                  [commandEncoder setRenderPipelineState:_renderPipelineState];
                  [commandEncoder setFragmentTexture:CVMetalTextureGetTexture(textureY) atIndex:0];
                  [commandEncoder setFragmentTexture:CVMetalTextureGetTexture(textureUV) atIndex:1];
                  [commandEncoder setFragmentTexture:CVMetalTextureGetTexture(textureA) atIndex:2];
                  [commandEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
                  [commandEncoder endEncoding];
                  [commandBuffer presentDrawable:metalDrawable];
                  [commandBuffer commit];
                }
              }
              CFRelease(textureA);
            }
            CFRelease(textureUV);
          }
          CFRelease(textureY);
        }
      }
      CFRelease(imageBuffer);

      // Increase the picture number so it refers to the next input image, and
      // increase the frame number so it refers to the next output image.
      _picture = (_picture + 1) % _pictures.GetCount();
      ++_frame;
      if (_loop) {
        if (_frame == _decoder.GetNumberOfFrames()) {
          _frame = 0;
          _picture = 0;
          _sample = 0;
        }
      }
      if (self.delegate) {
        [self.delegate playerView:self didUpdateFrame:(NSInteger)_frame];
      }

      // Decode the next samples in advance if this player is still active.
      // (This player may become inactive while it renders an image above. In
      // this case, it decodes the next samples when it becomes active again.)
      if (!_displayLink.isPaused) {
        [self decodePictureAt:_picture];
      }
    }
  }
}

- (void)clearScreen {
  @autoreleasepool {
    // Commit an empty command buffer to clear the `CAMetalLayer` object.
    CAMetalLayer *metalLayer = _metalLayer;
    id<CAMetalDrawable> metalDrawable = [metalLayer nextDrawable];
    if (metalDrawable) {
      MTLRenderPassDescriptor *renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
      renderPassDescriptor.colorAttachments[0].texture = metalDrawable.texture;
      renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
      renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
      renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
      id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
      id<MTLRenderCommandEncoder> commandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
      if (commandEncoder) {
        [commandEncoder endEncoding];
        [commandBuffer presentDrawable:metalDrawable];
        [commandBuffer commit];
      }
    }
  }
}

@end
