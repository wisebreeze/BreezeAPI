#!/bin/bash
# Build script for BreezeAPI — Android ARM64
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

# Default Android NDK path (override with ANDROID_NDK env)
NDK="${ANDROID_NDK:-${SCRIPT_DIR}/third_party/dobby/dependency/android-ndk-r25c}"

# NDK toolchain
TOOLCHAIN="${NDK}/build/cmake/android.toolchain.cmake"

if [ ! -f "${TOOLCHAIN}" ]; then
    echo "Error: Android NDK toolchain not found at ${TOOLCHAIN}"
    echo "Set ANDROID_NDK environment variable to your NDK path."
    exit 1
fi

echo "=== Building BreezeAPI for Android arm64-v8a ==="

cmake -B "${BUILD_DIR}/arm64-v8a" \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-26 \
    -DCMAKE_BUILD_TYPE=Release \
    -S "${SCRIPT_DIR}"

cmake --build "${BUILD_DIR}/arm64-v8a" -j"$(nproc)"

echo "=== Build complete ==="
echo "Output: ${BUILD_DIR}/arm64-v8a/out/lib/arm64-v8a/libbreeze_api.so"
