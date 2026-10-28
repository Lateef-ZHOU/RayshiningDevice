pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Lidar.UI

ModulePage {
    id: root
    objectName: "LaserModule"
    readonly property string kind: "device"
    readonly property string name: internals.device.displayName
    readonly property string subtitle: internals.device.subtitle
    readonly property bool isOpen: internals.device.isOpen
    readonly property bool isLocked: internals.device.isLocked
    readonly property string holder: internals.device.holder
    // 表单与回报分开；发令成功不预写设备状态。模块选择只控制后续发送。
    property int targetModule: LaserProtocol.Seed
    property alias portName: portSelector.editText
    property int widthIndex: 0
    property int frequencyKhz: 10
    property int powerPercent: 100
    property bool polling: true
    property int pollSeconds: 1
    property bool internalMode: true
    property int opticalChannel: 0
    property var seedReadings: ({})
    property var amplifierReadings: ({})
    readonly property var selectedReadings: targetModule === LaserProtocol.Seed ? seedReadings : amplifierReadings
    readonly property var selectedWidth: selectedReadings[LaserProtocol.Width0 + widthIndex]
    readonly property var measurementRegisters: [LaserProtocol.LaserTemperature, LaserProtocol.McuTemperature,
        LaserProtocol.PA1Current, LaserProtocol.PA2Current, LaserProtocol.PA3Current, LaserProtocol.Voltage]
    readonly property var statusRegisters: [LaserProtocol.Emission, LaserProtocol.Frequency, LaserProtocol.Power,
        LaserProtocol.WidthIndex, LaserProtocol.ControlMode, LaserProtocol.Alarms]
    readonly property var identityRegisters: [LaserProtocol.Model1, LaserProtocol.Model2, LaserProtocol.Model3,
        LaserProtocol.Serial1, LaserProtocol.Serial2, LaserProtocol.Serial3, LaserProtocol.Serial4,
        LaserProtocol.Width0, LaserProtocol.Width1, LaserProtocol.Width2, LaserProtocol.Width3, LaserProtocol.Width4, LaserProtocol.Width5]
    QtObject {
        id: internals
        readonly property LaserDevice device: root.LaserDevice
        property var ports: device.ports()
    }
    function query(registers, manual) {
        const sent = internals.device.readRegisters(targetModule, widthIndex, registers)
        if (sent && manual) logMessage(qsTr("%1：查询已发送").arg(targetModule === LaserProtocol.Seed ? "种子" : "放大"))
        return sent
    }
    function sendEmission() {
        if (!selectedWidth || !selectedWidth.selectable) {
            logMessage(qsTr("请先查询脉宽列表，并选择设备支持的脉宽"))
            return false
        }
        const sent = internals.device.startEmission(targetModule, widthIndex, frequencyKhz, powerPercent, selectedWidth.value)
        if (sent) logMessage(qsTr("%1：设置参数并出光命令已发送").arg(targetModule === LaserProtocol.Seed ? "种子" : "放大"))
        return sent
    }
    onIsOpenChanged: {
        if (isOpen) {
            seedReadings = ({}); amplifierReadings = ({})
            seedMonitor.clear(); amplifierMonitor.clear()
            query(identityRegisters.concat(statusRegisters, measurementRegisters), false)
        }
    }
    Timer {
        interval: root.pollSeconds * 1000
        repeat: true
        running: root.polling && root.isOpen
        // 定时只发读取命令；读取回包不会触发响应式查询。
        onTriggered: root.query(root.measurementRegisters.concat(root.statusRegisters), false)
    }
    Connections {
        target: internals.device.logSource
        function onLogMessage(text): void { root.logMessage(text) }
    }
    Connections {
        target: internals.device
        function onTelemetryReady(at, module, values): void {
            if (module === LaserProtocol.Seed) seedMonitor.append(at, values)
            else amplifierMonitor.append(at, values)
        }
        function onReplyReady(at, reply): void {
            if (reply.command === LaserProtocol.ReadReply) {
                if (reply.module === LaserProtocol.Seed) root.seedReadings = Object.assign({}, root.seedReadings, reply.values)
                else root.amplifierReadings = Object.assign({}, root.amplifierReadings, reply.values)
                return
            }
            // 无参数的出光/关光/模式确认仅说明命令被接受；查询实际值，不保存待确认命令。
            if (reply.command === LaserProtocol.EmitReply || reply.command === LaserProtocol.StopReply)
                internals.device.readRegisters(reply.module, reply.widthIndex, root.statusRegisters)
            else if (reply.command === LaserProtocol.ModeReply)
                internals.device.readRegisters(LaserProtocol.Seed, 0, [LaserProtocol.ControlMode])
            else if (reply.command === LaserProtocol.ChannelReply)
                root.logMessage(qsTr("光开关切换已确认；协议未提供当前通道读口"))
        }
    }
    mainPages: [Page {
        title: qsTr("种子模块")
        contentItem: LaserMonitor { id: seedMonitor; readings: root.seedReadings; connected: root.isOpen }
    }, Page {
        title: qsTr("放大模块")
        contentItem: LaserMonitor { id: amplifierMonitor; readings: root.amplifierReadings; connected: root.isOpen }
    }]
    sections: [connection, control, queries]
    readonly property Section connection: Section {
        title: qsTr("设备连接")
        ComboBox {
            id: portSelector
            Layout.fillWidth: true
            model: internals.ports
            editable: true
        }
        Label { text: "115200 · 8N1 · 无流控"; opacity: 0.6 }
        RowLayout {
            Button { text: qsTr("刷新"); onClicked: internals.ports = internals.device.ports() }
            Button { text: qsTr("连接"); onClicked: { if (internals.device.open(root.portName)) root.logMessage(qsTr("连接请求已提交")) } }
            Button { text: qsTr("断开"); onClicked: internals.device.close() }
        }
    }
    readonly property Section control: Section {
        title: qsTr("激光控制")
        RowLayout {
            RadioButton { text: qsTr("种子模块"); checked: root.targetModule === LaserProtocol.Seed; onClicked: root.targetModule = LaserProtocol.Seed }
            RadioButton { text: qsTr("放大模块"); checked: root.targetModule === LaserProtocol.Amplifier; onClicked: root.targetModule = LaserProtocol.Amplifier }
        }
        RowLayout {
            Label { text: qsTr("脉宽号") }
            SpinBox { from: 0; to: 5; value: root.widthIndex; onValueModified: root.widthIndex = value }
            Label { text: root.selectedWidth ? (root.selectedWidth.selectable ? root.selectedWidth.value + " ns" : qsTr("不可选")) : qsTr("未查询") }
        }
        RowLayout {
            Label { text: qsTr("频率 (kHz)") }
            IntField { Layout.fillWidth: true; minimum: 1; maximum: 2147483647; value: root.frequencyKhz; onCommitted: v => root.frequencyKhz = v }
        }
        RowLayout {
            Label { text: qsTr("功率 (%)") }
            SpinBox { from: 0; to: 100; value: root.targetModule === LaserProtocol.Seed ? 100 : root.powerPercent; enabled: root.targetModule !== LaserProtocol.Seed; onValueModified: root.powerPercent = value }
        }
        RowLayout {
            Button { text: qsTr("设置参数并出光"); enabled: root.isOpen; onClicked: root.sendEmission() }
            Button { text: qsTr("关光"); enabled: root.isOpen; onClicked: { if (internals.device.stopEmission(root.targetModule, root.widthIndex)) root.logMessage(qsTr("关光命令已发送")) } }
        }
        Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: qsTr("频率须符合设备允许范围；种子功率固定 100%。下列控制按协议固定发往地址 0。"); opacity: 0.6 }
        RowLayout {
            ComboBox { model: [qsTr("内控"), qsTr("外控")]; onActivated: root.internalMode = currentIndex === 0 }
            Button { text: qsTr("切换模式"); enabled: root.isOpen; onClicked: { if (internals.device.setControlMode(root.internalMode)) root.logMessage(qsTr("模式切换命令已发送")) } }
        }
        RowLayout {
            Label { text: qsTr("光开关通道") }
            SpinBox { from: 0; to: 3; value: root.opticalChannel; onValueModified: root.opticalChannel = value }
            Button { text: qsTr("切换"); enabled: root.isOpen; onClicked: { if (internals.device.setOpticalChannel(root.opticalChannel)) root.logMessage(qsTr("光开关切换命令已发送")) } }
        }
    }
    readonly property Section queries: Section {
        title: qsTr("状态查询")
        RowLayout {
            Button { text: qsTr("状态与测量"); enabled: root.isOpen; onClicked: root.query(root.statusRegisters.concat(root.measurementRegisters), true) }
            Button { text: qsTr("型号与脉宽"); enabled: root.isOpen; onClicked: root.query(root.identityRegisters, true) }
        }
        CheckBox { text: qsTr("定时查询当前所选模块"); checked: root.polling; onClicked: root.polling = checked }
        RowLayout {
            Label { text: qsTr("间隔 (s)") }
            SpinBox { from: 1; to: 60; value: root.pollSeconds; onValueModified: root.pollSeconds = value }
        }
    }
}
