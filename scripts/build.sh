#!/usr/bin/env bash
#
# 本地多架构静态交叉编译脚本（与 .github/workflows/release.yml 使用相同的 musl 工具链）
#
# 用法:
#   scripts/build.sh              # 编译全部架构
#   scripts/build.sh x86_64       # 只编译指定架构
#
# 产物输出到 dist/ 目录，工具链缓存在 .cache/toolchains/ 下，重复运行不会重复下载。
#
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE_DIR="${CACHE_DIR:-$ROOT_DIR/.cache/toolchains}"
DIST_DIR="$ROOT_DIR/dist"
MUSL_CC_BASE="${MUSL_CC_BASE:-https://musl.cc}"
MUSL_CC_MIRROR="${MUSL_CC_MIRROR:-https://more.musl.cc}"

# name : toolchain_tarball(不含.tgz) : 编译器triple前缀 : 产物文件名后缀
TARGETS=(
  "x86_64:x86_64-linux-musl-cross:x86_64-linux-musl:dnsmultizone-linux-x86_64"
  "aarch64:aarch64-linux-musl-cross:aarch64-linux-musl:dnsmultizone-linux-aarch64"
  "armv7-hf:arm-linux-musleabihf-cross:arm-linux-musleabihf:dnsmultizone-linux-armv7-hf"
  "armv5-sf:arm-linux-musleabi-cross:arm-linux-musleabi:dnsmultizone-linux-armv5-sf"
  "mips-sf:mips-linux-muslsf-cross:mips-linux-muslsf:dnsmultizone-linux-mips-sf"
  "mipsel-sf:mipsel-linux-muslsf-cross:mipsel-linux-muslsf:dnsmultizone-linux-mipsel-sf"
)

FILTER="${1:-}"

mkdir -p "$CACHE_DIR" "$DIST_DIR"

for entry in "${TARGETS[@]}"; do
  IFS=":" read -r name toolchain triple output <<< "$entry"

  if [[ -n "$FILTER" && "$FILTER" != "$name" ]]; then
    continue
  fi

  echo "==> [$name] 准备工具链: $toolchain"
  TOOLCHAIN_DIR="$CACHE_DIR/$toolchain"
  if [[ ! -d "$TOOLCHAIN_DIR" ]]; then
    TARBALL="/tmp/${toolchain}.tgz"
    echo "    下载 $MUSL_CC_BASE/${toolchain}.tgz"
    curl -fL --retry 3 -o "$TARBALL" "$MUSL_CC_BASE/${toolchain}.tgz" \
      || curl -fL --retry 3 -o "$TARBALL" "$MUSL_CC_MIRROR/${toolchain}.tgz"
    tar -xzf "$TARBALL" -C "$CACHE_DIR"
    rm -f "$TARBALL"
  fi

  CXX_BIN="$TOOLCHAIN_DIR/bin/${triple}-g++"
  if [[ ! -x "$CXX_BIN" ]]; then
    echo "    !! 找不到编译器: $CXX_BIN"
    ls "$TOOLCHAIN_DIR/bin"
    exit 1
  fi

  echo "==> [$name] 编译中"
  (
    cd "$ROOT_DIR"
    make clean
    make CXX="$CXX_BIN" \
         CXXFLAGS="-Wall -std=c++11 -Wno-format-security" \
         CPPFLAGS="-O2" \
         LDFLAGS="-static -s"
    mkdir -p "$DIST_DIR"
    cp dnsmultizone "$DIST_DIR/$output"
    file "$DIST_DIR/$output" || true
  )
  echo "==> [$name] 完成 -> dist/$output"
  echo
done

echo "全部构建完成，产物见 $DIST_DIR"
