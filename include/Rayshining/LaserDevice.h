#pragma once
#include <QList>
#include <QMetaObject>
#include <QMetaType>
#include <Device/Base/DeviceBase.h>
#include "LaserHandle.h"
#include "Reply.h"

Q_DECLARE_METATYPE(Rayshining::Reply)

namespace Rayshining {
// 激光器尚无任务族或 Collector；直接复用设备身份、状态与锁，Handle 独持收包回调。
class LaserDevice final : public DeviceBase
{
    Q_OBJECT
public:
    explicit LaserDevice(QObject *parent = nullptr) : DeviceBase(parent), handle_(makeOnStream(this))
    {
        static_assert(FinalDevice<LaserDevice, decltype(&LaserDevice::PreDestruction)>);
        QObject::connect(&handle_, &LaserHandle::connectionChanged, this, [this](bool connected) {
            if (!connected)
                dropLock();
            Q_EMIT statusChanged();
        });
    }
    ~LaserDevice() override { PreDestruction(); }
    [[nodiscard]] QString displayName() const final { return QStringLiteral("镭晟激光器"); }
    [[nodiscard]] QString subtitle() const final { return QStringLiteral("串口 · RS-1550-300-MINI"); }
    [[nodiscard]] bool isOpen() const noexcept final { return handle_.isOpen(); }
    [[nodiscard]] QObject *logSourceObject() noexcept final { return &handle_; }

    // 表单只交端口；协议规格固定，不缓存串口设置或 UI 选择。
    [[nodiscard]] bool open(const SerialPortInfo &port) { return handle_.start(LaserHandle::parameters(port)); }
    void close() { handle_.stop(); }

    [[nodiscard]] bool readRegisters(Module module, quint8 width, const QList<Register> &addresses)
    {
        if ((module != Module::Seed && module != Module::Amplifier) || width > 5
            || addresses.isEmpty() || addresses.size() > static_cast<qsizetype>(MaxDataSize / 4)) {
            Q_EMIT handle_.logMessage(QStringLiteral("激光器查询参数无效，未发送"));
            return false;
        }
        QByteArray data(addresses.size() * 4, Qt::Uninitialized);
        for (qsizetype i = 0; i < addresses.size(); ++i) {
            if (!detail::format(addresses[i])) {
                Q_EMIT handle_.logMessage(QStringLiteral("激光器查询包含未定义寄存器，未发送"));
                return false;
            }
            qToLittleEndian<quint32>(static_cast<quint32>(addresses[i]), data.data() + i * 4);
        }
        if (!canEngage({})) {
            Q_EMIT handle_.logMessage(QStringLiteral("设备未连接或正被占用，激光器查询未发送"));
            return false;
        }
        return handle_.send(encode(static_cast<quint16>(module), static_cast<quint8>(Command::Read), width, data));
    }

Q_SIGNALS:
    // 主线程交付一整包；时间原样传递，接收者决定如何显示/保存，不另存 latest 副本。
    void replyReady(RxTime at, const Rayshining::Reply &reply);

private:
    void PreDestruction() noexcept final
    {
        handle_.stop(); // 等待回调退出后，才撤借用和清理本对象。
        Q_EMIT retiring();
        DeviceBase::PreDestruction();
    }
    [[nodiscard]] static DataCallback makeOnStream(LaserDevice *self)
    {
        return [self](const QByteArray &body, RxTime at) {
            auto result = decodeReply(body);
            if (const auto *error = std::get_if<QString>(&result)) {
                Q_EMIT self->handle_.logMessage(*error);
                return;
            }
            QMetaObject::invokeMethod(self, [self, at, reply = std::get<Reply>(std::move(result))] {
                Q_EMIT self->replyReady(at, reply);
            }, Qt::QueuedConnection);
        };
    }
    LaserHandle handle_;
};
}
