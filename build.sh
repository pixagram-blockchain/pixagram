#!/usr/bin/env bash

set -euo pipefail

# Simple macOS (Apple Silicon) build helper for Hive/Pixagram
# - Configures CMake with Ninja (if available) in ./build
# - Points CMake at Homebrew Boost and OpenSSL
# - Enables Snappy (used by RocksDB)
# - Clears linker flags that can break AppleClang (e.g., -lunwind)

usage() {
  cat <<EOF
Usage: $(basename "$0") [target ...]

Environment variables:
  BUILD_DIR                 Build directory (default: build)
  BUILD_TYPE                CMake build type (default: Release)
  HIVE_BUILD_ON_MINIMAL_FC  ON/OFF (default: OFF)
  CMAKE_BIN                 CMake executable (default: cmake in PATH)
  CMAKE_GENERATOR           CMake generator (default: Ninja if available)
  PYTHON_BIN                Python 3 executable (default: python3 in PATH)
  BOOST_PREFIX              Homebrew Boost prefix (auto-detected)
  OPENSSL_PREFIX            Homebrew OpenSSL@3 prefix (auto-detected)
  SNAPPY_PREFIX             Homebrew Snappy prefix (auto-detected)
  JOBS                      Parallel build jobs (default: number of CPUs)

Examples:
  # Build hived and cli_wallet
  ./build.sh hived cli_wallet

  # Full build
  ./build.sh

  # Enable minimal fc build (requires system secp256k1)
  HIVE_BUILD_ON_MINIMAL_FC=ON ./build.sh
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "This build helper targets macOS. For Linux, follow doc/building.md." >&2
  exit 1
fi

BUILD_DIR=${BUILD_DIR:-build}
BUILD_TYPE=${BUILD_TYPE:-Release}
HIVE_BUILD_ON_MINIMAL_FC=${HIVE_BUILD_ON_MINIMAL_FC:-OFF}

CMAKE_MIN_VERSION=3.22.1
CMAKE_BIN=${CMAKE_BIN:-$(command -v cmake || true)}
if [[ -z "${CMAKE_BIN}" ]]; then
  echo "CMake not found. Install it with: brew install cmake" >&2
  exit 1
fi

version_ge() {
  local IFS=.
  local -a v1=($1) v2=($2)
  local i
  for ((i=0; i<${#v1[@]} || i<${#v2[@]}; i++)); do
    local a=${v1[i]:-0}
    local b=${v2[i]:-0}
    if ((10#$a > 10#$b)); then
      return 0
    fi
    if ((10#$a < 10#$b)); then
      return 1
    fi
  done
  return 0
}

CMAKE_VERSION=$("$CMAKE_BIN" --version | head -n1 | awk '{print $3}')
if ! version_ge "$CMAKE_VERSION" "$CMAKE_MIN_VERSION"; then
  echo "CMake >= ${CMAKE_MIN_VERSION} is required (found ${CMAKE_VERSION})." >&2
  exit 1
fi

CMAKE_GENERATOR=${CMAKE_GENERATOR:-}
if [[ -z "${CMAKE_GENERATOR}" ]]; then
  if command -v ninja >/dev/null 2>&1; then
    CMAKE_GENERATOR="Ninja"
  else
    echo "Ninja not found; falling back to Unix Makefiles." >&2
    CMAKE_GENERATOR="Unix Makefiles"
  fi
elif [[ "${CMAKE_GENERATOR}" == "Ninja" ]]; then
  if ! command -v ninja >/dev/null 2>&1; then
    echo "CMAKE_GENERATOR=Ninja but ninja is not installed. Try: brew install ninja" >&2
    exit 1
  fi
fi

# Detect Homebrew prefixes
BREW_BIN=$(command -v brew || true)
if [[ -z "${BREW_BIN}" ]]; then
  echo "Homebrew not found. Please install dependencies manually (Boost >=1.74, OpenSSL 3, Snappy)." >&2
  exit 1
fi

BOOST_PREFIX=${BOOST_PREFIX:-$( (brew --prefix boost@1.85 || brew --prefix boost) 2>/dev/null || true)}
OPENSSL_PREFIX=${OPENSSL_PREFIX:-$(brew --prefix openssl@3 2>/dev/null || true)}
SDK_PREFIX=${SDK_PREFIX:-$(xcrun --sdk macosx --show-sdk-path 2>/dev/null || echo "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk")}

if [[ -z "${BOOST_PREFIX}" || ! -d "${BOOST_PREFIX}" ]]; then
  echo "Boost not found via Homebrew. Try: brew install boost@1.85" >&2
  exit 1
fi
if [[ -z "${OPENSSL_PREFIX}" || ! -d "${OPENSSL_PREFIX}" ]]; then
  echo "OpenSSL@3 not found via Homebrew. Try: brew install openssl@3" >&2
  exit 1
fi

# Snappy is required by the vendor CMake setup
SNAPPY_PREFIX=${SNAPPY_PREFIX:-$(brew --prefix snappy 2>/dev/null || true)}
if [[ -z "${SNAPPY_PREFIX}" || ! -d "${SNAPPY_PREFIX}" ]]; then
  echo "Snappy not found via Homebrew. Try: brew install snappy" >&2
  exit 1
fi

SNAPPY_DIR=${SNAPPY_DIR:-"${SNAPPY_PREFIX}/lib/cmake/Snappy"}
if [[ -d "${SNAPPY_DIR}" ]]; then
  export Snappy_DIR="${SNAPPY_DIR}"
fi

# Resolve compression libs explicitly so FindZLIB/FindBZip2 work on macOS.
ZLIB_PREFIX=${ZLIB_PREFIX:-$(brew --prefix zlib 2>/dev/null || true)}
ZLIB_ROOT_CMAKE=""
ZLIB_LIBRARY_CMAKE=${ZLIB_LIBRARY:-}
ZLIB_INCLUDE_CMAKE=${ZLIB_INCLUDE_DIR:-}
if [[ -n "${ZLIB_PREFIX}" && -d "${ZLIB_PREFIX}" ]]; then
  ZLIB_ROOT_CMAKE="$ZLIB_PREFIX"
  if [[ -z "${ZLIB_LIBRARY_CMAKE}" && -f "${ZLIB_PREFIX}/lib/libz.a" ]]; then
    ZLIB_LIBRARY_CMAKE="${ZLIB_PREFIX}/lib/libz.a"
  fi
  if [[ -z "${ZLIB_INCLUDE_CMAKE}" && -d "${ZLIB_PREFIX}/include" ]]; then
    ZLIB_INCLUDE_CMAKE="${ZLIB_PREFIX}/include"
  fi
elif [[ -n "${SDK_PREFIX}" ]]; then
  ZLIB_ROOT_CMAKE="${SDK_PREFIX}/usr"
  if [[ -z "${ZLIB_LIBRARY_CMAKE}" && -f "${SDK_PREFIX}/usr/lib/libz.tbd" ]]; then
    ZLIB_LIBRARY_CMAKE="${SDK_PREFIX}/usr/lib/libz.tbd"
  fi
  if [[ -z "${ZLIB_INCLUDE_CMAKE}" && -d "${SDK_PREFIX}/usr/include" ]]; then
    ZLIB_INCLUDE_CMAKE="${SDK_PREFIX}/usr/include"
  fi
fi

BZIP2_LIBRARIES_CMAKE=${BZIP2_LIBRARIES:-}
BZIP2_INCLUDE_CMAKE=${BZIP2_INCLUDE_DIR:-}
if [[ -z "${BZIP2_LIBRARIES_CMAKE}" && -n "${SDK_PREFIX}" && -f "${SDK_PREFIX}/usr/lib/libbz2.tbd" ]]; then
  BZIP2_LIBRARIES_CMAKE="${SDK_PREFIX}/usr/lib/libbz2.tbd"
fi
if [[ -z "${BZIP2_INCLUDE_CMAKE}" && -n "${SDK_PREFIX}" && -d "${SDK_PREFIX}/usr/include" ]]; then
  BZIP2_INCLUDE_CMAKE="${SDK_PREFIX}/usr/include"
fi

# Check Python codegen dependency for jsonball
PYTHON_BIN=${PYTHON_BIN:-$(command -v python3 || true)}
if [[ -z "${PYTHON_BIN}" ]]; then
  echo "Python 3 not found. Install it with: brew install python@3" >&2
  exit 1
fi
if ! "${PYTHON_BIN}" -c 'import jinja2' >/dev/null 2>&1; then
  echo "Python module 'jinja2' is required for code generation." >&2
  echo "Install it with: ${PYTHON_BIN} -m pip install --user jinja2" >&2
  exit 1
fi

mkdir -p "$BUILD_DIR"

EXTRA_CMAKE_ARGS=()
EXTRA_CMAKE_ARGS+=(-DWITH_LIBURING=OFF)
if [[ -n "${ZLIB_PREFIX}" && -d "${ZLIB_PREFIX}" ]]; then
  EXTRA_CMAKE_ARGS+=("-DCMAKE_PREFIX_PATH=${BOOST_PREFIX};${OPENSSL_PREFIX};${SNAPPY_PREFIX};${ZLIB_PREFIX}${CMAKE_PREFIX_PATH:+;${CMAKE_PREFIX_PATH}}")
else
  EXTRA_CMAKE_ARGS+=("-DCMAKE_PREFIX_PATH=${BOOST_PREFIX};${OPENSSL_PREFIX};${SNAPPY_PREFIX}${CMAKE_PREFIX_PATH:+;${CMAKE_PREFIX_PATH}}")
fi

echo "Configuring CMake in '$BUILD_DIR' (type=$BUILD_TYPE, minimal_fc=$HIVE_BUILD_ON_MINIMAL_FC, generator=$CMAKE_GENERATOR)"
"${CMAKE_BIN}" -S . -B "$BUILD_DIR" -G "$CMAKE_GENERATOR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DHIVE_BUILD_ON_MINIMAL_FC="$HIVE_BUILD_ON_MINIMAL_FC" \
  -DBOOST_ROOT="$BOOST_PREFIX" \
  -DBoost_INCLUDE_DIR="$BOOST_PREFIX/include" \
  -DOPENSSL_ROOT_DIR="$OPENSSL_PREFIX" \
  -DOPENSSL_INCLUDE_DIR="$OPENSSL_PREFIX/include" \
  -DWITH_SNAPPY=ON \
  -DCMAKE_EXE_LINKER_FLAGS= \
  -DCMAKE_SHARED_LINKER_FLAGS= \
  -DCMAKE_MODULE_LINKER_FLAGS= \
  "${EXTRA_CMAKE_ARGS[@]}" \
  ${ZLIB_ROOT_CMAKE:+-DZLIB_ROOT="$ZLIB_ROOT_CMAKE"} \
  ${ZLIB_LIBRARY_CMAKE:+-DZLIB_LIBRARY="$ZLIB_LIBRARY_CMAKE"} \
  ${ZLIB_INCLUDE_CMAKE:+-DZLIB_INCLUDE_DIR="$ZLIB_INCLUDE_CMAKE"} \
  ${BZIP2_LIBRARIES_CMAKE:+-DBZIP2_LIBRARIES="$BZIP2_LIBRARIES_CMAKE"} \
  ${BZIP2_INCLUDE_CMAKE:+-DBZIP2_INCLUDE_DIR="$BZIP2_INCLUDE_CMAKE"}

# Build
JOBS=${JOBS:-$( (command -v sysctl >/dev/null 2>&1 && sysctl -n hw.ncpu) || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4 )}
if [[ $# -gt 0 ]]; then
  TARGETS=("$@")
else
  TARGETS=(all)
fi

echo "Building targets: ${TARGETS[*]} (parallel jobs: $JOBS)"
"${CMAKE_BIN}" --build "$BUILD_DIR" --parallel "$JOBS" --target "${TARGETS[@]}"

echo "Build completed. Binaries (if built):"
echo "  - $BUILD_DIR/programs/hived/hived"
echo "  - $BUILD_DIR/programs/cli_wallet/cli_wallet"
