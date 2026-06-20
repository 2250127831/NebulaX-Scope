import QtQuick
import NebulaX.Scope 1.0

Item {
    id: root

    property real targetHeadAngle: 0
    property real targetTailAngle: 0
    property real renderHeadAngle: 0
    property real renderTailAngle: 0
    property real displayUtilization: 0
    property real displayUtilizationText: 0
    property real displayUsedMBText: 0
    property real renderVelocity: 0
    property real glowIntensity: 0

    readonly property color ringColor:
        Monitor.ringUtilization < 0.60 ? "#22C55E" :
        Monitor.ringUtilization < 0.90 ? "#F59E0B" :
                                         "#EF4444"

    function ptrToAngle(ptr) {
        if (Monitor.ringCapacity <= 0) return 0
        return (ptr / Monitor.ringCapacity) * Math.PI * 2
    }

    function updateTargets() {
        targetHeadAngle = ptrToAngle(Monitor.ringWritePtr)
        targetTailAngle = ptrToAngle(Monitor.ringReadPtr)
    }

    Connections {
        target: Monitor
        function onRingUpdated() {
            root.updateTargets()
            root.canvasDirty = true
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: {
            displayUtilizationText = Monitor.ringUtilization
            displayUsedMBText = Monitor.ringUsedBytes
        }
    }

    Component.onCompleted: {
        updateTargets()
        renderHeadAngle = targetHeadAngle
        renderTailAngle = targetTailAngle
        displayUtilization = Monitor.ringUtilization
        displayUtilizationText = Monitor.ringUtilization
        displayUsedMBText = Monitor.ringUsedBytes
    }

    // ── Update loop (no fixed timer — driven by ringUpdated signal) ──
    property bool canvasDirty: false

    Timer {
        interval: 16
        repeat: true
        running: true
        onTriggered: {
            let dh = targetHeadAngle - renderHeadAngle
            let dt = targetTailAngle - renderTailAngle
            if (dh > Math.PI) dh -= Math.PI * 2
            if (dh < -Math.PI) dh += Math.PI * 2
            if (dt > Math.PI) dt -= Math.PI * 2
            if (dt < -Math.PI) dt += Math.PI * 2
            var moved = Math.abs(dh) > 0.0001 || Math.abs(dt) > 0.0001
            renderHeadAngle += dh * 0.12
            renderTailAngle += dt * 0.12
            renderVelocity = Math.abs(dt)
            displayUtilization += (Monitor.ringUtilization - displayUtilization) * 0.10

            if (moved) {
                canvasDirty = true
                glowIntensity = Math.min(1.0, 0.5 + renderVelocity * 3.0)
            } else {
                glowIntensity = 0.3   // dim when idle
            }

            if (canvasDirty) { canvas.requestPaint(); canvasDirty = false }
        }
    }

    // ── Title ──
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 12
        text: "SPSC RING"
        color: "#94A3B8"
        font.pixelSize: 13
        font.bold: true
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.resetTransform()
            ctx.clearRect(0, 0, width, height)

            const cx = width / 2, cy = height / 2
            const radius = Math.min(width, height) * 0.28
            const startOffset = -Math.PI / 2
            let start = startOffset + renderHeadAngle
            let end = startOffset + renderTailAngle
            if (end < start) end += Math.PI * 2

            // background ring
            ctx.beginPath()
            ctx.lineWidth = 18
            ctx.strokeStyle = "#334155"
            ctx.arc(cx, cy, radius, 0, Math.PI * 2)
            ctx.stroke()

            // occupancy arc (cheap glow via two layers, no shadowBlur)
            // glow layer (wider, semi-transparent)
            ctx.beginPath()
            ctx.lineWidth = 26
            ctx.lineCap = "butt"
            ctx.globalAlpha = 0.12 + glowIntensity * 0.08
            ctx.strokeStyle = ringColor
            ctx.arc(cx, cy, radius, start, end)
            ctx.stroke()
            ctx.globalAlpha = 1.0

            // solid layer
            ctx.beginPath()
            ctx.lineWidth = 18
            ctx.lineCap = "round"
            ctx.strokeStyle = ringColor
            ctx.arc(cx, cy, radius, start, end)
            ctx.stroke()
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 6
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Math.round(Monitor.ringUtilization * 100) + "%"
            color: "#F8FAFC"
            font.pixelSize: 34
            font.bold: true
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: (displayUsedMBText / 1024 / 1024).toFixed(1)
                  + " / " + (Monitor.ringCapacity / 1024 / 1024).toFixed(1) + " MB"
            color: "#94A3B8"
            font.pixelSize: 15
        }
    }
}
