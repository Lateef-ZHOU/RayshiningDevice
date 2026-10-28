#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <concepts>
#include <optional>
#include <limits>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include <QList>
#include <QMetaObject>
#include <QMetaType>
#include <QStringList>
#include <QVariantMap>
#include <QtEndian>
#include <QtQml/qqmlregistration.h>
#include <QtQml/qqml.h>
#include <Device/Base/DeviceBase.h>
#include "LaserHandle.h"

namespace Rayshining {
Q_NAMESPACE
QML_NAMED_ELEMENT(LaserProtocol)
enum class Module : quint16 { Seed = 0x0000, Amplifier = 0x0001 };
Q_ENUM_NS(Module)
enum class Command : quint8 {
    Read = 0x31, Emit = 0x35, Stop = 0x36, Mode = 0x37, Channel = 0x38,
    ReadReply = 0xb1, EmitReply = 0xb5, StopReply = 0xb6, ModeReply = 0xb7, ChannelReply = 0xb8
};
Q_ENUM_NS(Command)
enum class Register : quint32 {
    ControlMode = 0x07, Serial1 = 0x10, Serial2 = 0x11, Serial3 = 0x12,
    Emission = 0x18, Frequency = 0x80, Power = 0x81, WidthIndex = 0x82,
    Voltage = 0x83, Alarms = 0x84, PA1Current = 0x86, PA2Current = 0x87,
    PA3Current = 0x88, LaserTemperature = 0x89, McuTemperature = 0x90,
    Model1 = 0x93, Model2 = 0x94, Model3 = 0x95,
    Width0 = 0x96, Width1 = 0x97, Width2 = 0x98, Width3 = 0x99,
    Width4 = 0x9a, Width5 = 0x9b, Serial4 = 0xa0
};
Q_ENUM_NS(Register)
enum class EmissionState : quint8 { Off = 0, On = 1 };
struct PulseWidthOption { bool selectable; quint32 value; };
using TextWord = std::array<char, 4>;
// 16 位模式/故障寄存器保留完整位图；不把未定义位解释成业务状态。
using RegisterValue = std::variant<quint16, quint32, float, TextWord, EmissionState, PulseWidthOption>;
struct Reading { Register address; RegisterValue value; };
struct Reply {
    Q_GADGET
    QML_VALUE_TYPE(laserReply)
    Q_PROPERTY(int module READ qmlModule CONSTANT)
    Q_PROPERTY(int command READ qmlCommand CONSTANT)
    Q_PROPERTY(int widthIndex READ qmlWidth CONSTANT)
    Q_PROPERTY(QVariantMap values READ values CONSTANT)
public:
    Module module;
    Command command;
    quint8 widthIndex;
    std::vector<Reading> readings;
    [[nodiscard]] int qmlModule() const { return static_cast<int>(module); }
    [[nodiscard]] int qmlCommand() const { return static_cast<int>(command); }
    [[nodiscard]] int qmlWidth() const { return widthIndex; }
    [[nodiscard]] QVariantMap values() const
    {
        QVariantMap result;
        for (const auto &reading : readings) {
            result.insert(QString::number(static_cast<quint32>(reading.address)), std::visit([](const auto &v) -> QVariant {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::same_as<T, TextWord>) return QString::fromLatin1(v.data(), static_cast<qsizetype>(v.size()));
                else if constexpr (std::same_as<T, PulseWidthOption>) return QVariantMap{{"selectable", v.selectable}, {"value", v.value}};
                else if constexpr (std::same_as<T, EmissionState>) return static_cast<int>(v);
                else return QVariant::fromValue(v);
            }, reading.value));
        }
        return result;
    }
};
}

Q_DECLARE_METATYPE(Rayshining::Reply)
Q_DECLARE_METATYPE(Rayshining::Module)

namespace Rayshining {
// 激光器尚无任务族或 Collector；直接复用设备身份、状态与锁，Handle 独持收包回调。
class LaserDevice final : public DeviceBase
{
    Q_OBJECT
    Q_PROPERTY(QString connectionDescription READ connectionDescription CONSTANT)
    QML_NAMED_ELEMENT(LaserDevice)
    QML_ATTACHED(LaserDevice)
    QML_UNCREATABLE("由激光器 QML 官邸拥有")
public:
    static LaserDevice *qmlAttachedProperties(QObject *owner) { return new LaserDevice(owner); }
    explicit LaserDevice(QObject *parent) : DeviceBase(parent), handle_(makeOnStream(this))
    {
        static_assert(FinalDevice<LaserDevice, decltype(&LaserDevice::PreDestruction)>);
        QObject::connect(&handle_, &LaserHandle::connectionChanged, this, [this](bool connected) {
            if (connected)
                telemetry_ = {}; // 本轮连接的通知先于本轮回报入队；在主线程清空上一轮测量。
            else
                dropLock();
            Q_EMIT statusChanged();
        });
    }
    ~LaserDevice() override { PreDestruction(); }
    [[nodiscard]] QString displayName() const final { return QStringLiteral("镭晟激光器"); }
    [[nodiscard]] QString subtitle() const final { return QStringLiteral("串口 · RS-1550-300-MINI"); }
    [[nodiscard]] bool isOpen() const noexcept final { return handle_.isOpen(); }
    [[nodiscard]] QObject *logSourceObject() noexcept final { return &handle_; }

    [[nodiscard]] QString connectionDescription() const
    {
        const auto params = LaserHandle::parameters({});
        return QStringLiteral("%1 baud · %2 数据位 · %3 停止位\n校验：%4 · 流控：%5")
            .arg(params.baudRate).arg(toString(params.dataBits)).arg(toString(params.stopBits))
            .arg(params.parity == SP_PARITY_NONE ? QStringLiteral("无") : toString(params.parity))
            .arg(params.flowControl == SP_FLOWCONTROL_NONE ? QStringLiteral("无") : QString::number(params.flowControl));
    }

    // 表单只交端口；协议规格固定，不缓存串口设置或 UI 选择。
    [[nodiscard]] bool open(const SerialPortInfo &port) { return handle_.start(LaserHandle::parameters(port)); }
    Q_INVOKABLE bool open(const QString &portName)
    {
        if (portName.trimmed().isEmpty()) {
            Q_EMIT handle_.logMessage(QStringLiteral("请选择串口，连接未提交"));
            return false;
        }
        return open(SerialPortInfo{portName, {}});
    }
    Q_INVOKABLE QStringList ports() const
    {
        QStringList names;
        for (const auto &port : availablePorts()) names.append(port.portName);
        return names;
    }
    Q_INVOKABLE void close() { handle_.stop(); }

    Q_INVOKABLE bool readRegisters(Module module, int width, const QList<Register> &addresses)
    {
        if ((module != Module::Seed && module != Module::Amplifier) || width < 0 || width > 5
            || addresses.isEmpty() || addresses.size() > static_cast<qsizetype>(MaxDataSize / 4)) {
            Q_EMIT handle_.logMessage(QStringLiteral("激光器查询参数无效，未发送"));
            return false;
        }
        QByteArray data(addresses.size() * 4, Qt::Uninitialized);
        for (qsizetype i = 0; i < addresses.size(); ++i) {
            if (!format(addresses[i])) {
                Q_EMIT handle_.logMessage(QStringLiteral("激光器查询包含未定义寄存器，未发送"));
                return false;
            }
            qToLittleEndian<quint32>(static_cast<quint32>(addresses[i]), data.data() + i * 4);
        }
        if (!canEngage({})) {
            Q_EMIT handle_.logMessage(QStringLiteral("设备未连接或正被占用，激光器查询未发送"));
            return false;
        }
        return handle_.send(encode(static_cast<quint16>(module), static_cast<quint8>(Command::Read), static_cast<quint8>(width), data));
    }

    Q_INVOKABLE bool startEmission(Module module, int width, int frequencyKhz, int powerPercent, int widthNs)
    {
        if (frequencyKhz <= 0 || powerPercent < 0 || powerPercent > 100 || widthNs <= 0 || widthNs > 0x00ffffff) {
            Q_EMIT handle_.logMessage(QStringLiteral("出光参数无效，未发送"));
            return false;
        }
        QByteArray data(12, '\0');
        qToLittleEndian<quint32>(static_cast<quint32>(frequencyKhz), data.data());
        qToLittleEndian<quint32>(module == Module::Seed ? 100u : static_cast<quint32>(powerPercent), data.data() + 4);
        qToBigEndian<quint32>(static_cast<quint32>(widthNs), data.data() + 8); // 协议开光示例中的脉宽字段为大端。
        return sendCommand(module, Command::Emit, width, data);
    }
    Q_INVOKABLE bool stopEmission(Module module, int width) { return sendCommand(module, Command::Stop, width, {}); }
    Q_INVOKABLE bool setControlMode(bool internal)
    {
        QByteArray data(8, '\0'); data[0] = internal ? '\1' : '\0';
        return sendCommand(Module::Seed, Command::Mode, 0, data);
    }
    Q_INVOKABLE bool setOpticalChannel(int channel)
    {
        if (channel < 0 || channel > 3) {
            Q_EMIT handle_.logMessage(QStringLiteral("光开关通道须为 0..3，未发送"));
            return false;
        }
        QByteArray data(8, '\0'); data[0] = static_cast<char>(channel);
        return sendCommand(Module::Seed, Command::Channel, 0, data);
    }

Q_SIGNALS:
    // 非绘图寄存器和命令确认：不缓存状态，成功解析后直接交显示业务。
    void replyReady(RxTime at, const Rayshining::Reply &reply);
    // 同包最多一次完整快照。六值均使用本包时刻；尚未收到过的量为 NaN。
    void telemetryReady(RxTime at, Rayshining::Module module, const QVariantMap &values);

private:
    [[nodiscard]] bool sendCommand(Module module, Command command, int width, const QByteArray &data)
    {
        if ((module != Module::Seed && module != Module::Amplifier) || width < 0 || width > 5) {
            Q_EMIT handle_.logMessage(QStringLiteral("激光器目标模块或脉宽号无效，未发送"));
            return false;
        }
        if (!canEngage({})) {
            Q_EMIT handle_.logMessage(QStringLiteral("激光器未连接或正被占用，命令未发送"));
            return false;
        }
        return handle_.send(encode(static_cast<quint16>(module), static_cast<quint8>(command), static_cast<quint8>(width), data));
    }
    struct Telemetry {
        static constexpr double unknown = std::numeric_limits<double>::quiet_NaN();
        double laserTemperature = unknown, mcuTemperature = unknown;
        double pa1Current = unknown, pa2Current = unknown, pa3Current = unknown;
        double voltage = unknown;
    };
    enum class Format { Bits16, Unsigned32, Float32, Text, Emission, PulseWidth };
    [[nodiscard]] static std::optional<Format> format(Register address) noexcept
    {
        switch (address) {
        case Register::ControlMode: case Register::Alarms: return Format::Bits16;
        case Register::Frequency: case Register::Power: case Register::WidthIndex: return Format::Unsigned32;
        case Register::Voltage: case Register::PA1Current: case Register::PA2Current:
        case Register::PA3Current: case Register::LaserTemperature: case Register::McuTemperature:
            return Format::Float32;
        case Register::Serial1: case Register::Serial2: case Register::Serial3: case Register::Serial4:
        case Register::Model1: case Register::Model2: case Register::Model3: return Format::Text;
        case Register::Emission: return Format::Emission;
        case Register::Width0: case Register::Width1: case Register::Width2:
        case Register::Width3: case Register::Width4: case Register::Width5: return Format::PulseWidth;
        }
        return std::nullopt;
    }
    void PreDestruction() noexcept final
    {
        handle_.stop(); // 等待回调退出后，才撤借用和清理本对象。
        Q_EMIT retiring();
        DeviceBase::PreDestruction();
    }
    [[nodiscard]] static DataCallback makeOnStream(LaserDevice *self)
    {
        return [self](const QByteArray &body, RxTime at) {
            // Handle 已校验外层帧；本回调解释主体，失败就地记录，不交出半份结果。
            constexpr qsizetype metadataSize = HeaderSize - FrameHeadSize;
            if (body.size() < metadataSize || body.size() != metadataSize + readBe16(body[4], body[5])) {
                Q_EMIT self->handle_.logMessage(QStringLiteral("激光器回包主体长度不符"));
                return;
            }
            const auto module = static_cast<Module>(readBe16(body[0], body[1]));
            if (module != Module::Seed && module != Module::Amplifier) {
                Q_EMIT self->handle_.logMessage(QStringLiteral("激光器回包模块地址未定义"));
                return;
            }
            const auto command = static_cast<Command>(static_cast<quint8>(body[2]));
            const auto width = static_cast<quint8>(body[3]);
            if (width > 5) {
                Q_EMIT self->handle_.logMessage(QStringLiteral("激光器回包脉宽号超出 0..5"));
                return;
            }
            Reply reply{module, command, width, {}};
            switch (command) {
            case Command::ModeReply: case Command::ChannelReply:
                if (module != Module::Seed || width != 0) {
                    Q_EMIT self->handle_.logMessage(QStringLiteral("激光器模式/光开关确认的地址与脉宽号必须为 0"));
                    return;
                }
                [[fallthrough]];
            case Command::EmitReply: case Command::StopReply:
                if (body.size() != metadataSize) {
                    Q_EMIT self->handle_.logMessage(QStringLiteral("激光器命令确认不应携带数据"));
                    return;
                }
                break;
            case Command::ReadReply: {
                if (body.size() == metadataSize || (body.size() - metadataSize) % 8 != 0) {
                    Q_EMIT self->handle_.logMessage(QStringLiteral("激光器读取回包必须包含完整的 8 字节寄存器记录"));
                    return;
                }
                reply.readings.reserve(static_cast<std::size_t>((body.size() - metadataSize) / 8));
                for (qsizetype offset = metadataSize; offset < body.size(); offset += 8) {
                    const auto rawAddress = qFromLittleEndian<quint32>(body.constData() + offset);
                    const auto address = static_cast<Register>(rawAddress);
                    const auto valueFormat = format(address);
                    if (!valueFormat) {
                        Q_EMIT self->handle_.logMessage(QStringLiteral("激光器回包含未定义寄存器 0x%1")
                            .arg(rawAddress, 8, 16, QLatin1Char('0')));
                        return;
                    }
                    const auto *data = body.constData() + offset + 4;
                    RegisterValue value;
                    switch (*valueFormat) {
                    case Format::Bits16:
                        value = qFromLittleEndian<quint16>(data); // 只解释文档定义的低 16 位
                        break;
                    case Format::Unsigned32: {
                        const auto number = qFromLittleEndian<quint32>(data);
                        if (address == Register::WidthIndex && number > 5) {
                            Q_EMIT self->handle_.logMessage(QStringLiteral("激光器脉宽号寄存器超出 0..5"));
                            return;
                        }
                        value = number;
                        break;
                    }
                    case Format::Float32: {
                        const auto number = std::bit_cast<float>(qFromLittleEndian<quint32>(data));
                        if (!std::isfinite(number)) {
                            Q_EMIT self->handle_.logMessage(QStringLiteral("激光器回包含非有限浮点值"));
                            return;
                        }
                        value = number;
                        break;
                    }
                    case Format::Text:
                        value = TextWord{data[0], data[1], data[2], data[3]}; // 不丢零字节，不拼接缓存
                        break;
                    case Format::Emission:
                        if (static_cast<quint8>(data[0]) > 1) {
                            Q_EMIT self->handle_.logMessage(QStringLiteral("激光器回包出光状态未定义"));
                            return;
                        }
                        value = static_cast<EmissionState>(static_cast<quint8>(data[0]));
                        break;
                    case Format::PulseWidth: {
                        const auto flag = static_cast<quint8>(data[0]);
                        if (flag != 0 && flag != 0x80) {
                            Q_EMIT self->handle_.logMessage(QStringLiteral("激光器回包脉宽可选标记未定义"));
                            return;
                        }
                        // 文档查询示例的字段例外：首字节标记 + 后三字节大端脉宽。
                        value = PulseWidthOption{flag == 0x80, qFromBigEndian<quint32>(data) & 0x00ffffffu};
                        break;
                    }
                    }
                    reply.readings.push_back({address, std::move(value)});
                }
                break;
            }
            default:
                Q_EMIT self->handle_.logMessage(QStringLiteral("激光器回包命令未定义或属于请求命令"));
                return;
            }
            // 先验证整包，再更新缓存，坏包不会留下前半段测量。缓存与 UI 交付均在主线程。
            QMetaObject::invokeMethod(self, [self, at, reply = std::move(reply)]() mutable {
                auto &values = self->telemetry_[static_cast<std::size_t>(reply.module)];
                bool plotUpdated = false; // 本包的局部标志，不跨包等待或节流。
                for (const auto &reading : reply.readings) {
                    double *destination = nullptr;
                    switch (reading.address) {
                    case Register::LaserTemperature: destination = &values.laserTemperature; break;
                    case Register::McuTemperature: destination = &values.mcuTemperature; break;
                    case Register::PA1Current: destination = &values.pa1Current; break;
                    case Register::PA2Current: destination = &values.pa2Current; break;
                    case Register::PA3Current: destination = &values.pa3Current; break;
                    case Register::Voltage: destination = &values.voltage; break;
                    default: break;
                    }
                    if (destination) {
                        *destination = std::get<float>(reading.value);
                        plotUpdated = true;
                    }
                }
                // 当前协议的 Float32 寄存器恰为六项绘图量；其余状态不经过图表通知。
                std::erase_if(reply.readings, [](const Reading &reading) {
                    return std::holds_alternative<float>(reading.value);
                });
                if (!reply.readings.empty() || reply.command != Command::ReadReply)
                    Q_EMIT self->replyReady(at, reply);
                if (plotUpdated)
                    Q_EMIT self->telemetryReady(at, reply.module,
                        QVariantMap{{"laserTemperature", values.laserTemperature}, {"mcuTemperature", values.mcuTemperature},
                            {"pa1Current", values.pa1Current}, {"pa2Current", values.pa2Current},
                            {"pa3Current", values.pa3Current}, {"voltage", values.voltage}});
            }, Qt::QueuedConnection);
        };
    }
    std::array<Telemetry, 2> telemetry_; // 种子/放大各一份六值缓存；仅主线程读写，不保存图表历史。
    LaserHandle handle_;
};
}
QML_DECLARE_TYPEINFO(Rayshining::LaserDevice, QML_HAS_ATTACHED_PROPERTIES)
