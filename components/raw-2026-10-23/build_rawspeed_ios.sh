#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
# Rebuild the RawSpeed and C-API static libraries from this source snapshot.
# Build products are written only below the caller-supplied RAWSPEED_BUILD_ROOT.
set -euo pipefail

PACKAGE_ROOT="$(cd "$(dirname "$0")" && pwd -P)"
[[ -n "${RAWSPEED_BUILD_ROOT:-}" ]] || { echo "RAWSPEED_BUILD_ROOT required" >&2; exit 2; }
WORK_ROOT="$RAWSPEED_BUILD_ROOT"
[[ -d "$(dirname "$WORK_ROOT")" ]] || { echo "build parent missing" >&2; exit 2; }
[[ ! -e "$WORK_ROOT" ]] || { echo "scratch output already exists: $WORK_ROOT" >&2; exit 2; }
mkdir -p "$WORK_ROOT"
cp -R "$PACKAGE_ROOT/RawSpeed" "$WORK_ROOT/source"
SOURCE="$WORK_ROOT/source"
# Clang can embed source paths in type names, even after debug symbols are
# stripped. Map both roots to stable public labels before compiling.
prefix_flags=(
  "-ffile-prefix-map=$WORK_ROOT=/rawspeed-build"
  "-ffile-prefix-map=$PACKAGE_ROOT=/rawspeed-components"
)
# CMake canonicalizes macOS /private/tmp to /tmp for source arguments.
if [[ "$WORK_ROOT" == /private/tmp/* ]]; then
  prefix_flags+=( "-ffile-prefix-map=${WORK_ROOT#/private}=/rawspeed-build" )
fi
cmake_prefix_flags="${prefix_flags[*]}"

# 01 is reversed intentionally: the original vendor patch makes
# CameraMetaData final, while LibRaw derives from it.
patch --batch --forward -R -d "$SOURCE" -p1 < "$PACKAGE_ROOT/RawSpeed-patches/01.CameraMeta-extensibility.patch"
for patch_name in \
  02.Makernotes-processing.patch \
  03.remove-limits-and-logging.patch \
  04.clang-cl-compatibility.patch \
  05.no-phase-one-correction.patch \
  06.lossless-jpeg-predictors-2-7.patch \
  07.bounded-dng-tile-parallelism.patch \
  08.source-modification-notices.patch; do
  patch --batch --forward -d "$SOURCE" -p1 < "$PACKAGE_ROOT/RawSpeed-patches/$patch_name"
done

CMAKE_BIN="${CMAKE_BIN:-$(command -v cmake || true)}"
[[ -x "$CMAKE_BIN" ]] || { echo "CMake binary required (set CMAKE_BIN)" >&2; exit 2; }
for sdk in iphoneos iphonesimulator; do
  sdk_path="$(xcrun --sdk "$sdk" --show-sdk-path)"
  build="$WORK_ROOT/build-$sdk"
  out="$WORK_ROOT/output/$sdk"
  mkdir -p "$build/deps" "$build/capi-objects" "$out"
  platform_flags=( -arch arm64 -isysroot "$sdk_path" )
  if [[ "$sdk" == iphonesimulator ]]; then
    platform_flags+=( -mios-simulator-version-min=17.0 )
  else
    platform_flags+=( -mios-version-min=17.0 )
  fi

  xcrun --sdk "$sdk" clang++ -std=c++17 -O2 -DNDEBUG "${platform_flags[@]}" "${prefix_flags[@]}" \
    -I"$PACKAGE_ROOT/pugixml" -c "$PACKAGE_ROOT/pugixml/pugixml.cpp" \
    -o "$build/deps/pugixml.o"
  xcrun --sdk "$sdk" ar rcs "$build/deps/libpugixml.a" "$build/deps/pugixml.o"

  "$CMAKE_BIN" -S "$SOURCE" -B "$build" -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT="$sdk_path" \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
    -DCMAKE_C_COMPILER="$(xcrun --sdk "$sdk" --find clang)" \
    -DCMAKE_CXX_COMPILER="$(xcrun --sdk "$sdk" --find clang++)" \
    -DCMAKE_C_FLAGS="$cmake_prefix_flags" \
    -DCMAKE_CXX_FLAGS="$cmake_prefix_flags" \
    -DMARCHNATIVE=FALSE -DMTUNENATIVE=FALSE -DMTUNEGENERIC=FALSE \
    -DRAWSPEED_CACHELINESIZE_EXITCODE=0 \
    -DRAWSPEED_CACHELINESIZE_EXITCODE__TRYRUN_OUTPUT=64 \
    -DRAWSPEED_PAGESIZE_EXITCODE=0 \
    -DRAWSPEED_PAGESIZE_EXITCODE__TRYRUN_OUTPUT=16384 \
    -DWITH_OPENMP=OFF -DWITH_PUGIXML=ON -DUSE_BUNDLED_PUGIXML=OFF \
    -DPugixml_INCLUDE_DIR="$PACKAGE_ROOT/pugixml" \
    -DPugixml_LIBRARY="$build/deps/libpugixml.a" \
    -DWITH_JPEG=OFF -DWITH_ZLIB=OFF -DWITH_SSE2=OFF \
    -DBUILD_TESTING=OFF -DBUILD_TOOLS=OFF -DBUILD_BENCHMARKING=OFF \
    -DBUILD_DOCS=OFF -DBUILD_FUZZERS=OFF -DUSE_XMLLINT=OFF \
    -DRAWSPEED_ENABLE_WERROR=OFF -DRAWSPEED_ENABLE_DEBUG_INFO=OFF \
    -DRAWSPEED_ENABLE_LTO=OFF
  "$CMAKE_BIN" --build "$build" --target rawspeed --parallel "${RAWSPEED_JOBS:-4}"
  raw_archive="$(find "$build" -type f -name librawspeed.a -print -quit)"
  [[ -n "$raw_archive" ]] || { echo "RawSpeed archive missing" >&2; exit 1; }
  cp "$raw_archive" "$out/librawspeed.a"
  xcrun --sdk "$sdk" clang++ -std=c++17 -O2 -DNDEBUG "${platform_flags[@]}" "${prefix_flags[@]}" \
    -I"$SOURCE/src" -I"$SOURCE/src/librawspeed" -I"$build/src" \
    -c "$SOURCE/src/librawspeed/common/GetNumberOfProcessorCores.cpp" \
    -o "$build/rawspeed_get_number_of_processor_cores.o"
  xcrun --sdk "$sdk" ar rcs "$out/librawspeed.a" \
    "$build/rawspeed_get_number_of_processor_cores.o"
  xcrun --sdk "$sdk" ranlib "$out/librawspeed.a"

  common_flags=( -std=c++17 -O2 -DNDEBUG "${platform_flags[@]}" "${prefix_flags[@]}"
    -I"$SOURCE/src" -I"$SOURCE/src/librawspeed" -I"$build/src"
    -I"$build/src/librawspeed" -I"$SOURCE/src/external"
    -I"$PACKAGE_ROOT/RawSpeed-CAPI" -I"$PACKAGE_ROOT/pugixml" )
  xcrun --sdk "$sdk" clang++ "${common_flags[@]}" -c \
    "$PACKAGE_ROOT/RawSpeed-CAPI/rawspeed3_capi.cpp" \
    -o "$build/capi-objects/rawspeed3_capi.o"
  xcrun --sdk "$sdk" clang++ "${common_flags[@]}" -c \
    "$PACKAGE_ROOT/RawSpeed-CAPI/cameras.cpp" \
    -o "$build/capi-objects/cameras.o"
  xcrun --sdk "$sdk" clang++ "${common_flags[@]}" -c \
    "$PACKAGE_ROOT/pugixml/pugixml.cpp" \
    -o "$build/capi-objects/pugixml.o"
  xcrun --sdk "$sdk" ar rcs "$out/librawspeed3_capi.a" \
    "$build/capi-objects/rawspeed3_capi.o" "$build/capi-objects/cameras.o" \
    "$build/capi-objects/pugixml.o"
  xcrun --sdk "$sdk" ranlib "$out/librawspeed3_capi.a"
done
echo "Built archives: $WORK_ROOT/output"
