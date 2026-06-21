import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import NebulaX.Scope 1.0

ApplicationWindow {
    id: root

    flags: Qt.FramelessWindowHint

    width: 900
    height: 700
    visible: true
    title: "NebulaX Scope"
    color: "transparent"

    // ── Rounded window background + content ──
    Rectangle {
        anchors.fill: parent
        radius: 10
        clip: true
        color: "#0B0F14"

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(0.25, 0.35, 0.7, 0.03) }
                GradientStop { position: 0.5; color: "transparent" }
                GradientStop { position: 1.0; color: Qt.rgba(0.15, 0.25, 0.5, 0.02) }
            }
        }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        // ── Custom title bar ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            radius: 8
            color: "#10151C"
            border.width: 1
            border.color: "#1E293B"

            // ── Window drag area ──
            MouseArea {
                anchors.fill: parent
                property variant lastPos: Qt.point(0, 0)
                onPressed: { lastPos = Qt.point(mouseX, mouseY) }
                onPositionChanged: {
                    var delta = Qt.point(mouseX - lastPos.x, mouseY - lastPos.y)
                    root.x += delta.x; root.y += delta.y
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 0

                Column {
                    spacing: 2
                    Text { text: "NebulaX Scope"; color: "#E5E7EB"; font.pixelSize: 16; font.bold: true }
                    Text { text: "Telemetry Dashboard"; color: "#64748B"; font.pixelSize: 11 }
                }

                Item { Layout.fillWidth: true }

                // ── Breathing RUNNING dot ──
                Rectangle {
                    id: statusDot
                    width: 10; height: 10; radius: 5
                    Layout.alignment: Qt.AlignVCenter
                    color: Monitor.connected ? "#22C55E" : "#EF4444"
                    SequentialAnimation on scale {
                        loops: Animation.Infinite; running: Monitor.connected
                        NumberAnimation { to: 0.7; duration: 800; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutSine }
                    }
                    SequentialAnimation on opacity {
                        loops: Animation.Infinite; running: Monitor.connected
                        NumberAnimation { to: 0.5; duration: 1000; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0; duration: 1000; easing.type: Easing.InOutSine }
                    }
                }

                Text {
                    text: Monitor.connected ? "RUNNING" : "DISCONNECTED"
                    color: Monitor.connected ? "#22C55E" : "#EF4444"
                    font.pixelSize: 14; font.bold: true
                    leftPadding: 6; rightPadding: 8
                }

                // ── Window buttons ──
                Item { width: 8; height: 1 }

                Rectangle {
                    width: 36; height: 36; radius: 6; color: "transparent"
                    Text {
                        anchors.centerIn: parent; text: "─"; color: "#94A3B8"; font.pixelSize: 16
                    }
                    MouseArea {
                        anchors.fill: parent; hoverEnabled: true
                        onEntered: parent.color = "#1E293B"
                        onExited:  parent.color = "transparent"
                        onClicked: root.showMinimized()
                    }
                }

                Rectangle {
                    id: maxBtn
                    width: 36; height: 36; radius: 6; color: "transparent"
                    property bool maximized: false
                    Text {
                        anchors.centerIn: parent
                        text: maxBtn.maximized ? "❐" : "□"
                        color: "#94A3B8"; font.pixelSize: 14
                    }
                    MouseArea {
                        anchors.fill: parent; hoverEnabled: true
                        onEntered: parent.color = "#1E293B"
                        onExited:  parent.color = "transparent"
                        onClicked: {
                            if (maxBtn.maximized) {
                                root.showNormal()
                                maxBtn.maximized = false
                            } else {
                                root.showMaximized()
                                maxBtn.maximized = true
                            }
                        }
                    }
                }

                Rectangle {
                    id: closeBtn
                    width: 36; height: 36; radius: 6; color: "transparent"
                    Text {
                        anchors.centerIn: parent; text: "✕"; color: "#94A3B8"; font.pixelSize: 14
                    }
                    MouseArea {
                        anchors.fill: parent; hoverEnabled: true
                        onEntered: { closeBtn.color = "#EF4444"; closeBtn.children[0].color = "#FFFFFF" }
                        onExited:  { closeBtn.color = "transparent"; closeBtn.children[0].color = "#94A3B8" }
                        onClicked: Qt.quit()
                    }
                }
            }
        }

        // ── Metrics ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 72
            radius: 8
            color: "#10151C"
            border.width: 1
            border.color: "#1E293B"

            RowLayout {
                anchors.fill: parent; anchors.margins: 10; spacing: 10
                MetricCard { Layout.fillWidth: true; title: "QPS"; valueNumber: Monitor.qps }
                MetricCard { Layout.fillWidth: true; title: "TRADES/s"; valueNumber: Monitor.tradeRate }
                MetricCard { Layout.fillWidth: true; title: "OUTPUT"; valueText: (Monitor.sendThroughput / 1024.0 / 1024.0).toFixed(2) + " MB/s" }
                MetricCard { Layout.fillWidth: true; title: "ORDER/s"; valueNumber: Monitor.orderRate }
                MetricCard { Layout.fillWidth: true; title: "ERRORS"; valueNumber: Monitor.errors; valueColor: Monitor.errors > 0 ? "#EF4444" : "#22C55E" }
            }
        }

        // ── Ring ──
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 8
            color: "#10151C"
            border.width: 1
            border.color: "#1E293B"
            RingTelemetry { anchors.fill: parent }
        }

        // ── Order Pool ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            radius: 8
            color: "#10151C"
            border.width: 1
            border.color: "#1E293B"
            OrderPoolBar { anchors.fill: parent }
        }

        // ── Status ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            radius: 8
            color: "#10151C"
            border.width: 1
            border.color: "#1E293B"

            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 16

                Row { spacing: 6
                    Rectangle { width: 8; height: 8; radius: 4; anchors.verticalCenter: parent.verticalCenter; color: Monitor.ioAlive ? "#22C55E" : "#EF4444" }
                    Text { text: "IO"; color: "#94A3B8"; font.pixelSize: 11 }
                    Text { text: "PID " + (Monitor.ioPid || "—"); color: "#64748B"; font.pixelSize: 11 }
                }
                Row { spacing: 6
                    Rectangle { width: 8; height: 8; radius: 4; anchors.verticalCenter: parent.verticalCenter; color: Monitor.sendAlive ? "#22C55E" : "#EF4444" }
                    Text { text: "Send"; color: "#94A3B8"; font.pixelSize: 11 }
                    Text { text: "PID " + (Monitor.sendPid || "—"); color: "#64748B"; font.pixelSize: 11 }
                }

                Item { Layout.fillWidth: true }

                Text { text: "UP " + formatUptime(Monitor.uptimeSeconds); color: "#64748B"; font.pixelSize: 11 }
                Text { text: "120Hz"; color: "#3A4050"; font.pixelSize: 11 }
            }
        }
    }
    }

    function formatUptime(s) {
        var m = Math.floor(s / 60);
        var h = Math.floor(m / 60);
        if (h > 0) return h + "h " + (m % 60) + "m";
        if (m > 0) return m + "m " + (s % 60) + "s";
        return s + "s";
    }
}
