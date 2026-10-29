# 验证记录

## 2026-10-29：连接通知统一查询与关闭收尾，并发布固定依赖（用户定）

- Windows Qt 6.11.2 / MSVC Release 构建、公开头与 qmllint 通过，无编译及 QML 静态诊断；CTest 8/8（17.12 秒），其中真实 QML 官邸 110 项断言，串口仅替换为受控 C API，不连接实物。覆盖首查脉宽/型号、立即状态查询、实际回包填入选项、关闭前一次关光、清空与停表、重复关闭、迟到 true 及关闭后入队回包。
- Linux Qt 6.12.0 / GCC 16.2.1 Release 构建、公开头及 CTest 8/8（11.38 秒）通过，编译无警告。qmllint 目标通过；Qt 6.12 对上游 SurfaceChart.qml 既有 state id 给出 id-shadows-member 提示，本轮激光器 QML 无诊断，未扩展修改曲面代码。
- 两平台均取消 HandleCore、DeviceCore、ChartCore 和 UICore 本地源码覆盖，核对实际远端检出依次为 3470a1d、b392e3f、a12d764、5d4ccc1。测试构建的 Windows 后端使用可替换串口 C 符号，只有独立 BUILD_TESTING 路径启用；被主程序作为依赖使用时仍链接真实串口库。
- Windows 证据：build/publish-configure.log、publish-build.log、publish-lint.log、Testing/Temporary/LastTest.log；Linux 证据：维护机 ~/.cache/lidar-connection-20261029/laser-*.log。没有实物频率、脉宽或关光确认验证，不发布新 Release。

## 2026-10-29：下拉弹层统一到 UI 库（用户定）

- 脉宽、频率由 Qt ComboBox 换为 UICore ChoiceBox，仍直接读取 currentIndex/currentValue；串口 ChoiceField 通过同一基类获得上下展开、长列表滚动和空表不弹层。查询、发令、初始空值、稀疏编号与回报状态保持，无新增表单状态。
- UICore 本地 5d4ccc1：Windows Release、qmllint 无警告，CTest 3/3，Basic 与 FluentWinUI3 各 9 个交互场景通过（含初始化/收尾各 11 项）。验证初始空表、鼠标/键盘、清空/重填、选择接受/拒绝、上下位置和移动后重定位，已查看截图。详细证据见 UICore README。
- 本仓原 build 增加 FETCHCONTENT_SOURCE_DIR_LIDAR_UICORE 指向相邻 UICore，原有 ChartCore 覆盖保持。Release 增量构建、qmllint 无警告；既有 QML CTest 1/1、97 项通过，含 8 秒离屏，仅既有字体提示，无 QML 运行警告。证据 build/configure-choice.log、build/build-choice.log、build/test-choice.log。
- 主程序原 build/Release 和原脚本更新后 8 秒冒烟通过，stderr 为空；见主仓维护操作。仅本地提交，正式依赖/Release/dist 不变，不推送、未连接实物、未重跑 Linux/协议全套。

## 2026-10-29：按钮等分、状态分层与单曲线图例（用户定）

- 三组双按钮各铺满一行且等宽；状态栏按出光/模式、四项数值、型号/SN 三层排布。脉宽用实际回报编号查设备列表，与待发送选项独立；没有新增状态缓存或查询触发。收发、500 ms 查询、断开清理不变。
- Windows Qt 6.11.2 Release 增量构建与 qmllint 通过，无编译/QML 警告；实际 QML 测试含截图共 99 项通过（正常运行 97 项，截图保存另 2 项）。覆盖三对按钮的等宽/整行几何、回报脉宽与选项分离、SN 一行与宽度稳定，并保留现有回报/菜单及 8 秒离屏检查。
- 查看 400 像素状态栏和完整官邸截图；默认 Windows 控件样式在离屏截图中出现黑色背景，另用 Basic 样式复核按钮文字和布局，同样 99 项通过。仅测试环境切换样式，生产样式不变。ChartCore 本地变更使“系统电压”单曲线图例可见，图例与状态文字已核对；离屏截图不用于判断 GPU 曲线画质。
- 证据 build/build-status-layout.log、build/test-status-layout.log、build/test-status-layout-basic.log、build/status-layout-status.png、build/status-layout-basic-page.png。主程序原入口增量更新及 8 秒离屏通过，stderr 为空，记录见主仓维护操作。无实物连接，未重复协议/Linux 全套；不推送、不更新 Release/dist。

## 2026-10-29：参数表单对齐与连接时查询脉宽（用户定）

- 对照 Cycle/Snake 的 PlanRow，三个参数行复用 UICore FieldRow 的统一标签宽度（120）；“脉宽（ns）”与“频率（kHz）”“功率（%）”对齐，脉宽菜单只显示数值，空列表留白。连接成功的现有分支追加一次六槽查询，手动更新保留，定时/确认补查不含脉宽；无新增查询状态或 C++ 改动。
- Windows Qt 6.11.2 Release 增量构建和 qmllint 无警告，既有 QML CTest 1/1、93 项通过；适配空白和数值文案，保留稀疏原编号、回报与输入分离及 8 秒离屏检查。仅既有 FreeType 提示；未连接硬件，不宣称实机已回传首次列表。证据 build/build-form-width.log、build/test-form-width.log。
- 主程序原 build/Release 增量更新，经原启动脚本 8 秒离屏通过且 stderr 为空，证据见主仓维护操作。仅本地提交，不推送或更新 Release/dist，不重复未改动的协议及 Linux 全套。

## 2026-10-29：精简控制界面与断开清屏（用户定）

- 删除查询分区及轮询开关/间隔，在线固定 500 ms 只查种子状态与测量；连接初查仍取型号/SN，脉宽列表只由专用更新按钮读取。控制按钮不按连接或脉宽置灰，出光时直接取候选，取不到失败返回。删除 selectedWidth/widthIndex、模式/通道/轮询草稿；功率独持于共享 IntField，整数 0..100、初值 1，模式用内控/外控两个按钮。删除光开关 UI 和发送入口，底层回复兼容保持。
- close 在原 Handle 同步关闭前尝试一次种子关光，即使发送失败仍关闭；不等回报、不新增关闭状态。断开清空种子状态和旧脉宽选项，下一次输入与图表历史保留。按用户纠正，关光不清空显示，空确认仍补查，实际状态只随真实回报更新。
- Windows Qt 6.11.2 Release 增量构建及 RayshiningDevice_qmllint 无警告；CTest device/telemetry/commands/qml 4/4 通过，分别 47/41/50/93 项。串口替身验证关闭前单次关光、占用下强制关闭、重复关闭不发帧及发送失败后仍释放连接。QML 检查固定周期、离线按钮、整数输入、稀疏脉宽原编号、关光不清屏及断连处理器清空显示，含 8 秒离屏。QML 测试不打开真实端口，通过连接属性通知驱动清屏处理器；串口关闭由独立替身测试验证。
- 初轮新增按钮计数同时统计到了按钮内文字，测试据此改为只统计具有 clicked 信号的按钮；修正测试选择范围后 4/4 通过，未据测试问题改生产行为。仅既有 FreeType 提示，无新增 QML 运行错误。证据 build/build-ui-simplify.log、build/build-ui-simplify-qml.log、build/test-ui-simplify.log、build/Testing/Temporary/LastTest.log。
- 主程序原 build/Release 已增量更新并通过原启动脚本 8 秒离屏，stderr 为空，记录见主仓维护操作。未连接硬件、未验证实机收到关光；未重复 Linux 和未改动的成帧全套。仅本地提交，不推送、不更新 Release/dist。

## 2026-10-29：频率收紧为三档选择（用户定）

- 删除自由整数输入，改为不可编辑的 5/8/10 kHz 下拉菜单，保持默认 10。删除单独的 frequencyKhz 草稿，出光提交直接读取控件 currentValue；选择本身不发令、不更新状态。设备回报仍原样显示且不改所选值，编解码与查询链不变；删除已无必要的频率范围提示。
- Windows Qt 6.11.2 Release 增量构建和 qmllint 无警告；QML CTest 1/1、71 项检查通过，含三档及默认值、选择不发令/不预写回报、回报与请求独立、原有 SN 布局及 8 秒离屏。证据 build/build-frequency-menu.log、build/test-frequency-menu.log；仅既有 FreeType 提示。
- 主程序原 build/Release 已更新，经原启动脚本 8 秒离屏存活、stderr 为空。未连接实物，未重复未改动的协议/平台全套；只做本地提交，不推送或发 Release。主程序证据见其维护操作。

## 2026-10-29：频率协议核查与 SN 布局修复

- 对照 V1.3.1 协议表 3、表 4、3.2 示例和表 12：发送频率小端 uint32，10 kHz 示例为 0A 00 00 00；0x80 SysFreq 回报为 kHz 单位的 uint32。现有发送、解析及 QML 显示一致，无分档、缩放或错误的浮点解释。RS-1550-300-MINI V2.0 手册规格表写明 5/8/10 kHz（可定制），用户随后确认 8 kHz 正确回报。本轮保留频率控制与报文代码，不将非档位请求的实机响应推测为已抓包确认的事实。
- 使用无换行符的完整 SN 15500300MN260011 复现旧布局：400 像素分区内 SN 标签仅 138 像素（文本隐式宽 145），显示两行；频率变为两位数后宽度变为 144。型号/SN/报警移出两列网格，直接由 Section 纵向布局分配整行宽度，保留完整文本与必要的自然折行。初次诊断脚本的临时入口已移除，改为普通回归检查。
- Windows Qt 6.11.2 Release 增量构建 qml-tests、command-tests、reply-tests 及 RayshiningDevice_qmllint 通过，无编译/QML 警告。CTest 3/3：208 项解析、45 项命令、67 项 QML 检查；1/5/8/9/10/12/20 kHz 经生产组包及生产收包解析均保留原值，界面同样原样显示。相同 SN 在不同频率下始终一行且宽度不变，既有 8 秒离屏保持；只有既有 FreeType 提示。
- 证据 build/probe-status-layout-before.log、build/build-frequency-audit.log、build/test-frequency-audit.log、build/Testing/Temporary/LastTest.log。串口测试使用替身，无硬件命令；未重跑 Linux，主程序原入口更新记录见主仓维护操作。不推送、不发 Release。

## 2026-10-29：设备回报脉宽菜单与功率量程（用户定）

- 脉宽输入改为不可编辑下拉框，初始无选项；按设备回报的可选标记和实际 ns 值生成，保留原编号。专用“更新”按钮查询六个协议槽；连接初查、轮询、型号查询及确认补查均不查询列表。部分列表按编号更新，普通状态/相同值不重建列表；插入、删除和数值变化尽量保持当前编号，无可选项不提交出光。
- 用户已实测种子支持调光，设置 10 时回报 25，写入上限 100、回报上限 255。本轮只在 QML 显示处乘 100/255，保留一位小数；原始回报和发送百分比不改，发送成功不伪造回报。
- Windows Qt 6.11.2 Release 增量构建（qml-tests、command-tests）和 RayshiningDevice_qmllint 无警告，CTest 2/2 通过：31 项命令检查、41 项 QML 检查。覆盖空初值、实际回报 ns、稀疏编号 2/5、部分插入/删除、同值及普通状态不更新菜单、全部六槽、空列表拒绝提交，以及回报 0/25/255 的显示换算。串口替身检查实际报文携带编号 5、915 ns 和原值 10%；无真实串口连接或出光。
- 证据 build/build-pulse-options.log、build/test-pulse-options.log、build/Testing/Temporary/LastTest.log。QML 8 秒离屏只有既有 FreeType 提示，无新增运行错误；主程序原启动脚本 8 秒离屏通过且 stderr 为空，详见主仓维护操作。未重跑未改动的解析/平台全套；仅本地提交，不推送、不发 Release。

## 2026-10-29：仅查询种子并开放功率输入（用户定）

- 用户实测后要求结束放大地址排查，仅使用种子。移除 QML 放大查询、诊断日志和双地址说明；连接初查、定时、两个手动查询按钮均只发种子。非种子回报不参与显示或补查；底层协议接收兼容保留。
- 功率改为 QML 独持 0..100% 草稿，初值 1%，经原“设置参数并出光”按钮手动提交。C++ 组包删除种子强制 100% 的分支，保留上下限校验。状态栏仍只显示设备回报，发送与编辑不改写当前功率；空出光确认后仍查询实际状态。没有新增独立在线调功命令，没有自动连接或出光。
- Windows Qt 6.11.2 Release 全目标（含四个公开头独立编译）、RayshiningDevice_qmllint 无警告；CTest 8/8 通过。串口替身的 29 项命令检查含地址 0、80% 的文档完整帧，以及 0/1/37/100 原值编码和越界零发送；QML 20 项含功率初值、编辑不发送、草稿与回报独立、单次种子查询及 8 秒离屏。证据 build/build-seed-power.log、build/lint-seed-power.log、build/test-seed-power.log。
- 主程序按用户关闭后的原 build/Release 路径更新，通过原启动脚本 8 秒离屏，stderr 为空；不另设入口。未重复 Linux、未连接硬件，不宣称实物调功成功；用户后续对照当前功率回报和实际效果测试。仅本地提交，不推送、不更新 Release/dist。

## 2026-10-29：查询覆盖两个协议地址（用户定）

- 连接初查、定时器和两个手动查询按钮共用双地址查询入口；分别调用地址 0、1，首次失败不短路第二次。界面标明查询范围，手动发送日志分别标注地址。控制与确认补查仍只针对种子，不恢复放大图窗和状态缓存。
- 放大有效非绘图回报与测量回报均写入页面日志；不会触发额外查询或覆盖种子状态。不增加收发账本、超时或假状态；发送成功不代表收到了回复。
- 重新核对 V1.3.1 原协议：脉宽号为 0x00..0x05（六槽），0x96..0x9B 分别回报可选标记及脉宽值，文档示例仅三槽可选；不能据此要求当前硬件开放六档。没有改动脉宽选择器或功率控制。
- Windows Qt 6.11.2 Release 增量构建及 RayshiningDevice_qmllint 无警告；QML CTest 1/1、17 项检查通过，含双查询失败不短路、放大日志与 8 秒离屏。日志 build/build-query-both.log、build/test-query-both.log；仅有既有 FreeType 提示。主程序增量构建及 8 秒离屏记录于主仓维护操作。
- 未连接实物，尚未验证地址 1 是否确实无回包；本轮仅本地联调，不发布 Release，不推送，不重复 Linux/协议全套。下方单模块查询为已被本轮取代的历史记录。

## 2026-10-29：按实物配置仅显示和控制种子（用户定）

- 老师确认当前激光器没有放大模块。删除 QML 放大图窗、状态栏、模块单选及对应回报/功率草稿；功率固定只读 100%。初查、定时读取、手动命令和确认补查只发种子。定时每轮一条读取包含状态与六项测量；电流范围改为 0–5 A，电压范围改为 15–30 V，温度 0–80 °C 保持。
- C++ 协议与按模块交付代码不变，保留放大回复/测量解析。QML 忽略非种子回包，尤其放大出光/关光确认不会触发补查。既有 QML 用例适配单页，并验证放大回报不覆盖种子状态、不产生查询。
- Windows Qt 6.11.2：Release 增量构建、RayshiningDevice_qmllint 零警告；reply、telemetry、qml 三项 CTest 3/3 通过，QML 14 项检查含 8 秒离屏。日志 build/build-seed-only.log、build/test-seed-only.log。主程序生产冒烟记录于主仓维护操作。未重复 Linux/协议全套，未打开实物串口。
- 本轮仅本地提交、源码覆盖联调；不推送、不更新既有 Release。此节取代此前双模块定时监测的现行行为，下节保留当时验证记录。

## 2026-10-29：两模块独立定时查询（用户定）

- QML 定时器每轮分别提交种子、放大模块的六项测量和状态查询，不再读取控制区所选模块；第一条查询失败也不短路第二条。勾选项改为“定时查询种子与放大模块”。手动发令、连接初查、确认补查、解析及图表缓存保持。
- Windows Qt 6.11.2 / MSVC：增量 Release、RayshiningDevice_qmllint 通过，无编译/QML 警告；既有 QML CTest 1/1（12 项、8 秒离屏）通过。日志 build/build-poll-both.log、build/test-poll-both.log。按小改范围不重复 Linux/协议全套，不新增镜像实现的测试。
- 本轮未连接实物或发出光命令；放大模块是否回传测量仍由硬件联调确认，不能从空图推断不支持。仅本地提交，主程序使用 FetchContent 源码覆盖联调，不更新远端固定版本或 Release。

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
