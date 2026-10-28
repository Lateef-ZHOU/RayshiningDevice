#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>
#include <QCoreApplication>
#include <QEvent>
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
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    using namespace Rayshining;
    FakeSerialPort::reset();
    std::vector<Reply> received;
    int errors = 0;
    bool mainThread = true, timePreserved = true, retiredClosed = false;
    const auto before = std::chrono::system_clock::now();
    const auto good = QByteArray::fromHex("57484c530000b7000000af2755");
    {
        LaserDevice device;
        const auto *logs = qobject_cast<LaserHandle *>(device.logSourceObject());
        require(logs != nullptr, "log source is existing Handle");
        QObject::connect(logs, &LaserHandle::logMessage, &app, [&](const QString &text) {
            mainThread = mainThread && QThread::currentThread() == app.thread();
            if (text.contains(QStringLiteral("未定义"))) ++errors;
        });
        QObject::connect(&device, &LaserDevice::replyReady, &app, [&](RxTime at, const Reply &reply) {
            mainThread = mainThread && QThread::currentThread() == app.thread();
            timePreserved = timePreserved && at >= before && at <= std::chrono::system_clock::now();
            received.push_back(reply);
        }, Qt::DirectConnection); // signal itself must be emitted on main thread
        QObject::connect(&device, &DeviceBase::retiring, &app, [&] {
            retiredClosed = !device.isOpen() && FakeSerialPort::livePorts() == 0;
        });
        require(!device.readRegisters(Module::Seed, 0, {Register::Frequency}), "offline query rejected");
        require(device.open({"controlled", {}}) && until([&] { return device.isOpen(); }), "device opens");
        require(FakeSerialPort::baudRate() == 115200, "protocol baud fixed");
        const QList<Register> query{Register::Model1,Register::Model2,Register::Model3,Register::Width0,
            Register::Width1,Register::Width2,Register::Width3,Register::Width4,Register::Width5};
        require(device.readRegisters(Module::Seed, 0, query), "query sent");
        require(FakeSerialPort::writes() == std::vector<QByteArray>{QByteArray::fromHex(
            "57484c53000031000024930000009400000095000000960000009700000098000000990000009a0000009b000000acb955")},
            "query matches document bytes");
        auto lock = std::make_shared<DeviceLock>(QStringLiteral("test"));
        bool cancelled = false;
        lock->arm([&] { cancelled = true; });
        require(device.tryLock(lock) && !device.readRegisters(Module::Seed, 0, query), "busy query rejected");
        device.releaseLock(lock);
        require(!device.readRegisters(Module::Seed, 6, query)
            && !device.readRegisters(static_cast<Module>(2), 0, query)
            && !device.readRegisters(Module::Seed, 0, {})
            && !device.readRegisters(Module::Seed, 0, {static_cast<Register>(0xffffffff)})
            && FakeSerialPort::writes().size() == 1, "invalid query never sent");
        const auto readings = encode(1, 0xb1, 5, QByteArray::fromHex("800000000a000000890000000000c8c1"));
        const auto errorsBeforeReceive = errors;
        FakeSerialPort::enqueue(good.first(4));
        FakeSerialPort::enqueue(good.sliced(4) + encode(2, 0xb1, 0, {}) + readings);
        require(until([&] { return received.size() == 2 && errors == errorsBeforeReceive + 1; }), "actual stream parses and recovers after semantic error");
        require(received[0].command == Command::ModeReply && received[0].readings.empty()
            && received[1].readings.size() == 2 && std::get<float>(received[1].readings[1].value) == -25.f,
            "typed multi-value payload through device");
        require(mainThread && timePreserved, "main-thread delivery and unchanged timestamp");
        require(device.tryLock(lock), "lock held before close");
        device.close();
        require(until([&] { return cancelled && !device.isLocked(); }), "disconnect releases lock through existing notification");
        require(device.open({"controlled", {}}) && until([&] { return device.isOpen(); }), "reopen after close");
        FakeSerialPort::enqueue(good);
        require(until([&] { return received.size() == 3; }), "receive after reopen");
    }
    require(retiredClosed && FakeSerialPort::livePorts() == 0, "receive thread stops before retiring");
    require(received[1].readings.size() == 2, "results survive device destruction");

    // 不泵主线程队列，先让收包线程排入结果，再删除设备；上下文销毁撤销未交付调用。
    const auto countBefore = received.size();
    for (int i = 0; i < 10; ++i) {
        auto device = std::make_unique<LaserDevice>();
        QObject::connect(device.get(), &LaserDevice::replyReady, &app,
                         [&](RxTime, const Reply &reply) { received.push_back(reply); });
        require(device->open({"controlled", {}}) && until([&] { return device->isOpen(); }), "stress open");
        std::atomic<bool> reached{false};
        const auto *logs = qobject_cast<LaserHandle *>(device->logSourceObject());
        QObject::connect(logs, &LaserHandle::logMessage, device.get(), [&](const QString &text) {
            if (text.contains(QStringLiteral("未定义"))) reached = true;
        }, Qt::DirectConnection);
        FakeSerialPort::enqueue(good + encode(2, 0xb1, 0, {}));
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!reached && std::chrono::steady_clock::now() < end) QThread::msleep(1);
        require(reached, "worker queued a good reply before destruction");
        device.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        require(received.size() == countBefore && FakeSerialPort::livePorts() == 0, "pending callbacks cannot access destroyed device");
    }
    std::cout << checks << " checks passed\n";
}
