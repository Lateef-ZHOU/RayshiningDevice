#include <cstdlib>
#include <iostream>
#include <memory>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlListReference>
#include <QQuickWindow>
#include <QTimer>
#include <QDebug>
#include <QtQml/QQmlExtensionPlugin>
#include <Rayshining/LaserDevice.h>
Q_IMPORT_QML_PLUGIN(LidarUIPlugin)
Q_IMPORT_QML_PLUGIN(LidarChartsPlugin)
Q_IMPORT_QML_PLUGIN(LidarRayshiningPlugin)
namespace {
int checks = 0;
void require(bool v, const char *text) { ++checks; if (!v) { qCritical() << text; std::exit(1); } }
}
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"QML(import QtQuick
import Lidar.Rayshining
LaserModule { width: 1280; height: 900; polling: false }
)QML", QUrl());
    std::unique_ptr<QObject> page(component.create());
    if (!page) { qCritical() << component.errors(); return 1; }
    auto *device = qobject_cast<Rayshining::LaserDevice *>(qmlAttachedPropertiesObject<Rayshining::LaserDevice>(page.get()));
    require(device && device->parent() == page.get(), "page owns exact backend");
    require(qmlAttachedPropertiesObject<Rayshining::LaserDevice>(page.get()) == device, "unique attached backend");
    require(!device->isOpen(), "loading UI never connects hardware");
    const QQmlListReference charts(page.get(), "mainPages");
    require(charts.isValid() && charts.count() == 1, "only seed chart page assembled");
    int logs = 0;
    QObject::connect(qobject_cast<Rayshining::LaserHandle *>(device->logSourceObject()), &Rayshining::LaserHandle::logMessage,
        &app, [&](const QString &) { ++logs; });
    using namespace Rayshining;
    Q_EMIT device->replyReady({}, Reply{Module::Seed, Command::ReadReply, 0, {{Register::Frequency, quint32(12)}, {Register::Emission, EmissionState::On}}});
    auto values = page->property("seedReadings").value<QJSValue>().toVariant().toMap();
    require(values.value("128").toInt() == 12 && values.value("24").toInt() == 1, "seed replies update seed status");
    require(logs == 0, "read reply never triggers another query");
    Q_EMIT device->replyReady({}, Reply{Module::Amplifier, Command::ReadReply, 0, {{Register::Frequency, quint32(30)}}});
    require(page->property("seedReadings").value<QJSValue>().toVariant().toMap() == values,
        "amplifier replies cannot overwrite seed status");
    Q_EMIT device->replyReady({}, Reply{Module::Amplifier, Command::EmitReply, 0, {}});
    Q_EMIT device->replyReady({}, Reply{Module::Amplifier, Command::StopReply, 0, {}});
    require(logs == 0, "amplifier acknowledgements never trigger follow-up commands");
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
    QTimer::singleShot(8000, &app, &QCoreApplication::quit);
    const auto code = app.exec();
    require(!device->isOpen() && logs == 2, "idle eight-second UI has no spontaneous connection or queries");
    std::cout << checks << " QML checks passed\n";
    return code;
}
