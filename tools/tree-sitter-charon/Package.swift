// swift-tools-version:5.3
import PackageDescription

let package = Package(
    name: "TreeSitterCharon",
    products: [
        .library(name: "TreeSitterCharon", targets: ["TreeSitterCharon"]),
    ],
    dependencies: [
        .package(url: "https://github.com/ChimeHQ/SwiftTreeSitter", from: "0.8.0"),
    ],
    targets: [
        .target(
            name: "TreeSitterCharon",
            dependencies: [],
            path: ".",
            sources: [
                "src/parser.c",
                // NOTE: if your language has an external scanner, add it here.
            ],
            resources: [
                .copy("queries")
            ],
            publicHeadersPath: "bindings/swift",
            cSettings: [.headerSearchPath("src")]
        ),
        .testTarget(
            name: "TreeSitterCharonTests",
            dependencies: [
                "SwiftTreeSitter",
                "TreeSitterCharon",
            ],
            path: "bindings/swift/TreeSitterCharonTests"
        )
    ],
    cLanguageStandard: .c11
)
