import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    property string title: ""
    property real   valueNumber: 0
    property string valueText: ""          // static text override (e.g. "1.82 MB/s")
    property color valueColor: "#F8FAFC"

    // ── Smoothed display value ──
    property real displayValue: 0

    // ── Flash on change ──
    property real flashOpacity: 0

    NumberAnimation on flashOpacity {
        id: flashAnim
        from: 0.25; to: 0; duration: 500
        running: false
    }

    Rectangle {
        anchors.fill: parent
        radius: 6
        color: valueColor
        opacity: flashOpacity
    }

    Column {
        anchors.centerIn: parent
        spacing: 8
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.title
            color: "#94A3B8"
            font.pixelSize: 13
            font.bold: true
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.valueText !== ""
                  ? root.valueText
                  : Number(root.displayValue).toLocaleString(Qt.locale(), 'f', 0)
            color: root.valueColor
            font.pixelSize: 26
            font.bold: true
        }
    }

    // ── 60 Hz smoothing + flash ──
    property real lastRawValue: 0

    Timer {
        interval: 16; repeat: true; running: true
        onTriggered: {
            if (valueNumber !== lastRawValue) {
                flashOpacity = 0.25; flashAnim.start()
                lastRawValue = valueNumber
            }
            var diff = valueNumber - displayValue
            if (Math.abs(diff) < 0.5) {
                displayValue = valueNumber
            } else {
                // 高位(大diff)慢, 低位(小diff)快
                var abs_diff = Math.abs(diff)
                var base_speed = 0.12
                var speed = base_speed / Math.log2(abs_diff + 1)
                displayValue += diff * speed
            }
        }
    }
}
