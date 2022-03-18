# HEVCPlayerView

HEVCPlayerView is an animation view that plays [HEVC video with alpha](https://developer.apple.com/videos/play/wwdc2019/506/) files using [Video Toolbox](https://developer.apple.com/documentation/videotoolbox).
[HEVC video with alpha](https://developer.apple.com/videos/play/wwdc2019/506/) is an [HEVC](https://www.itu.int/rec/T-REC-H.265) extension that adds alpha-channel support to [HEVC](https://www.itu.int/rec/T-REC-H.265) video.
It is designed for video composition, e.g. playing video over custom backgrounds.

## Evaluating HEVCPlayerView

This repository has an example app that composes [HEVC video with alpha](https://developer.apple.com/videos/play/wwdc2019/506/) files over camera preview.
To run the example app on your iOS devices:
1. Clone this repository;
```shell
git clone https://github.dena.jp/pococha/HEVCPlayerView.git
```
2. Open [Example/Example.xcworkspace](/Example/Example.xcworkspace) with Xcode;
3. Select your personal team ID in the "Signing & Capabilities" tab;
4. Modify its bundle identifier, and;
5. Select "Product" > "Run".

## Adding HEVCPlayerView into your Xcode project

[HEVCPlayerView](https://github.com/DeNA/HEVCPlayerView/) is a [swift package](https://developer.apple.com/documentation/swift_packages) written in C++ and Objective-C++.
To add [HEVCPlayerView](https://github.com/DeNA/HEVCPlayerView/) to your Xcode project:
1. Select "File" > "Add Packages";
2. Enter "https://github.com/DeNA/HEVCPlayerView" to its search box, and;
3. Click its "Add Package" button.

## Using HEVCPlayerView in your app

[HEVCPlayerView](https://github.com/DeNA/HEVCPlayerView/) is a custom view derived from the [UIView]<https://developer.apple.com/documentation/uikit/uiview> class.
To add an [HEVCPlayerView](https://github.com/DeNA/HEVCPlayerView/) instance to your app view using the [Interface Builder](https://developer.apple.com/xcode/interface-builder/) editor,
1. Open the [Interface Builder](https://developer.apple.com/xcode/interface-builder/) editor;
2. Select "View" > "Show Library";
3. Drag the "View" icon in the Object library;
4. Drop the icon onto the view in the dock;
5. Select "View" > "Inspectors" > "Identity", and;
6. Change the "Class" name to "HEVCPlayerView".

[HEVCPlayerView](https://github.com/DeNA/HEVCPlayerView/) implements methods required for [Interface Builder](https://developer.apple.com/xcode/interface-builder/) outlets. To connect an [HEVCPlayerView](https://github.com/DeNA/HEVCPlayerView/) instance added by the [Interface Builder](https://developer.apple.com/xcode/interface-builder/) editor to your controller code (e.g. `ViewController.swift`),
1. Select the [HEVCPlayerView](https://github.com/DeNA/HEVCPlayerView/) instance in the dock;
2. Select "View" > "Inspectors" > "Connections";
3. Drag the "New Referencing Outlut" item, and;
4. Drop it onto your controller code.

An [HEVCPlayerView](https://github.com/DeNA/HEVCPlayerView/) instance can be added to your app programatically.
To add an [HEVCPlayerView](https://github.com/DeNA/HEVCPlayerView/) instance to your UIView-derived view, create an [HEVCPlayerView](https://github.com/DeNA/HEVCPlayerView/) instance in its `init()` methods and add it as a sub view as listed in the following code snippet.
```swift
class YourView: UIView {
  init?(frame: CGRect) {
    super.init(frame: frame)

    let hevcPlayerView = HEVCPlayerView(frame: .zero, device: MTLCreateSystemDevice())!
    addSubView(hevcPlayerView)
  }

  init?(coder: NSCoder) {
    super.init(coder: coder)

    let hevcPlayerView = HEVCPlayerView(frame: .zero, device: MTLCreateSystemDevice())!
    addSubView(hevcPlayerView)
  }
```
```objective-c
@implementation YourView {
}

- (id)initWithCoder:(NSCoder *)coder {
  self = [super initWithCoder:coder];
  if (self) {
    HEVCPlayerView *hevcPlayerView = [[HEVCPlayerView alloc] initwithFrame:CGRectZero device: MTLCreateSystemDevice()];
    [self addSubView:hevcPlayerView];
  }
}

- (id)initWithFrame:(CGRect)rect {
  self = [super initWithFrame:rect];
  if (self) {
    HEVCPlayerView *hevcPlayerView = [[HEVCPlayerView alloc] initwithFrame:CGRectZero device: MTLCreateSystemDevice()];
    [self addSubView:hevcPlayerView];
  }
}

@end
```

## Creating HEVC video with alpha files

Apple provides a command-line tool [avconvert](https://en.wikipedia.org/wiki/QuickTime) to create [HEVC video with alpha](https://developer.apple.com/videos/play/wwdc2019/506/) files from [QuickTime animation](https://en.wikipedia.org/wiki/QuickTime_Animation) files. (It is integrated with [Finder](https://en.wikipedia.org/wiki/Finder_(software)).)

[FFmpeg](https://ffmpeg.org/) (4.4.1 or newer) can also create [HEVC video with alpha](https://developer.apple.com/videos/play/wwdc2019/506/) files on macOS. It can create [HEVC video with alpha](https://developer.apple.com/videos/play/wwdc2019/506/) files not only from [QuickTime Animation](https://en.wikipedia.org/wiki/QuickTime_Animation) files but also from any files supported by [FFmpeg](https://ffmpeg.org) (e.g. Animation-GIF images, PNG images, Animation-PNG images, etc.)

The following command creates an [HEVC video with alpha](https://developer.apple.com/videos/play/wwdc2019/506/) file `hevc.mov` from a [QuickTime Animation](https://en.wikipedia.org/wiki/QuickTime_Animation) file `clock.mov`.
```shell
ffmpeg -i clock.mov -c:v hevc_videotoolbox -require_sw 1 -allow_sw 1 -alpha_quality 0.5 -vtag hvc1 hevc.mov
```
(I recommend adding a `-require_sw 1` option to use the software encoder of macOS due to quality issues of its hardware encoder.)

The following command creates an [HEVC video with alpha](https://developer.apple.com/videos/play/wwdc2019/506/) file `hevc.mov` from a list of PNG images `clock01.png`,...,`clock10.png`.
```shell
ffmpeg -i clock%02d.png -c:v hevc_videotoolbox -require_sw 1 -allow_sw 1 -alpha_quality 0.5 -vtag hvc1 hevc.mov
```

Use the `-q:v` option to specify the quality of YUV images as listed in the following example.
```shell
ffmpeg -i clock%02d.png -c:v hevc_videotoolbox -q:v 50 -require_sw 1 -allow_sw 1 -alpha_quality 0.5 -vtag hvc1 hevc.mov
```
Unfortunately, the `-q:v` option is currently available only on Apple Silion Macs due to [Commit #efece444](http://git.ffmpeg.org/gitweb/ffmpeg.git/commit/efece4442f3f583f7d04f98ef5168dfd08eaca5c).
(The `-q:v` option works well on Intel Macs with my local build of `fFmpeg`, though.)

Use the `-b:v` option to specify the bitrate of the output file as listed in the following example.
```shell
ffmpeg -i clock%02d.png -c:v hevc_videotoolbox -b:v 1000k -require_sw 1 -allow_sw 1 -alpha_quality 0.5 -vtag hvc1 hevc.mov
```

## Previewing HEVC video with alpha files

[Safari](https://www.apple.com/safari/) also supports [HEVC video with alpha](https://developer.apple.com/videos/play/wwdc2019/506/) files.
To play an [HEVC video with alpha](https://developer.apple.com/videos/play/wwdc2019/506/) file `hevc.mov` on a grid:
1. Create an HTML file `hevc.html` from the following snippet, and;
2. Open `hevc.html` with [Safari](https://www.apple.com/safari/).

```html
<html>
  <head></head>
  <body>
    <div style="width: 720px; height: 1080px; background: repeating-linear-gradient(90deg, gray 0 1px, transparent 1px 20px), repeating-linear-gradient(180deg, gray 0 1px, transparent 1px 20px);">
      <video width="720" height="1080" autoplay muted loop playsinline>
        <source src="hevc.mov" type="video/quicktime">
      </video>
    </div>
  </body>
</html>
```

## Contributing

## License

Copyright (c) DeNA Co., Ltd. All rights reserved.

Licensed under the [MIT](LICENSE) license.
