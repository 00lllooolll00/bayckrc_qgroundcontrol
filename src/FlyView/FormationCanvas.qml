pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

// 动态编队画布：北向上，领机固定原点。
// 空心圆 = 生成的几何位置；实心点 = 飞机实时位置；点击实心点选中成员。
Item {
    id: root

    property var  model
    property var  positions
    property var  livePositions: ({})
    property int  selectedSysid: -1
    property real pixelsPerMeter: 4
    property real panX: 0
    property real panY: 0

    signal memberSelected(int sysid)

    implicitWidth: 300
    implicitHeight: 260
    clip: true

    QGCPalette { id: qgcPal }

    function canvasX(forward, right) {
        return width * 0.5 + panX + right * pixelsPerMeter
    }
    function canvasY(forward, right) {
        return height * 0.5 + panY - forward * pixelsPerMeter
    }

    Canvas {
        id: gridCanvas
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = qgcPal.window
            ctx.fillRect(0, 0, width, height)

            var stepM = pixelsPerMeter * 10 < 30 ? 1 : (pixelsPerMeter * 10 < 80 ? 5 : 10)
            var stepPx = stepM * pixelsPerMeter
            ctx.strokeStyle = qgcPal.text
            ctx.globalAlpha = 0.15
            ctx.lineWidth = 1
            var cx = width * 0.5 + panX
            var cy = height * 0.5 + panY
            for (var x = cx % stepPx; x < width; x += stepPx) {
                ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke()
            }
            for (var y = cy % stepPx; y < height; y += stepPx) {
                ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
            }
            ctx.globalAlpha = 1
        }
    }

    // 领机十字标
    Item {
        x: root.width * 0.5 + root.panX - 6
        y: root.height * 0.5 + root.panY - 6
        width: 12; height: 12
        Rectangle { anchors.centerIn: parent; width: 2; height: 12; color: qgcPal.warningText }
        Rectangle { anchors.centerIn: parent; width: 12; height: 2; color: qgcPal.warningText }
    }

    // 空白拖动平移 + 滚轮缩放
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
        property real lastX: 0
        property real lastY: 0
        onPressed: (mouse) => { lastX = mouse.x; lastY = mouse.y }
        onPositionChanged: (mouse) => {
            if (!pressed) return
            root.panX += mouse.x - lastX
            root.panY += mouse.y - lastY
            lastX = mouse.x
            lastY = mouse.y
            gridCanvas.requestPaint()
        }
        onWheel: (wheel) => {
            var factor = wheel.angleDelta.y > 0 ? 1.15 : 0.87
            root.pixelsPerMeter = Math.max(0.5, Math.min(40, root.pixelsPerMeter * factor))
            gridCanvas.requestPaint()
        }
    }

    // 生成的几何位置
    Repeater {
        model: root.positions
        delegate: Item {
            id: target
            required property var modelData
            required property int index

            property real px: root.canvasX(modelData.forward, modelData.right)
            property real py: root.canvasY(modelData.forward, modelData.right)

            x: 0; y: 0; width: root.width; height: root.height

            Rectangle {
                x: target.px - 7
                y: target.py - 7
                width: 14; height: 14; radius: 7
                color: "transparent"
                border.color: qgcPal.text
                border.width: 1
                opacity: 0.7
            }

            QGCLabel {
                x: target.px + 8
                y: target.py - 8
                text: target.index
                font.pointSize: ScreenTools.smallFontPointSize
                color: qgcPal.text
            }
        }
    }

    // 飞机实时位置
    Repeater {
        model: root.model
        delegate: Item {
            id: live
            required property int sysid
            required property int formationFlags
            required property bool online

            property var position: root.livePositions[sysid] || null
            property real px: position ? root.canvasX(position.north, position.east) : -1000
            property real py: position ? root.canvasY(position.north, position.east) : -1000
            property bool isLeader: (formationFlags & 0x04) !== 0

            x: 0; y: 0; width: root.width; height: root.height

            Rectangle {
                visible: live.position !== null
                x: live.px - (live.isLeader ? 5 : 3)
                y: live.py - (live.isLeader ? 5 : 3)
                width: live.isLeader ? 10 : 6
                height: live.isLeader ? 10 : 6
                radius: width / 2
                color: live.sysid === root.selectedSysid ? qgcPal.colorGreen
                     : (live.isLeader ? qgcPal.warningText : qgcPal.text)
                opacity: live.online ? 1.0 : 0.4
            }

            MouseArea {
                visible: live.position !== null
                x: live.px - 14
                y: live.py - 14
                width: 28
                height: 28
                onClicked: root.memberSelected(live.sysid)
            }
        }
    }
}
