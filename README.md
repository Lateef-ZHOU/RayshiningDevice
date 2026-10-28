# 镭晟激光器官邸（RayshiningDevice）

本地开发阶段：协议、环形缓冲成帧器、串口 Handle 与 C++ 激光器官邸。官邸接通寄存器查询和回包解析；尚无 QML、任务族、Collector 或主程序接线，不自动连接、查询或出光。发布许可证在正式发布前单独确认，厂商文档不纳入仓库。

## 边界

- `Protocol.h`：CRC-16/MODBUS、帧长度校验及原样数据区封装。CRC 初值 FFFF、反射多项式 A001；范围为模块地址至数据末尾，线上高字节先发。空输入保留初值 FFFF。
- `Framer.h`：每个串口连接独持 `boost::circular_buffer<char>`，构造时预留 65548 字节、实际长度为 0。容量以内追加保留旧数据，满后再次追加才覆盖最旧字节；长度和回绕由 Boost 管理，不自写容器或另加封装。逐字节加入；收齐 `HeaderSize`（10 字节）后才进入检查循环，核对 WHLS 并读取大端长度。整帧大小为数据长度 + 13。不足时 break 返回外层收字节；帧尾/CRC 失败只丢一字节重新找头；通过后移除并同步交付自有 QByteArray。
- `LaserHandle.h`：复用 HandleCore 的 SerialHandle。RX 交出已验证并去除外层帧头尾的报文主体；TX 接收 `encode` 产生的完整帧，再校验后发送，不重复加封套。parameters 提供 115200/8N1/无流控参数。现有 start 仍接收 SerialParams，调用方应采用该参数工厂。
- `LaserDevice.h`：本型号的模块/命令/寄存器枚举、回复值类型和语义解析均在此定义；寄存器格式映射为类内私有静态函数，供查询校验和解析共用。final C++ 官邸直接继承 DeviceBase，按值持有唯一 LaserHandle。现有 Device 模板强制配套 Collector，本型号暂不参加任务，因此不造空 Collector 或新设备族。借用 DeviceBase 的身份、状态、锁和日志源契约，未修改 DeviceCore。
- 回调 RxTime 是补齐帧时所在读取块的观测时间，不冒充首字节或硬件时刻；回调不得重入同一成帧器。
- 最大数据长度 65535、最小帧长 13、最大帧长 65548。每次追加后先处理候选，达到最大帧长时必然验证或丢弃坏头，因此合法最大帧不会因覆盖而损失头部。移除前缀不搬移剩余字节；有效帧直接从环中复制主体一次，不先复制完整帧再裁剪。
- 成帧与 Handle 层不检查命令的业务合法性、模块能力或数据区字段含义。长度字节可表达的所有值均在容量内；坏长度声明过大时会等待收齐再重新同步，未加入帧超时或内部候选头抢占。数据停止时不会自行完成。
- 串口打开、重连、写超时、停止和等待沿用 HandleCore；成帧状态随连接销毁。LaserHandle 析构首句 stop，且所有收包状态位于连接中。

协议依据：镭晟《激光器控制接口及串口通讯协议 V1.3.1》。帧头 WHLS，地址和长度大端；数据区按字段区分字节序，由官邸解释，不统一转换为整数。

## 接收交付格式

成帧器检查完整线上帧后，移除开头 `WHLS`（4 字节）和末尾 `CRC16 + 0x55`（3 字节），交付自有 `QByteArray`：

```text
地址（2） + 命令（1） + 脉宽号（1） + 数据长度（2） + 数据（N）
```

主体相对偏移 0/2/3/4/6 分别对应以上字段。地址和数据长度仍保留大端原始字节，数据区不作数值解释；正文中的 WHLS/55 不会被删除。空数据确认仍有 6 字节，例如模式切换确认上交 `00 00 B7 00 00 00`，不会退化成空数组。

官邸的 `makeOnStream` 现场生成回调交给 Handle 独持，直接在回调内解释主体；失败处触发现有 `handle_.logMessage` 后返回，不通过返回值中转错误字符串。成功后以官邸为上下文排队到主线程发出 `replyReady(RxTime, const Reply&)`。不另设公开解码入口或独立回复文件，出现实际复用需求时再抽取。RXRuling 和 DataCallback 仍使用 QByteArray，HandleCore 接口不变。发送仍采用完整线上帧，不套用接收主体格式。

## C++ 官邸接口与数据

- `open(SerialPortInfo)` / `close()`：显式连接与同步关闭；连接规格固定 115200/8N1/无流控。重连行为仍由 Handle 负责，官邸不缓存端口、模块选择或 UI 草稿。串口枚举直接使用 HandleCore 的串口接口。
- `readRegisters(Module, quint8 widthIndex, const QList<Register>&)`：显式提交一次查询，模块为 Seed/Amplifier，脉宽号 0..5，寄存器非空且属于已定义枚举。发送前使用 canEngage；拒绝时写具体失败日志，Handle 已报告的发送失败不重复记录。返回 true 仅表示发送成功，不等于收到确认；没有待回包账本、轮询或自动重试业务。
- `replyReady`：整包交付 `Reply{module, command, widthIndex, readings}`。B1 支持任意多个完整 8 字节寄存器记录，保留原顺序及重复项。B5/B6/B7/B8 为无数据确认；其中 B7/B8 要求地址和脉宽号均为 0。无数据确认不伪造当前模式、光开关通道或最新出光参数。

`Reading` 持有 `Register` 地址和 `RegisterValue`，后者用 variant 表达 uint16 位图、uint32、float、固定四字符、EmissionState 或 PulseWidthOption，不再另存可与值不一致的“数据类型”字段。地址与主体长度大端；寄存器地址及普通数值小端；脉宽选项按文档示例为首字节可选标记（00/80）及后三字节大端值。序列号/型号片段保留零字节，不在后端拼接或缓存。模式与报警保留文档定义的低 16 位位图，位解释留给显示业务。

未定义地址/命令/寄存器、非法枚举值、残缺记录和非有限浮点数均拒绝整份业务回包，并通过现有 Handle.logMessage 报告原因；不上传部分结果，下一帧继续解析。Handle 的成帧校验不受这些业务拒绝影响。未来协议新增寄存器时需补充本仓类型映射，不把未知地址冒充已定义枚举。

时间按 RxTime 值传递；官邸不持 latest 状态或历史数组。`Reply` 已登记 Qt 元类型，当前是 C++ 接口，未登记 QML 类型或暴露 QVariant 转换门面。创建、连接、查询、关闭和析构均在对象所属主线程进行。析构首句执行私有 PreDestruction：先 Handle.stop 等回调退出，再 retiring 和父层释放锁；Qt 上下文销毁撤销尚未交付的回复。连接 false 仍按现有异步通知释放锁，未添加连接代数或改变已接受的延迟通知边界。

## 构建与测试

需要 CMake >= 3.28、C++20、Qt Core >= 6.8.3、libserialport >= 0.1.1、Boost.CircularBuffer。默认从 HandleCore 远端拉取固定提交 `379401c85f0da11adde9a2f6fde8fe767fbae707`，只启用 Serial；DeviceCore 固定 `5c63ec5c501d83a15daa65a41dca438ee4ff330f`。上层已提供目标时直接复用。没有 UI 或 TaskCore 依赖。

Boost 优先复用上层已有的 `Boost::circular_buffer` 目标，或查找已安装的 Boost >= 1.88 CMake 包；否则自动下载 Boost 1.88.0 官方 CMake 归档，并核验 SHA256。仅启用 CircularBuffer 及其必要的头文件依赖，不编译其他 Boost 库、不增加运行时动态库。首次自动下载约 100 MB；离线构建可通过 `FETCHCONTENT_SOURCE_DIR_BOOST` 指向已解压的同版 CMake 归档。

容器使用 [Boost Software License 1.0](https://www.boost.org/LICENSE_1_0.txt)，第三方源码随下载归档保留许可证。容器行为见 [Boost.CircularBuffer 官方说明](https://www.boost.org/doc/libs/1_88_0/doc/html/circular_buffer.html)。这里使用固定容量的 `circular_buffer`，不使用按需分配的 `circular_buffer_space_optimized`。其纯 C++ 头文件也可供后续风廓线和 Pelco-D 复用；本次不修改这两条现有链路。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="Qt安装前缀;libserialport安装前缀"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Windows 运行测试时将 Qt/bin 和 libserialport/bin 加到 PATH；Linux 自定义安装 libserialport 时提供其 pkg-config 路径。集成目标为 `Lidar::RayshiningDevice`；如上层已配置 HandleCore，需预先启用其 Serial 模块。

协议验证覆盖文档七个报文的 CRC/逐字节编码及上交主体、所有两段分割位置、逐字节输入、粘包、垃圾前缀、跨读取帧头、错误头/CRC/帧尾/长度重同步、坏帧内部的有效帧、数据内 WHLS/55、最大帧与环形回绕、超长发送拒绝、读取时刻、连接状态隔离、Handle 创建销毁与 TX 裁决。串口替身验证真实接收链交付的主体、空数据确认、未定义业务字段原样交付、连接销毁后的数据所有权及发送完整帧。独立容器测试只依赖 Boost，验证预留不填满、容量内保留、超出覆盖、多次回绕、移除后再追加、清空后复用，以及只读共享份额的覆盖释放。每个公开头还单独编译。测试不打开串口，不代表实物/ARM 已验证。

## 后续

由用户选择种子/放大通信对象；UI 独持选项，每次发令显式传目标。下一步设计 QML 页面、回复展示，以及出光/关光、内外控和光开关的显式命令入口；内外控与光开关命令遵守固定地址 0。当前仅接通查询发送及所有已定义回复的接收，不加入自动出光、控制联动或主程序接线。
