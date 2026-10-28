#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <optional>
#include <utility>
#include <variant>
#include <vector>
#include <QString>
#include <QtEndian>
#include "Protocol.h"

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

namespace detail {
enum class Format { Bits16, Unsigned32, Float32, Text, Emission, PulseWidth };
[[nodiscard]] inline std::optional<Format> format(Register address) noexcept
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
}

// 输入是 Handle 已验帧后交出的主体，不再次解析 WHLS/CRC。
// 整包解释成功才返回 Reply；失败返回具体原因，不交出半份寄存器结果。
[[nodiscard]] inline std::variant<Reply, QString> decodeReply(QByteArrayView body)
{
    constexpr qsizetype metadataSize = HeaderSize - FrameHeadSize;
    if (body.size() < metadataSize || body.size() != metadataSize + readBe16(body[4], body[5]))
        return QStringLiteral("激光器回包主体长度不符");
    const auto module = static_cast<Module>(readBe16(body[0], body[1]));
    if (module != Module::Seed && module != Module::Amplifier)
        return QStringLiteral("激光器回包模块地址未定义");
    const auto command = static_cast<Command>(static_cast<quint8>(body[2]));
    const auto width = static_cast<quint8>(body[3]);
    if (width > 5)
        return QStringLiteral("激光器回包脉宽号超出 0..5");
    Reply reply{module, command, width, {}};
    switch (command) {
    case Command::ModeReply: case Command::ChannelReply:
        if (module != Module::Seed || width != 0)
            return QStringLiteral("激光器模式/光开关确认的地址与脉宽号必须为 0");
        [[fallthrough]];
    case Command::EmitReply: case Command::StopReply:
        if (body.size() != metadataSize)
            return QStringLiteral("激光器命令确认不应携带数据");
        return reply;
    case Command::ReadReply: break;
    default: return QStringLiteral("激光器回包命令未定义或属于请求命令");
    }
    if (body.size() == metadataSize || (body.size() - metadataSize) % 8 != 0)
        return QStringLiteral("激光器读取回包必须包含完整的 8 字节寄存器记录");
    reply.readings.reserve(static_cast<std::size_t>((body.size() - metadataSize) / 8));
    for (qsizetype offset = metadataSize; offset < body.size(); offset += 8) {
        const auto rawAddress = qFromLittleEndian<quint32>(body.data() + offset);
        const auto address = static_cast<Register>(rawAddress);
        const auto format = detail::format(address);
        if (!format)
            return QStringLiteral("激光器回包含未定义寄存器 0x%1").arg(rawAddress, 8, 16, QLatin1Char('0'));
        const auto *data = body.data() + offset + 4;
        RegisterValue value;
        switch (*format) {
        case detail::Format::Bits16:
            value = qFromLittleEndian<quint16>(data); // 只解释文档定义的低 16 位
            break;
        case detail::Format::Unsigned32: {
            const auto number = qFromLittleEndian<quint32>(data);
            if (address == Register::WidthIndex && number > 5)
                return QStringLiteral("激光器脉宽号寄存器超出 0..5");
            value = number;
            break;
        }
        case detail::Format::Float32: {
            const auto number = std::bit_cast<float>(qFromLittleEndian<quint32>(data));
            if (!std::isfinite(number))
                return QStringLiteral("激光器回包含非有限浮点值");
            value = number;
            break;
        }
        case detail::Format::Text:
            value = TextWord{data[0], data[1], data[2], data[3]}; // 不丢零字节，不拼接缓存
            break;
        case detail::Format::Emission:
            if (static_cast<quint8>(data[0]) > 1)
                return QStringLiteral("激光器回包出光状态未定义");
            value = static_cast<EmissionState>(static_cast<quint8>(data[0]));
            break;
        case detail::Format::PulseWidth: {
            const auto flag = static_cast<quint8>(data[0]);
            if (flag != 0 && flag != 0x80)
                return QStringLiteral("激光器回包脉宽可选标记未定义");
            // 文档查询示例的字段例外：首字节标记 + 后三字节大端脉宽。
            value = PulseWidthOption{flag == 0x80, qFromBigEndian<quint32>(data) & 0x00ffffffu};
            break;
        }
        }
        reply.readings.push_back({address, std::move(value)});
    }
    return reply;
}
}
