#pragma once
#include <memory>
#include <SerialHandle.h>
#include "Framer.h"

namespace Rayshining {
class LaserHandle final : public SerialHandle<Framer>
{
    Q_OBJECT
public:
    using SerialHandle::SerialHandle;
    ~LaserHandle() override { stop(); }

    // 固定协议串口规格，调用者只提供端口；连接快照仍由 HandleBase 持有。
    [[nodiscard]] static SerialParams parameters(const SerialPortInfo &port)
    {
        SerialParams params;
        params.portInfo = port;
        params.baudRate = 115200;
        return params;
    }

Q_SIGNALS:
    void connectionChanged(bool connected) override;
    void logMessage(const QString &text) const final;

private:
    [[nodiscard]] RXRuling onRX(const QByteArray &body, RxTime at, std::shared_ptr<ConnType>) override
    {
        return {RXRuling::Decode, body, at}; // 已校验并去掉外层帧头尾；语义解析归官邸 onStream。
    }
    [[nodiscard]] TXRuling onTX(const QByteArray &frame, std::shared_ptr<ConnType>) const override
    {
        if (!validFrame(frame)) {
            Q_EMIT logMessage(QStringLiteral("激光器指令帧格式或 CRC 错误，未发送"));
            return {};
        }
        return {TXRuling::Encode, frame};
    }
};
}
