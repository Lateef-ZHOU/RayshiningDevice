#pragma once
#include <algorithm>
#include <boost/circular_buffer.hpp>
#include <HandleBase.h>
#include "Protocol.h"

namespace Rayshining {
// 每个 SerialConn 独持；仅收包线程使用。RxTime 为补齐整帧的读取块时刻。
class Framer
{
public:
    void feed(const QByteArray &chunk, const RxTime at, const DataCallback &deliver)
    {
        for (const char byte : chunk) {
            bytes_.push_back(byte);
            while (bytes_.size() >= HeaderSize) {
                if (bytes_[0] != 'W' || bytes_[1] != 'H' || bytes_[2] != 'L' || bytes_[3] != 'S') {
                    bytes_.pop_front();
                    continue;
                }
                const std::size_t total = readBe16(bytes_[8], bytes_[9]) + MinFrameSize;
                if (bytes_.size() < total)
                    break;
                if (bytes_[total - 1] != '\x55' || !validCrc(total)) {
                    bytes_.pop_front(); // 坏帧只破坏一个字节，保留其内部可能存在的完整帧。
                    continue;
                }
                QByteArray frame(static_cast<qsizetype>(total), Qt::Uninitialized);
                std::copy_n(bytes_.begin(), total, frame.data());
                bytes_.erase_begin(total);
                deliver(frame, at); // 同步交付自有字节，不借出环形数组。
            }
        }
    }

private:
    [[nodiscard]] bool validCrc(std::size_t total) const noexcept
    {
        std::uint16_t crc = 0xffff;
        for (std::size_t i = 4; i < total - 3; ++i)
            crc = crcStep(crc, static_cast<unsigned char>(bytes_[i]));
        return crc == readBe16(bytes_[total - 3], bytes_[total - 2]);
    }
    // 预留上限但初始为空；未满追加、满后覆盖最旧元素，由容器维护长度和回绕。
    boost::circular_buffer<char> bytes_{MaxFrameSize};
};
}
