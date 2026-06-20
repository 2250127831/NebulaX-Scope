import QtQuick
import QtQuick.Layouts
import NebulaX.Scope 1.0

Item {
    id: root

    property real displayUtilization: 0

    readonly property color poolColor:
        displayUtilization < 0.60 ? "#22C55E" :
        displayUtilization < 0.90 ? "#F59E0B" :
                                    "#EF4444"

    // ── Shimmer (NumberAnimation, no JS timer) ──
    property real shimmerPos: -1
    NumberAnimation on shimmerPos {
        from: -1; to: 2; duration: 2500; loops: Animation.Infinite
        running: displayUtilization > 0.01
    }

    Timer {
        interval: 16; repeat: true; running: true
        onTriggered: { displayUtilization = Monitor.poolUtilization }
    }

    Column {
        anchors.centerIn: parent
        width: parent.width
        spacing: 10

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "ORDER POOL"
            color: "#94A3B8"
            font.pixelSize: 13; font.bold: true
        }

        Rectangle {
            width: parent.width; height: 22
            radius: 11; color: "#1E293B"
            border.width: 1; border.color: "#334155"
            clip: true

            Rectangle {
                anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                width: parent.width * displayUtilization
                radius: 11
                color: poolColor
                Behavior on width { NumberAnimation { duration: 80 } }
            }

            // ── Shimmer overlay ──
            Rectangle {
                anchors.fill: parent
                radius: 11
                gradient: Gradient {
                    GradientStop { position: root.shimmerPos - 0.3; color: "transparent" }
                    GradientStop { position: root.shimmerPos; color: Qt.rgba(1, 1, 1, 0.15) }
                    GradientStop { position: root.shimmerPos + 0.3; color: "transparent" }
                }
            }
        }

        RowLayout {
            width: parent.width
            Text {
                text: Number(Monitor.orderPoolUsed).toLocaleString(Qt.locale(), 'f', 0)
                      + " / " + Number(Monitor.orderPoolCapacity).toLocaleString(Qt.locale(), 'f', 0)
                color: "#F8FAFC"; font.pixelSize: 15
            }
            Item { Layout.fillWidth: true }
            Text {
                text: (displayUtilization * 100).toFixed(1) + "%"
                color: poolColor; font.pixelSize: 15; font.bold: true
            }
        }
    }
}
