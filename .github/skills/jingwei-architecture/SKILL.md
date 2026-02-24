---
name: jingwei-architecture
description: JingWei(精卫) 渲染显示库的架构设计和实现标准定义
---

JingWei(精卫) 是一个2d底层渲染/显示库。主要实现了一层非常轻量和扁平的渲染显示抽象层以及硬件加速抽象。严格使用C语言作为程序语言，设计目的是为低性能嵌入式设备提供一套标准的渲染显示端口。

JingWei使用CMake作为构建系统，支持多平台构建和交叉编译。其代码结构和模块划分遵循一定的规范，以保证代码的可维护性和可扩展性。

JingWei的目录包含以下几个主要部分：

- `src/`: 包含JingWei的核心源代码，按照功能模块划分为多个子目录。
- `include/`: 包含JingWei的公共头文件，供外部使用者调用。
- `tests/`: 包含JingWei的测试代码，用于验证库的功能和性能。
- `examples/`: 包含JingWei的示例代码，展示如何使用JingWei进行渲染显示开发。
- `docs/`: 包含JingWei的文档资料，包括设计文档、API文档和使用指南等。
- `playground/`: 包含JingWei的实验性代码和原型实现，用于测试新的设计思路和功能。

JingWei主要的模块划分包括：
- `core/`: 包含JingWei的核心模块，包含 绘制、渲染、图层系统、资源管理等。
- `event/`: 包含JingWei的事件系统模块，用于处理输入事件、系统事件等。
- `proxy/`: 包含JingWei的代理模块，如渲染代理、显示代理等，用于实现不同平台和设备的适配。
- `platform/`: 包含JingWei的跨平台适配模块，如文件系统、线程等平台相关功能
- `accelerator/`: 包含JingWei的硬件加速模块，如G2D、OpenGL、Vulkan等图形API的封装和适配。
- `utils/`: 包含JingWei的工具模块，如日志系统、内存管理等辅助功能。
