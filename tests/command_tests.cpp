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
    require(device.readRegisters(Module::Seed, 0, {Register::Voltage, Register::SDCurrent, Register::PztOffset}),
            "SD and PZT can be appended to the existing multi-register read");
    require(FakeSerialPort::writes().size() == 1
        && FakeSerialPort::writes().back() == encode(QByteArray::fromHex("00003100000c83000000850000008f000000")),
        "one ordinary 0x31 query contains three little-endian register addresses");
    require(device.setControlMode(false), "external mode sent");
    require(FakeSerialPort::writes().back() == QByteArray::fromHex("57484c5300003700000800000000000000004a7955"), "document external vector");
    require(device.setControlMode(true), "internal mode sent");
    require(FakeSerialPort::writes().back() == QByteArray::fromHex("57484c53000037000008010000000000000086b855"), "document internal vector");
    require(device.startEmission(Module::Amplifier, 1, 10, 80, 200), "amplifier emission");
    require(FakeSerialPort::writes().back() == encode(makeBody(1, 0x35, 1, QByteArray::fromHex("0a00000050000000000000c8"))), "frequency power little endian and width big endian");
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
    require(device.stopEmission(Module::Amplifier, 5) && FakeSerialPort::writes().back() == encode(makeBody(1, 0x36, 5, {})), "targeted stop");
    const auto count = FakeSerialPort::writes().size();
    require(!device.stopEmission(Module::Seed, 256), "no truncation of invalid command parameters");
    require(!device.readRegisters(Module::Seed, -1, {Register::Voltage}), "invalid query width");
    require(!device.startEmission(Module::Seed, 0, 0, 100, 100), "zero frequency rejected");
    require(!device.startEmission(Module::Seed, 0, 10, 101, 100), "power above 100 rejected");
    require(!device.startEmission(Module::Seed, 0, 10, -1, 100), "negative power rejected");
    require(FakeSerialPort::writes().size() == count, "invalid commands do not send");
    auto lock = std::make_shared<DeviceLock>(QStringLiteral("test"));
    require(device.tryLock(lock), "lock held");
    require(!device.stopEmission(Module::Seed, 0), "manual command respects lock");
    // 与 QML 一样订阅 Handle 的关闭前通知；Device 先撤锁，业务槽再发送关光。
    QObject::connect(qobject_cast<LaserHandle *>(device.logSourceObject()), &LaserHandle::connectionChanged,
        &device, [&](bool connected) { if (!connected) (void)device.stopEmission(Module::Seed, 0); });
    const auto beforeClose = FakeSerialPort::writes().size();
    device.close();
    require(!device.isOpen() && FakeSerialPort::livePorts() == 0, "close releases the connection synchronously");
    require(FakeSerialPort::writes().size() == beforeClose + 1 && FakeSerialPort::writes().back() == encode(makeBody(0, 0x36, 0, {})),
            "forced close sends exactly one seed off command before stopping, even while locked");
    require(until([&] { return !device.isLocked(); }), "disconnect notification releases lock");
    device.close();
    require(FakeSerialPort::writes().size() == beforeClose + 1, "already disconnected close cannot send another frame");
    require(device.open(SerialPortInfo{"controlled", {}}) && until([&] { return device.isOpen(); }), "reopen for failed close write");
    FakeSerialPort::writeResult(SP_ERR_FAIL);
    const auto attempts = FakeSerialPort::writesStarted();
    device.close();
    require(FakeSerialPort::writesStarted() == attempts + 1 && !device.isOpen() && FakeSerialPort::livePorts() == 0,
            "failed off write is tried once and does not prevent disconnection");
    std::cout << checks << " command checks passed\n";
}
