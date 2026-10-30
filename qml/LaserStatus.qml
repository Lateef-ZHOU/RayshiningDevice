pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Lidar.UI

Section {
    id: root
    required property var readings
    property var widthOptions: []
    function value(address, suffix) {
        const v = readings[address]
        return v === undefined ? "—" : String(v) + suffix
    }
    function identity(addresses) {
        if (addresses.every(address => readings[address] === undefined)) return "—"
        return addresses.map(address => readings[address] === undefined ? "…" : readings[address]).join("").replace(/\0/g, "")
    }
    // 与云台读数统一：浅色左对齐标题，下方正常字重的等宽数值。
    component Readout: Label {
        color: palette.placeholderText
        Layout.fillWidth: true
        Layout.preferredWidth: 64
        elide: Text.ElideRight
    }
    component ReadoutValue: Label {
        font.family: "Consolas"
        font.pointSize: 13
        Layout.fillWidth: true
        Layout.preferredWidth: 64
    }
    GridLayout {
        columns: 5
        uniformCellWidths: true
        columnSpacing: 10
        rowSpacing: 2
        Layout.fillWidth: true
        Readout { text: qsTr("激光输出") }
        Readout { text: qsTr("模式") }
        Readout { text: qsTr("脉宽") }
        Readout { text: qsTr("功率") }
        Readout { text: qsTr("频率") }
        ReadoutValue {
            objectName: "emissionReadback"
            text: root.readings[LaserProtocol.Emission] === undefined ? "—" : root.readings[LaserProtocol.Emission] ? qsTr("出光") : qsTr("关光")
        }
        ReadoutValue {
            text: root.readings[LaserProtocol.ControlMode] === undefined ? "—" : (root.readings[LaserProtocol.ControlMode] & 0x400) ? qsTr("内控") : qsTr("外控")
        }
        ReadoutValue {
            objectName: "widthReadback"
            text: {
                const option = root.widthOptions.find(item => item.code === root.readings[LaserProtocol.WidthIndex])
                return option ? option.value + " ns" : "—"
            }
        }
        // 实机功率读口为 0..255；写口仍为百分比，只在显示边界换算。
        ReadoutValue {
            objectName: "powerReadback"
            text: root.readings[LaserProtocol.Power] === undefined ? "—"
                : (root.readings[LaserProtocol.Power] * 100 / 255).toFixed(1) + " %"
        }
        ReadoutValue { objectName: "frequencyReadback"; text: root.value(LaserProtocol.Frequency, " kHz") }
    }
    GridLayout {
        columns: 2
        columnSpacing: 12
        rowSpacing: 6
        Layout.fillWidth: true
        Layout.topMargin: 20
        Label { text: qsTr("型号"); font.pixelSize: 12; opacity: 0.7 }
        Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            wrapMode: Text.Wrap
            text: root.identity([LaserProtocol.Model1, LaserProtocol.Model2, LaserProtocol.Model3])
            font.pixelSize: 12
            opacity: 0.7
        }
        Label { text: "SN"; font.pixelSize: 12; opacity: 0.7 }
        Label {
            objectName: "serialReadback"
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            wrapMode: Text.Wrap
            text: root.identity([LaserProtocol.Serial1, LaserProtocol.Serial2, LaserProtocol.Serial3, LaserProtocol.Serial4])
            font.pixelSize: 12
            opacity: 0.7
        }
    }
}
