#include <cstdlib>
#include <iostream>
#include <Rayshining/Reply.h>

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
bool invalid(const QByteArray &data) { return std::holds_alternative<QString>(Rayshining::decodeReply(data)); }
}

int main()
{
    using namespace Rayshining;
    const auto sample = hex("0000b10000489300000031353530940000002d414349950000002d360000960000008000006497000000800000c8980000008000012c99000000000000009a000000000000009b00000000000000");
    const auto decoded = decodeReply(sample);
    require(std::holds_alternative<Reply>(decoded), "document reply decoded");
    const auto &reply = std::get<Reply>(decoded);
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
    const auto measurements = std::get<Reply>(decodeReply(body(1, 0xb1, 5, values)));
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
        const auto read = std::get<Reply>(decodeReply(body(0, 0xb1, 0, data))).readings[0];
        require(read.address == reg && std::get<TextWord>(read.value) == TextWord{0,'W','H','\xff'},
                "serial fragments are owned raw bytes");
    }
    for (quint8 command : {0xb5, 0xb6, 0xb7, 0xb8}) {
        const auto ack = std::get<Reply>(decodeReply(body(0, command, 0)));
        require(ack.command == static_cast<Command>(command) && ack.readings.empty(), "empty ack carries no invented state");
        require(invalid(body(0, command, 0, hex("00"))), "ack payload rejected");
    }
    require(!invalid(body(1, 0xb5, 5)) && !invalid(body(1, 0xb6, 5)), "module-specific acknowledgments");
    require(invalid(body(1, 0xb7, 0)) && invalid(body(0, 0xb8, 1)), "fixed mode/channel metadata");
    require(invalid(body(2, 0xb1, 0, values)), "unknown module rejected");
    require(invalid(body(0, 0xb1, 6, values)), "unknown width rejected");
    for (quint8 command : {0x31,0x35,0x36,0x37,0x38,0xb2,0xff})
        require(invalid(body(0, command, 0)), "requests and unknown commands rejected");
    for (qsizetype i = 0; i < sample.size(); ++i)
        require(invalid(sample.first(i)), "truncated body rejected");
    require(invalid(sample + '\0'), "extra body byte rejected");
    require(invalid(body(0, 0xb1, 0)), "empty read rejected");
    require(invalid(body(0, 0xb1, 0, values + '\0')), "partial record rejected");
    require(invalid(body(0, 0xb1, 0, values + hex("ffffffff00000000"))), "unknown register rejects whole reply");
    require(invalid(body(0, 0xb1, 0, hex("1800000002000000"))), "unknown emission state rejected");
    require(invalid(body(0, 0xb1, 0, hex("8200000006000000"))), "invalid register width rejected");
    require(invalid(body(0, 0xb1, 0, hex("9600000040000064"))), "unknown availability flag rejected");
    for (const auto word : {"0000c07f", "0000807f", "000080ff"})
        require(invalid(body(0, 0xb1, 0, hex("83000000") + hex(word))), "NaN and infinity rejected");
    const auto duplicate = std::get<Reply>(decodeReply(body(0, 0xb1, 0, hex("80000000010000008000000002000000"))));
    require(duplicate.readings.size() == 2 && std::get<quint32>(duplicate.readings[1].value) == 2,
            "duplicate addresses retained in wire order without merging state");
    const auto off = std::get<Reply>(decodeReply(body(0, 0xb1, 0, hex("1800000000000000"))));
    require(std::get<EmissionState>(off.readings[0].value) == EmissionState::Off, "off enum");
    std::cout << checks << " checks passed\n";
}
