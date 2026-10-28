#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <optional>
#include <utility>
#include <variant>
#include <vector>
#include <QList>
#include <QMetaObject>
#include <QMetaType>
#include <QtEndian>
#include <Device/Base/DeviceBase.h>
#include "LaserHandle.h"

namespace Rayshining {
enum class Module : quint16 { Seed = 0x0000, Amplifier = 0x0001 };
enum class Command : quint8 {
    Read = 0x31, Emit = 0x35, Stop = 0x36, Mode = 0x37, Channel = 0x38,
    ReadReply = 0xb1, EmitReply = 0xb5, StopReply = 0xb6, ModeReply = 0xb7, ChannelReply = 0xb8
};
enum class Register : quint32 {
    ControlMode = 0x07, Serial1 = 0x10, Serial2 = 0x11, Serial3 = 0x12,
    Emission = 0x18, Frequency = 0x80, Power = 0x81, WidthIndex = 0x82,
    Voltage = 0x83, Alarms = 0x84, PA1Current = 0x86, PA2Current = 0x87,
    PA3Current = 0x88, LaserTemperature = 0x89, McuTemperature = 0x90,
    Model1 = 0x93, Model2 = 0x94, Model3 = 0x95,
    Width0 = 0x96, Width1 = 0x97, Width2 = 0x98, Width3 = 0x99,
    Width4 = 0x9a, Width5 = 0x9b, Serial4 = 0xa0
};
enum class EmissionState : quint8 { Off = 0, On = 1 };
struct PulseWidthOption { bool selectable; quint32 value; };
using TextWord = std::array<char, 4>;
// 16 位模式/故障寄存器保留完整位图；不把未定义位解释成业务状态。
using RegisterValue = std::variant<quint16, quint32, float, TextWord, EmissionState, PulseWidthOption>;
struct Reading { Register address; RegisterValue value; };
struct Reply {
    Module module;
    Command command;
    quint8 widthIndex;
    std::vector<Reading> readings;
};
}

Q_DECLARE_METATYPE(Rayshining::Reply)

namespace Rayshining {
// 激光器尚无任务族或 Collector；直接复用设备身份、状态与锁，Handle 独持收包回调。
class LaserDevice final : public DeviceBase
{
    Q_OBJECT
public:
    explicit LaserDevice(QObject *parent = nullptr) : DeviceBase(parent), handle_(makeOnStream(this))
    {
        static_assert(FinalDevice<LaserDevice, decltype(&LaserDevice::PreDestruction)>);
        QObject::connect(&handle_, &LaserHandle::connectionChanged, this, [this](bool connected) {
            if (!connected)
                dropLock();
            Q_EMIT statusChanged();
        });
    }
    ~LaserDevice() override { PreDestruction(); }
    [[nodiscard]] QString displayName() const final { return QStringLiteral("镭晟激光器"); }
    [[nodiscard]] QString subtitle() const final { return QStringLiteral("串口 · RS-1550-300-MINI"); }
    [[nodiscard]] bool isOpen() const noexcept final { return handle_.isOpen(); }
    [[nodiscard]] QObject *logSourceObject() noexcept final { return &handle_; }

    // 表单只交端口；协议规格固定，不缓存串口设置或 UI 选择。
    [[nodiscard]] bool open(const SerialPortInfo &port) { return handle_.start(LaserHandle::parameters(port)); }
    void close() { handle_.stop(); }

    [[nodiscard]] bool readRegisters(Module module, quint8 width, const QList<Register> &addresses)
    {
        if ((module != Module::Seed && module != Module::Amplifier) || width > 5
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
        return handle_.send(encode(static_cast<quint16>(module), static_cast<quint8>(Command::Read), width, data));
    }

Q_SIGNALS:
    // 主线程交付一整包；时间原样传递，接收者决定如何显示/保存，不另存 latest 副本。
    void replyReady(RxTime at, const Rayshining::Reply &reply);

private:
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
            QMetaObject::invokeMethod(self, [self, at, reply = std::move(reply)] {
                Q_EMIT self->replyReady(at, reply);
            }, Qt::QueuedConnection);
        };
    }
    LaserHandle handle_;
};
}
