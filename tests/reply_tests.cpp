#include <cstdlib>
#include <chrono>
#include <iostream>
#include <optional>
#include <vector>
#include <QCoreApplication>
#include <QStringList>
#include <QThread>
#include <Rayshining/LaserDevice.h>
#include "FakeSerialPort.h"

namespace {
int checks = 0;
void require(bool ok, const char *message)
{
    ++checks;
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
QByteArray hex(const char *s) { return QByteArray::fromHex(s); }
QByteArray body(quint16 module, quint8 command, quint8 width, const QByteArray &data = {})
{
    const auto wire = Rayshining::encode(module, command, width, data);
    return wire.sliced(4, wire.size() - 7);
}
template <typename Predicate> bool until(Predicate ready)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!ready() && std::chrono::steady_clock::now() < end) {
        QCoreApplication::processEvents(); QThread::msleep(1);
    }
    return ready();
}

// 所有语义用例经完整线上帧进入生产收包回调，不公开解析入口供测试反调。
struct Receiver {
    QObject context;
    std::vector<Rayshining::Reply> replies;
    QStringList logs;
    bool markerReceived = false;
    Rayshining::LaserDevice device; // 首先销毁设备、等收包退出，再释放测试接收状态。

    Receiver()
    {
        using namespace Rayshining;
        QObject::connect(&device, &LaserDevice::replyReady, &context, [this](RxTime, const Reply &reply) {
            if (reply.module == Module::Amplifier && reply.command == Command::StopReply && reply.widthIndex == 4)
                markerReceived = true;
            else
                replies.push_back(reply);
        });
        QObject::connect(qobject_cast<LaserHandle *>(device.logSourceObject()), &LaserHandle::logMessage,
                         &context, [this](const QString &text) { logs.push_back(text); });
        require(device.open({"controlled", {}}) && until([this] {
            return device.isOpen() && logs.contains(QStringLiteral("设备连接成功"));
        }), "open receiver and finish connection notifications");
    }
    std::optional<Rayshining::Reply> receive(const QByteArray &payload)
    {
        replies.clear(); logs.clear(); markerReceived = false;
        auto frame = QByteArray("WHLS", 4) + payload;
        const auto crc = Rayshining::crc16(payload);
        frame.append(static_cast<char>(crc >> 8)); frame.append(static_cast<char>(crc)); frame.append('\x55');
        require(Rayshining::validFrame(frame), "semantic test has valid outer framing");
        FakeSerialPort::enqueue(frame + Rayshining::encode(1, 0xb6, 4, {}));
        // 后续有效帧是交付屏障，同时验证坏业务帧未阻塞后续收包。
        require(until([this] { return markerReceived; }), "subsequent valid reply delivered");
        require((replies.size() == 1 && logs.empty()) || (replies.empty() && logs.size() == 1),
                "exactly one reply or one direct Handle diagnostic, no partial delivery");
        return replies.empty() ? std::nullopt : std::optional(std::move(replies.front()));
    }
};
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    FakeSerialPort::reset();
    Receiver receiver;
    const auto invalid = [&receiver](const QByteArray &data) { return !receiver.receive(data); };
    using namespace Rayshining;
    const auto sample = hex("0000b10000489300000031353530940000002d414349950000002d360000960000008000006497000000800000c8980000008000012c99000000000000009a000000000000009b00000000000000");
    const auto decoded = receiver.receive(sample);
    require(decoded.has_value(), "document reply decoded");
    const auto &reply = *decoded;
    require(reply.module == Module::Seed && reply.command == Command::ReadReply && reply.widthIndex == 0,
            "typed metadata");
    require(reply.readings.size() == 9, "all nine document registers preserved");
    require(std::get<TextWord>(reply.readings[0].value) == TextWord{'1','5','5','0'}, "text byte order");
    require(std::get<TextWord>(reply.readings[2].value) == TextWord{'-','6',0,0}, "text nulls preserved");
    for (std::size_t i = 0; i < 6; ++i) {
        const auto option = std::get<PulseWidthOption>(reply.readings[i + 3].value);
        require(option.selectable == (i < 3), "pulse availability");
        require(option.value == (i < 3 ? (i + 1) * 100 : 0), "pulse big endian 24-bit field");
    }
    const auto values = hex("070000000004000084000000008100001800000001000000800000007856341281000000500000008200000005000000830000000000c041860000000000803f87000000000000408800000000004040890000000000c8c19000000000004842");
    const auto measurements = receiver.receive(body(1, 0xb1, 5, values)).value();
    require(measurements.module == Module::Amplifier && measurements.widthIndex == 5, "amplifier width");
    require(std::get<quint16>(measurements.readings[0].value) == 0x400, "mode raw bits retained");
    require(std::get<quint16>(measurements.readings[1].value) == 0x8100, "alarm bits retained");
    require(std::get<EmissionState>(measurements.readings[2].value) == EmissionState::On, "emission enum");
    require(std::get<quint32>(measurements.readings[3].value) == 0x12345678, "little endian uint32");
    require(std::get<quint32>(measurements.readings[4].value) == 80, "power percentage");
    require(std::get<quint32>(measurements.readings[5].value) == 5, "width index");
    const std::array expected{24.f, 1.f, 2.f, 3.f, -25.f, 50.f};
    for (std::size_t i = 0; i < expected.size(); ++i)
        require(std::get<float>(measurements.readings[i + 6].value) == expected[i], "IEEE float little endian");
    for (const auto reg : {Register::Serial1, Register::Serial2, Register::Serial3, Register::Serial4}) {
        QByteArray data(8, '\0');
        qToLittleEndian<quint32>(static_cast<quint32>(reg), data.data());
        data.replace(4, 4, hex("005748ff"));
        const auto read = receiver.receive(body(0, 0xb1, 0, data)).value().readings[0];
        require(read.address == reg && std::get<TextWord>(read.value) == TextWord{0,'W','H','\xff'},
                "serial fragments are owned raw bytes");
    }
    for (quint8 command : {0xb5, 0xb6, 0xb7, 0xb8}) {
        const auto ack = receiver.receive(body(0, command, 0)).value();
        require(ack.command == static_cast<Command>(command) && ack.readings.empty(), "empty ack carries no invented state");
        require(invalid(body(0, command, 0, hex("00"))), "ack payload rejected");
    }
    require(!invalid(body(1, 0xb5, 5)) && !invalid(body(1, 0xb6, 5)), "module-specific acknowledgments");
    require(invalid(body(1, 0xb7, 0)) && invalid(body(0, 0xb8, 1)), "fixed mode/channel metadata");
    require(invalid(body(2, 0xb1, 0, values)), "unknown module rejected");
    require(invalid(body(0, 0xb1, 6, values)), "unknown width rejected");
    for (quint8 command : {0x31,0x35,0x36,0x37,0x38,0xb2,0xff})
        require(invalid(body(0, command, 0)), "requests and unknown commands rejected");
    // 不完整外层帧不进入官邸；拆包、长度与 CRC 拒绝由 protocol-tests 覆盖。
    require(invalid(body(0, 0xb1, 0)), "empty read rejected");
    require(invalid(body(0, 0xb1, 0, values + '\0')), "partial record rejected");
    require(invalid(body(0, 0xb1, 0, values + hex("ffffffff00000000"))), "unknown register rejects whole reply");
    require(invalid(body(0, 0xb1, 0, hex("1800000002000000"))), "unknown emission state rejected");
    require(invalid(body(0, 0xb1, 0, hex("8200000006000000"))), "invalid register width rejected");
    require(invalid(body(0, 0xb1, 0, hex("9600000040000064"))), "unknown availability flag rejected");
    for (const auto word : {"0000c07f", "0000807f", "000080ff"})
        require(invalid(body(0, 0xb1, 0, hex("83000000") + hex(word))), "NaN and infinity rejected");
    const auto duplicate = receiver.receive(body(0, 0xb1, 0, hex("80000000010000008000000002000000"))).value();
    require(duplicate.readings.size() == 2 && std::get<quint32>(duplicate.readings[1].value) == 2,
            "duplicate addresses retained in wire order without merging state");
    const auto off = receiver.receive(body(0, 0xb1, 0, hex("1800000000000000"))).value();
    require(std::get<EmissionState>(off.readings[0].value) == EmissionState::Off, "off enum");
    std::cout << checks << " checks passed\n";
}
