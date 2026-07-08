#!/usr/bin/env bash
#
# 本地多架构静态交叉编译脚本（与 .github/workflows/release.yml 使用相同的
# abcfy2/muslcc-toolchain-ubuntu Docker 镜像 —— 它把 musl.cc 的完整工具链
# 打包分发到了 Docker Hub 上，tag 即工具链三元组名，避免直连 musl.cc 不稳定）
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
DOCKER_IMAGE_BASE="abcfy2/muslcc-toolchain-ubuntu"

# name : 工具链三元组tag : 产物文件名
TARGETS=(
  "x86_64:x86_64-linux-musl:dnsmultizone-linux-x86_64"
  "aarch64:aarch64-linux-musl:dnsmultizone-linux-aarch64"
  "armv7-hf:arm-linux-musleabihf:dnsmultizone-linux-armv7-hf"
  "armv5-sf:arm-linux-musleabi:dnsmultizone-linux-armv5-sf"
  "mips-sf:mips-linux-muslsf:dnsmultizone-linux-mips-sf"
  "mipsel-sf:mipsel-linux-muslsf:dnsmultizone-linux-mipsel-sf"
)

FILTER="${1:-}"

if ! command -v docker >/dev/null 2>&1; then
  echo "错误: 未找到 docker，请先安装并启动 Docker。" >&2
  exit 1
fi

mkdir -p "$DIST_DIR"

for entry in "${TARGETS[@]}"; do
  IFS=":" read -r name tag output <<< "$entry"

  if [[ -n "$FILTER" && "$FILTER" != "$name" ]]; then
    continue
  fi

  IMAGE="${DOCKER_IMAGE_BASE}:${tag}"
  echo "==> [$name] 拉取镜像: $IMAGE"
  docker pull "$IMAGE"

  echo "==> [$name] 编译中"
  (
    cd "$ROOT_DIR"
    make clean
    docker run --rm \
      -v "$ROOT_DIR":/work \
      -w /work \
      "$IMAGE" \
      bash -c "export DEBIAN_FRONTEND=noninteractive; apt-get update -qq && apt-get install -y -qq --no-install-recommends make >/dev/null && make CXX=${tag}-g++ CXXFLAGS='-Wall -std=c++11 -Wno-format-security' CPPFLAGS=-O2 LDFLAGS='-static -s'"
    file dnsmultizone
    mkdir -p "$DIST_DIR"
    cp dnsmultizone "$DIST_DIR/$output"
  )
  echo "==> [$name] 完成 -> dist/$output"
  echo
done

echo "全部构建完成，产物见 $DIST_DIR"
