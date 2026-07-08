#!/usr/bin/env bash
#
# 本地多架构静态交叉编译脚本（与 .github/workflows/release.yml 使用相同的
# dockcross Docker 交叉编译镜像，避免依赖 musl.cc 这类不稳定的第三方站点）
#
# 依赖：本机已安装并可用 Docker。
#
# 用法:
#   scripts/build.sh              # 编译全部架构
#   scripts/build.sh x86_64       # 只编译指定架构
#
# 产物输出到 dist/ 目录。
#
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_DIR="$ROOT_DIR/dist"

# name : docker镜像 : 产物文件名
TARGETS=(
  "x86_64:dockcross/linux-x64:dnsmultizone-linux-x86_64"
  "aarch64:dockcross/linux-arm64-musl:dnsmultizone-linux-aarch64"
  "armv7-hf:dockcross/linux-armv7l-musl:dnsmultizone-linux-armv7-hf"
  "armv5-sf:dockcross/linux-armv5-musl:dnsmultizone-linux-armv5-sf"
  "mips:dockcross/linux-mips-lts:dnsmultizone-linux-mips"
  "mipsel:dockcross/linux-mipsel-lts:dnsmultizone-linux-mipsel"
)

FILTER="${1:-}"

if ! command -v docker >/dev/null 2>&1; then
  echo "错误: 未找到 docker，请先安装并启动 Docker。" >&2
  exit 1
fi

mkdir -p "$DIST_DIR"

for entry in "${TARGETS[@]}"; do
  IFS=":" read -r name image output <<< "$entry"

  if [[ -n "$FILTER" && "$FILTER" != "$name" ]]; then
    continue
  fi

  echo "==> [$name] 拉取镜像: $image"
  docker pull "$image"

  echo "==> [$name] 编译中"
  (
    cd "$ROOT_DIR"
    make clean
    docker run --rm \
      -v "$ROOT_DIR":/work \
      -w /work \
      "$image" \
      bash -c 'make CXX=$CXX CXXFLAGS="-Wall -std=c++11 -Wno-format-security" CPPFLAGS=-O2 LDFLAGS="-static -s"'
    file dnsmultizone
    mkdir -p "$DIST_DIR"
    cp dnsmultizone "$DIST_DIR/$output"
  )
  echo "==> [$name] 完成 -> dist/$output"
  echo
done

echo "全部构建完成，产物见 $DIST_DIR"
