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
    function alarms() {
        const bits = readings[LaserProtocol.Alarms]
        if (bits === undefined) return "—"
        if (bits === 0) return qsTr("无报警")
        const names = ["光频率1", "光功率1", "光频率2", "光功率2", "Seed过流", "PA1过流", "PA2过流", "PA3过流",
            "温度点1", "温度点2", "温度点3", "TEC温度1", "TEC温度2", "TEC温度3", "TEC温度4", "供电电压"]
        return names.filter((name, bit) => (bits & (1 << bit)) !== 0).join(" · ") + "  [0x" + bits.toString(16) + "]"
    }
    function identity(addresses) {
        if (addresses.every(address => readings[address] === undefined)) return "—"
        return addresses.map(address => readings[address] === undefined ? "…" : readings[address]).join("").replace(/\0/g, "")
    }
    component StatusText: Label {
        font.pixelSize: 14
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
    }
    GridLayout {
        columns: 4
        uniformCellWidths: true
        columnSpacing: 8
        rowSpacing: 8
        Layout.fillWidth: true
        StatusText { text: qsTr("出光"); font.pixelSize: 18 }
        StatusText {
            objectName: "emissionReadback"
            text: root.readings[LaserProtocol.Emission] === undefined ? "—" : root.readings[LaserProtocol.Emission] ? qsTr("出光") : qsTr("关光")
            font.pixelSize: 18
            font.bold: true
        }
        StatusText { text: qsTr("模式"); font.pixelSize: 18 }
        StatusText {
            text: root.readings[LaserProtocol.ControlMode] === undefined ? "—" : (root.readings[LaserProtocol.ControlMode] & 0x400) ? qsTr("内控") : qsTr("外控")
            font.pixelSize: 18
            font.bold: true
        }
    }
    GridLayout {
        columns: 4
        uniformCellWidths: true
        columnSpacing: 8
        rowSpacing: 6
        Layout.fillWidth: true
        Layout.topMargin: 8
        StatusText { text: qsTr("脉宽") }
        StatusText { text: qsTr("功率") }
        StatusText { text: qsTr("频率") }
        StatusText { text: qsTr("报警") }
        StatusText {
            objectName: "widthReadback"
            text: {
                const option = root.widthOptions.find(item => item.code === root.readings[LaserProtocol.WidthIndex])
                return option ? option.value + " ns" : "—"
            }
        }
        // 实机功率读口为 0..255；写口仍为百分比，只在显示边界换算。
        StatusText {
            objectName: "powerReadback"
            text: root.readings[LaserProtocol.Power] === undefined ? "—"
                : (root.readings[LaserProtocol.Power] * 100 / 255).toFixed(1) + " %"
        }
        StatusText { objectName: "frequencyReadback"; text: root.value(LaserProtocol.Frequency, " kHz") }
        StatusText {
            text: root.alarms()
            color: root.readings[LaserProtocol.Alarms] ? "#f2a56c" : palette.windowText
        }
    }
    GridLayout {
        columns: 2
        columnSpacing: 12
        rowSpacing: 6
        Layout.fillWidth: true
        Layout.topMargin: 8
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
