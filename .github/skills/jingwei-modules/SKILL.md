---
name: jingwei-modules
description: JingWei(精卫) 的核心模块设计方案和详细定义。
---

JingWei 采用极简的模块化设计，核心围绕 `Core` (逻辑)、`Proxy` (接口) 和 `Accelerator` (实现) 展开。本设计严格遵循 `jingwei-architecture` 定义的原则，使用 C 语言实现。

## 1. 核心模块 (Core)

**职责**：管理全局上下文、显示设备对象、图层树以及渲染循环调度。Core 是 JingWei 的大脑，负责逻辑控制，不直接操作硬件。

#### 关键结构体设计

```c
// 全局上下文，管理所有 Display 和事件循环
typedef struct jw_context {
    struct jw_display **displays;  // 支持多显示器
    int display_count;
    struct jw_event_loop *loop;    // 主事件循环
    void *user_data;               // 用户自定义数据
} jw_context_t;

// 显示设备对象，对应物理屏幕或虚拟输出
typedef struct jw_display {
    int id;
    int width, height;
    struct jw_layer *layers;       // 图层链表头节点 (Z轴最底层)
    struct jw_proxy *proxy;        // 对应的后端代理 (桥接硬件)
    struct jw_buffer *framebuffer; // 最终合成的 Buffer
} jw_display_t;

// 图层对象，扁平化管理，仅通过链表顺序表达 Z 轴 (Paint Order)
typedef struct jw_layer {
    int id;
    int x, y, width, height;       // 图层几何信息
    uint8_t opacity;               // 不透明度 (0-255)
    bool visible;                  // 可见性
    struct jw_buffer *buffer;      // 图层内容 (像素数据)
    
    struct jw_layer *next;         // 下一个图层 (Z轴更靠上)
    struct jw_layer *prev;         // 上一个图层 (Z轴更靠下)

    // 脏矩形区域，用于局部渲染优化
    struct jw_rect dirty_rect; 
} jw_layer_t;
```

## 2. 代理模块 (Proxy)

**职责**：将 Core 层的渲染/显示请求桥接到具体的硬件或软件实现。这是架构中最关键的解耦层，允许 JingWei 在不同平台上无缝运行。

#### 接口设计 (V-Table)

```c
// Proxy 操作接口表
typedef struct jw_proxy_ops {
    // 生命周期管理
    int (*init)(struct jw_proxy *proxy);
    void (*deinit)(struct jw_proxy *proxy);

    // 显示控制
    int (*commit)(struct jw_proxy *proxy, struct jw_buffer *fb); // 提交帧缓冲区到屏幕 (Flip/PageFlip)
    int (*vsync_wait)(struct jw_proxy *proxy); // 等待垂直同步信号
    
    // 硬件能力查询
    bool (*has_capability)(struct jw_proxy *proxy, jw_capability_t cap);
} jw_proxy_ops_t;

// Proxy 实例
typedef struct jw_proxy {
    struct jw_context *ctx;
    const jw_proxy_ops_t *ops;
    
    // 关联的硬件加速器
    struct jw_accelerator *accle;
    
    // 实现方自行扩展的结构体通常通过 container_of 宏获取，不需要泛型指针
    // 例如 struct drm_proxy { struct jw_proxy base; int drm_fd; ... };
} jw_proxy_t;
```

**典型实现**：
*   `backend_fbdev`: Linux Framebuffer 设备。
*   `backend_drm`: Linux DRM/KMS 子系统。
*   `backend_sdl`: SDL2 窗口 (用于 PC/Mac 开发调试)。

## 3. 硬件加速模块 (Accelerator)

**职责**：提供 2D 图形加速能力的抽象。Core 层在进行图层合成或绘制操作时，优先调用加速器接口，否则回退到软件实现。

#### 接口设计

```c
typedef struct jw_accelerator jw_accelerator_t;

// 加速器操作接口表
struct jw_accelerator_ops {
    // 基础 2D 操作
    int (*fill_rect)(jw_accelerator_t *acc, struct jw_buffer *dst, struct jw_rect *rect, uint32_t color);
    
    // 拷贝与合成
    int (*blit)(jw_accelerator_t *acc, struct jw_buffer *src, struct jw_rect *src_rect,
                struct jw_buffer *dst, struct jw_rect *dst_rect);
    
    // 高级操作 (可选)
    int (*blend)(jw_accelerator_t *acc, struct jw_buffer *src, struct jw_buffer *dst, jw_blend_mode_t mode);
    int (*scale)(jw_accelerator_t *acc, struct jw_buffer *src, struct jw_buffer *dst); // 缩放
    
    // 硬件特定的同步操作
    int (*sync)(jw_accelerator_t *acc);
};

// 加速器实例
struct jw_accelerator {
    const struct jw_accelerator_ops *ops;
    
    // 加速器持有的资源 (如 G2D Handle, GPU Context 等)
    // 通常通过派生结构体实现，例如 struct g2d_accelerator { struct jw_accelerator base; int fd; ... };
};
```

**实现示例**：
*   `accelerator/soft`: 纯 CPU 实现 (默认回退)。
*   `accelerator/g2d`: 针对特定 SoC (如 Allwinner, Rockchip) 的 G2D 硬件接口。
*   `accelerator/gl`: 基于 OpenGL ES 的实现。

## 4. 事件模块 (Event)

**职责**：统一管理输入事件 (Input) 和系统信号 (System)，驱动主循环。

#### 设计方案

```c
typedef enum {
    JW_EVENT_NONE,
    JW_EVENT_TOUCH,      // 多点触摸事件 (手指)
    JW_EVENT_MOUSE_KEY,  // 鼠标按键 (左键、右键、中键)
    JW_EVENT_MOUSE_MOVE, // 鼠标移动事件
    JW_EVENT_MOUSE_WHEEL,// 鼠标滚轮事件
    JW_EVENT_KEY,        // 物理按键事件 (键盘)
    JW_EVENT_SYSTEM,     // 系统消息 (休眠、唤醒、热插拔)
    JW_EVENT_VSYNC       // 垂直同步信号
} jw_event_type_t;

typedef struct jw_event {
    jw_event_type_t type;
    uint64_t timestamp;
    union {
        // 多点触摸 (JW_EVENT_TOUCH)
        struct { 
            int x, y;       // 坐标
            int id;         // 触摸点 ID (0-9)
            int state;      // JW_TOUCH_DOWN, JW_TOUCH_UP, JW_TOUCH_MOVE
            float pressure; // 压力值 (0.0-1.0)
        } touch;

        // 鼠标移动 (JW_EVENT_MOUSE_MOVE)
        struct { 
            int x, y;       // 绝对坐标
            int dx, dy;     // 相对位移
        } mouse_move;

        // 鼠标按键 (JW_EVENT_MOUSE_KEY)
        struct {
            int button;     // JW_MOUSE_LEFT, JW_MOUSE_RIGHT, JW_MOUSE_MIDDLE
            int state;      // JW_MOUSE_DOWN, JW_MOUSE_UP
            int x, y;       // 按键触发时的坐标
        } mouse_key;

        // 鼠标滚轮 (JW_EVENT_MOUSE_WHEEL)
        struct {
            int x, y;       // 滚动方向和距离
        } wheel;

        // 物理按键 (JW_EVENT_KEY)
        struct { 
            int code;       // Key Code (Linux Input Event Code)
            int state;      // JW_KEY_DOWN, JW_KEY_UP, JW_KEY_REPEAT
        } key;
    } data;
} jw_event_t;

// API
void jw_event_loop_run(jw_context_t *ctx);           // 启动主循环
void jw_dispatch_event(jw_context_t *ctx, jw_event_t *ev); // 分发事件到当前 Display
```

## 5. 目录结构规划 (src/)

```text
src/
├── core/
│   ├── jw_context.c      // 上下文与主循环逻辑
│   ├── jw_display.c      // 显示对象逻辑
│   ├── jw_layer.c        // 图层树操作与管理
│   └── jw_buffer.c       // 显存/Buffer 管理器
├── proxy/
│   └── jw_proxy.c        // Proxy 基类与工厂模式
├── accelerator/
│   ├── jw_accel.c        // 加速器抽象层
│   └── soft_renderer.c   // 软件渲染实现 (纯 CPU)
├── event/
│   └── jw_event.c        // 事件队列与分发
└── platform/
    ├── sys/              // 系统基础适配
    │   ├── jw_time.c     // 高精度时间封装
    │   └── jw_log.c      // 日志与调试系统
    ├── backend/          // 显示后端实现 (实现 Proxy 接口)
    │   ├── fbdev/        // Linux Framebuffer
    │   ├── drm/          // Linux DRM/KMS
    │   └── sdl/          // SDL2 模拟实现
    └── input/            // 输入设备适配
        └── evdev/        // Linux Input Subsystem
    └── accelerator/      // 硬件加速器实现
        ├── g2d/          // Allwinner G2D
        ├── rga/          // Rockchip RGA
        └── gl/           // OpenGL ES
```
