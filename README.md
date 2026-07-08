# dnsmultizone

[English](#english) | [中文](#中文)

---

<a id="english"></a>
## English

An epoll-based multi-zone (multi-upstream) DNS forwarder for Linux. It can split DNS queries across different upstream DNS servers (`zoneList`) based on policies matching the destination IP ranges resolved, and supports hot-reloading the routing rule files (`ipList`) via inotify with no restart required.

### Features

- High-performance async networking based on `epoll`
- Multi-zone (multi-upstream DNS) query splitting/forwarding
- Routing rule files support inotify hot-reload, no restart needed
- Pure C++11 + POSIX syscalls, no third-party dependencies, fully static-linkable
- Single JSON config file, parsed with the bundled cJSON

### Quick start

#### Build locally

```bash
make
./dnsmultizone -c config/DNSMZConfig.json -b 0
```

Command-line options:

| Flag | Description |
|---|---|
| `-c <file>` | Path to the config file, defaults to `DNSMZConfig.json` |
| `-l <file>` | Log output file (used together with `-b 1` daemon mode) |
| `-b <0/1>`  | Whether to run as a background daemon |

#### Example config

See [`config/DNSMZConfig.json.example`](config/DNSMZConfig.json.example). Field reference:

- `bindIP` / `serverPort`: local listen address and port
- `zoneList`: routing rule groups, matched in ascending `priority` order
  - `dnsIP` / `dnsPort`: the upstream DNS used by this zone (or use `resolvFile` to point at a `resolv.conf`-style file that is auto-watched for changes)
  - `ipList`: list of IP-range rule files that must match for this zone to hit; `inverseIPList` inverts the match, `deny` rejects on match

### Cross-compiling / static multi-arch binaries

The project uses [musl](https://musl.libc.org/) cross toolchains for static linking, so the resulting binaries have no dependency on the target system's dynamic libraries and can be copied straight onto routers / embedded devices.

GitHub Actions (`.github/workflows/release.yml`) automatically builds static binaries for the following architectures:

| Arch | Notes | Artifact name |
|---|---|---|
| x86_64 | General PC / server | `dnsmultizone-linux-x86_64` |
| aarch64 | 64-bit ARM (Raspberry Pi 4, newer routers) | `dnsmultizone-linux-aarch64` |
| armv7-hf | 32-bit ARM hard-float (mainstream modern router SoCs) | `dnsmultizone-linux-armv7-hf` |
| armv5-sf | 32-bit ARM soft-float (old / low-end routers) | `dnsmultizone-linux-armv5-sf` |
| mips-sf | MIPS big-endian soft-float (FPU-less router SoCs) | `dnsmultizone-linux-mips-sf` |
| mipsel-sf | MIPS little-endian soft-float (common on MTK/Ralink routers) | `dnsmultizone-linux-mipsel-sf` |

**Triggers:**

- Push to `main` / open a PR: builds all architectures and uploads them as Actions artifacts for testing, no Release is created
- Push a tag (e.g. `v1.0.0`): builds all architectures and automatically creates a GitHub Release with all binaries plus a `SHA256SUMS.txt` checksum file

To cut a new release:

```bash
git tag v1.0.0
git push origin v1.0.0
```

#### Local cross-compiling (optional)

To reproduce the CI cross-compile flow locally (requires network access to download [musl.cc](https://musl.cc) toolchains):

```bash
./scripts/build.sh            # build all architectures, output in dist/
./scripts/build.sh aarch64    # build a single architecture
```

### Project layout

```
.
├── Makefile                      # supports CXX/CXXFLAGS/LDFLAGS overrides; `make` alone does a static local build
├── src/                          # source code
├── config/                       # example config
├── scripts/build.sh              # local multi-arch cross-compile script
└── .github/workflows/release.yml # CI: multi-arch build + auto release
```

### License

Released under [GPL-3.0](LICENSE).

---

<a id="中文"></a>
## 中文

基于 epoll 实现的 Linux 多分区（多上游）DNS 转发服务。可根据目标域名解析结果所属 IP 段等策略，将 DNS 查询分流到不同的上游 DNS 服务器（`zoneList`），并支持通过 `ipList` 文件热更新（inotify 监听）分流规则，无需重启。

### 特性

- 基于 `epoll` 的高性能异步网络模型
- 多 Zone（多上游 DNS）分流转发
- 分流规则文件支持 inotify 热加载，无需重启
- 纯 C++11 + POSIX 系统调用实现，无第三方依赖，可完全静态编译
- 单文件配置（JSON），基于内置的 cJSON 解析

### 快速开始

#### 本地编译

```bash
make
./dnsmultizone -c config/DNSMZConfig.json -b 0
```

命令行参数：

| 参数 | 说明 |
|---|---|
| `-c <file>` | 配置文件路径，默认为 `DNSMZConfig.json` |
| `-l <file>` | 日志输出文件（配合 `-b 1` 后台模式使用）|
| `-b <0/1>`  | 是否以后台守护进程模式运行 |

#### 配置文件示例

参见 [`config/DNSMZConfig.json.example`](config/DNSMZConfig.json.example)，字段说明：

- `bindIP` / `serverPort`：本地监听地址和端口
- `zoneList`：按 `priority` 从小到大依次匹配的分流规则组
  - `dnsIP` / `dnsPort`：该 Zone 使用的上游 DNS（也可用 `resolvFile` 指向一个 `resolv.conf` 风格文件，自动监听变化）
  - `ipList`：命中该 Zone 需要满足的 IP 段规则文件列表，`inverseIPList` 表示反选，`deny` 表示命中即拒绝

### 交叉编译 / 多架构静态可执行文件

项目使用 [musl](https://musl.libc.org/) 交叉工具链进行静态编译，产物不依赖目标系统的动态库，可直接拷贝到路由器 / 嵌入式设备上运行。

GitHub Actions（`.github/workflows/release.yml`）会自动为以下架构构建静态二进制：

| 架构 | 说明 | 产物文件名 |
|---|---|---|
| x86_64 | 通用 PC / 服务器 | `dnsmultizone-linux-x86_64` |
| aarch64 | 64 位 ARM（树莓派4等/新款路由器） | `dnsmultizone-linux-aarch64` |
| armv7-hf | 32 位 ARM 硬浮点（主流现代路由器 SoC） | `dnsmultizone-linux-armv7-hf` |
| armv5-sf | 32 位 ARM 软浮点（老旧/低端路由器） | `dnsmultizone-linux-armv5-sf` |
| mips-sf | MIPS 大端软浮点（无 FPU 路由器 SoC） | `dnsmultizone-linux-mips-sf` |
| mipsel-sf | MIPS 小端软浮点（MTK/Ralink 等常见路由器 SoC） | `dnsmultizone-linux-mipsel-sf` |

**触发方式：**

- push 到 `main` 分支 / 提 PR：仅编译全部架构并上传为 Actions Artifact，方便测试，不发布 Release
- push tag（如 `v1.0.0`）：编译全部架构，并自动创建 GitHub Release，附带全部二进制及 `SHA256SUMS.txt` 校验文件

发布一个新版本：

```bash
git tag v1.0.0
git push origin v1.0.0
```

#### 本地交叉编译（可选）

如果想在本地复现 CI 的交叉编译流程（依赖网络下载 [musl.cc](https://musl.cc) 工具链）：

```bash
./scripts/build.sh            # 编译全部架构，产物在 dist/
./scripts/build.sh aarch64    # 只编译指定架构
```

### 项目结构

```
.
├── Makefile                      # 支持 CXX/CXXFLAGS/LDFLAGS 覆盖，默认 make 即本机静态编译
├── src/                          # 源代码
├── config/                       # 示例配置
├── scripts/build.sh              # 本地多架构交叉编译脚本
└── .github/workflows/release.yml # CI: 多架构编译 + 自动发布
```

### License

本项目基于 [GPL-3.0](LICENSE) 协议开源。
