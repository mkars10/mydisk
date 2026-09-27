// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "MyDiskKit",
    platforms: [.iOS(.v17), .macOS(.v14)],
    products: [
        .library(name: "MyDiskKit", targets: ["MyDiskKit"]),
    ],
    targets: [
        .target(name: "MyDiskKit"),
        .testTarget(name: "MyDiskKitTests", dependencies: ["MyDiskKit"]),
    ]
)
