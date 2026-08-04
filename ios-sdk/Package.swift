// swift-tools-version:5.5
import PackageDescription

let package = Package(
    name: "AluSDK",
    platforms: [
        .iOS(.v12)
    ],
    products: [
        .library(
            name: "AluSDK",
            targets: ["AluSDK"]
        )
    ],
    targets: [
        .binaryTarget(
            name: "AluSDK",
            url: "https://github.com/Alu6hel/alu-language/releases/download/v1.0.0/AluSDK.xcframework.zip",
            checksum: "TO_BE_COMPUTED"
        )
    ]
)
