pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

// 动态编队面板：队形配置、成员状态、踢人/重入、Apply/Stop。
// 数据真源是 FormationController.model；位置由机上计算并回传。
Item {
    id: panel

    width: ScreenTools.defaultFontPixelWidth * 44
    implicitWidth: width
    implicitHeight: Math.min(content.implicitHeight, ScreenTools.defaultFontPixelHeight * 30)

    property var  _controller: QGroundControl.formationController
    property var  _model:      _controller.model
    property var  _vehicles:   QGroundControl.multiVehicleManager.vehicles
    property int  _selectedMember: -1
    property real _tick: 0

    readonly property real _pad: ScreenTools.defaultFontPixelWidth
    readonly property real _rowPad: ScreenTools.defaultFontPixelHeight * 0.28
    readonly property real _radius: ScreenTools.defaultBorderRadius
    // 动作卡内宽 = content.width - 2*pad，两个按钮中间再留 1 个 pad。
    readonly property real _actionButtonWidth:
        Math.max(0, (content.width - _pad * 3) * 0.5)

    // 机上 formation_phase_e：Idle / Waiting / Streaming / Frozen
    readonly property var _memberPhaseNames: [qsTr("Idle"), qsTr("Waiting"),
                                              qsTr("Streaming"), qsTr("Hold")]
    readonly property var _shapeNames: [qsTr("Line"), qsTr("V"), qsTr("Column"), qsTr("Grid")]
    readonly property var _phaseNames: [qsTr("Idle"), qsTr("Applying"), qsTr("Active"),
                                        qsTr("Stopping"), qsTr("Failed")]
    readonly property bool _canApply: _controller.phase !== 1
    readonly property var _memberSysids: {
        void panel._tick
        return _model.sysids
    }
    readonly property var _leaderOptions: {
        void panel._tick
        var out = ["Auto"]
        for (var i = 0; i < panel._memberSysids.length; i++) {
            out.push("0x" + panel._memberSysids[i].toString(16).toUpperCase())
        }
        return out
    }
    readonly property int _leaderIndex: {
        void panel._tick
        var override = _model.leaderOverride
        if (override === 0) {
            return 0
        }
        var index = panel._memberSysids.indexOf(override)
        return index < 0 ? 0 : index + 1
    }
    readonly property int _leaderSysid: {
        void panel._tick
        return _model.leaderSysid()
    }
    readonly property bool _selectedOnline: {
        void panel._tick
        return panel._selectedMember > 0 && _model.isOnline(panel._selectedMember)
    }
    readonly property bool _selectedLeaving: {
        void panel._tick
        return panel._selectedMember > 0 && _model.isLeaving(panel._selectedMember)
    }
    readonly property int _onlineCount: {
        void panel._tick
        var n = 0
        for (var i = 0; i < _vehicles.count; i++) {
            var v = _vehicles.get(i)
            if (v && v.maplrsFirmware) {
                n++
            }
        }
        return n
    }

    QGCPalette { id: qgcPal }

    Timer {
        // 实时位置点跟随遥测 5Hz 刷新，避免一秒才跳一次。
        interval: 200
        running: true
        repeat: true
        onTriggered: {
            panel._tick++
            // 成员首次出现、或当前选中项已不在列表时，选中第一个成员。
            var ids = panel._memberSysids
            if (ids.length > 0 && (panel._selectedMember <= 0
                                   || ids.indexOf(panel._selectedMember) < 0)) {
                panel._selectedMember = ids[0]
            }
        }
    }

    ScrollView {
        id: scroller
        anchors.fill: parent
        contentWidth: availableWidth
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        clip: true

        ColumnLayout {
            id: content
            width: Math.max(0, scroller.availableWidth - panel._pad * 2)
            x: panel._pad
            spacing: panel._pad

            // 顶部概览
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: headerLayout.implicitHeight + panel._pad * 2
                color: qgcPal.windowShade
                radius: panel._radius
                border.width: 1
                border.color: qgcPal.buttonBorder

                RowLayout {
                    id: headerLayout
                    anchors.fill: parent
                    anchors.margins: panel._pad
                    spacing: panel._pad

                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        width: ScreenTools.defaultFontPixelHeight * 0.55
                        height: width
                        radius: width / 2
                        color: qgcPal.colorGreen
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        QGCLabel {
                            text: qsTr("Formation")
                            font.bold: true
                            font.pointSize: ScreenTools.defaultFontPointSize + 2
                            color: qgcPal.text
                        }
                        QGCLabel {
                            text: qsTr("%1 online").arg(panel._onlineCount)
                            color: qgcPal.text
                            opacity: 0.65
                        }
                    }

                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: phaseLabel.implicitWidth + panel._pad
                        Layout.preferredHeight: phaseLabel.implicitHeight
                                                + ScreenTools.defaultFontPixelHeight * 0.3
                        radius: height / 2
                        color: panel._controller.phase === 4 ? qgcPal.colorRed
                             : panel._controller.phase === 2 ? qgcPal.colorGreen
                             : qgcPal.button

                        QGCLabel {
                            id: phaseLabel
                            anchors.centerIn: parent
                            text: panel._phaseNames[panel._controller.phase]
                            color: panel._controller.phase === 4 || panel._controller.phase === 2
                                   ? "white" : qgcPal.buttonText
                        }
                    }
                }
            }

            // 队形参数
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: configLayout.implicitHeight + panel._pad * 2
                color: qgcPal.windowShade
                radius: panel._radius
                border.width: 1
                border.color: qgcPal.buttonBorder

                ColumnLayout {
                    id: configLayout
                    anchors.fill: parent
                    anchors.margins: panel._pad
                    spacing: panel._pad * 0.6

                    QGCLabel {
                        text: qsTr("CONFIGURATION")
                        font.bold: true
                        color: qgcPal.text
                        opacity: 0.6
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: panel._pad
                        rowSpacing: panel._pad * 0.6

                        QGCLabel {
                            text: qsTr("Shape")
                            Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 9
                            Layout.alignment: Qt.AlignVCenter
                        }
                        QGCComboBox {
                            Layout.fillWidth: true
                            model: panel._shapeNames
                            currentIndex: panel._model.shape
                            onActivated: (index) => { panel._model.shape = index }
                        }

                        QGCLabel {
                            text: qsTr("Spacing (m)")
                            Layout.alignment: Qt.AlignVCenter
                        }
                        QGCTextField {
                            Layout.fillWidth: true
                            text: String(panel._model.spacingM)
                            validator: DoubleValidator { bottom: 3; top: 500 }
                            onEditingFinished: {
                                var value = parseFloat(text)
                                if (!isNaN(value)) {
                                    panel._model.spacingM = value
                                }
                            }
                        }

                        QGCLabel {
                            text: qsTr("V angle (deg)")
                            visible: panel._model.shape === 1
                            Layout.alignment: Qt.AlignVCenter
                        }
                        QGCTextField {
                            Layout.fillWidth: true
                            visible: panel._model.shape === 1
                            text: String(panel._model.vAngleDeg)
                            validator: DoubleValidator { bottom: 0; top: 90 }
                            onEditingFinished: {
                                var value = parseFloat(text)
                                if (!isNaN(value)) {
                                    panel._model.vAngleDeg = value
                                }
                            }
                        }

                        QGCLabel {
                            text: qsTr("Leader")
                            Layout.alignment: Qt.AlignVCenter
                        }
                        QGCComboBox {
                            Layout.fillWidth: true
                            model: panel._leaderOptions
                            currentIndex: panel._leaderIndex
                            onActivated: (index) => {
                                if (index === 0) {
                                    panel._model.leaderOverride = 0
                                } else if (index - 1 < panel._memberSysids.length) {
                                    panel._model.leaderOverride = panel._memberSysids[index - 1]
                                }
                            }
                        }
                    }
                }
            }

            // 画布
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: canvasLayout.implicitHeight + panel._pad * 2
                color: qgcPal.windowShade
                radius: panel._radius
                border.width: 1
                border.color: qgcPal.buttonBorder

                ColumnLayout {
                    id: canvasLayout
                    anchors.fill: parent
                    anchors.margins: panel._pad
                    spacing: panel._pad * 0.5

                    RowLayout {
                        Layout.fillWidth: true

                        QGCLabel {
                            text: qsTr("FORMATION LAYOUT")
                            font.bold: true
                            color: qgcPal.text
                            opacity: 0.6
                        }
                        Item { Layout.fillWidth: true }
                        QGCLabel {
                            text: qsTr("Select a dot")
                            color: qgcPal.text
                            opacity: 0.45
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 10
                        color: qgcPal.window
                        radius: Math.max(0, panel._radius - 2)
                        border.width: 1
                        border.color: qgcPal.buttonBorder
                        clip: true

                        FormationCanvas {
                            anchors.fill: parent
                            model: panel._model
                            positions: panel._model.positions
                            livePositions: panel._buildLivePositions()
                            selectedSysid: panel._selectedMember
                            onMemberSelected: (sysid) => { panel._selectedMember = sysid }
                        }
                    }
                }
            }

            // 成员表
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: membersLayout.implicitHeight + panel._pad * 2
                color: qgcPal.windowShade
                radius: panel._radius
                border.width: 1
                border.color: qgcPal.buttonBorder

                ColumnLayout {
                    id: membersLayout
                    anchors.fill: parent
                    anchors.margins: panel._pad
                    spacing: panel._pad * 0.4

                    RowLayout {
                        Layout.fillWidth: true

                        QGCLabel {
                            text: qsTr("MEMBERS")
                            font.bold: true
                            color: qgcPal.text
                            opacity: 0.6
                        }
                        Item { Layout.fillWidth: true }
                        QGCLabel {
                            text: qsTr("%1 in formation").arg(panel._model.formationMemberCount())
                            color: qgcPal.text
                            opacity: 0.45
                        }
                    }

                    Repeater {
                        model: panel._model
                        delegate: Rectangle {
                            required property int index
                            required property int sysid
                            required property string role
                            required property bool online
                            required property int position
                            required property int phase
                            required property int formationFlags

                            readonly property bool leaving: (formationFlags & 0x01) !== 0
                            readonly property bool leader: online && !leaving && sysid === panel._leaderSysid
                            readonly property bool selected: sysid === panel._selectedMember

                            Layout.fillWidth: true
                            Layout.preferredHeight: rowLayout.implicitHeight + panel._rowPad * 2
                            radius: Math.max(0, panel._radius - 3)
                            color: selected ? qgcPal.windowShadeLight
                                 : leader ? qgcPal.windowShade
                                 : "transparent"
                            opacity: online ? 1 : 0.55

                            Rectangle {
                                visible: selected
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 3
                                radius: 1.5
                                color: qgcPal.primaryButton
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: panel._selectedMember = sysid
                            }

                            RowLayout {
                                id: rowLayout
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: panel._pad
                                anchors.rightMargin: panel._pad
                                spacing: panel._pad * 0.55

                                Rectangle {
                                    Layout.alignment: Qt.AlignVCenter
                                    width: ScreenTools.defaultFontPixelHeight * 0.45
                                    height: width
                                    radius: width / 2
                                    color: !online ? qgcPal.buttonBorder
                                         : leaving ? qgcPal.warningText
                                         : leader ? qgcPal.primaryButton
                                         : qgcPal.colorGreen
                                }

                                QGCLabel {
                                    text: "0x" + sysid.toString(16).toUpperCase()
                                    font.family: ScreenTools.fixedFontFamily
                                    color: qgcPal.text
                                }

                                Rectangle {
                                    Layout.alignment: Qt.AlignVCenter
                                    Layout.preferredWidth: roleLabel.implicitWidth + panel._pad * 0.8
                                    Layout.preferredHeight: roleLabel.implicitHeight
                                                            + ScreenTools.defaultFontPixelHeight * 0.2
                                    radius: height / 2
                                    color: "transparent"
                                    border.width: 1
                                    border.color: leaving ? qgcPal.warningText
                                                : leader ? qgcPal.primaryButton
                                                : qgcPal.buttonBorder

                                    QGCLabel {
                                        id: roleLabel
                                        anchors.centerIn: parent
                                        text: leaving ? qsTr("LEAVING") : role.toUpperCase()
                                        font.bold: true
                                        font.pointSize: ScreenTools.defaultFontPointSize - 1
                                        color: leaving ? qgcPal.warningText
                                             : leader ? qgcPal.primaryButton
                                             : qgcPal.text
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                QGCLabel {
                                    text: position >= 0 ? qsTr("Slot %1").arg(position) : "—"
                                    color: qgcPal.text
                                    opacity: 0.65
                                }

                                QGCLabel {
                                    text: online ? panel._memberPhaseLabel(phase) : qsTr("Offline")
                                    color: qgcPal.text
                                    opacity: 0.65
                                }
                            }
                        }
                    }
                }
            }

            // 错误提示
            QGCLabel {
                Layout.fillWidth: true
                visible: text.length > 0
                wrapMode: Text.WordWrap
                color: qgcPal.colorRed
                text: panel._controller.errorString
            }

            // 操作
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: actionLayout.implicitHeight + panel._pad * 2
                color: qgcPal.windowShade
                radius: panel._radius
                border.width: 1
                border.color: qgcPal.buttonBorder

                ColumnLayout {
                    id: actionLayout
                    anchors.fill: parent
                    anchors.margins: panel._pad
                    spacing: panel._pad * 0.6

                    QGCLabel {
                        text: qsTr("ACTIONS")
                        font.bold: true
                        color: qgcPal.text
                        opacity: 0.6
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: panel._pad * 0.5

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: panel._pad

                            QGCButton {
                                Layout.minimumWidth: panel._actionButtonWidth
                                Layout.maximumWidth: panel._actionButtonWidth
                                text: qsTr("Apply")
                                primary: true
                                enabled: panel._canApply
                                onClicked: panel._controller.apply(panel._vehicles)
                            }
                            QGCButton {
                                Layout.minimumWidth: panel._actionButtonWidth
                                Layout.maximumWidth: panel._actionButtonWidth
                                text: qsTr("Stop")
                                enabled: panel._controller.phase === 2 || panel._controller.phase === 1
                                onClicked: panel._controller.stop(panel._vehicles, true)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: panel._pad

                            QGCButton {
                                Layout.minimumWidth: panel._actionButtonWidth
                                Layout.maximumWidth: panel._actionButtonWidth
                                text: qsTr("Match online")
                                onClicked: panel._model.matchOnline(panel._vehicles)
                            }
                            QGCButton {
                                Layout.minimumWidth: panel._actionButtonWidth
                                Layout.maximumWidth: panel._actionButtonWidth
                                text: qsTr("Takeoff all")
                                enabled: panel._controller.phase !== 1 && panel._onlineCount > 0
                                onClicked: panel._controller.takeoffAll(panel._vehicles)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: panel._pad

                            QGCButton {
                                Layout.minimumWidth: panel._actionButtonWidth
                                Layout.maximumWidth: panel._actionButtonWidth
                                text: qsTr("Land all")
                                enabled: panel._onlineCount > 0
                                onClicked: panel._controller.landAll(panel._vehicles)
                            }
                            QGCButton {
                                Layout.minimumWidth: panel._actionButtonWidth
                                Layout.maximumWidth: panel._actionButtonWidth
                                text: qsTr("Kick")
                                enabled: panel._selectedMember > 0 && panel._selectedOnline
                                         && !panel._selectedLeaving && panel._controller.phase !== 1
                                onClicked: panel._controller.kick(panel._vehicles, panel._selectedMember)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: panel._pad

                            QGCButton {
                                Layout.minimumWidth: panel._actionButtonWidth
                                Layout.maximumWidth: panel._actionButtonWidth
                                text: qsTr("Rejoin")
                                enabled: panel._selectedMember > 0 && panel._selectedOnline
                                         && panel._selectedLeaving
                                onClicked: panel._controller.rejoin(panel._vehicles, panel._selectedMember)
                            }
                            QGCButton {
                                Layout.minimumWidth: panel._actionButtonWidth
                                Layout.maximumWidth: panel._actionButtonWidth
                                text: qsTr("Leader focus")
                                enabled: panel._leaderSysid > 0 && panel._vehicleById(panel._leaderSysid) !== null
                                onClicked: {
                                    var v = panel._vehicleById(panel._leaderSysid)
                                    if (v) {
                                        QGroundControl.multiVehicleManager.activeVehicle = v
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    function _vehicleById(sysid) {
        for (var i = 0; i < _vehicles.count; i++) {
            var v = _vehicles.get(i)
            if (v && v.id === sysid) {
                return v
            }
        }
        return null
    }

    function _memberPhaseLabel(phase) {
        if (phase < 0 || phase >= panel._memberPhaseNames.length) {
            return "—"
        }
        return panel._memberPhaseNames[phase]
    }

    function _buildLivePositions() {
        void panel._tick
        var out = {}
        var leader = panel._vehicleById(_model.leaderSysid())
        if (!leader || !leader.coordinate.isValid) {
            for (var i = 0; i < _vehicles.count; i++) {
                var candidate = _vehicles.get(i)
                if (candidate && candidate.maplrsFirmware && candidate.coordinate.isValid) {
                    leader = candidate
                    break
                }
            }
        }
        if (!leader || !leader.coordinate.isValid) {
            return out
        }
        for (var j = 0; j < _vehicles.count; j++) {
            var vehicle = _vehicles.get(j)
            if (!vehicle || !vehicle.coordinate.isValid) {
                continue
            }
            var north = (vehicle.coordinate.latitude - leader.coordinate.latitude) * 111320
            var east = (vehicle.coordinate.longitude - leader.coordinate.longitude)
                       * 111320 * Math.cos(leader.coordinate.latitude * Math.PI / 180)
            out[vehicle.id] = { north: north, east: east }
        }
        return out
    }
}
