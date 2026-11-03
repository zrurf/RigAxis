#!/usr/bin/env bash
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
rust="$repo/Rust"
lib="$repo/Source/ThirdParty/RigAxis/lib"
target="${1:-all}"
configuration="${2:-Release}"

if [[ "$configuration" == "Debug" ]]; then
    profile="debug"
    cargo_args=(build)
else
    profile="release"
    cargo_args=(build --release)
fi

export RUSTFLAGS="${RUSTFLAGS:--g}"

build() {
    local triple="$1"
    cargo "${cargo_args[@]}" -p rigaxis-ffi --target "$triple"
}

copy_android() {
    local triple="$1"
    local arch="$2"
    mkdir -p "$lib/Android/$arch"
    cp "$rust/target/$triple/$profile/librigaxis_ffi.a" "$lib/Android/$arch/rigaxis_ffi.a"
    echo "Copied Android $arch library"
}

build_android() {
    local ndk="${ANDROID_NDK_HOME:-${NDK_HOME:-}}"
    if [[ -z "$ndk" ]]; then
        echo "ANDROID_NDK_HOME is not set" >&2
        exit 1
    fi

    local host
    case "$(uname -s)" in
        Darwin) host="darwin-x86_64" ;;
        Linux) host="linux-x86_64" ;;
        *) echo "Unsupported Android host: $(uname -s)" >&2; exit 1 ;;
    esac

    local toolchain="$ndk/toolchains/llvm/prebuilt/$host/bin"
    local api="26"
    export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="$toolchain/aarch64-linux-android$api-clang"
    export CARGO_TARGET_X86_64_LINUX_ANDROID_LINKER="$toolchain/x86_64-linux-android$api-clang"

    build aarch64-linux-android
    copy_android aarch64-linux-android arm64
    build x86_64-linux-android
    copy_android x86_64-linux-android x86_64
}

build_mac() {
    if [[ "$(uname -s)" != "Darwin" ]]; then
        echo "The Mac target requires macOS." >&2
        exit 1
    fi

    rustup target add aarch64-apple-darwin x86_64-apple-darwin
    build aarch64-apple-darwin
    build x86_64-apple-darwin
    mkdir -p "$lib/Mac"
    lipo -create \
        "$rust/target/aarch64-apple-darwin/$profile/librigaxis_ffi.a" \
        "$rust/target/x86_64-apple-darwin/$profile/librigaxis_ffi.a" \
        -output "$lib/Mac/rigaxis_ffi.a"
    echo "Copied universal Mac library"
}

build_ios() {
    if [[ "$(uname -s)" != "Darwin" ]]; then
        echo "The iOS target requires macOS." >&2
        exit 1
    fi

    rustup target add aarch64-apple-ios aarch64-apple-ios-sim
    build aarch64-apple-ios
    build aarch64-apple-ios-sim
    mkdir -p "$lib/iOS/arm64" "$lib/iOS/simulator"
    cp "$rust/target/aarch64-apple-ios/$profile/librigaxis_ffi.a" "$lib/iOS/arm64/rigaxis_ffi.a"
    cp "$rust/target/aarch64-apple-ios-sim/$profile/librigaxis_ffi.a" "$lib/iOS/simulator/rigaxis_ffi.a"
    echo "Copied iOS device and simulator libraries"
}

build_linux() {
    rustup target add x86_64-unknown-linux-gnu
    build x86_64-unknown-linux-gnu
    mkdir -p "$lib/Linux/x64"
    cp "$rust/target/x86_64-unknown-linux-gnu/$profile/librigaxis_ffi.a" "$lib/Linux/x64/rigaxis_ffi.a"
    echo "Copied Linux x64 library"
}

case "$target" in
    android) build_android ;;
    mac) build_mac ;;
    ios) build_ios ;;
    linux) build_linux ;;
    all)
        case "$(uname -s)" in
            Darwin)
                build_mac
                build_ios
                build_android
                ;;
            Linux)
                build_linux
                build_android
                ;;
            *)
                echo "Use Scripts/BuildRust.ps1 on Windows." >&2
                exit 1
                ;;
        esac
        ;;
    *)
        echo "Usage: $0 {android|mac|ios|linux|all} [Release|Debug]" >&2
        exit 1
        ;;
esac
