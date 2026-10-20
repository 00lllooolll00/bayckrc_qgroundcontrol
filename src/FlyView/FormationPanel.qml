import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

// 编队设计面板：在线数/匹配、队形模板/间距/朝向/成员数、2D 预览、槽位分配、
// 命名预设、下发与实时编队误差。协议见 FORMATION_DESIGN.md。
// 预览为北向上平面图（x=东向右，y=北向上），槽 0 = 领航机。
ColumnLayout {
    id: panel

    spacing: ScreenTools.defaultFontHeight / 2
    width:   ScreenTools.defaultFontPixelWidth * 30

    property var  _formation:   QGroundControl.formationManager
    property var  _vehicles:    QGroundControl.multiVehicleManager.vehicles
    property int  _tick:        0       // 每秒自增，驱动在线数与误差绑定重算
    property var  _presetNames: []
    property var  _onlineIds:   {
        void _tick
        return _formation.maplrsVehicleIds(_vehicles)
    }
    property bool _anyMaplrs:   _onlineIds.length > 0

    // 预览用最大槽位半径（米），用于自动缩放；至少 1 避免除零
    readonly property real _slotExtent: {
        var slots = _formation.formationSlots
        var maxExtent = 1
        for (var i = 0; i < slots.length; i++) {
            maxExtent = Math.max(maxExtent, Math.abs(slots[i].x), Math.abs(slots[i].y))
        }
        return maxExtent
    }

    QGCPalette { id: qgcPal }

    // 预设加载会直接改 Manager 属性，SpinBox/ComboBox 的双向绑定在用户操作后已断开，
    // 因此在这里显式回填控件。
    function _syncControls() {
        _shapeCombo.currentIndex = _formation.shapeIndex
        _spacingSpin.value      = _formation.spacing
        _headingSpin.value      = _formation.headingDeg
        _slotSpin.value         = _formation.slotCount
    }

    function _presetName() {
        return _presetNameField.text.trim()
    }

    function _refreshPresets(selectName) {
        _presetNames = _formation.presetNames()
        _presetCombo.currentIndex = selectName ? Math.max(0, _presetNames.indexOf(selectName)) : -1
    }

    function _presetMessage(text, ok) {
        _presetResult.text  = text
        _presetResult.color = ok ? qgcPal.colorGreen : qgcPal.warningText
        _presetResultTimer.restart()
    }

    function _savePreset() {
        var name = _presetName()
        if (!name) {
            _presetMessage(qsTr("Enter a preset name"), false)
            return
        }
        var ok = _formation.savePreset(name)
        _refreshPresets(name)
        _presetMessage(ok ? qsTr("Preset \"%1\" saved").arg(name) : qsTr("Failed to save preset"), ok)
    }

    function _loadPreset() {
        var name = _presetName()
        var ok = name !== "" && _formation.loadPreset(name)
        if (ok) {
            _syncControls()
            _presetCombo.currentIndex = Math.max(0, _presetNames.indexOf(name))
        }
        _presetMessage(ok ? qsTr("Preset \"%1\" loaded").arg(name) : qsTr("Failed to load preset"), ok)
    }

    function _deletePreset() {
        var name = _presetName()
        var ok = name !== "" && _formation.deletePreset(name)
        _refreshPresets()
        _presetMessage(ok ? qsTr("Preset \"%1\" deleted").arg(name) : qsTr("Failed to delete preset"), ok)
    }

    Component.onCompleted: _refreshPresets()

    QGCLabel {
        text:           qsTr("Formation")
        font.pointSize: ScreenTools.largeFontPointSize
        font.bold:      true
    }

    RowLayout {
        Layout.fillWidth: true
        spacing:          ScreenTools.defaultFontPixelWidth

        QGCLabel {
            Layout.fillWidth: true
            text:             qsTr("Online MAPLRS: %1").arg(_onlineIds.length)
            color:            _anyMaplrs ? qgcPal.text : qgcPal.warningText
        }

        QGCButton {
            text:      qsTr("Match online")
            enabled:   _anyMaplrs && !_formation.busy
            onClicked: _formation.matchOnline(_vehicles)
        }
    }

    GridLayout {
        columns:       2
        columnSpacing: ScreenTools.defaultFontHeight
        Layout.fillWidth: true

        QGCLabel { text: qsTr("Shape") }
        QGCComboBox {
            id:                 _shapeCombo
            Layout.fillWidth:   true
            model:              [qsTr("Line Abreast"), qsTr("Wedge"), qsTr("Column"), qsTr("Circle")]
            currentIndex:       _formation.shapeIndex
            onActivated: (index) => _formation.shapeIndex = index
        }

        QGCLabel { text: qsTr("Spacing (m)") }
        SpinBox {
            id:              _spacingSpin
            Layout.fillWidth: true
            from:             1
            to:               200
            value:            _formation.spacing
            onValueModified:  _formation.spacing = value
        }

        QGCLabel { text: qsTr("Heading (deg)") }
        SpinBox {
            id:              _headingSpin
            Layout.fillWidth: true
            from:             0
            to:               359
            wrap:             true
            editable:         true
            value:            _formation.headingDeg
            onValueModified:  _formation.headingDeg = value
        }

        QGCLabel { text: qsTr("Members") }
        SpinBox {
            id:              _slotSpin
            Layout.fillWidth: true
            from:             1
            to:               20
            value:            _formation.slotCount
            onValueModified:  _formation.slotCount = value
        }
    }

    // 北向上 2D 预览：按 formationSlots 的东/北偏移画点，自动缩放到可用区域
    Item {
        id:                     preview
        Layout.fillWidth:       true
        Layout.preferredHeight: width * 0.85
        clip:                   true

        readonly property real _margin: ScreenTools.defaultFontPixelHeight * 1.8
        // 槽位最远点落在可用矩形内，留出点半径与边距
        readonly property real _scale: Math.max(0, Math.min((width - 2 * _margin) / (2 * panel._slotExtent),
                                                            (height - 2 * _margin) / (2 * panel._slotExtent)))

        Rectangle {
            anchors.fill:   parent
            color:          qgcPal.windowShade
            border.color:   qgcPal.windowShadeDark
            border.width:   1
        }

        QGCLabel {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top:              parent.top
            anchors.topMargin:        ScreenTools.defaultFontPixelHeight * 0.3
            text:                     qsTr("N")
            font.bold:                true
        }

        Repeater {
            model: panel._formation.formationSlots

            Rectangle {
                id:                         dot
                required property int index
                required property var modelData

                readonly property bool isLeader: index === 0
                readonly property real dotSize: (isLeader ? 1.3 : 1.0) * ScreenTools.defaultFontPixelHeight * 1.1

                width:      dotSize
                height:     dotSize
                radius:     dotSize / 2
                x:          preview.width / 2 + modelData.x * preview._scale - width / 2
                y:          preview.height / 2 - modelData.y * preview._scale - height / 2
                color:      isLeader ? qgcPal.colorGreen : qgcPal.button
                border.color: isLeader ? qgcPal.colorGreen : qgcPal.buttonBorder
                border.width: isLeader ? 2 : 1

                QGCLabel {
                    anchors.centerIn: parent
                    text:             dot.isLeader ? qsTr("L") : dot.index
                    font.pointSize:   ScreenTools.smallFontPointSize
                    color:            dot.isLeader ? qgcPal.buttonText : qgcPal.text
                }
            }
        }
    }

    QGCLabel {
        Layout.fillWidth: true
        wrapMode:         Text.WordWrap
        text:             qsTr("Slot assignment (slot 0 = leader, not sent a FORM_ASSIGN)")
        font.bold:        true
        visible:          _anyMaplrs
    }

    Repeater {
        model: _formation.formationSlots

        GridLayout {
            columns:       3
            columnSpacing: ScreenTools.defaultFontHeight / 2
            Layout.fillWidth: true

            readonly property int slotIndex: index
            readonly property var slotEntry: modelData
            // _tick 参与表达式让绑定每秒重算 formationStatus
            readonly property var _status: {
                void _tick
                var list = _formation.formationStatus(_vehicles)
                return list[slotIndex]
            }

            QGCLabel { text: qsTr("Slot ") + slotIndex + (slotIndex === 0 ? qsTr(" (Leader)") : "") }

            QGCComboBox {
                Layout.fillWidth: true
                textRole: "text"
                valueRole: "value"
                model: {
                    var options = [{ "text": qsTr("(Empty)"), "value": 0 }]
                    var ids = panel._onlineIds
                    for (var i = 0; i < ids.length; i++) {
                        options.push({ "text": "AIR 0x" + ids[i].toString(16), "value": ids[i] })
                    }
                    return options
                }
                currentIndex: {
                    var entryVehicle = slotEntry.vehicleId
                    var options = model
                    for (var i = 0; i < options.length; i++) {
                        if (options[i].value === entryVehicle) return i
                    }
                    return 0
                }
                onActivated: (index) => _formation.assignSlot(slotIndex, currentValue)
            }

            QGCLabel {
                text: {
                    if (!_status) return ""
                    if (_status.vehicleId === 0) return qsTr("Unassigned")
                    if (_status.errorM < 0) return qsTr("Offline")
                    return qsTr("Error %1 m").arg(_status.errorM.toFixed(1))
                }
            }
        }
    }

    QGCButton {
        Layout.fillWidth: true
        text:      qsTr("Auto Assign")
        visible:   _anyMaplrs
        enabled:   !_formation.busy
        onClicked: _formation.autoAssign(_vehicles)
    }

    QGCLabel { text: qsTr("Presets"); font.bold: true }

    QGCComboBox {
        id:               _presetCombo
        Layout.fillWidth: true
        model:            _presetNames
        currentIndex:     -1
        onActivated: (index) => _presetNameField.text = textAt(index)
    }

    QGCTextField {
        id:              _presetNameField
        Layout.fillWidth: true
        placeholderText: qsTr("Preset name")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing:          ScreenTools.defaultFontPixelWidth / 2

        QGCButton {
            Layout.fillWidth: true
            text:             qsTr("Save")
            onClicked:        _savePreset()
        }

        QGCButton {
            Layout.fillWidth: true
            text:             qsTr("Load")
            enabled:          _presetName() !== ""
            onClicked:        _loadPreset()
        }

        QGCButton {
            Layout.fillWidth: true
            text:             qsTr("Delete")
            enabled:          _presetName() !== ""
            onClicked:        _deletePreset()
        }
    }

    QGCLabel {
        id:         _presetResult
        Layout.fillWidth: true
        wrapMode:   Text.WordWrap
        text:       ""
        visible:    text !== ""
        color:      qgcPal.colorGreen
    }

    QGCButton {
        Layout.fillWidth: true
        text:      _formation.busy ? qsTr("Applying…") : qsTr("Apply formation")
        enabled:   !_formation.busy && _anyMaplrs
        onClicked: _formation.applyFormation(_vehicles)
    }

    QGCLabel {
        Layout.fillWidth: true
        wrapMode:         Text.WordWrap
        visible:          _formation.busy
        text:             qsTr("Applying formation — do not operate the vehicles…")
        color:            qgcPal.warningText
    }

    QGCLabel {
        Layout.fillWidth: true
        wrapMode:         Text.WordWrap
        visible:          !_anyMaplrs
        text:             qsTr("No online AIR nodes (MAPLRS cluster not connected)")
        color:            qgcPal.warningText
    }

    QGCLabel {
        id:         _result
        Layout.fillWidth: true
        wrapMode:   Text.WordWrap
        text:       ""
        visible:    text !== ""
    }

    Timer {
        interval: 1000
        running:  true
        repeat:   true
        onTriggered: panel._tick += 1
        onRunningChanged: if (running) panel._tick = 0
    }

    Timer {
        id: _presetResultTimer
        interval: 5000
        onTriggered: _presetResult.text = ""
    }

    Connections {
        target: _formation

        function onDispatchFinished(success) {
            _result.text    = success ? qsTr("Formation applied") : qsTr("Formation apply failed")
            _result.color   = success ? qgcPal.colorGreen : qgcPal.warningText
            _resultTimer.restart()
        }
    }

    Timer {
        id: _resultTimer
        interval: 5000
        onTriggered: _result.text = ""
    }
}
