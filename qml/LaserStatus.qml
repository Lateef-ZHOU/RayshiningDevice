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
    ColumnLayout {
        Layout.fillWidth: true
        Layout.topMargin: 20
        spacing: FormStyle.rowSpacing
        InfoRow {
            compact: true
            caption: qsTr("型号")
            text: root.identity([LaserProtocol.Model1, LaserProtocol.Model2, LaserProtocol.Model3])
        }
        InfoRow {
            objectName: "serialReadback"
            compact: true
            caption: "SN"
            text: root.identity([LaserProtocol.Serial1, LaserProtocol.Serial2, LaserProtocol.Serial3, LaserProtocol.Serial4])
        }
    }
}
