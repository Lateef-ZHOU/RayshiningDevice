pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Lidar.UI
import Lidar.Charts

ModulePage {
    id: root
    objectName: "LaserModule"
    readonly property string kind: "device"
    readonly property string name: internals.device.displayName
    readonly property string subtitle: internals.device.subtitle
    readonly property bool isOpen: internals.device.isOpen
    readonly property bool isLocked: internals.device.isLocked
    readonly property string holder: internals.device.holder
    // 当前设备只使用种子地址；表单与回报分开，发令成功不预写设备状态。
    readonly property string portName: portSelector.currentText
    property var widthOptions: []
    readonly property var selectedWidth: widthOptions[widthSelector.currentIndex]
    // 未取得可选列表时，查询和关光使用协议编号 0；出光必须先有有效选择。
    readonly property int widthIndex: selectedWidth ? selectedWidth.code : 0
    property int frequencyKhz: 10
    property int powerPercent: 1
    property bool polling: true
    property int pollSeconds: 1
    property bool internalMode: true
    property int opticalChannel: 0
    property var seedReadings: ({})
    readonly property var measurementRegisters: [LaserProtocol.LaserTemperature, LaserProtocol.McuTemperature,
        LaserProtocol.PA1Current, LaserProtocol.PA2Current, LaserProtocol.PA3Current, LaserProtocol.Voltage]
    readonly property var statusRegisters: [LaserProtocol.Emission, LaserProtocol.Frequency, LaserProtocol.Power,
        LaserProtocol.WidthIndex, LaserProtocol.ControlMode, LaserProtocol.Alarms]
    readonly property var identityRegisters: [LaserProtocol.Model1, LaserProtocol.Model2, LaserProtocol.Model3,
        LaserProtocol.Serial1, LaserProtocol.Serial2, LaserProtocol.Serial3, LaserProtocol.Serial4]
    readonly property var widthRegisters: [LaserProtocol.Width0, LaserProtocol.Width1, LaserProtocol.Width2,
        LaserProtocol.Width3, LaserProtocol.Width4, LaserProtocol.Width5]
    QtObject {
        id: internals
        readonly property LaserDevice device: root.LaserDevice
        readonly property var ports: device.ports()
    }
    function query(registers, manual) {
        const sent = internals.device.readRegisters(LaserProtocol.Seed, widthIndex, registers)
        if (sent && manual) logMessage(qsTr("种子：查询已发送"))
        return sent
    }
    function sendEmission() {
        if (!selectedWidth) {
            logMessage(qsTr("请先查询脉宽列表，并选择设备支持的脉宽"))
            return false
        }
        const sent = internals.device.startEmission(LaserProtocol.Seed, widthIndex, frequencyKhz, powerPercent, selectedWidth.value)
        if (sent) logMessage(qsTr("种子：设置参数并出光命令已发送"))
        return sent
    }
    function acceptReadings(values) {
        const readings = ({})
        const options = widthOptions.slice()
        let optionsChanged = false
        for (const [address, value] of Object.entries(values)) {
            const code = Number(address) - LaserProtocol.Width0
            if (code < 0 || code >= widthRegisters.length) {
                readings[address] = value
                continue
            }
            const position = options.findIndex(option => option.code === code)
            if (value.selectable && value.value > 0) {
                if (position < 0) {
                    options.push({code: code, value: value.value})
                    optionsChanged = true
                } else if (options[position].value !== value.value) {
                    options[position] = {code: code, value: value.value}
                    optionsChanged = true
                }
            } else if (position >= 0) {
                options.splice(position, 1)
                optionsChanged = true
            }
        }
        if (Object.keys(readings).length > 0)
            seedReadings = Object.assign({}, seedReadings, readings)
        if (!optionsChanged) return
        const previousCode = selectedWidth ? selectedWidth.code : -1
        options.sort((a, b) => a.code - b.code)
        widthOptions = options
        const position = options.findIndex(option => option.code === previousCode)
        widthSelector.currentIndex = position >= 0 ? position : (options.length > 0 ? 0 : -1)
    }
    onIsOpenChanged: {
        widthOptions = []
        widthSelector.currentIndex = -1
        if (isOpen) {
            seedReadings = ({})
            seedChart.clear()
            query(identityRegisters.concat(statusRegisters, measurementRegisters), false)
        }
    }
    Timer {
        interval: root.pollSeconds * 1000
        repeat: true
        running: root.polling && root.isOpen
        // 每轮只读取种子状态和测量值；读取回包不会触发响应式查询。
        onTriggered: root.query(root.measurementRegisters.concat(root.statusRegisters), false)
    }
    Connections {
        target: internals.device.logSource
        function onLogMessage(text): void { root.logMessage(text) }
    }
    Connections {
        target: internals.device
        function onTelemetryReady(at, module, values): void {
            if (module === LaserProtocol.Seed) seedChart.append(at, values)
        }
        function onReplyReady(at, reply): void {
            if (reply.module !== LaserProtocol.Seed) return
            if (reply.command === LaserProtocol.ReadReply) {
                root.acceptReadings(reply.values)
                return
            }
            // 无参数的出光/关光/模式确认仅说明命令被接受；查询实际值，不保存待确认命令。
            if (reply.command === LaserProtocol.EmitReply || reply.command === LaserProtocol.StopReply)
                internals.device.readRegisters(LaserProtocol.Seed, reply.widthIndex, root.statusRegisters)
            else if (reply.command === LaserProtocol.ModeReply)
                internals.device.readRegisters(LaserProtocol.Seed, 0, [LaserProtocol.ControlMode])
            else if (reply.command === LaserProtocol.ChannelReply)
                root.logMessage(qsTr("光开关切换已确认；协议未提供当前通道读口"))
        }
    }
    mainPages: [Page {
        title: qsTr("种子模块")
        contentItem: LaserTelemetryChart {
            id: seedChart
            configuration: ({temperature: {minimum: 0, maximum: 80}, current: {minimum: 0, maximum: 5}, voltage: {minimum: 15, maximum: 30}, windowSeconds: 120, capacity: 7200})
        }
    }]
    sections: [connection, seedStatus, control, queries]
    readonly property Section seedStatus: LaserStatus {
        title: qsTr("种子模块状态")
        readings: root.seedReadings
        connected: root.isOpen
    }
    readonly property Section connection: Section {
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
            Button { text: qsTr("连接"); onClicked: { if (internals.device.open(root.portName)) root.logMessage(qsTr("连接请求已提交")) } }
            Button { text: qsTr("断开"); onClicked: internals.device.close() }
        }
    }
    readonly property Section control: Section {
        title: qsTr("激光控制")
        RowLayout {
            Label { text: qsTr("脉宽") }
            ComboBox {
                id: widthSelector
                objectName: "pulseWidthSelector"
                Layout.fillWidth: true
                model: root.widthOptions.map(option => option.value + " ns")
                editable: false
                enabled: count > 0
                displayText: currentIndex >= 0 ? currentText : qsTr("暂无可选脉宽")
            }
            Button {
                objectName: "refreshPulseWidths"
                text: qsTr("更新")
                enabled: root.isOpen
                onClicked: root.query(root.widthRegisters, true)
            }
        }
        RowLayout {
            Label { text: qsTr("频率 (kHz)") }
            IntField { Layout.fillWidth: true; minimum: 1; maximum: 2147483647; value: root.frequencyKhz; onCommitted: v => root.frequencyKhz = v }
        }
        RowLayout {
            Label { text: qsTr("功率 (%)") }
            SpinBox { from: 0; to: 100; editable: true; value: root.powerPercent; onValueModified: root.powerPercent = value }
        }
        RowLayout {
            Button { text: qsTr("设置参数并出光"); enabled: root.isOpen && !!root.selectedWidth; onClicked: root.sendEmission() }
            Button { text: qsTr("关光"); enabled: root.isOpen; onClicked: { if (internals.device.stopEmission(LaserProtocol.Seed, root.widthIndex)) root.logMessage(qsTr("关光命令已发送")) } }
        }
        Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: qsTr("频率须符合设备允许范围。"); opacity: 0.6 }
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
            Button { text: qsTr("型号与序列号"); enabled: root.isOpen; onClicked: root.query(root.identityRegisters, true) }
        }
        CheckBox { text: qsTr("定时查询种子模块"); checked: root.polling; onClicked: root.polling = checked }
        RowLayout {
            Label { text: qsTr("间隔 (s)") }
            SpinBox { from: 1; to: 60; value: root.pollSeconds; onValueModified: root.pollSeconds = value }
        }
    }
}
