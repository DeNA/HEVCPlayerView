// swift-tools-version:5.3
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "HEVCPlayerView",
    platforms: [.iOS(.v13)],
    products: [
        .library(
            name: "HEVCPlayerView",
            targets: ["HEVCPlayerView"]
        ),
    ],
    targets: [
        .target(
            name: "HEVCPlayerView",
            resources: [.process("HEVCPlayerView.metal")],
            linkerSettings: [
                .linkedFramework("SystemConfiguration"),
                .linkedFramework("CoreMedia"),
                .linkedFramework("CoreVideo"),
                .linkedFramework("Metal"),
                .linkedFramework("VideoToolbox"),
                .linkedFramework("UIKit"),
            ]),
    ],
    cLanguageStandard: .gnu11,
    cxxLanguageStandard: .gnucxx11
)
