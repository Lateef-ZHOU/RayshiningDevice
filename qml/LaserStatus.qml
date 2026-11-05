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
        const v = readings[address];
        return v === undefined ? "—" : String(v) + suffix;
    }
    function identity(addresses) {
        if (addresses.every(address => readings[address] === undefined))
            return "—";
        return addresses.map(address => readings[address] === undefined ? "…" : readings[address]).join("").replace(/\0/g, "");
    }
    ReadoutRow {
        Readout {
            caption: qsTr("激光输出")
            objectName: "emissionReadback"
            text: root.readings[LaserProtocol.Emission] === undefined ? "—" : root.readings[LaserProtocol.Emission] ? qsTr("出光") : qsTr("关光")
        }
        Readout {
            caption: qsTr("模式")
            text: root.readings[LaserProtocol.ControlMode] === undefined ? "—" : (root.readings[LaserProtocol.ControlMode] & 0x400) ? qsTr("内控") : qsTr("外控")
        }
        Readout {
            caption: qsTr("脉宽")
            objectName: "widthReadback"
            text: {
                const option = root.widthOptions.find(item => item.code === root.readings[LaserProtocol.WidthIndex]);
                return option ? option.value + " ns" : "—";
            }
        }
        Readout {
            caption: qsTr("功率")
            objectName: "powerReadback"
            text: root.readings[LaserProtocol.Power] === undefined ? "—" : (root.readings[LaserProtocol.Power] * 100 / 255).toFixed(1) + " %"
        }
        Readout {
            caption: qsTr("频率")
            objectName: "frequencyReadback"
            text: root.value(LaserProtocol.Frequency, " kHz")
        }
    }
    GridLayout {
        columns: 2
        columnSpacing: FormStyle.columnSpacing
        rowSpacing: FormStyle.rowSpacing
        Layout.fillWidth: true
        Layout.topMargin: 20
        Label {
            text: qsTr("型号")
            font.pixelSize: 12
            opacity: 0.7
        }
        Label {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            wrapMode: Text.Wrap
            text: root.identity([LaserProtocol.Model1, LaserProtocol.Model2, LaserProtocol.Model3])
            font.pixelSize: 12
            opacity: 0.7
        }
        Label {
            text: "SN"
            font.pixelSize: 12
            opacity: 0.7
        }
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
