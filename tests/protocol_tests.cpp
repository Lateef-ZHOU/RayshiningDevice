#include <cstdlib>
#include <iostream>
#include <vector>
#include <QCoreApplication>
#include <Rayshining/LaserHandle.h>

namespace {
int checks = 0;
void require(bool ok, const char *message)
{
    ++checks;
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
QByteArray hex(const char *s) { return QByteArray::fromHex(s); }
struct Receiver {
    Rayshining::Framer framer;
    std::vector<QByteArray> frames;
    RxTime last{};
    void feed(const QByteArray &data, RxTime at = {})
    {
        framer.feed(data, at, [this](const QByteArray &frame, RxTime time) {
            frames.push_back(frame); last = time;
        });
    }
};
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    using namespace Rayshining;
    require(crc16(QByteArrayView("123456789", 9)) == 0x4b37, "CRC standard vector");
    require(crc16({}) == 0xffff, "CRC empty accumulator");
    const std::vector<QByteArray> examples{
        hex("57484c5300003700000800000000000000004a7955"),
        hex("57484c53000037000008010000000000000086b855"),
        hex("57484c530000b7000000af2755"),
        hex("57484c5300003501000c0a00000050000000000000c8dd6a55"),
        hex("57484c530000b5010000d77755"),
        hex("57484c53000031000024930000009400000095000000960000009700000098000000990000009a0000009b000000acb955"),
        hex("57484c530000b10000489300000031353530940000002d414349950000002d360000960000008000006497000000800000c8980000008000012c99000000000000009a000000000000009b00000000000000066055")
    };
    for (const auto &frame : examples) {
        require(validFrame(frame), "document frame CRC/length");
        require(encode(readBe16(frame[4], frame[5]), static_cast<unsigned char>(frame[6]),
                       static_cast<unsigned char>(frame[7]), QByteArrayView(frame).sliced(10, frame.size()-13)) == frame,
                "document frame exact encoding");
        for (qsizetype split = 0; split <= frame.size(); ++split) {
            Receiver r;
            r.feed(frame.first(split));
            require(r.frames.size() == (split == frame.size() ? 1u : 0u), "partial frame waits");
            r.feed(frame.sliced(split));
            require(r.frames == std::vector<QByteArray>{frame}, "all split points");
        }
    }
    const auto good = examples[2];
    Receiver joined;
    QByteArray bytes;
    for (const auto &frame : examples) bytes += frame;
    joined.feed(bytes);
    require(joined.frames == examples, "multiple frames in one read");
    Receiver single;
    for (const char byte : bytes) single.feed(QByteArray(1, byte));
    require(single.frames == examples, "one byte reads");
    for (qsizetype i : {qsizetype(0), qsizetype(6), good.size()-3, good.size()-1}) {
        auto bad = good; bad[i] = static_cast<char>(bad[i] ^ 1);
        Receiver r; r.feed(bad + good);
        require(r.frames == std::vector<QByteArray>{good}, "bad header/body/CRC/tail resync");
    }
    auto nested = encode(0, 0xb1, 0, good); nested[nested.size()-1] = 0;
    Receiver inner; inner.feed(nested);
    require(inner.frames == std::vector<QByteArray>{good}, "bad outer frame preserves inner frame");
    const auto special = encode(1, 0xb1, 5, QByteArray("WHLS\x55WHLS\x55", 10));
    Receiver embedded; embedded.feed(special);
    require(embedded.frames == std::vector<QByteArray>{special}, "header and tail in payload");
    Receiver prefix; prefix.feed("junkWH"); prefix.feed(good.sliced(2));
    require(prefix.frames == std::vector<QByteArray>{good}, "partial header across reads");
    const auto maximum = encode(1, 0xb1, 0, QByteArray(static_cast<qsizetype>(MaxDataSize), '\x55'));
    require(maximum.size() == static_cast<qsizetype>(MaxFrameSize), "maximum size");
    Receiver wrap; wrap.feed(QByteArray(static_cast<qsizetype>(MaxFrameSize)-7, 'x'));
    wrap.feed(maximum + good + maximum);
    require(wrap.frames == std::vector<QByteArray>{maximum, good, maximum}, "ring wrap and maximum frames");
    require(encode(0, 0x31, 0, QByteArray(65536, 'x')).isEmpty(), "oversize payload rejected");
    require(!validFrame(good + good), "validFrame rejects trailing frame");
    auto lengthBad = good; lengthBad[9] = 1;
    Receiver length; length.feed(lengthBad + good);
    require(length.frames == std::vector<QByteArray>{good}, "corrupted length resync");
    auto longBad = QByteArray("WHLS", 4) + QByteArray(4, '\0') + hex("ffff");
    longBad += QByteArray(static_cast<qsizetype>(MaxFrameSize)-longBad.size()-good.size(), 'x');
    longBad += good;
    Receiver longLength; longLength.feed(longBad);
    require(longLength.frames == std::vector<QByteArray>{good}, "full capacity bad length preserves later frame");
    Receiver timestamp; const auto at = RxTime{} + std::chrono::seconds(42);
    timestamp.feed(good.first(8)); timestamp.feed(good.sliced(8), at);
    require(timestamp.last == at, "timestamp is completing read time");
    Receiver fresh; fresh.feed(good.sliced(8)); fresh.feed(good);
    require(fresh.frames == std::vector<QByteArray>{good}, "new connection no old partial state");
    const auto params = LaserHandle::parameters({QStringLiteral("not-opened"), {}});
    require(params.baudRate == 115200 && params.dataBits == DataBits::D8
            && params.stopBits == StopBits::One && params.parity == SP_PARITY_NONE
            && params.flowControl == SP_FLOWCONTROL_NONE, "fixed serial configuration");
    { LaserHandle handle([](const QByteArray &, RxTime) {});
      require(!handle.isOpen(), "handle constructs without opening hardware");
      QString diagnostic;
      QObject::connect(&handle, &LaserHandle::logMessage, &app,
                       [&diagnostic](const QString &message) { diagnostic = message; });
      require(!handle.send("broken") && diagnostic.contains(QStringLiteral("CRC")), "TX rejects malformed frame");
      diagnostic.clear();
      require(!handle.send(good) && diagnostic.contains(QStringLiteral("未连接")), "valid TX reaches serial transport"); }
    std::cout << checks << " checks passed\n";
}
