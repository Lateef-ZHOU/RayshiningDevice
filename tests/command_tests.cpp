#include <chrono>
#include <cstdlib>
#include <iostream>
#include <QCoreApplication>
#include <QThread>
#include <QtEndian>
#include <Rayshining/LaserDevice.h>
#include "FakeSerialPort.h"
namespace {
int checks = 0;
void require(bool v, const char *text) { ++checks; if (!v) { std::cerr << text << '\n'; std::exit(1); } }
template<class F> bool until(F f) {
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!f() && std::chrono::steady_clock::now() < end) { QCoreApplication::processEvents(); QThread::msleep(1); }
    return f();
}
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    using namespace Rayshining;
    FakeSerialPort::reset();
    LaserDevice device(nullptr);
    require(!device.startEmission(Module::Seed, 0, 10, 100, 100), "offline emission rejected");
    require(device.open(SerialPortInfo{"controlled", {}}) && until([&]{ return device.isOpen(); }), "fake connection");
    require(FakeSerialPort::writes().empty(), "connection does not emit or query by itself");
    require(device.setControlMode(false), "external mode sent");
    require(FakeSerialPort::writes().back() == QByteArray::fromHex("57484c5300003700000800000000000000004a7955"), "document external vector");
    require(device.setControlMode(true), "internal mode sent");
    require(FakeSerialPort::writes().back() == QByteArray::fromHex("57484c53000037000008010000000000000086b855"), "document internal vector");
    require(device.startEmission(Module::Amplifier, 1, 10, 80, 200), "amplifier emission");
    require(FakeSerialPort::writes().back() == encode(1, 0x35, 1, QByteArray::fromHex("0a00000050000000000000c8")), "frequency power little endian and width big endian");
    require(device.startEmission(Module::Seed, 1, 10, 80, 200), "seed emission");
    require(FakeSerialPort::writes().back() == QByteArray::fromHex("57484c5300003501000c0a00000050000000000000c8dd6a55"), "seed uses requested power and matches document 80 percent vector");
    for (const int frequency : {1, 5, 8, 9, 10, 12, 20}) {
        require(device.startEmission(Module::Seed, 0, frequency, 1, 200), "frequency request accepted");
        const auto frame = FakeSerialPort::writes().back();
        require(qFromLittleEndian<quint32>(frame.constData() + HeaderSize) == static_cast<quint32>(frequency),
                "requested kHz is sent unchanged, without five/ten-kHz quantization");
    }
    for (const int power : {0, 1, 37, 100}) {
        require(device.startEmission(Module::Seed, 1, 10, power, 200), "seed power range accepted");
        const auto frame = FakeSerialPort::writes().back();
        require(readBe16(frame[4], frame[5]) == 0 && qFromLittleEndian<quint32>(frame.constData() + HeaderSize + 4) == static_cast<quint32>(power),
                "seed frame carries exact requested power");
    }
    require(device.startEmission(Module::Seed, 5, 10, 10, 915), "sparse width code and returned nanoseconds accepted");
    const auto selectedFrame = FakeSerialPort::writes().back();
    require(static_cast<quint8>(selectedFrame[7]) == 5
            && qFromBigEndian<quint32>(selectedFrame.constData() + HeaderSize + 8) == 915
            && qFromLittleEndian<quint32>(selectedFrame.constData() + HeaderSize + 4) == 10,
            "wire keeps width code five, actual 915 ns and unscaled ten-percent request");
    require(device.stopEmission(Module::Amplifier, 5) && FakeSerialPort::writes().back() == encode(1, 0x36, 5, {}), "targeted stop");
    require(device.setOpticalChannel(3) && FakeSerialPort::writes().back() == encode(0, 0x38, 0, QByteArray::fromHex("0300000000000000")), "channel fixed seed address");
    const auto count = FakeSerialPort::writes().size();
    require(!device.setOpticalChannel(4) && !device.stopEmission(Module::Seed, 256), "no truncation of invalid command parameters");
    require(!device.readRegisters(Module::Seed, -1, {Register::Voltage}), "invalid query width");
    require(!device.startEmission(Module::Seed, 0, 0, 100, 100), "zero frequency rejected");
    require(!device.startEmission(Module::Seed, 0, 10, 101, 100), "power above 100 rejected");
    require(!device.startEmission(Module::Seed, 0, 10, -1, 100), "negative power rejected");
    require(FakeSerialPort::writes().size() == count, "invalid commands do not send");
    auto lock = std::make_shared<DeviceLock>(QStringLiteral("test"));
    require(device.tryLock(lock), "lock held");
    require(!device.stopEmission(Module::Seed, 0), "manual command respects lock");
    device.releaseLock(lock);
    device.close();
    std::cout << checks << " command checks passed\n";
}
