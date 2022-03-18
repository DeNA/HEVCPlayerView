/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 DeNA Co., Ltd.
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

import AVFoundation
import UIKit
import HEVCPlayerView

class ViewController: UIViewController, ResourceViewControllerDelegate {
  private var currentFile: URL? = nil
  private lazy var camera: AVCaptureDevice? = .default(for: .video)
  private lazy var cameraInput: AVCaptureDeviceInput? = try? .init(device: camera!)
  private lazy var cameraSession: AVCaptureSession = .init()
  private lazy var cameraLayer: AVCaptureVideoPreviewLayer = .init(session: cameraSession)
  @IBOutlet weak var backgroundImageView: UIImageView!
  @IBOutlet weak var hevcPlayerView: HEVCPlayerView!
    
  override func viewDidLoad() {
    super.viewDidLoad()
    if TARGET_OS_SIMULATOR == 0 {
      cameraSession.addInput(cameraInput!)
      cameraLayer.frame = view.bounds
      cameraLayer.videoGravity = .resizeAspectFill
      backgroundImageView.image = nil
      backgroundImageView.layer.insertSublayer(cameraLayer, at: 0)
      cameraSession.startRunning()
    }
    hevcPlayerView.delegate = self
  }

  @IBAction internal func handleTapScreen(_ sender: Any) {
    let resourceViewController = ResourceViewController.make(selected: currentFile)
    resourceViewController.delegate = self
    let nc =  UINavigationController(rootViewController: resourceViewController)
    present(nc, animated: true, completion: nil)
  }

  internal func resource(_ viewController: ResourceViewController, didSelected file: URL) {
    self.currentFile = file
    viewController.dismiss(animated: true, completion: { [weak self] in
      self?.play()
    })
  }

  private func play() {
    guard let url = self.currentFile else { return }
    hevcPlayerView.playFile(from: url, fps: 30, loop: false)
  }
}

extension ViewController: HEVCPlayerViewDelegate {
  internal func playerView(_ hevcPlayerView: HEVCPlayerView, didFail error: NSObject) {
    print("Error: \(error)")
  }
    
  internal func playerView(_ hevcPlayerView: HEVCPlayerView, didFinish error: NSObject?) {
    play()
  }

  internal func playerView(_ hevcPlayerView: HEVCPlayerView, didUpdateFrame index: Int) {
  }
}
