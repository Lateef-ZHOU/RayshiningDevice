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
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            opacity: 0.6
            text: internals.device.connectionDescription
        }
        RowLayout {
            Layout.fillWidth: true
            uniformCellSizes: true
            spacing: 8
            Button { Layout.fillWidth: true; text: qsTr("连接"); onClicked: { if (internals.device.open(root.portName)) root.logMessage(qsTr("连接请求已提交")) } }
            Button { Layout.fillWidth: true; text: qsTr("断开"); onClicked: internals.device.close() }
        }
    }
}
