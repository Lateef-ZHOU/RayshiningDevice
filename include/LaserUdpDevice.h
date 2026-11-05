#pragma once
#include <PcapHandle.h>
#include "LaserDevice.h"

namespace Rayshining {
// 桥接 UDP 的载荷就是协议主体，不再套 WHLS 或第二层业务信封。
class LaserUdpDevice final
#ifdef Q_MOC_RUN
    : public PcapHandle
#else
    : public LaserDeviceT<PcapHandle>
#endif
{
    Q_OBJECT
    Q_PROPERTY(QString displayName MEMBER displayName CONSTANT)
    Q_PROPERTY(QString subtitle MEMBER subtitle CONSTANT)
    Q_PROPERTY(bool isOpen READ isOpen)
    Q_PROPERTY(QVariant holder READ qmlHolder)
    Q_PROPERTY(QVariantMap connectionParameters READ connectionParameters CONSTANT)
    QML_NAMED_ELEMENT(LaserUdpDevice)
    QML_ATTACHED(LaserUdpDevice)
    QML_UNCREATABLE("由 UDP 激光器 QML 官邸拥有")
    using Base = LaserDeviceT<PcapHandle>;
public:
    static LaserUdpDevice *qmlAttachedProperties(QObject *owner) { return new LaserUdpDevice(owner); }
    explicit LaserUdpDevice(QObject *parent) : Base(QStringLiteral("RS-1550-300-MINI"), parent)
    { static_assert(FinalDevice<LaserUdpDevice, decltype(&LaserUdpDevice::PreDestruction)>); }
    ~LaserUdpDevice() override { PreDestruction(); }
    [[nodiscard]] QVariantMap connectionParameters() const
    {
        QStringList names, tips;
        for (const auto &entry : interfaces()) { names << entry.friendlyName; tips << entry.description; }
        return {{"networkInterfaceOptions", names}, {"networkInterfaceTips", tips}};
    }
    Q_INVOKABLE bool open(int interfaceIndex)
    {
        if (interfaceIndex < 0 || interfaceIndex >= interfaces().size()) {
            Q_EMIT logMessage(QStringLiteral("未选择有效网卡，连接请求未提交"));
            return false;
        }
        return PcapHandle::open(parameters(interfaces()[interfaceIndex]));
    }
    Q_INVOKABLE void close() { Base::close(); }
    Q_INVOKABLE bool readRegisters(Module module, int width, const QList<Register> &addresses)
    { return Base::readRegisters(module, width, addresses); }
    Q_INVOKABLE bool startEmission(Module module, int width, int frequencyKhz, int powerPercent, int widthNs)
    { return Base::startEmission(module, width, frequencyKhz, powerPercent, widthNs); }
    Q_INVOKABLE bool stopEmission(Module module, int width) { return Base::stopEmission(module, width); }
    Q_INVOKABLE bool setControlMode(bool internal) { return Base::setControlMode(internal); }
Q_SIGNALS:
    void lockChanged(HandleCore::ResourcePhase phase) override;
    void replyReady(RxTime at, const Rayshining::Reply &reply) override;
    void telemetryReady(RxTime at, Rayshining::Module module, const QVariantMap &values) override;
private:
    void PreDestruction() noexcept final { Base::PreDestruction(); }

    // 选项与下标共用同一次枚举；不缓存连接表单或第二份运行参数。
    static const QList<NetworkInterfaceInfo> &interfaces()
    {
        static const auto result = availableInterfaces();
        return result;
    }
    static PcapParams parameters(const NetworkInterfaceInfo &network)
    {
        return {network, IPv4(QByteArray::fromHex("c0a864c8")), IPv4(QByteArray::fromHex("c0a86464")), {},
            55100, 55100}; // Windows 192.168.100.200 ↔ PetaLinux 192.168.100.100；不要求系统配置这些 IP。
    }
};
}
QML_DECLARE_TYPEINFO(Rayshining::LaserUdpDevice, QML_HAS_ATTACHED_PROPERTIES)
