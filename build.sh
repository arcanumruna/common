#!/bin/bash
set -e

echo "🔧 Installing Conan dependencies..."
conan install . --build=missing -s build_type=Release

echo "🏗️ Generating CMake build system..."
cmake . -B build \
  -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake \
  -DCMAKE_PREFIX_PATH="$PWD/build/Release/generators" \
  -DCMAKE_BUILD_TYPE=Release

echo "🚧 Building project..."
cmake --build build -- VERBOSE=1

echo "📦 Creating Conan package..."
conan create . --user=vradu --channel=dev

echo "📤 Exporting package to local cache..."
conan export . --user=vradu --channel=dev

echo "✅ Done."
