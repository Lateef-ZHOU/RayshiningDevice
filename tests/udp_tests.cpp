#include <Rayshining/LaserUdpDevice.h>
#include "FakePcap.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <cstdio>
#include <cstdlib>
using namespace Rayshining;
namespace {
int checks = 0;
void require(bool ok, const char *message) {
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
template<class F> void until(F ready) {
    QElapsedTimer time; time.start();
    while (!ready() && time.elapsed() < 3000) { QCoreApplication::processEvents(); QThread::msleep(1); }
    require(ready(), "asynchronous operation completed");
}
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    FakePcap::reset();
    LaserUdpDevice device(nullptr);
    auto *handle = qobject_cast<PcapHandle *>(device.logSourceObject());
    require(handle != nullptr, "UDP backend directly owns PcapHandle");
    const auto options = device.connectionParameters();
    require(options["hostIPv4"] == "192.168.100.200" && options["deviceIPv4"] == "192.168.100.100"
        && options["hostPort"].toInt() == 55100 && options["devicePort"].toInt() == 55100, "fixed reversed bridge endpoints");
    require(!device.open(-1) && FakePcap::opens() == 0, "bad selection does not open");
    require(!options["networkInterfaceOptions"].toStringList().isEmpty(), "test machine provides an Ethernet interface for enumeration");
    bool mainThread = true;
    int replies = 0, plots = 0, errors = 0;
    QVariantMap measured;
    QObject::connect(handle, &PcapHandle::logMessage, &app, [&](const QString &m) { if (m.contains("主体长度")) ++errors; });
    QObject::connect(&device, &LaserUdpDevice::replyReady, &app, [&](RxTime, const Reply &r) {
        mainThread &= QThread::currentThread() == app.thread();
        require(r.command == Command::ReadReply && r.module == Module::Seed, "same typed reply from UDP body");
        require(std::get<quint32>(r.readings.front().value) == 8, "returned kHz unchanged");
        ++replies;
    }, Qt::DirectConnection);
    QObject::connect(&device, &LaserUdpDevice::telemetryReady, &app, [&](RxTime, Module, const QVariantMap &v) {
        measured = v; ++plots; mainThread &= QThread::currentThread() == app.thread();
    }, Qt::DirectConnection);
    require(device.open(0), "UDP start accepted");
    until([&] { return device.isOpen(); });
    QCoreApplication::processEvents();
    auto session = FakePcap::last();
    require(device.readRegisters(Module::Seed, 0,
        {Register::Frequency, Register::Voltage, Register::SDCurrent, Register::PztOffset}), "query accepted");
    const auto request = FakePcap::sent().back().bytes;
    require(kindOf(request) == FrameKind::ARPRequest, "MAC is learned rather than hardcoded");
    bool ok = false;
    const auto peer = MAC::fromString("02:00:00:00:00:11", ok);
    FakePcap::inject(session, ARPReply(ARPRequest(request), peer));
    until([&] { return FakePcap::sent().size() >= 2; });
    const UDPDatagram sent(FakePcap::sent().back().bytes);
    require(sent.bodyView() == makeBody(0, 0x31, 0, QByteArray::fromHex("8000000083000000850000008f000000")),
        "one UDP read contains old and new addresses, body only without WHLS wrapper");
    const auto incoming = [&](const QByteArray &body) {
        const auto arp = ARPRequest(request);
        return UDPDatagram(arp.senderMAC(), peer, arp.targetIP(), arp.senderIP(), 55100, 55100, body);
    };
    FakePcap::inject(session, incoming(makeBody(0, 0xb1, 0,
        QByteArray::fromHex("8000000008000000830000000000c041850000000000003d8f00000000000840"))));
    until([&] { return replies == 1 && plots == 1; });
    require(mainThread && measured["voltage"].toDouble() == 24 && measured.size() == 8
        && measured["sdCurrent"].toDouble() == 0.03125 && measured["pztOffset"].toDouble() == 2.125,
        "borrowed UDP buffer decoded before reuse, full snapshot delivered on main thread");
    FakePcap::inject(session, incoming(QByteArray::fromHex("0000b100000800")));
    until([&] { return errors == 1; });
    require(replies == 1 && plots == 1, "bad body never partially publishes");
    bool closedBeforeSignal = true;
    QObject::connect(handle, &PcapHandle::connectionChanged, &app, [&](bool connected) {
        if (!connected) {
            closedBeforeSignal = session->closed;
            require(device.stopEmission(Module::Seed, 0), "off command can be sent before close");
        }
    });
    device.close();
    require(!closedBeforeSignal && session->closed && !device.isOpen(), "close notification precedes synchronous transport stop");
    require(UDPDatagram(FakePcap::sent().back().bytes).bodyView() == makeBody(0, 0x36, 0, {}), "off command also uses body boundary");
    const auto count = FakePcap::sent().size(); device.close();
    require(FakePcap::sent().size() == count, "repeat close does not send again");
    std::printf("PASS %d checks\n", checks);
}
