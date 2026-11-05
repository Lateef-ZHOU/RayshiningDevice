pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Lidar.UI

Section {
    id: root
    required property var readings
    function identity(addresses) {
        if (addresses.every(address => readings[address] === undefined))
            return "—";
        return addresses.map(address => readings[address] === undefined ? "…" : readings[address]).join("").replace(/\0/g, "");
    }
    ReadoutRow {
        Readout {
            caption: qsTr("型号")
            text: root.identity([LaserProtocol.Model1, LaserProtocol.Model2, LaserProtocol.Model3])
        }
        Readout {
            objectName: "serialReadback"
            caption: "SN"
            text: root.identity([LaserProtocol.Serial1, LaserProtocol.Serial2, LaserProtocol.Serial3, LaserProtocol.Serial4])
        }
    }
    ReadoutRow {
        Layout.topMargin: 20
        Readout {
            caption: qsTr("激光输出")
            objectName: "emissionReadback"
            text: root.readings[LaserProtocol.Emission] === undefined ? "—" : root.readings[LaserProtocol.Emission] ? qsTr("出光") : qsTr("关光")
        }
        Readout {
            caption: qsTr("模式")
            text: root.readings[LaserProtocol.ControlMode] === undefined ? "—" : (root.readings[LaserProtocol.ControlMode] & 0x400) ? qsTr("内控") : qsTr("外控")
        }
    }
}
