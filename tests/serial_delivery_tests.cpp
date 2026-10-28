#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <vector>
#include <QCoreApplication>
#include <QThread>
#include <Rayshining/LaserHandle.h>
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
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    return ready();
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    FakeSerialPort::reset();
    std::mutex gate;
    std::vector<QByteArray> received;
    std::atomic<bool> worker{true}, typedTime{true};
    const auto ack = QByteArray::fromHex("57484c530000b7000000af2755");
    const auto data = QByteArray("WHLS\x55", 5);
    const auto reply = Rayshining::encode(1, 0xb1, 5, data);
    // 未定义业务字段仍完整上传，由官邸 onStream 决定是否支持。
    const auto opaque = Rayshining::encode(0xabcd, 0x42, 0xee, QByteArray::fromHex("0080"));
    {
        Rayshining::LaserHandle handle([&](const QByteArray &body, RxTime at) {
            worker = worker && QThread::currentThread() != app.thread();
            typedTime = typedTime && at != RxTime{};
            const std::lock_guard lock(gate);
            received.push_back(body);
        });
        const auto count = [&] { const std::lock_guard lock(gate); return received.size(); };
        require(handle.start(Rayshining::LaserHandle::parameters({"controlled", {}})), "serial open submitted");
        require(until([&] { return handle.isOpen(); }), "serial open completed");
        auto bad = ack;
        bad[bad.size() - 3] = static_cast<char>(bad[bad.size() - 3] ^ 1);
        FakeSerialPort::enqueue(bad + ack + reply + opaque);
        require(until([&] { return count() == 3; }), "valid bodies delivered through production Handle");
        require(handle.send(reply) && FakeSerialPort::writes() == std::vector<QByteArray>{reply},
                "TX still sends complete wire frame");
        handle.stop();
        require(worker && typedTime, "callback thread and RxTime preserved");
    }
    require(received == std::vector<QByteArray>{QByteArray::fromHex("0000b7000000"),
                QByteArray::fromHex("0001b1050005") + data, QByteArray::fromHex("abcd42ee00020080")},
            "only outer framing removed and delivered bytes survive connection destruction");
    require(FakeSerialPort::livePorts() == 0, "connection released");
    std::cout << checks << " checks passed\n";
}
