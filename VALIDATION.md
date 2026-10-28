# 验证记录

## 2026-10-28：统一连接规格摘要（用户定）

规格摘要从实际 SerialParams 生成，QML 只读显示；连接、协议和轮询不变。Windows Qt 6.11.2 Release 增量构建、qmllint、既有 QML 1/1（12 项、8 秒离屏）通过，无编译/QML 警告。首轮测试未加入 libserialport 运行库搜索路径而超时，补齐 PATH 后通过，未改运行代码或测试超时。证据：build/build-connection-summary.log、build/test-connection-summary.log。未重复 Linux/协议全套，无实物连接；仅本地提交。

## 2026-10-28：删除串口刷新按钮（用户定）

删除刷新按钮及其处理逻辑，端口枚举结果改为只读；固定线路参数、端口选择和连接/断开保持。Windows Release 增量构建、qmllint、既有 QML 1/1（12 项、8 秒离屏）通过，日志 build/build-no-refresh.log、build/test-no-refresh.log；未重复协议和 Linux 全套，无实物连接。本地提交，未推送。

## 2026-10-28：状态收回右侧栏与串口选择统一（用户定）

- 种子/放大的状态分别由 LaserStatus 收缩栏显示，左侧两个图窗直接使用 LaserTelemetryChart，删除混合状态与图表的 LaserMonitor 及 append/clear 转发。状态仍引用原有回报，不新增数据缓存或查询触发。
- 串口改用网卡同款 ChoiceField，不可编辑；连接读取当前选项，刷新尽量保持同一端口。未修改 C++、协议、轮询或连接行为。
- Windows Qt 6.11.2：增量 Release 和 RayshiningDevice_qmllint 通过，既有 QML 检查 1/1（12 项、含 8 秒离屏）通过。未新增镜像实现的测试，未重复协议及 Linux 全套。日志为 build/build-sidebar.log、build/build-sidebar-qml.log、build/test-sidebar.log。
- 无编译/QML 警告；重新配置出现本机 Qt 可选 QmlAssetDownloaderPrivate 缺 Qt6TaskTree 提示，不影响本项目所用目标。主程序固定版本接入与实际布局检查记在主程序维护操作。本次仅本地提交，不推送。

## 2026-10-28：QML 官邸与主程序接入（用户定）

- 新增种子/放大两个状态与趋势页、串口、显式发令和查询分区。Qt attached 对象保证页面唯一拥有具体后端；Reply 在原头内提供只读值适配。UI 不缓存待确认发送参数，不因读取回包、空图或测量值更新自动查询；B5/B6/B7 确认才补一次对应状态查询，B8 仅记确认。
- 新增 20 项发送检查：文档内/外控字节向量、功率/频率小端及脉宽大端、种子固定 100%、模块与通道地址、非法输入不发送及验锁。新增 12 项实际 QML 检查：唯一所有权、离线不连接、类型化回报按模块显示、查询不循环、确认仅查一次、绘图不查询、未知脉宽不出光与 8 秒存活。全部使用替身或离线对象，未打开真实串口。
- Windows MSVC 19.51 / Qt 6.11.2 与 Linux GCC 16.2.1 / Qt 6.12.0：Release、四个公开头单独编译、CTest 8/8 共 921 项检查通过，编译无警告；新官邸 CMake qmllint 通过。Windows 离屏有既有 FreeType 字体提示；Linux 最终使用 C.UTF-8，无 QML 运行错误。
- 主程序装配验证读取真实 Workbench.qml，检查三设备、三任务、GMB/DAQ 各一项，旧设备未实例化、激光器唯一后端及 8 秒存活；并检查实际 Direct3D 界面截图。主程序仅 Windows 验证；独立官邸 Linux 检查不等于主程序整体 Linux 移植。主程序部署与依赖证据见其维护操作。
- 初次接入修正了 AUTOMOC 入口、生成注册代码的私有头搜索路径，以及 Qt 对 final 类型默认构造的限制；保留 final，改为明确传入 parent。截图检查发现串口显示与草稿可能不一致，改为直接别名绑定编辑框的实际值，未增加 C++ 表单缓存。

证据：本仓 Windows build/build-ui-final.log、test-ui-final.log、lint-ui.log；Linux ~/.cache/rayshining-ring-20261028/ 同名 build/test 日志及 lint-ui-final.log。主程序图为本机维护目录 laser-ui-main-final-20261028.png（仅验证程序注入示例曲线）。未推送。

## 2026-10-28：官邸缓存六值、按包全量推送（用户定）

- LaserDevice 按种子/放大模块各持一份六值缓存；完整报文先验证，再在主线程更新。每包有绘图寄存器时只发一次 telemetryReady，未回报项沿用旧值、从未回报项为 NaN。replyReady 只交非绘图寄存器及命令确认；纯状态包不触发绘图，没有跨包等待、更新掩码或自动查询。
- 新增 41 项生产接收链验证：首包缺项占位、同包六项合并、部分更新沿用、已交出快照独立、模块隔离、重复寄存器末值生效、状态与确认不绘图、混合包分别交付、坏包不污染缓存、六个粘包分别产生完整快照、主线程交付及显式重新连接清空旧值。原回包测试适配两类信号，并继续检查非法报文不交付部分结果。
- Windows MSVC 19.51 / Qt 6.11.2 与 Linux GCC 16.2.1 / Qt 6.12.0：Release 构建和四个公开头独立编译通过，无编译警告；两端 CTest 均 6/6 通过。协议 505、串口交付 7、环形容器 109、官邸回复 180、设备生命周期 47、绘图快照 41，共 889 项断言通过。
- ChartCore 在其本地仓库同步收紧为完整六值入口，验证记录在该仓 README；激光器 QML 官邸及实际信号接线仍待实现。本轮串口测试使用替身，未连接激光器、未进行出光操作。主程序、simonly 和公共 Handle 运行逻辑未改，不推送。

证据：Windows build/build-telemetry.log、build/test-telemetry.log；Linux ~/.cache/rayshining-ring-20261028/build-telemetry.log、test-telemetry.log。

## 2026-10-28：解析收回官邸、失败就地记日志（用户定）

- 回复类型和语义解析移入 LaserDevice.h，删除 Reply.h、公开 decodeReply 及 variant<Reply, QString> 错误返回/转发。makeOnStream 返回的回调直接解析；失败处触发现有 Handle.logMessage 并返回，成功仍排队到主线程交出完整 Reply。格式映射收为类内私有静态函数，查询与解析共用。
- 原语义测试改经串口替身、真实成帧器及官邸回调执行；每个用例追加有效确认作为交付屏障，检查只产生一份结果或一条 Handle 诊断、无部分交付，并验证坏业务回包后仍能继续收包。类型、字节序、确认规则保持。原来直接调用解码器的主体截断检查退出此组，线上拆包与长度/CRC 拒绝仍由原 protocol 测试覆盖，不保留测试专用的公开解析口。
- Windows MSVC 19.51 / Qt 6.11.2 与 Linux GCC 16.2.1 / Qt 6.12.0：Release 增量构建和四个公开头独立编译通过，无编译警告；CTest 两端均 5/5 通过。协议 505、串口交付 7、环形容器 109、官邸回复 179、设备生命周期 47，共 847 项断言通过。
- 文档和构建清单已同步。没有修改协议规则、Handle、主程序或 UI；没有实物操作，不推送。

证据：Windows build/build-inline-parser.log、build/build-inline-parser-final.log、build/test-inline-parser-final.log；Linux ~/.cache/rayshining-ring-20261028/ 下同名构建与测试日志。

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
