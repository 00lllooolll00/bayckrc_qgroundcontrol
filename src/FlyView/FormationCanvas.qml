pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

import QGroundControl
import QGroundControl.Controls

// 编队小地图：以领机为原点、正北朝上。
// 空心圈 = 队形预计槽位（数字 = 槽位号）；箭头 = 飞机实时位置与绝对航向；
// 金色箭头 + 星标 = 领机；槽位到飞机的连线颜色表示偏差大小。
// 槽位 forward/right 就是世界系北/东，不随领机实时朝向旋转。
Item {
    id: root

    // [{forward, right, distance, angle}]，forward=北、right=东；下标即槽位号，槽 0 = 领机
    property var  slots: []
    // FormationModel.liveLayout() 的输出：[{sysid, north, east, heading(绝对), ...}]
    property var  members: []
    property int  selectedSysid: -1
    property int  leaderSysid: 0

    signal memberSelected(int sysid)

    readonly property color _leaderColor: "#F5B800"
    readonly property real  _margin: ScreenTools.defaultFontPixelHeight * 1.6
    readonly property real  _minAutoExtentM: 2.0
    readonly property real  _defaultIdleExtentM: 5.0
    readonly property real  _minManualExtentM: 1.0
    readonly property real  _maxManualExtentM: 1000.0

    property bool autoZoom: true
    property real _autoExtentM: _defaultIdleExtentM
    property real _manualExtentM: _defaultIdleExtentM
    readonly property real _targetExtentM: autoZoom ? _autoExtentM : _manualExtentM
    property real displayExtentM: _targetExtentM

    Behavior on displayExtentM {
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }

    readonly property real _pxPerMeter:
        Math.max(0.1, (Math.min(width, height) * 0.5 - _margin) / Math.max(0.5, displayExtentM))

    implicitWidth: 300
    implicitHeight: 300
    clip: true

    QGCPalette { id: qgcPal }

    function _computeDesiredExtentM() {
        var maxDist = 0
        for (var i = 0; i < slots.length; i++) {
            if (slots[i].visible === false) continue
            var dSlot = Math.hypot(slots[i].forward, slots[i].right)
            if (isFinite(dSlot)) maxDist = Math.max(maxDist, dSlot)
        }
        for (var j = 0; j < members.length; j++) {
            var dMem = Math.hypot(members[j].north, members[j].east)
            if (isFinite(dMem)) maxDist = Math.max(maxDist, dMem)
        }
        return maxDist < 0.1 ? _defaultIdleExtentM : Math.max(_minAutoExtentM, maxDist * 1.3)
    }

    function _updateAutoExtent(force) {
        var desired = _computeDesiredExtentM()
        if (force || desired > _autoExtentM * 0.92 || desired < _autoExtentM * 0.62) {
            _autoExtentM = desired
        }
    }

    function _zoomBy(factor) {
        var base = autoZoom ? displayExtentM : _manualExtentM
        autoZoom = false
        _manualExtentM = Math.max(_minManualExtentM, Math.min(_maxManualExtentM, base * factor))
    }

    function _resetAutoZoom() {
        autoZoom = true
        _updateAutoExtent(true)
    }

    function _formatMeters(m) {
        return m < 1 ? (m.toFixed(1) + " m") : (String(m) + " m")
    }

    function _cx(east) { return width * 0.5 + east * _pxPerMeter }
    function _cy(north) { return height * 0.5 - north * _pxPerMeter }

    // 选一个让相邻圈间距 ≥ 40px 的整齐步长。
    function _ringStepM() {
        var steps = [0.5, 1, 2, 5, 10, 20, 50, 100, 200, 500]
        for (var i = 0; i < steps.length; i++) {
            if (steps[i] * _pxPerMeter >= 40) {
                return steps[i]
            }
        }
        return steps[steps.length - 1]
    }

    function _slotFor(position) {
        return (position >= 0 && position < slots.length) ? slots[position] : null
    }

    function _errorColor(errM) {
        if (errM < 2) return qgcPal.colorGreen
        if (errM < 5) return qgcPal.colorOrange
        return qgcPal.colorRed
    }

    function _sysidText(sysid) { return String(sysid) }

    function _drawStar(ctx, x, y, outer, inner) {
        ctx.beginPath()
        for (var i = 0; i < 10; i++) {
            var r = (i % 2 === 0) ? outer : inner
            var a = -Math.PI / 2 + i * Math.PI / 5
            var px = x + r * Math.cos(a)
            var py = y + r * Math.sin(a)
            if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py)
        }
        ctx.closePath()
    }

    function _drawArrow(ctx, x, y, headingDeg, size, fill, stroke) {
        ctx.save()
        ctx.translate(x, y)
        ctx.rotate(headingDeg * Math.PI / 180)
        ctx.beginPath()
        ctx.moveTo(0, -size)
        ctx.lineTo(size * 0.75, size * 0.8)
        ctx.lineTo(0, size * 0.4)
        ctx.lineTo(-size * 0.75, size * 0.8)
        ctx.closePath()
        ctx.fillStyle = fill
        ctx.fill()
        ctx.lineWidth = 1.5
        ctx.strokeStyle = stroke
        ctx.stroke()
        ctx.restore()
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = qgcPal.window
            ctx.fillRect(0, 0, width, height)

            var cx = width * 0.5
            var cy = height * 0.5
            var fontPx = Math.round(ScreenTools.defaultFontPixelHeight * 0.75)
            ctx.font = fontPx + "px sans-serif"
            ctx.textBaseline = "middle"

            // 距离圈 + 十字轴
            var step = root._ringStepM()
            ctx.lineWidth = 1
            ctx.strokeStyle = qgcPal.text
            ctx.fillStyle = qgcPal.text
            ctx.globalAlpha = 0.18
            var maxR = Math.hypot(width, height) * 0.5
            for (var r = step; r * root._pxPerMeter < maxR; r += step) {
                ctx.beginPath()
                ctx.arc(cx, cy, r * root._pxPerMeter, 0, Math.PI * 2)
                ctx.stroke()
            }
            ctx.beginPath(); ctx.moveTo(cx, 0); ctx.lineTo(cx, height); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(0, cy); ctx.lineTo(width, cy); ctx.stroke()
            ctx.globalAlpha = 0.5
            ctx.textAlign = "left"
            // 将比例尺刻度标在东北 45° 象限，避免与正东侧 1 号槽位/飞机标识重叠。
            var ringLabelR = step * root._pxPerMeter
            ctx.fillText(root._formatMeters(step), cx + ringLabelR * 0.707 + 3, cy - ringLabelR * 0.707 - 2)
            ctx.textAlign = "center"
            ctx.fillText(qsTr("N"), cx, fontPx * 0.8)
            ctx.globalAlpha = 1

            // 槽位到飞机的偏差连线
            for (var m = 0; m < root.members.length; m++) {
                var mem = root.members[m]
                var slot = root._slotFor(mem.position)
                if (!slot || mem.leaving || !mem.online) continue
                var err = Math.hypot(mem.north - slot.forward, mem.east - slot.right)
                ctx.strokeStyle = root._errorColor(err)
                ctx.lineWidth = 1.5
                ctx.globalAlpha = 0.8
                ctx.beginPath()
                ctx.moveTo(root._cx(slot.right), root._cy(slot.forward))
                ctx.lineTo(root._cx(mem.east), root._cy(mem.north))
                ctx.stroke()
            }
            ctx.globalAlpha = 1

            // 槽位
            ctx.textAlign = "center"
            for (var i = 1; i < root.slots.length; i++) {
                if (root.slots[i].visible === false) continue
                var sx = root._cx(root.slots[i].right)
                var sy = root._cy(root.slots[i].forward)
                ctx.beginPath()
                ctx.arc(sx, sy, ScreenTools.defaultFontPixelHeight * 0.75, 0, Math.PI * 2)
                ctx.setLineDash([3, 3])
                ctx.lineWidth = 1.2
                ctx.strokeStyle = qgcPal.text
                ctx.globalAlpha = 0.65
                ctx.stroke()
                ctx.setLineDash([])
                ctx.globalAlpha = 0.5
                ctx.fillStyle = qgcPal.text
                ctx.fillText(String(i), sx, sy + ScreenTools.defaultFontPixelHeight * 1.25)
                ctx.globalAlpha = 1
            }

            // 飞机：先画跟随机，最后画领机保证在最上层
            for (var pass = 0; pass < 2; pass++) {
                for (var k = 0; k < root.members.length; k++) {
                    var v = root.members[k]
                    var isLeader = v.isLeader
                    if ((pass === 1) !== isLeader) continue

                    var vx = root._cx(v.east)
                    var vy = root._cy(v.north)
                    var size = isLeader ? ScreenTools.defaultFontPixelHeight * 0.95
                                        : ScreenTools.defaultFontPixelHeight * 0.7
                    var fill = isLeader ? root._leaderColor
                             : (v.leaving ? qgcPal.buttonBorder : qgcPal.colorGreen)
                    ctx.globalAlpha = v.online ? 1.0 : 0.4

                    if (v.sysid === root.selectedSysid) {
                        ctx.beginPath()
                        ctx.arc(vx, vy, size * 1.7, 0, Math.PI * 2)
                        ctx.lineWidth = 2
                        ctx.strokeStyle = qgcPal.primaryButton
                        ctx.stroke()
                    }
                    if (isLeader) {
                        ctx.beginPath()
                        ctx.arc(vx, vy, size * 1.35, 0, Math.PI * 2)
                        ctx.lineWidth = 1.5
                        ctx.strokeStyle = root._leaderColor
                        ctx.stroke()
                    }
                    root._drawArrow(ctx, vx, vy, v.heading, size, fill, "#202020")
                    if (isLeader) {
                        root._drawStar(ctx, vx, vy - size * 1.75, size * 0.55, size * 0.25)
                        ctx.fillStyle = root._leaderColor
                        ctx.fill()
                        ctx.lineWidth = 1
                        ctx.strokeStyle = "#202020"
                        ctx.stroke()
                    }

                    ctx.fillStyle = qgcPal.text
                    ctx.textAlign = "center"
                    ctx.fillText(root._sysidText(v.sysid), vx, vy + size * 1.9)
                    ctx.globalAlpha = 1
                }
            }
        }
    }

    Component.onCompleted: _updateAutoExtent(true)
    onSlotsChanged: { _updateAutoExtent(false); canvas.requestPaint() }
    onMembersChanged: { _updateAutoExtent(false); canvas.requestPaint() }
    onSelectedSysidChanged: canvas.requestPaint()
    onDisplayExtentMChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    // 点最近的飞机选中；支持滚轮缩放与双击回自动适配视野。
    MouseArea {
        anchors.fill: parent
        onWheel: (wheel) => {
            if (wheel.angleDelta.y === 0) {
                return
            }
            root._zoomBy(wheel.angleDelta.y > 0 ? 0.8 : 1.25)
            wheel.accepted = true
        }
        onDoubleClicked: root._resetAutoZoom()
        onClicked: (mouse) => {
            var best = -1
            var bestDist = ScreenTools.defaultFontPixelHeight * 1.8
            for (var i = 0; i < root.members.length; i++) {
                var v = root.members[i]
                var d = Math.hypot(root._cx(v.east) - mouse.x, root._cy(v.north) - mouse.y)
                if (d < bestDist) {
                    bestDist = d
                    best = v.sysid
                }
            }
            if (best > 0) {
                root.memberSelected(best)
            }
        }
    }

    // 右下角紧凑缩放栏（+ / - / 自动适配）
    Row {
        id: zoomControls
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: ScreenTools.defaultFontPixelHeight * 0.45
        spacing: 4

        readonly property real _btnHeight: ScreenTools.defaultFontPixelHeight * 1.45

        Rectangle {
            width: zoomControls._btnHeight
            height: zoomControls._btnHeight
            radius: ScreenTools.defaultBorderRadius * 0.7
            color: qgcPal.windowShade
            border.width: 1
            border.color: qgcPal.buttonBorder
            opacity: 0.9

            QGCLabel {
                anchors.centerIn: parent
                text: "+"
                font.bold: true
                color: qgcPal.text
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root._zoomBy(0.8)
            }
        }

        Rectangle {
            width: zoomControls._btnHeight
            height: zoomControls._btnHeight
            radius: ScreenTools.defaultBorderRadius * 0.7
            color: qgcPal.windowShade
            border.width: 1
            border.color: qgcPal.buttonBorder
            opacity: 0.9

            QGCLabel {
                anchors.centerIn: parent
                text: "−"
                font.bold: true
                color: qgcPal.text
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root._zoomBy(1.25)
            }
        }

        Rectangle {
            width: autoLabel.implicitWidth + ScreenTools.defaultFontPixelWidth * 1.2
            height: zoomControls._btnHeight
            radius: ScreenTools.defaultBorderRadius * 0.7
            color: root.autoZoom ? qgcPal.primaryButton : qgcPal.windowShade
            border.width: 1
            border.color: root.autoZoom ? qgcPal.primaryButton : qgcPal.buttonBorder
            opacity: 0.92

            QGCLabel {
                id: autoLabel
                anchors.centerIn: parent
                text: qsTr("Auto")
                font.pointSize: ScreenTools.defaultFontPointSize - 1
                font.bold: root.autoZoom
                color: root.autoZoom ? qgcPal.primaryButtonText : qgcPal.text
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root._resetAutoZoom()
            }
        }
    }

    QGCLabel {
        visible: root.members.length === 0
        anchors.centerIn: parent
        text: qsTr("Waiting for vehicle positions")
        opacity: 0.6
    }
}
