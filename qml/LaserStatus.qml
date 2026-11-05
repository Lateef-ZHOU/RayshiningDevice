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
}
