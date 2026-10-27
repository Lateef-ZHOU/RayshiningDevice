#pragma once
#include <array>
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
            // 前一轮必已处理达到最大长度的候选，追加前 size_ < MaxFrameSize。
            bytes_[(head_ + size_) % MaxFrameSize] = byte;
            ++size_;
            while (size_ >= 4) {
                if (get(0) != 'W' || get(1) != 'H' || get(2) != 'L' || get(3) != 'S') {
                    discard(1);
                    continue;
                }
                if (size_ < HeaderSize)
                    break;
                const std::size_t total = readBe16(get(8), get(9)) + MinFrameSize;
                if (size_ < total)
                    break;
                if (get(total - 1) != '\x55' || !validCrc(total)) {
                    discard(1); // 坏帧只破坏一个字节，保留其内部可能存在的完整帧。
                    continue;
                }
                QByteArray frame(static_cast<qsizetype>(total), Qt::Uninitialized);
                for (std::size_t i = 0; i < total; ++i)
                    frame[static_cast<qsizetype>(i)] = get(i);
                discard(total);
                deliver(frame, at); // 同步交付自有字节，不借出环形数组。
            }
        }
    }

private:
    [[nodiscard]] char get(std::size_t i) const noexcept { return bytes_[(head_ + i) % MaxFrameSize]; }
    void discard(std::size_t count) noexcept
    {
        head_ = (head_ + count) % MaxFrameSize;
        size_ -= count;
    }
    [[nodiscard]] bool validCrc(std::size_t total) const noexcept
    {
        std::uint16_t crc = 0xffff;
        for (std::size_t i = 4; i < total - 3; ++i)
            crc = crcStep(crc, static_cast<unsigned char>(get(i)));
        return crc == readBe16(get(total - 3), get(total - 2));
    }
    std::array<char, MaxFrameSize> bytes_{};
    std::size_t head_ = 0;
    std::size_t size_ = 0;
};
}
