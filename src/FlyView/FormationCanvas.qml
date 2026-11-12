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
    readonly property real  _minExtentM: 15

    // 画面半径对应的实际米数：覆盖全部槽位与飞机，并留 10% 余量。
    readonly property real _extentM: {
        var extent = _minExtentM
        for (var i = 0; i < slots.length; i++) {
            extent = Math.max(extent, Math.hypot(slots[i].forward, slots[i].right))
        }
        for (var j = 0; j < members.length; j++) {
            extent = Math.max(extent, Math.hypot(members[j].north, members[j].east))
        }
        return extent * 1.1
    }
    readonly property real _pxPerMeter: Math.max(0.1, (Math.min(width, height) * 0.5 - _margin) / _extentM)

    implicitWidth: 300
    implicitHeight: 300
    clip: true

    QGCPalette { id: qgcPal }

    function _cx(east) { return width * 0.5 + east * _pxPerMeter }
    function _cy(north) { return height * 0.5 - north * _pxPerMeter }

    // 选一个让相邻圈间距 ≥ 40px 的整齐步长。
    function _ringStepM() {
        var steps = [1, 2, 5, 10, 20, 50, 100, 200, 500]
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
            ctx.fillText(step + " m", cx + step * root._pxPerMeter + 3, cy - fontPx * 0.7)
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

    onSlotsChanged: canvas.requestPaint()
    onMembersChanged: canvas.requestPaint()
    onSelectedSysidChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    // 点最近的飞机选中；点空白不做任何事。
    MouseArea {
        anchors.fill: parent
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

    QGCLabel {
        visible: root.members.length === 0
        anchors.centerIn: parent
        text: qsTr("Waiting for vehicle positions")
        opacity: 0.6
    }
}
