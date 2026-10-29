#include <cstdlib>
#include <iostream>
#include <memory>
#include <QGuiApplication>
#include <QImage>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlListReference>
#include <QQuickWindow>
#include <QQuickItem>
#include <QThread>
#include <QTimer>
#include <QDebug>
#include <QtQml/QQmlExtensionPlugin>
#include <Rayshining/LaserDevice.h>
#include "FakeSerialPort.h"
Q_IMPORT_QML_PLUGIN(LidarUIPlugin)
Q_IMPORT_QML_PLUGIN(LidarChartsPlugin)
Q_IMPORT_QML_PLUGIN(LidarRayshiningPlugin)
namespace {
int checks = 0;
void require(bool v, const char *text) { ++checks; if (!v) { qCritical() << text; std::exit(1); } }
template<typename Predicate> bool until(Predicate ready) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!ready() && std::chrono::steady_clock::now() < end) {
        QCoreApplication::processEvents(); QThread::msleep(1);
    }
    return ready();
}
}
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QQmlEngine engine;
    const int captureIndex = app.arguments().indexOf("--capture");
    const QString capture = captureIndex >= 0 && captureIndex + 1 < app.arguments().size()
        ? app.arguments()[captureIndex + 1] : QString{};
    {
        QQuickWindow window;
        window.resize(400, 500);
        QQmlComponent status(&engine);
        status.setData(R"QML(import QtQuick
import Lidar.Rayshining
LaserStatus {
    width: 400; height: implicitHeight
    title: "激光状态"
    widthOptions: [{code: 0, value: 200}]
    readings: ({"24": 1, "128": 5, "129": 2, "130": 0, "7": 1024, "132": 0,
        "147": "1550", "148": "-ACI", "149": "-MP",
        "16": "1550", "17": "0300", "18": "MN26", "160": "0011"})
}
)QML", QUrl());
        std::unique_ptr<QObject> section(status.create());
        require(bool(section), "status layout creates production component");
        auto *item = qobject_cast<QQuickItem *>(section.get());
        item->setParentItem(window.contentItem());
        window.show();
        auto readings = section->property("readings").value<QJSValue>().toVariant().toMap();
        qreal serialWidth = 0;
        for (const int frequency : {1, 5, 8, 9, 10, 12, 20, 5}) {
            readings["128"] = frequency;
            section->setProperty("readings", readings);
            for (int i = 0; i < 50; ++i) { QCoreApplication::processEvents(); QThread::msleep(2); }
            for (auto *label : section->findChildren<QQuickItem *>()) {
                const auto text = label->property("text").toString();
                if (label->objectName() == "frequencyReadback")
                    require(text == QStringLiteral("%1 kHz").arg(frequency), "UI shows returned kHz unchanged");
                if (label->objectName() == "serialReadback") {
                    require(text == QStringLiteral("15500300MN260011") && label->property("lineCount").toInt() == 1,
                            "complete serial fits one line in the normal sidebar");
                    require(serialWidth == 0 || qFuzzyCompare(label->width(), serialWidth),
                            "serial width does not depend on frequency digit count");
                    serialWidth = label->width();
                }
            }
        }
        require(serialWidth > 0, "serial readback was checked");
        if (!capture.isEmpty()) require(window.grabWindow().save(capture + "-status.png"), "status capture saved");
    }
    QQuickWindow pageWindow;
    pageWindow.resize(1280, 900);
    QQmlComponent component(&engine);
    component.setData(R"QML(import QtQuick
import Lidar.Rayshining
LaserModule {
    width: 1280; height: 900
    property int displayedLogs: 0
    property string lastLog: ""
    property int widthListUpdates: 0
    onLogMessage: text => { ++displayedLogs; lastLog = text }
    onWidthOptionsChanged: ++widthListUpdates
}
)QML", QUrl());
    std::unique_ptr<QObject> page(component.create());
    if (!page) { qCritical() << component.errors(); return 1; }
    qobject_cast<QQuickItem *>(page.get())->setParentItem(pageWindow.contentItem());
    pageWindow.show();
    for (int i = 0; i < 50; ++i) { QCoreApplication::processEvents(); QThread::msleep(2); }
    QList<QQuickItem *> buttons;
    for (auto *item : page->findChildren<QQuickItem *>())
        if (item->metaObject()->indexOfSignal("clicked()") >= 0) buttons.append(item);
    for (const auto &pair : {QStringList{QStringLiteral("连接"), QStringLiteral("断开")},
                           QStringList{QStringLiteral("设置参数并出光"), QStringLiteral("关光")},
                           QStringList{QStringLiteral("切换为内控"), QStringLiteral("切换为外控")}}) {
        QQuickItem *left = nullptr, *right = nullptr;
        for (auto *button : buttons) {
            if (button->property("text").toString() == pair[0]) left = button;
            if (button->property("text").toString() == pair[1]) right = button;
        }
        require(left && right && qAbs(left->width() - right->width()) <= 1
                && qAbs(left->x()) <= 1 && qAbs(right->x() + right->width() - right->parentItem()->width()) <= 1,
                "paired buttons split the full row evenly");
    }
    auto *device = qobject_cast<Rayshining::LaserDevice *>(qmlAttachedPropertiesObject<Rayshining::LaserDevice>(page.get()));
    require(device && device->parent() == page.get(), "page owns exact backend");
    require(qmlAttachedPropertiesObject<Rayshining::LaserDevice>(page.get()) == device, "unique attached backend");
    require(!device->isOpen(), "loading UI never connects hardware");
    const QQmlListReference charts(page.get(), "mainPages");
    require(charts.isValid() && charts.count() == 1, "only seed chart page assembled");
    auto *powerInput = page->findChild<QObject *>(QStringLiteral("powerInput"));
    require(powerInput && powerInput->property("value").toInt() == 1, "power field starts at one percent");
    for (const auto *text : {"0", "1", "37", "100"}) {
        powerInput->setProperty("text", text);
        require(powerInput->property("acceptableInput").toBool(), "power field accepts valid percentage");
    }
    for (const auto *text : {"", "-1", "101", "1.5", "abc"}) {
        powerInput->setProperty("text", text);
        require(!powerInput->property("acceptableInput").toBool(), "power validator rejects invalid percentages");
    }
    QMetaObject::invokeMethod(powerInput, "restore");
    const QQmlListReference sections(page.get(), "sections");
    require(sections.count() == 3, "only connection, readback and control sections remain");
    auto *timer = page->findChild<QObject *>(QStringLiteral("statusTimer"));
    require(timer && timer->property("interval").toInt() == 500 && timer->property("repeat").toBool()
            && !timer->property("running").toBool(), "fixed half-second polling is stopped while offline");
    int controlButtons = 0;
    for (auto *item : page->findChildren<QObject *>()) {
        if (item->metaObject()->indexOfSignal("clicked()") < 0) continue;
        const auto text = item->property("text").toString();
        if (QStringList{QStringLiteral("更新"), QStringLiteral("设置参数并出光"), QStringLiteral("关光"),
                        QStringLiteral("切换为内控"), QStringLiteral("切换为外控")}.contains(text)) {
            ++controlButtons;
            require(item->property("enabled").toBool(), "commands remain enabled without a connection or width options");
        }
    }
    require(controlButtons == 5, "five control buttons available");
    auto *frequencySelector = page->findChild<QObject *>(QStringLiteral("frequencySelector"));
    require(frequencySelector && !frequencySelector->property("editable").toBool()
            && frequencySelector->property("count").toInt() == 3
            && frequencySelector->property("currentValue").toInt() == 10,
            "frequency uses a closed three-option menu, initially ten kHz");
    auto *widthSelector = page->findChild<QObject *>(QStringLiteral("pulseWidthSelector"));
    auto *powerReadback = page->findChild<QObject *>(QStringLiteral("powerReadback"));
    require(widthSelector && powerReadback, "width selector and power readback present");
    const auto selectedWidth = [&]() {
        const auto options = page->property("widthOptions").value<QJSValue>().toVariant().toList();
        const auto index = widthSelector->property("currentIndex").toInt();
        return index >= 0 && index < options.size() ? options[index].toMap() : QVariantMap{};
    };
    require(widthSelector->property("count").toInt() == 0 && widthSelector->property("currentIndex").toInt() == -1
            && widthSelector->property("displayText").toString().isEmpty(),
            "width selector stays blank before device reply");
    require(powerReadback->property("text").toString().endsWith(QStringLiteral("—")), "missing power stays unknown");
    for (const auto *name : {"identityRegisters", "statusRegisters", "measurementRegisters"}) {
        bool containsWidth = false;
        for (const auto &address : page->property(name).value<QJSValue>().toVariant().toList())
            containsWidth |= address.toInt() >= static_cast<int>(Rayshining::Register::Width0)
                && address.toInt() <= static_cast<int>(Rayshining::Register::Width5);
        require(!containsWidth, "ordinary query groups exclude widths; connection has its separate one-shot width query");
    }
    int logs = 0;
    QObject::connect(qobject_cast<Rayshining::LaserHandle *>(device->logSourceObject()), &Rayshining::LaserHandle::logMessage,
        &app, [&](const QString &) { ++logs; });
    using namespace Rayshining;
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0, {{Register::Frequency, quint32(12)}, {Register::Emission, EmissionState::On}}});
    auto values = page->property("seedReadings").value<QJSValue>().toVariant().toMap();
    require(values.value("128").toInt() == 12 && values.value("24").toInt() == 1, "seed replies update seed status");
    require(logs == 0, "read reply never triggers another query");
    frequencySelector->setProperty("currentIndex", 0);
    require(frequencySelector->property("currentValue").toInt() == 5, "first frequency is five kHz");
    frequencySelector->setProperty("currentIndex", 1);
    require(frequencySelector->property("currentValue").toInt() == 8 && logs == 0
            && page->property("seedReadings").value<QJSValue>().toVariant().toMap() == values,
            "choosing eight kHz neither sends a command nor changes the reported status");
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0, {{Register::Frequency, quint32(5)}}});
    values = page->property("seedReadings").value<QJSValue>().toVariant().toMap();
    require(values.value("128").toInt() == 5 && frequencySelector->property("currentValue").toInt() == 8 && logs == 0,
            "reported frequency updates status without overriding the next requested frequency");
    Q_EMIT device->replyReady({}, Reply{Module::Amplifier, Command::ReadReply, 0, {{Register::Frequency, quint32(30)}}});
    require(page->property("seedReadings").value<QJSValue>().toVariant().toMap() == values,
        "amplifier replies cannot overwrite seed status");
    Q_EMIT device->replyReady({}, Reply{Module::Amplifier, Command::EmitReply, 0, {}});
    Q_EMIT device->replyReady({}, Reply{Module::Amplifier, Command::StopReply, 0, {}});
    require(logs == 0, "amplifier acknowledgements never trigger follow-up commands");
    require(page->property("displayedLogs").toInt() == 0, "amplifier replies have no UI or diagnostic side effects");
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0, {{Register::Alarms, quint16(0)}}});
    require(page->property("displayedLogs").toInt() == 0, "initial healthy status does not fill the log");
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0, {{Register::Alarms, quint16(0x8020)}}});
    require(page->property("displayedLogs").toInt() == 1
            && page->property("lastLog").toString().contains(QStringLiteral("PA1过流"))
            && page->property("lastLog").toString().contains(QStringLiteral("供电电压")),
            "received alarm bits are logged together with their meaning");
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0, {{Register::Alarms, quint16(0x8020)}}});
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0, {{Register::Power, quint32(0)}}});
    require(page->property("displayedLogs").toInt() == 1, "unchanged alarms and unrelated replies do not repeat the log");
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0, {{Register::Alarms, quint16(0)}}});
    require(page->property("displayedLogs").toInt() == 2
            && page->property("lastLog").toString() == QStringLiteral("激光报警已解除") && logs == 0,
            "cleared alarm is logged without triggering another command");
    require(powerInput->setProperty("value", 37) && logs == 0, "editing power does not send commands");
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0, {{Register::Power, quint32(25)}}});
    require(page->property("seedReadings").value<QJSValue>().toVariant().toMap().value("129").toInt() == 25
            && powerInput->property("value").toInt() == 37 && logs == 0,
            "reported power and editable power are independent; readback does not query");
    require(powerReadback->property("text").toString().endsWith(QStringLiteral("9.8 %")), "raw power 25 displays 9.8 percent");
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0, {{Register::Power, quint32(255)}}});
    require(powerReadback->property("text").toString().endsWith(QStringLiteral("100.0 %")), "raw power 255 displays 100 percent");
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0, {{Register::Power, quint32(0)}}});
    require(powerReadback->property("text").toString().endsWith(QStringLiteral("0.0 %")) && logs == 0,
            "raw power zero displays zero without querying");
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::EmitReply, 0, {}});
    require(logs == 1, "empty emission ack initiates exactly one query (offline rejection)");
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ModeReply, 0, {}});
    require(logs == 2, "mode ack initiates one query");
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ChannelReply, 0, {}});
    require(logs == 2, "channel ack has no invented readback");
    const auto at = std::chrono::system_clock::now();
    Q_EMIT device->telemetryReady(at, Module::Seed, {{"laserTemperature", 30.0}, {"mcuTemperature", 40.0},
        {"pa1Current", 1.0}, {"pa2Current", 2.0}, {"pa3Current", 3.0}, {"voltage", 24.0}});
    require(logs == 2, "graph update never initiates a query");
    QVariant result;
    QMetaObject::invokeMethod(page.get(), "sendEmission", Q_RETURN_ARG(QVariant, result));
    require(!result.toBool() && logs == 2, "unknown width does not send emission command");
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0,
        {{Register::Width0, PulseWidthOption{false, 200}}, {Register::Width2, PulseWidthOption{true, 420}},
         {Register::Width5, PulseWidthOption{true, 915}}}});
    require(widthSelector->property("count").toInt() == 2 && widthSelector->property("currentText").toString() == "420"
            && selectedWidth().value("code").toInt() == 2, "only selectable reply values shown with original sparse codes");
    require(!page->property("seedReadings").value<QJSValue>().toVariant().toMap().contains("152"),
            "width data is not duplicated in status cache");
    widthSelector->setProperty("currentIndex", 1);
    require(selectedWidth().value("code").toInt() == 5
            && selectedWidth().value("value").toInt() == 915,
            "selection carries device code five and returned nanoseconds, not row index");
    const auto widthUpdates = page->property("widthListUpdates").toInt();
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0,
        {{Register::Frequency, quint32(13)}, {Register::Power, quint32(25)}, {Register::WidthIndex, quint32(0)}}});
    require(page->property("widthListUpdates").toInt() == widthUpdates && selectedWidth().value("code").toInt() == 5,
            "ordinary status does not rebuild options or overwrite chosen width");
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0, {{Register::Width1, PulseWidthOption{true, 175}}}});
    require(widthSelector->property("count").toInt() == 3 && selectedWidth().value("code").toInt() == 5
            && widthSelector->property("currentIndex").toInt() == 2, "partial reply inserts by code and preserves selected code");
    const auto beforeIdentical = page->property("widthListUpdates").toInt();
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0,
        {{Register::Width1, PulseWidthOption{true, 175}}, {Register::Width2, PulseWidthOption{true, 420}},
         {Register::Width5, PulseWidthOption{true, 915}}}});
    require(page->property("widthListUpdates").toInt() == beforeIdentical, "identical width reply does not rebuild menu");
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0, {{Register::Width5, PulseWidthOption{true, 916}}}});
    require(widthSelector->property("currentText").toString() == "916" && selectedWidth().value("code").toInt() == 5,
            "updated physical width keeps the same protocol code");
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0, {{Register::Width2, PulseWidthOption{false, 420}}}});
    require(widthSelector->property("count").toInt() == 2 && selectedWidth().value("code").toInt() == 5,
            "option removal does not change the selected code");
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0, {{Register::Width5, PulseWidthOption{false, 916}}}});
    require(widthSelector->property("count").toInt() == 1 && selectedWidth().value("code").toInt() == 1,
            "unavailable selection falls back to an actual selectable reply");
    std::vector<Reading> sixWidths;
    for (int code = 0; code < 6; ++code)
        sixWidths.push_back({static_cast<Register>(static_cast<int>(Register::Width0) + code), PulseWidthOption{true, static_cast<quint32>(111 + code * 133)}});
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0, sixWidths});
    require(widthSelector->property("count").toInt() == 6 && selectedWidth().value("code").toInt() == 1,
            "all six slots can be offered without a hardcoded four-option limit");
    require(page->findChild<QObject *>("widthReadback")->property("text").toString() == "111 ns",
            "pulse readback maps reported code zero, independently of selected code one");
    if (!capture.isEmpty()) {
        for (int i = 0; i < 50; ++i) { QCoreApplication::processEvents(); QThread::msleep(2); }
        require(pageWindow.grabWindow().save(capture + "-page.png"), "page capture saved");
    }
    for (auto &reading : sixWidths) reading.value = PulseWidthOption{false, 0};
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0, sixWidths});
    require(widthSelector->property("count").toInt() == 0 && widthSelector->property("currentIndex").toInt() == -1,
            "all-unavailable reply leaves the menu empty");
    QMetaObject::invokeMethod(page.get(), "sendEmission", Q_RETURN_ARG(QVariant, result));
    require(!result.toBool() && logs == 2, "empty refreshed list prevents emission without sending");
    QVariant queried;
    require(QMetaObject::invokeMethod(page.get(), "query", Q_RETURN_ARG(QVariant, queried),
        Q_ARG(QVariant, QVariant::fromValue(QVariantList{static_cast<int>(Register::Frequency)})),
        Q_ARG(QVariant, QVariant(true))), "manual query entry callable");
    require(!queried.toBool() && logs == 3, "manual query only attempts seed address");
    const auto beforeStop = page->property("seedReadings").value<QJSValue>().toVariant().toMap();
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::StopReply, 0, {}});
    require(logs == 4 && page->property("seedReadings").value<QJSValue>().toVariant().toMap() == beforeStop,
            "off acknowledgement queries actual state without clearing or inventing displayed values");
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0, {{Register::Emission, EmissionState::Off}}});
    require(page->property("seedReadings").value<QJSValue>().toVariant().toMap().value("24").toInt() == 0,
            "off status follows the device report");
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0, {{Register::Width2, PulseWidthOption{true, 420}}}});
    // 真实 QML 官邸和实际 Handle/解析器，仅替换串口 C API，不访问实物。
    FakeSerialPort::reset();
    require(device->open(QStringLiteral("controlled")), "page backend starts controlled serial");
    require(until([&] { return FakeSerialPort::writes().size() >= 2; }), "connection triggers initial and immediate periodic queries");
    auto writes = FakeSerialPort::writes();
    QByteArray initial, periodic;
    for (const auto *name : {"widthRegisters", "identityRegisters"})
        for (const auto &address : page->property(name).value<QJSValue>().toVariant().toList()) {
            char bytes[4]; qToLittleEndian<quint32>(address.toUInt(), bytes); initial.append(bytes, 4);
        }
    for (const auto *name : {"measurementRegisters", "statusRegisters"})
        for (const auto &address : page->property(name).value<QJSValue>().toVariant().toList()) {
            char bytes[4]; qToLittleEndian<quint32>(address.toUInt(), bytes); periodic.append(bytes, 4);
        }
    require(writes[0] == encode(0, 0x31, 2, initial) && writes[1] == encode(0, 0x31, 2, periodic),
            "width/identity query precedes immediate state query, both seed only");
    FakeSerialPort::enqueue(encode(0, 0xb1, 0, QByteArray::fromHex("96000000800000c89700000080000190")));
    require(until([&] { return widthSelector->property("count").toInt() == 3; }), "automatic query reply populates actual pulse choices");
    require(timer->property("running").toBool(), "connection starts timer explicitly");
    const auto reads = FakeSerialPort::reads();
    FakeSerialPort::enqueue(encode(0, 0xb1, 0, QByteArray::fromHex("9900000080000258")));
    for (int i = 0; i < 3000 && FakeSerialPort::reads() <= reads + 1; ++i) QThread::msleep(1);
    require(FakeSerialPort::reads() > reads + 1, "late reply queued before close without pumping GUI");
    const auto beforeClose = FakeSerialPort::writes().size();
    device->close();
    require(FakeSerialPort::writes().size() == beforeClose + 1
            && FakeSerialPort::writes().back() == encode(0, 0x36, 0, {}) && FakeSerialPort::livePorts() == 0,
            "actual QML disconnect sends one off before serial release");
    require(!timer->property("running").toBool(), "disconnect stops timer synchronously");
    require(page->property("seedReadings").value<QJSValue>().toVariant().toMap().isEmpty()
            && widthSelector->property("count").toInt() == 0
            && powerReadback->property("text").toString().endsWith(QStringLiteral("—")),
            "disconnect restores unknown readbacks and clears old width options");
    require(powerInput->property("value").toInt() == 37 && frequencySelector->property("currentValue").toInt() == 8,
            "disconnect leaves next-command inputs unchanged");
    require(until([&] { return !page->property("isOpen").toBool(); }), "finished refreshes real open property");
    require(widthSelector->property("count").toInt() == 0, "queued reply cannot refill choices after disconnect");
    const auto closedWrites = FakeSerialPort::writes().size();
    device->close();
    require(FakeSerialPort::writes().size() == closedWrites, "repeated close sends no duplicate off");
    // 先不泵 GUI，等真正收包后立即关闭：迟到 true 不得重启定时查询。
    require(device->open(QStringLiteral("controlled")), "start before delayed true");
    for (int i = 0; i < 3000 && !device->isOpen(); ++i) QThread::msleep(1);
    require(device->isOpen(), "worker connected without pumping GUI");
    device->close();
    QCoreApplication::processEvents();
    require(!timer->property("running").toBool(), "delayed true does not restart polling after stop");
    const auto idleWrites = FakeSerialPort::writes().size();
    QTimer::singleShot(8000, &app, &QCoreApplication::quit);
    const auto code = app.exec();
    require(!device->isOpen() && FakeSerialPort::writes().size() == idleWrites,
            "idle eight-second UI has no spontaneous connection or queries");
    std::cout << checks << " QML checks passed\n";
    return code;
}
