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
        const width = widthOptions[widthSelector.currentIndex]
        const sent = internals.device.readRegisters(LaserProtocol.Seed, width ? width.code : 0, registers)
        if (sent && manual) logMessage(qsTr("种子：查询已发送"))
        return sent
    }
    function sendEmission() {
        const width = widthOptions[widthSelector.currentIndex]
        if (!width) {
            logMessage(qsTr("请先查询脉宽列表，并选择设备支持的脉宽"))
            return false
        }
        const sent = internals.device.startEmission(LaserProtocol.Seed, width.code, frequencySelector.currentValue, powerInput.value, width.value)
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
        const previousWidth = widthOptions[widthSelector.currentIndex]
        const previousCode = previousWidth ? previousWidth.code : -1
        options.sort((a, b) => a.code - b.code)
        widthOptions = options
        const position = options.findIndex(option => option.code === previousCode)
        widthSelector.currentIndex = position >= 0 ? position : (options.length > 0 ? 0 : -1)
    }
    onIsOpenChanged: {
        widthOptions = []
        widthSelector.currentIndex = -1
        seedReadings = ({})
        if (isOpen) {
            seedChart.clear()
            query(identityRegisters.concat(statusRegisters, measurementRegisters), false)
            query(widthRegisters, false)
        }
    }
    Timer {
        objectName: "statusTimer"
        interval: 500
        repeat: true
        running: root.isOpen
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
        }
    }
    mainPages: [Page {
        title: qsTr("种子模块")
        contentItem: LaserTelemetryChart {
            id: seedChart
            configuration: ({temperature: {minimum: 0, maximum: 80}, current: {minimum: 0, maximum: 5}, voltage: {minimum: 15, maximum: 30}, windowSeconds: 120, capacity: 7200})
        }
    }]
    sections: [connection, seedStatus, control]
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
        FieldRow {
            label: qsTr("脉宽（ns）")
            ComboBox {
                id: widthSelector
                objectName: "pulseWidthSelector"
                Layout.fillWidth: true
                model: root.widthOptions.map(option => String(option.value))
                editable: false
            }
            Button {
                objectName: "refreshPulseWidths"
                text: qsTr("更新")
                onClicked: root.query(root.widthRegisters, true)
            }
        }
        FieldRow {
            label: qsTr("频率（kHz）")
            ComboBox {
                id: frequencySelector
                objectName: "frequencySelector"
                Layout.fillWidth: true
                model: [5, 8, 10]
                currentIndex: 2
                editable: false
            }
        }
        FieldRow {
            label: qsTr("功率（%）")
            IntField {
                id: powerInput
                objectName: "powerInput"
                Layout.fillWidth: true
                minimum: 0
                maximum: 100
                value: 1
                onCommitted: v => value = v
            }
        }
        RowLayout {
            Button { text: qsTr("设置参数并出光"); onClicked: root.sendEmission() }
            Button {
                text: qsTr("关光")
                onClicked: {
                    const width = root.widthOptions[widthSelector.currentIndex]
                    if (internals.device.stopEmission(LaserProtocol.Seed, width ? width.code : 0))
                        root.logMessage(qsTr("关光命令已发送"))
                }
            }
        }
        RowLayout {
            Button { text: qsTr("切换为内控"); onClicked: { if (internals.device.setControlMode(true)) root.logMessage(qsTr("切换内控命令已发送")) } }
            Button { text: qsTr("切换为外控"); onClicked: { if (internals.device.setControlMode(false)) root.logMessage(qsTr("切换外控命令已发送")) } }
        }
    }
}
