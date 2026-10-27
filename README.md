# 镭晟激光器官邸（RayshiningDevice）

本地开发阶段：仅协议、环形缓冲成帧器与串口 Handle。尚无设备业务、QML、任务族、Collector 或主程序接线；不打开实物设备，不自动出光。不创建远端或推送。发布许可证在正式发布前单独确认，厂商文档不纳入仓库。

## 边界

- `Protocol.h`：CRC-16/MODBUS、帧长度校验及原样数据区封装。CRC 初值 FFFF、反射多项式 A001；范围为模块地址至数据末尾，线上高字节先发。空输入保留初值 FFFF。
- `Framer.h`：每个串口连接按值独持固定 65548 字节环形数组。逐字节加入；满 4 字节找 WHLS，满 10 字节读取大端长度。整帧大小为数据长度 + 13。不足时 break 返回外层收字节；帧尾/CRC 失败只丢一字节重新找头；通过后移除并同步交付自有 QByteArray。
- `LaserHandle.h`：复用 HandleCore 的 SerialHandle。RX 原样交完整帧；TX 接收 Protocol::encode 产生的完整帧，再校验后发送，不重复加封套。parameters 提供 115200/8N1/无流控参数。现有 start 仍接收 SerialParams，调用方应采用该参数工厂。
- 回调 RxTime 是补齐帧时所在读取块的观测时间，不冒充首字节或硬件时刻；回调不得重入同一成帧器。
- 最大数据长度 65535、最小帧长 13、最大帧长 65548。索引推进不搬移数据，只有有效交付才复制完整帧。
- 本层不检查命令的业务合法性、模块能力或数据区字段含义。长度字节可表达的所有值均在容量内；坏长度声明过大时会等待收齐再重新同步，未加入帧超时或内部候选头抢占。数据停止时不会自行完成。
- 串口打开、重连、写超时、停止和等待沿用 HandleCore；成帧状态随连接销毁。末端析构首句 stop，且所有收包状态位于连接中。

协议依据：镭晟《激光器控制接口及串口通讯协议 V1.3.1》。帧头 WHLS，地址和长度大端；数据区存在按字段区分的字节序，留待业务层实现。当前不会统一把数据区转换为整数。

## 构建与测试

需要 CMake >= 3.28、C++20、Qt Core >= 6.8.3、libserialport >= 0.1.1。默认从 HandleCore 远端拉取固定提交 `379401c85f0da11adde9a2f6fde8fe767fbae707`，只启用 Serial。没有 UI 依赖。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="Qt安装前缀;libserialport安装前缀"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Windows 运行测试时将 Qt/bin 和 libserialport/bin 加到 PATH；Linux 自定义安装 libserialport 时提供其 pkg-config 路径。集成目标为 `Lidar::RayshiningDevice`；如上层已配置 HandleCore，需预先启用其 Serial 模块。

验证覆盖文档七个报文的 CRC/逐字节编码、所有两段分割位置、逐字节输入、粘包、垃圾前缀、跨读取帧头、错误头/CRC/帧尾/长度重同步、坏帧内部的有效帧、数据内 WHLS/55、最大帧与环形回绕、超长发送拒绝、读取时刻、连接状态隔离、Handle 创建销毁与 TX 裁决。每个公开头还单独编译。测试不打开串口，不代表实物/ARM 已验证。

## 后续

由用户选择种子/放大通信对象；UI 独持选项，每次发令显式传目标。内外控与光开关命令遵守固定地址 0。业务层后续处理寄存器、脉宽特殊字节序、查询及回执，QML 后续处理页面与日志，不提前加入此层。
