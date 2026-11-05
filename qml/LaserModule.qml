pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Lidar.UI

LaserPage {
    id: root
    objectName: "LaserModule"
    readonly property string portName: portSelector.currentText
    backend: internals.device
    QtObject {
        id: internals
        readonly property LaserDevice device: root.LaserDevice
        readonly property var ports: device.ports()
    }
    connection: Section {
        title: qsTr("设备连接")
        FieldRow {
            label: qsTr("串口")
            ChoiceField {
                id: portSelector
                Layout.fillWidth: true
                model: internals.ports
                onCommitted: i => value = i
            }
        }
        DoubleButtonRow {
            firstAction: Action { text: qsTr("连接"); onTriggered: { if (internals.device.open(root.portName)) root.logMessage(qsTr("连接请求已提交")) } }
            secondAction: Action { text: qsTr("断开"); onTriggered: internals.device.close() }
        }
    }
}
