#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <QCoreApplication>
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
template <typename Predicate> bool until(Predicate ready)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!ready() && std::chrono::steady_clock::now() < end) {
        QCoreApplication::processEvents(); QThread::msleep(1);
    }
    return ready();
}
QByteArray record(Rayshining::Register address, float value)
{
    QByteArray result(8, '\0');
    qToLittleEndian<quint32>(static_cast<quint32>(address), result.data());
    qToLittleEndian<quint32>(std::bit_cast<quint32>(value), result.data() + 4);
    return result;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    using namespace Rayshining;
    FakeSerialPort::reset();
    struct Snapshot { RxTime at; Module module; QVariantMap values; };
    std::vector<Snapshot> plots;
    std::vector<Reply> states;
    int marker = 0, errors = 0;
    bool mainThread = true;
    LaserDevice device(nullptr);
    QObject::connect(&device, &LaserDevice::telemetryReady, &app,
                     [&](RxTime at, Module module, const QVariantMap &values) {
        mainThread = mainThread && QThread::currentThread() == app.thread();
        plots.push_back({at, module, values});
    }, Qt::DirectConnection);
    QObject::connect(&device, &LaserDevice::replyReady, &app, [&](RxTime, const Reply &reply) {
        if (reply.command == Command::StopReply && reply.widthIndex == 4) ++marker;
        else states.push_back(reply);
    });
    QObject::connect(qobject_cast<LaserHandle *>(device.logSourceObject()), &LaserHandle::logMessage,
                     &app, [&](const QString &text) { if (text.contains(QStringLiteral("未定义寄存器"))) ++errors; });
    const auto open = [&] {
        require(device.open({"controlled", {}}) && until([&] { return device.isOpen(); }), "open test transport");
    };
    const auto receive = [&](const QByteArray &wire) {
        const int expected = marker + 1;
        FakeSerialPort::enqueue(wire + encode(makeBody(0, 0xb6, 4, {})));
        require(until([&] { return marker == expected; }), "packet and barrier delivered");
    };
    open();
    receive(encode(makeBody(0, 0xb1, 0, record(Register::LaserTemperature, 30))));
    require(plots.size() == 1 && states.empty(), "telemetry-only packet produces one graph signal, no state signal");
    require(plots.back().values.size() == 8 && plots.back().values["laserTemperature"].toDouble() == 30,
            "first partial packet yields complete eight-key snapshot");
    for (const auto key : {"mcuTemperature", "pa1Current", "pa2Current", "pa3Current", "voltage", "sdCurrent", "pztOffset"})
        require(std::isnan(plots.back().values[key].toDouble()), "unseen value is NaN, never zero");

    const auto full = record(Register::LaserTemperature, 31) + record(Register::McuTemperature, 40)
        + record(Register::PA1Current, 1) + record(Register::PA2Current, 2)
        + record(Register::PA3Current, 3) + record(Register::Voltage, 24)
        + record(Register::SDCurrent, 0.03125f) + record(Register::PztOffset, 2.125f);
    const auto firstAt = std::chrono::system_clock::now();
    receive(encode(makeBody(0, 0xb1, 2, full)));
    require(plots.size() == 2 && plots.back().at >= firstAt, "eight records in one packet produce exactly one snapshot");
    const QVariantMap expected{{"laserTemperature", 31.0}, {"mcuTemperature", 40.0},
        {"pa1Current", 1.0}, {"pa2Current", 2.0}, {"pa3Current", 3.0}, {"voltage", 24.0},
        {"sdCurrent", 0.03125}, {"pztOffset", 2.125}};
    require(plots.back().values == expected, "all eight readings committed together");
    receive(encode(makeBody(0, 0xb1, 2, record(Register::Voltage, 25))));
    auto expectedHeld = expected;
    expectedHeld["voltage"] = 25.0;
    require(plots.size() == 3 && plots.back().values == expectedHeld, "partial packet holds other seven values");
    require(plots[1].values == expected, "earlier emitted snapshot remains immutable after later updates");

    receive(encode(makeBody(0, 0xb1, 0, record(Register::SDCurrent, 0.0625f))));
    expectedHeld["sdCurrent"] = 0.0625;
    require(plots.size() == 4 && plots.back().values == expectedHeld && states.empty(),
            "SD-only reply triggers one complete snapshot and holds PZT");
    receive(encode(makeBody(0, 0xb1, 0, record(Register::PztOffset, 2.5f))));
    expectedHeld["pztOffset"] = 2.5;
    require(plots.size() == 5 && plots.back().values == expectedHeld && states.empty(),
            "PZT-only reply triggers one complete snapshot and holds SD");

    receive(encode(makeBody(1, 0xb1, 0, record(Register::Voltage, 48))));
    require(plots.back().module == Module::Amplifier && plots.back().values["voltage"].toDouble() == 48
            && std::isnan(plots.back().values["laserTemperature"].toDouble()), "module caches are isolated");
    receive(encode(makeBody(0, 0xb1, 0, record(Register::Voltage, 26) + record(Register::Voltage, 27))));
    require(plots.back().values["voltage"].toDouble() == 27
            && plots.back().values["laserTemperature"].toDouble() == 31, "duplicate register uses last value in packet, seed cache intact");
    auto plotsBefore = plots.size();
    receive(encode(makeBody(0, 0xb1, 0, QByteArray::fromHex("1800000001000000"))));
    require(plots.size() == plotsBefore && states.size() == 1
            && states.back().readings[0].address == Register::Emission, "state-only packet never redraws");
    receive(encode(makeBody(0, 0xb5, 0, {})));
    require(plots.size() == plotsBefore && states.back().command == Command::EmitReply, "ack does not redraw or fabricate measurements");
    receive(encode(makeBody(0, 0xb1, 0, record(Register::PA1Current, 4) + QByteArray::fromHex("8400000001000000"))));
    require(plots.size() == plotsBefore + 1 && states.back().readings.size() == 1
            && states.back().readings[0].address == Register::Alarms, "mixed packet emits one state reply and one graph snapshot");
    plotsBefore = plots.size();
    receive(encode(makeBody(0, 0xb1, 0, record(Register::Voltage, 99) + record(Register::SDCurrent, 1)
        + record(Register::PztOffset, 9) + QByteArray::fromHex("ffffffff00000000"))));
    require(plots.size() == plotsBefore && errors == 1, "bad tail rejects entire packet before cache mutation");
    receive(encode(makeBody(0, 0xb1, 0, record(Register::McuTemperature, 41))));
    require(plots.back().values["voltage"].toDouble() == 27
        && plots.back().values["sdCurrent"].toDouble() == 0.0625
        && plots.back().values["pztOffset"].toDouble() == 2.5,
        "rejected packet did not contaminate held voltage, SD or PZT");

    plotsBefore = plots.size();
    const std::array registers{Register::LaserTemperature, Register::McuTemperature, Register::PA1Current,
        Register::PA2Current, Register::PA3Current, Register::Voltage, Register::SDCurrent, Register::PztOffset};
    QByteArray packets;
    for (const auto address : registers) packets += encode(makeBody(0, 0xb1, 0, record(address, 5)));
    receive(packets);
    require(plots.size() == plotsBefore + 8, "eight packets produce eight complete snapshots without cross-packet waiting");
    for (auto i = plotsBefore; i < plots.size(); ++i)
        require(plots[i].values.size() == 8, "each packet maintains a full aligned sample row");
    require(mainThread && FakeSerialPort::writes().empty(), "main-thread delivery never initiates queries");

    device.close();
    open();
    receive(encode(makeBody(0, 0xb1, 0, record(Register::Voltage, 12))));
    require(plots.back().values["voltage"].toDouble() == 12
            && std::isnan(plots.back().values["laserTemperature"].toDouble())
            && std::isnan(plots.back().values["sdCurrent"].toDouble())
            && std::isnan(plots.back().values["pztOffset"].toDouble()), "new explicit connection starts with unknown cached values");
    device.close();
    std::cout << checks << " checks passed\n";
}
