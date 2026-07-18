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

该环境在 Apple Silicon macOS 上使用原生 `linux/arm64` 容器，为 JingWei、WPE WebKit 2.52.5 和 `jingwei_wpe` 提供可重复的 Linux 工具链。

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

WPE 前缀完成后，只刷新 JingWei 源码快照并重编译适配器/浏览器：

```bash
./tools/dev/rebuild-jingwei.sh
```

启动确定性页面并从 Mac 回环端口验证真实像素、点击和键盘：

```bash
./tools/test/smoke-browser.sh
./tools/test/runtime-report.sh
open vnc://127.0.0.1:5900
```

macOS“屏幕共享”提示密码时输入 `jwview52`。这是只用于 loopback 开发预览的兼容密码，不是远程安全边界。需要自定义时准备一个只包含 1–8 个可打印 ASCII 字符的文件：

```bash
JINGWEI_VNC_PASSWORD_FILE=/absolute/path/to/private-password \
docker compose up -d --force-recreate jw-browser
```

Compose 将该文件作为 `/run/secrets/jingwei-vnc-password` 只读挂载；密码不会出现在 Browser 命令行、READY 行或容器环境变量中。经典 VNC Authentication 不加密画面和输入，因此端口仍必须保持 loopback，跨机器访问应另加 SSH/TLS 隧道。

smoke 通过后会保留 `jw-browser` 运行，并在 `test-results/rfb-wpe-e2e/` 保存初始、点击和键盘完成三张 PPM 证据帧。

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
| `container` | macOS 容器配置、软件/headless 开发 | DRM 库保留给 WPE 公共代码；GBM、DRM backend、Wayland、媒体、WebAudio、语音关闭；WPEPlatform headless 保留 |
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

1. `jingwei_wpe` 接收 WPEPlatform 的 SHM ARGB8888 buffer，并复制到 JingWei BGRA8888 surface。
2. `jw_vnc_backend` 把 JingWei CPU framebuffer 和 damage rectangles 交给 LibVNCServer；Mac 使用 VNC 客户端查看，输入事件再转换为 WPE pointer/key event。

WPE WebKit 2.52.5 的新 WPEPlatform 已同时提供 `WPEBufferDMABuf` 和 `WPEBufferSHM`。其 surfaceless renderer 在不能导出 DMA-BUF 时会选择 SharedMemory swap chain，因此容器预览可以直接消费 SHM buffer，不需要先设计新的跨虚拟机图形协议。该路径会通过 `glReadPixels` 回读，适合功能开发和视觉测试，不代表目标设备的零拷贝性能。

noVNC 不安装进 WPE/JingWei 核心工具链镜像，避免引入与浏览器开发无关的 OpenStack/Python 依赖。第二步只新增一个输出 backend，不重新设计图形协议，也不进入量产依赖。量产路径保持：

```text
WPE WebKit -> jingwei_wpe -> JingWei -> EGL/GLES -> DRM atomic KMS
```

现有 Compose 只把 VNC `5900` 发布到 Mac loopback。容器丢弃全部 capability 并设置 `no-new-privileges`；Browser 只挂载只读的 JingWei build 和 WPE prefix，不会得到 source、WPE build、download 或 compiler-cache 卷。Docker Desktop/OrbStack 不允许 WPE 再创建 Bubblewrap 用户命名空间，因此 `jw-browser` 仅在该受限容器内设置 `WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS=1`。原生 Linux/量产部署应恢复 WPE sandbox，不要使用 `xhost +`、`privileged: true` 或整段 `/dev` 挂载。

## 内存与速度开关

`jw-browser` 默认面向单页嵌入式 UI：

- `JINGWEI_JSC_USE_JIT=false`：请求 JavaScriptCore 通过 `JSC_useJIT` 关闭 JIT，优先内存和可预测性；容器入口只接受 `true` 或 `false`。
- `JINGWEI_CACHE_MODEL=document-viewer`：选择 WebKit 最小化缓存策略。
- 两项均可在 Compose 启动时覆盖：

```bash
JINGWEI_JSC_USE_JIT=true \
JINGWEI_CACHE_MODEL=web-browser \
docker compose up -d --force-recreate jw-browser
```

缓存模型还接受 `document-browser`。关闭 JIT 不改变 JavaScript 语言兼容性，但会降低计算密集 JS 性能；具体内存收益依页面脚本和运行时间而异。

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
