#!/bin/bash
# Build script for BreezeAPI — Android (16KB page aligned)
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

# Default ABI (override with BREEZE_ABI env)
ABI="${BREEZE_ABI:-arm64-v8a}"

echo "=== Building BreezeAPI for Android ${ABI} (16KB page aligned) ==="

cmake -B "${BUILD_DIR}/${ABI}" \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
    -DANDROID_ABI="${ABI}" \
    -DANDROID_PLATFORM=android-26 \
    -DCMAKE_BUILD_TYPE=Release \
    -S "${SCRIPT_DIR}"

cmake --build "${BUILD_DIR}/${ABI}" -j"$(nproc)"

echo "=== Build complete ==="
echo "Output: ${BUILD_DIR}/${ABI}/out/lib/${ABI}/libbreeze_api.so"

# Verify alignment
if command -v readelf &>/dev/null; then
    SO="${BUILD_DIR}/${ABI}/out/lib/${ABI}/libbreeze_api.so"
    ALIGN=$(readelf -l "${SO}" 2>/dev/null | grep LOAD | head -1 | awk '{print $NF}')
    echo "Page alignment: ${ALIGN}"
fi
