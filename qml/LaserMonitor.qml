pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Lidar.Charts

ColumnLayout {
    id: root
    required property var readings
    required property bool connected
    spacing: 8
    function value(address, suffix) {
        const v = readings[address]
        return v === undefined ? "—" : String(v) + suffix
    }
    function alarms() {
        const bits = readings[LaserProtocol.Alarms]
        if (bits === undefined) return qsTr("尚未回报")
        if (bits === 0) return qsTr("无报警")
        const names = ["光频率1", "光功率1", "光频率2", "光功率2", "Seed过流", "PA1过流", "PA2过流", "PA3过流",
            "温度点1", "温度点2", "温度点3", "TEC温度1", "TEC温度2", "TEC温度3", "TEC温度4", "供电电压"]
        return names.filter((name, bit) => (bits & (1 << bit)) !== 0).join(" · ") + "  [0x" + bits.toString(16) + "]"
    }
    function identity(addresses) {
        return addresses.map(address => readings[address] === undefined ? "…" : readings[address]).join("").replace(/\0/g, "")
    }
    function append(at, values) { chart.append(at, values) }
    function clear() { chart.clear() }

    GridLayout {
        columns: 3
        columnSpacing: 32
        rowSpacing: 8
        Layout.fillWidth: true
        Layout.margins: 12
        Label { text: qsTr("出光：") + (root.readings[LaserProtocol.Emission] === undefined ? "—" : root.readings[LaserProtocol.Emission] ? qsTr("出光") : qsTr("关光")); font.bold: true }
        Label { text: qsTr("频率：") + root.value(LaserProtocol.Frequency, " kHz") }
        Label { text: qsTr("功率：") + root.value(LaserProtocol.Power, " %") }
        Label { text: qsTr("模式：") + (root.readings[LaserProtocol.ControlMode] === undefined ? "—" : (root.readings[LaserProtocol.ControlMode] & 0x400) ? qsTr("内控") : qsTr("外控")) }
        Label { text: qsTr("脉宽号：") + root.value(LaserProtocol.WidthIndex, "") }
        Label { text: root.connected ? qsTr("设备回报值") : qsTr("未连接 · 历史回报"); opacity: 0.6 }
        Label { Layout.columnSpan: 3; Layout.fillWidth: true; wrapMode: Text.Wrap; text: qsTr("报警：") + root.alarms(); color: root.readings[LaserProtocol.Alarms] ? "#f2a56c" : palette.windowText }
        Label { Layout.columnSpan: 3; text: qsTr("型号：") + root.identity([LaserProtocol.Model1, LaserProtocol.Model2, LaserProtocol.Model3]); opacity: 0.7 }
        Label { Layout.columnSpan: 3; text: qsTr("SN：") + root.identity([LaserProtocol.Serial1, LaserProtocol.Serial2, LaserProtocol.Serial3, LaserProtocol.Serial4]); opacity: 0.7 }
    }
    LaserTelemetryChart {
        id: chart
        Layout.fillWidth: true
        Layout.fillHeight: true
        configuration: ({temperature: {minimum: 0, maximum: 80}, current: {minimum: 0, maximum: 20}, voltage: {minimum: 0, maximum: 60}, windowSeconds: 120, capacity: 7200})
    }
}
