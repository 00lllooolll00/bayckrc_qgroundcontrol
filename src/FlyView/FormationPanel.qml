import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

// 编队设计面板：队形模板/间距/槽位分配/下发/实时编队误差。
// 协议见 FORMATION_DESIGN.md：v1 走标准 mission 路径（单点 LOITER 任务）。
ColumnLayout {
    spacing: ScreenTools.defaultFontHeight / 2

    property var _formation:    QGroundControl.formationManager
    property var _vehicles:     QGroundControl.multiVehicleManager.vehicles
    property var _shapeNames:   [qsTr("Line Abreast"), qsTr("Wedge"), qsTr("Column"), qsTr("Circle")]
    property bool _anyMaplrs:   _formation.maplrsVehicleIds(_vehicles).length > 0
    property int  _tick:        0   // 每秒自增，驱动误差绑定重算

    QGCLabel {
        text:           qsTr("Formation")
        font.pointSize: ScreenTools.largeFontPointSize
        font.bold:      true
    }

    GridLayout {
        columns:       2
        columnSpacing: ScreenTools.defaultFontHeight

        QGCLabel { text: qsTr("Shape") }
        QGCComboBox {
            Layout.fillWidth: true
            model:            _shapeNames
            currentIndex:     _formation.shapeIndex
            onActivated: (index) => _formation.shapeIndex = index
        }

        QGCLabel { text: qsTr("Spacing (m)") }
        SpinBox {
            Layout.fillWidth: true
            from:             1
            to:               200
            value:            _formation.spacing
            onValueModified:  _formation.spacing = value
        }

        QGCLabel { text: qsTr("Members") }
        SpinBox {
            Layout.fillWidth: true
            from:             1
            to:               20
            value:            _formation.slotCount
            onValueModified:  _formation.slotCount = value
        }
    }

    QGCLabel {
        text:     qsTr("Slot assignment (slot 0 = leader, not rewritten by mission upload)")
        font.bold: true
        visible:  _anyMaplrs
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
                    var ids = _formation.maplrsVehicleIds(_vehicles)
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
        text:      qsTr("Auto Assign")
        visible:   _anyMaplrs
        onClicked: _formation.autoAssign(_vehicles)
    }

    QGCButton {
        text:    _formation.busy ? qsTr("Sending…") : qsTr("Send Formation")
        enabled: !_formation.busy && _anyMaplrs
        onClicked: _formation.dispatch(_vehicles)
    }

    QGCLabel {
        visible: _formation.busy
        text:    qsTr("Sending formation — do not operate the vehicle…")
        color:   "orange"
    }

    QGCLabel {
        visible: !_anyMaplrs
        text:    qsTr("No online AIR nodes (MAPLRS cluster not connected)")
        color:   "orange"
    }

    QGCLabel {
        id: _result
        property var _text: ""
        text:    _text
        visible: _text !== ""
    }

    Timer {
        interval: 1000
        running:  _anyMaplrs && !_formation.busy
        repeat:   true
        onTriggered: _tick += 1
        onRunningChanged: if (running) _tick = 0
    }

    Connections {
        target: _formation
        function onDispatchFinished(success) {
            _result._text = success ? qsTr("Formation sent") : qsTr("Formation send failed")
            _resultTimer.restart()
        }
    }

    Timer {
        id: _resultTimer
        interval: 5000
        onTriggered: _result._text = ""
    }
}
