#pragma once
#include <cstddef>
#include <cstdint>
#include <QByteArray>
#include <QByteArrayView>

namespace Rayshining {
inline constexpr std::size_t FrameHeadSize = 4; // WHLS
inline constexpr std::size_t FrameTailSize = 3; // CRC16 + 0x55
inline constexpr std::size_t HeaderSize = 10;
inline constexpr std::size_t MinFrameSize = HeaderSize + FrameTailSize;
inline constexpr std::size_t MaxDataSize = 65535;
inline constexpr std::size_t MaxFrameSize = MaxDataSize + MinFrameSize;

// CRC-16/MODBUS；校验范围为地址至数据末尾，协议线上 CRC 高字节先发。
[[nodiscard]] inline std::uint16_t crcStep(std::uint16_t crc, unsigned char byte) noexcept
{
    crc ^= byte;
    for (int i = 0; i < 8; ++i)
        crc = static_cast<std::uint16_t>((crc >> 1) ^ ((crc & 1) ? 0xa001 : 0));
    return crc;
}

[[nodiscard]] inline std::uint16_t crc16(QByteArrayView bytes) noexcept
{
    std::uint16_t crc = 0xffff;
    for (const char byte : bytes)
        crc = crcStep(crc, static_cast<unsigned char>(byte));
    return crc;
}

[[nodiscard]] inline std::uint16_t readBe16(char high, char low) noexcept
{
    return static_cast<std::uint16_t>((static_cast<unsigned char>(high) << 8)
                                     | static_cast<unsigned char>(low));
}

[[nodiscard]] inline bool validFrame(QByteArrayView bytes) noexcept
{
    if (bytes.size() < static_cast<qsizetype>(MinFrameSize)
        || bytes.size() > static_cast<qsizetype>(MaxFrameSize)
        || bytes.first(4) != QByteArrayView("WHLS", 4))
        return false;
    const qsizetype size = readBe16(bytes[8], bytes[9]) + static_cast<qsizetype>(MinFrameSize);
    return bytes.size() == size && bytes[size - 1] == '\x55'
        && crc16(bytes.sliced(4, size - 7)) == readBe16(bytes[size - 3], bytes[size - 2]);
}

// 数据区原样封装；寄存器、脉宽等混合字节序归型号业务，不由成帧层解释。
[[nodiscard]] inline QByteArray encode(std::uint16_t address, unsigned char command,
                                       unsigned char width, QByteArrayView data)
{
    if (data.size() > static_cast<qsizetype>(MaxDataSize))
        return {};
    QByteArray frame("WHLS", 4);
    frame.reserve(data.size() + static_cast<qsizetype>(MinFrameSize));
    frame.append(static_cast<char>(address >> 8));
    frame.append(static_cast<char>(address));
    frame.append(static_cast<char>(command));
    frame.append(static_cast<char>(width));
    frame.append(static_cast<char>(data.size() >> 8));
    frame.append(static_cast<char>(data.size()));
    frame.append(data.data(), data.size());
    const auto crc = crc16(QByteArrayView(frame).sliced(4));
    frame.append(static_cast<char>(crc >> 8));
    frame.append(static_cast<char>(crc));
    frame.append('\x55');
    return frame;
}
}
