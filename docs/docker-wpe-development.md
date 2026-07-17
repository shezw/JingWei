<!--
    JingWei
    docs docker-wpe-development.md    2026-07-18

     ______     __  __     ______     ______     __     __
    /\  ___\   /\ \_\ \   /\  ___\   /\___  \   /\ \  _ \ \
    \ \___  \  \ \  __ \  \ \  __\   \/_/  /__  \ \ \/ ".\ \
     \/\_____\  \ \_\ \_\  \ \_____\   /\_____\  \ \__/".~\_\
      \/_____/   \/_/\/_/   \/_____/   \/_____/   \/_/   \/_/.com

    @link    : https://github.com/shezw/JingWei
    @author  : shezw
    @email   : hello@shezw.com
-->

# WPE WebKit 2.52 容器开发环境

## 目标

该环境在 Apple Silicon macOS 上使用原生 `linux/arm64` 容器，为 JingWei、WPE WebKit 2.52.5 和后续 `jw-wpe-platform` 提供可重复的 Linux 工具链。

容器只负责编译、单元测试、软件/headless 视觉烟测和开发预览。真实 DRM atomic、GBM/EGL、DMA-BUF、modifier、fence、page-flip 和显示时序必须在有 `/dev/dri` 的原生 Linux 主机或目标板验证。

## 快速开始

```bash
docker compose build wpe-dev
docker compose run --rm wpe-dev ./tools/container/build-jingwei.sh
docker compose run --rm wpe-dev ./tools/wpe/fetch-wpe.sh
docker compose run --rm wpe-dev ./tools/wpe/configure-wpe.sh
```

默认镜像把当前 JingWei 源码作为开发快照带入镜像。修改源码后再次执行 `docker compose build wpe-dev`；依赖层、WPE 源码和编译缓存不会重建。这样可以绕开 OrbStack 对外置 `/Volumes` 目录 bind mount 读取可能卡死的问题。

原生 Linux，或已确认文件共享可靠的 macOS 目录，可以启用实时只读 bind override：

```bash
docker compose -f compose.yaml -f compose.bind.yaml run --rm wpe-dev ./tools/container/build-jingwei.sh
```

完整构建和安装 WPE：

```bash
docker compose run --rm wpe-dev ./tools/wpe/build-wpe.sh
```

只验证新的 WPEPlatform/Headless 图形接口（适合日常快速迭代）：

```bash
docker compose run --rm wpe-dev ./tools/wpe/build-wpe-platform.sh
```

OrbStack 当前给 Docker VM 约 8 GiB RAM，因此默认并行度为 3。可以按实际资源调整：

```bash
WPE_BUILD_JOBS=4 docker compose run --rm wpe-dev ./tools/wpe/build-wpe.sh
```

## 构建 profile

| Profile | 用途 | 图形选项 |
| --- | --- | --- |
| `container` | macOS 容器配置、软件/headless 开发 | DRM、GBM、Wayland、媒体、WebAudio、WebCodecs、语音、ATK、gamepad 关闭；WPEPlatform headless 保留 |
| `drm` | 原生 Linux GPU 或目标板 | DRM、GBM、WPEPlatform DRM 打开；Wayland 关闭；使用完整依赖镜像 |

默认 Docker target `wpe-dev` 是图形底座优先的精简开发环境。完整媒体依赖使用 `wpe-full-dev`：

```bash
WPE_DOCKER_TARGET=wpe-full-dev docker compose build wpe-dev
```

`container` profile 是集成烟测配置，不代表最终 Web 兼容产品配置。

原生 Linux 环境执行 DRM profile：

```bash
WPE_DOCKER_TARGET=wpe-full-dev WPE_PROFILE=drm docker compose run --rm --device /dev/dri wpe-dev ./tools/wpe/build-wpe.sh
```

目标 BSP 不能直接复用通用 Ubuntu 用户态；应额外挂载目标 sysroot，并使用 SoC 厂商提供的 libc、Mesa/GBM/EGL、DRM 和交叉工具链。

## Mac 画面路径

Docker/OrbStack 在 Mac 上运行的是 Linux VM，当前容器没有 `/dev/dri`。发布端口只能传输画面和输入，不能把 Apple GPU 变成 Linux DRM 设备。

推荐分两步：

1. WPE 独立烟测可临时使用现成的虚拟 display/VNC/noVNC 组合。
2. JingWei 集成使用开发专用 `jw_backend_vnc`：把 JingWei 最终 CPU framebuffer 和 damage rectangles 交给 LibVNCServer；Mac 可使用 VNC 客户端，或由独立 noVNC/websockify sidecar 在 `http://127.0.0.1:6080` 查看；输入事件再转换回 JingWei event。

WPE WebKit 2.52.5 的新 WPEPlatform 已同时提供 `WPEBufferDMABuf` 和 `WPEBufferSHM`。其 surfaceless renderer 在不能导出 DMA-BUF 时会选择 SharedMemory swap chain，因此容器预览可以直接消费 SHM buffer，不需要先设计新的跨虚拟机图形协议。该路径会通过 `glReadPixels` 回读，适合功能开发和视觉测试，不代表目标设备的零拷贝性能。

noVNC 不安装进 WPE/JingWei 核心工具链镜像，避免引入与浏览器开发无关的 OpenStack/Python 依赖。第二步只新增一个输出 backend，不重新设计图形协议，也不进入量产依赖。量产路径保持：

```text
WPE WebKit -> jw-wpe-platform -> JingWei -> EGL/GLES -> DRM atomic KMS
```

现有 Compose 只在 loopback 上预留 VNC `5900` 端口；独立 noVNC sidecar 也应只绑定 loopback。不要使用 `xhost +`、`privileged: true`、整段 `/dev` 挂载或关闭浏览器沙箱。

## 缓存布局

- WPE 源码、构建目录和安装前缀使用独立 named volumes。
- `ccache` 使用独立 volume，默认上限 20 GiB。
- 高频写入的 WebKit build tree 不放在 macOS bind mount 上。
- JingWei 源码默认由镜像快照提供；可选 override 以只读方式挂载；生成物全部写入 Linux volume。
- arm64、amd64、Debug、Release 和目标 BSP 必须使用不同构建目录或 builder cache。

## 固定版本

- WPE WebKit：`2.52.5`
- 官方源码 SHA-256：`bcfc6c91db7659dcf24f6ff79ad27ac1eae1bc61dca0dbfee154706926740b3b`
- 默认基础镜像：`ubuntu:24.04@sha256:4fbb8e6a8395de5a7550b33509421a2bafbc0aab6c06ba2cef9ebffbc7092d90`（multi-arch manifest）。

WPE 官方发布页和校验方法：

- <https://wpewebkit.org/release/>
- <https://wpewebkit.org/release/verify/>
