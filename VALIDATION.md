# 验证记录

## 2026-10-28：C++ 激光器官邸与回包解析

- LaserDevice 直接使用 DeviceBase + 唯一 LaserHandle，未修改共享库、未添加设备族或空 Collector。DeviceCore 从远端按固定提交 5c63ec5c501d83a15daa65a41dca438ee4ff330f 拉取。
- 新增主体语义验证 143 项：文档九寄存器示例、全部值类型、普通小端/脉宽三字节大端、字符零字节、重复项顺序、四类确认、主体所有截断位置及未知/非法值拒绝。模式和报警保留位图，没有声明完成各位业务解释。
- 新增官邸实际串口链路 47 项：查询线字节与文档一致、离线/占用/非法查询拒绝、半帧与粘包、多寄存器解析、错误回包后继续接收、RxTime 和主线程交付、断开/重连、结果独立持有、关闭后 retiring；另循环十次验证结果已排队但不泵主线程时析构，确认通知被撤销且串口资源释放。
- Windows MSVC 19.51 / Qt 6.11.2：Release /W4 构建、五个公开头独立编译、CTest 5/5 全部通过，811 项断言通过。Linux ArchHFNL GCC 16.2.1 / Qt 6.12.0：Release -Wall -Wextra -Wpedantic、相同五头/五组/811 项全部通过。最终构建无编译警告。
- 首轮测试暴露测试自身的诊断计数混用了查询失败和回包失败；改为记录接收前基数后重新通过。测试中的单个 char 常量截断警告已修正。Linux 结果回显命令曾收到 PowerShell 末尾 CR，另行直接读取最终日志确认测试全部通过；不涉及生产程序失败。
- 未打开实物串口、未进行激光出光或 ARM 验证；没有 UI，因此本轮不适用 QML/窗口冒烟。主程序、公共库与 simonly 未改，不推送。

证据：Windows build/configure-device.log、build/build-device-final.log、build/test-device-final.log；Linux ~/.cache/rayshining-ring-20261028/configure-device.log、build-device.log、build-device-final.log、test-device-final.log。

## 2026-10-28：接收只交付报文主体（用户定）

- 语义解析留给后续官邸 onStream。成帧器仍校验完整线上帧，交付时仅从环中复制地址至数据末尾，去掉 WHLS 与 CRC16/55；保留 6 字节元信息及 N 字节数据，发送仍为完整帧。
- 更新七份文档报文的预期上交主体，保留全部拆包、粘包、重同步与最大帧回绕验证。新增串口替身端到端测试，验证空数据确认、正文内 WHLS/55、未知业务字段原样上传、接收线程与时间、连接销毁后的数据所有权、完整 TX 报文。
- Windows：MSVC 19.51、Qt 6.11.2，Release /W4；Linux：GCC 16.2.1、Qt 6.12.0，Release -Wall -Wextra -Wpedantic。两端增量构建、三个公开头独立编译及 CTest 3/3 通过；协议 505 项、实际接收链 7 项、容器 109 项断言，共 621 项通过，无编译警告。
- 串口替身目标直接编译生产 LaserHandle 的元对象代码，统一使用测试 C API 声明；不链接已按真实 DLL 声明编译的生产静态库，避免 Windows 导入符号与替身重复定义。生产库仍在同次构建及原协议测试中验证。
- Linux 同步源码时曾出现不足一秒的时间戳偏差提示，按远端时间更新测试源时间戳后重新构建及执行，最终通过。未打开实物串口，不修改 HandleCore 或主程序，不推送。

证据：Windows `build/build-body-final.log`、`build/test-body-final.log`；Linux `~/.cache/rayshining-ring-20261028/build-body-final.log`、`test-body-final.log`、`test-body-confirm.log`。

## 2026-10-28：收齐头部后统一检查（用户定）

循环入口改为 `bytes_.size() >= HeaderSize`，删除循环内重复的头部长度判断。少于 10 字节时保留待续输入；完整帧仍按声明长度、帧尾和 CRC 判定，坏帧逐字节重同步。Windows（Qt 6.11.2）与 Linux（Qt 6.12.0）增量构建、公开头编译及 CTest 2/2 通过：协议 505 项、容器 109 项断言通过，无编译警告。证据为两端现有构建目录中的 `build-header-loop.log`、`test-header-loop.log`。

## 2026-10-28：改用 Boost.CircularBuffer

- 用户要求优先采用符合语义的现有库。改用 `boost::circular_buffer<char>`，删除 Framer 的自写数组、头索引、长度和取模辅助接口。协议长度、CRC、数据不足 break、坏帧丢一字节和同步交付保持。
- 两端均实际下载 SHA256 固定的 Boost 1.88.0 官方 CMake 归档，只启用 CircularBuffer 及其头文件依赖；HandleCore 固定版本保持不变。
- Windows：MSVC 19.51、Qt 6.11.2、Release /W4；库、两个测试及三个公开头构建通过，无编译警告或错误。CTest 2/2，协议 505 项、容器 109 项断言全部通过。
- Linux：ArchHFNL，GCC 16.2.1、系统 Qt 6.12.0、libserialport 0.1.2、Release -Wall -Wextra -Wpedantic；相同构建与测试全部通过，无编译警告或错误。CTest 2/2，共 614 项断言通过。
- 新增独立容器测试仅链接 Boost，无 Qt 依赖；覆盖空长度预留、容量内保留、首次和多次覆盖、移除后补满、跨回绕迭代、前缀消费、清空复用、单元素容量，以及 `shared_ptr<const T>` 的保留和覆盖释放。
- 风廓线和 Pelco-D 未迁移；未打开串口、未测试实物或 ARM，不推送仓库。

Windows 证据：build/configure-ring.log、build/build-ring.log、build/test-ring.log。
Linux 证据：~/.cache/rayshining-ring-20261028 的 configure.log、build.log、test.log。

## 2026-10-27：首次实现

2026-10-27，本地首次实现；未发布远端。

- Windows：MSVC 19.51，Qt 6.11.2，Release；库与测试启用 /W4，构建日志无警告或错误。505 项断言通过，CTest 1/1；三个公开头分别独立编译通过。
- Linux：ArchHFNL，系统 Qt 6.12.0、libserialport 0.1.2，Release；库与测试启用 -Wall -Wextra -Wpedantic，构建日志无警告或错误。505 项断言通过，CTest 1/1；三个公开头独立编译通过。Linux 本轮使用系统 Qt，未声称在 Linux Qt 6.11.2 上复测。
- 两端均从 HandleCore 远端固定提交获取通用串口依赖。未更改 HandleCore 运行逻辑。
- CRC 文档向量：外控请求 4A79、内控请求 86B8、模式确认 AF27、出光请求 DD6A、出光确认 D777、型号/脉宽查询 ACB9、对应查询回包 0660；CRC-16/MODBUS 标准字符串 123456789 得 4B37。
- 两端测试未打开任何串口。没有声称激光器实物、串口热插拔、ARM 或实际出光验证通过。

Windows 证据：build/build.log、build/Testing/Temporary/LastTest.log。
Linux 证据：~/.cache/rayshining-20261027 下 configure.log、build.log、build/Testing/Temporary/LastTest.log。

首次构建时测试生成的 Protocol.cpp 与 protocol.cpp 在 Windows 大小写不敏感目录中重名，已改为 header_Protocol.cpp 与 protocol_tests.cpp；最终构建与测试均重新执行通过。
