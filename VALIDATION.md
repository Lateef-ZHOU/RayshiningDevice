# 验证记录

2026-10-27，本地首次实现；未发布远端。

- Windows：MSVC 19.51，Qt 6.11.2，Release；库与测试启用 /W4，构建日志无警告或错误。505 项断言通过，CTest 1/1；三个公开头分别独立编译通过。
- Linux：ArchHFNL，系统 Qt 6.12.0、libserialport 0.1.2，Release；库与测试启用 -Wall -Wextra -Wpedantic，构建日志无警告或错误。505 项断言通过，CTest 1/1；三个公开头独立编译通过。Linux 本轮使用系统 Qt，未声称在 Linux Qt 6.11.2 上复测。
- 两端均从 HandleCore 远端固定提交获取通用串口依赖。未更改 HandleCore 运行逻辑。
- CRC 文档向量：外控请求 4A79、内控请求 86B8、模式确认 AF27、出光请求 DD6A、出光确认 D777、型号/脉宽查询 ACB9、对应查询回包 0660；CRC-16/MODBUS 标准字符串 123456789 得 4B37。
- 两端测试未打开任何串口。没有声称激光器实物、串口热插拔、ARM 或实际出光验证通过。

Windows 证据：build/build.log、build/Testing/Temporary/LastTest.log。
Linux 证据：~/.cache/rayshining-20261027 下 configure.log、build.log、build/Testing/Temporary/LastTest.log。

首次构建时测试生成的 Protocol.cpp 与 protocol.cpp 在 Windows 大小写不敏感目录中重名，已改为 header_Protocol.cpp 与 protocol_tests.cpp；最终构建与测试均重新执行通过。
