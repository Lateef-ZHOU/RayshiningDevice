# 第三方依赖

本仓自有源码、测试、构建脚本与文档采用 MIT，Copyright (c) 2026 周涛 (ZHOU TAO)。厂商手册、SDK、驱动和第三方运行库不随仓库发布。

- HandleCore、DeviceCore、UICore、ChartCore、Algorithm：MIT，保留各仓固定版本的 LICENSE。
- Qt 6 Core/Gui/Qml/Quick：按实际模块的 LGPLv3/GPL 或商业授权分发；ShaderTools 为构建依赖。GUI 私有接口要求与所用 Qt 版本一致。分发 Qt 动态库需保留其许可、版权与对应源码提供方式。
- libserialport（>=0.1.1）：LGPL-3.0-or-later，源码与许可证见 https://github.com/sigrokproject/libserialport 。分发动态库时保留原许可并提供对应源码及所需构建材料。
- Boost.CircularBuffer：Boost Software License 1.0；CMake 默认引用 Boost 1.88.0 并核验归档 SHA256。保留上游 LICENSE_1_0.txt 与相关版权声明，见 https://www.boost.org/LICENSE_1_0.txt 。

本设备同时启用 HandleCore 的 Serial 和 Pcap。Linux 的 libpcap 使用其 BSD 风格许可；Windows 的 Npcap SDK 与运行驱动按 Npcap 自身许可处理，SDK 可用不代表可任意再分发驱动安装包，具体边界沿用 HandleCore 的第三方说明。本仓不附带 Npcap 安装包。自有代码的 MIT 不会重新授权第三方依赖；库源码发布与包含运行库的程序分发须分别处理。
