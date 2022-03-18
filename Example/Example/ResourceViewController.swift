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

import UIKit
import HEVCPlayerView

protocol ResourceViewControllerDelegate: AnyObject {
  func resource(_ viewController: ResourceViewController, didSelected fileURL: URL)
}

internal final class ResourceViewController: UIViewController, UITableViewDelegate, UITableViewDataSource {
  private var selectedFile: URL? = nil
  private var files: [URL] = []
  private lazy var noticeLabel: UILabel = .init(frame: view.bounds)
  private lazy var tableView: UITableView = .init(frame: view.bounds)
  weak var delegate: ResourceViewControllerDelegate? = nil
  private let emptyResourceMessage: String = "Transfer HEVC video (*.mov) files using Finder."

  static func make(selected selectedFile: URL?) -> ResourceViewController {
    let resourceViewController = ResourceViewController()
    resourceViewController.selectedFile = selectedFile
    return resourceViewController
  }

  override func viewDidLoad() {
    super.viewDidLoad()

    self.tableView.register(UITableViewCell.self, forCellReuseIdentifier: "cell")

    self.noticeLabel.font = .systemFont(ofSize: 12)
    self.noticeLabel.numberOfLines = 0
    self.noticeLabel.backgroundColor = .systemBackground
    self.noticeLabel.textAlignment = .center
    self.view.addSubview(noticeLabel)

    self.tableView.delegate = self
    self.tableView.dataSource = self
    self.tableView.backgroundColor = .clear
    self.tableView.tableFooterView = UIView()
    self.view.addSubview(tableView)

    self.navigationItem.leftBarButtonItem = UIBarButtonItem(barButtonSystemItem: .cancel, target: self, action: #selector(tappedCloseButton))
    self.navigationItem.rightBarButtonItem = UIBarButtonItem(barButtonSystemItem: .refresh, target: self, action: #selector(tappedReloadButton))

    reloadData()
  }

  @objc private func tappedCloseButton(_ sender: UIBarButtonItem) {
    dismiss(animated: true, completion: nil)
  }

  @objc private func tappedReloadButton(_ sender: UIBarButtonItem) {
    reloadData()
  }

  internal func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
    return files.count
  }

  internal func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
    let cell = UITableViewCell(style: .subtitle, reuseIdentifier: "cell")
    let file = self.files[indexPath.row]
    cell.textLabel?.text = file.lastPathComponent
    let asset = HEVCQuickTimeAsset(url: file)
    if let asset = asset {
      cell.detailTextLabel?.text = "format=HEVC, width=\(asset.width), height=\(asset.height)"
    } else {
      cell.detailTextLabel?.text = "format=unknown"
    }
    if file == self.selectedFile {
      cell.accessoryType = .checkmark
    } else {
      cell.accessoryType = .none
    }
    return cell
  }

  internal func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
    let file = self.files[indexPath.row]
    self.selectedFile = file
    self.delegate?.resource(self, didSelected: file)
  }

  private func reloadData() {
    var files: [URL] = []
    let fileManager = FileManager.default
    let documentURLs = fileManager.urls(for: .documentDirectory, in: .userDomainMask)
    documentURLs.forEach{ documentURL in
      print("\(documentURL)")
      let contents = try? fileManager.contentsOfDirectory(at: documentURL, includingPropertiesForKeys: nil)
      if let contents = contents {
        files.append(contentsOf: contents.filter {
          return $0.pathExtension == "mov"
        })
      }
    }
    self.files = files
    self.noticeLabel.text = files.isEmpty ? emptyResourceMessage : ""
  }
}
