import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlightMap
import QGroundControl.FlyView

Item {
    id:             multiVehicleListRoot
    property real   _margin:                    ScreenTools.defaultFontPixelWidth * 0.42
    property real   _widgetHeight:              ScreenTools.defaultFontPixelHeight * 2.2
    property var    _guidedController:          globals.guidedControllerFlyView
    property var    _activeVehicleColor:        "green"
    readonly property color _leaderColor:           "#F5B800"
    readonly property color _inFormationColor:      "#29B6F6"
    readonly property color _inFormationBadgeColor: "#0288D1"
    readonly property color _leavingColor:          "#FFA726"
    readonly property color _leavingBadgeColor:     "#EF6C00"
    readonly property color _leavingCardBg:         "#6E4518"
    property int    _formationTick:             0
    property var    _activeVehicle:             QGroundControl.multiVehicleManager.activeVehicle
    property var    selectedVehicles:           QGroundControl.multiVehicleManager.selectedVehicles

    implicitWidth:  ScreenTools.defaultFontPixelHeight * 18
    implicitHeight: vehicleList.contentHeight

    Connections {
        target: QGroundControl.formationController.model
        function onDataChanged() { multiVehicleListRoot._formationTick++ }
        function onConfigChanged() { multiVehicleListRoot._formationTick++ }
        function onModelReset() { multiVehicleListRoot._formationTick++ }
        function onRowsInserted() { multiVehicleListRoot._formationTick++ }
        function onRowsRemoved() { multiVehicleListRoot._formationTick++ }
        function onFormationActiveChanged() { multiVehicleListRoot._formationTick++ }
    }

    function armAvailable() {
        for (var i = 0; i < selectedVehicles.count; i++) {
            var vehicle = selectedVehicles.get(i)
            if (vehicle.armed === false) {
                return true
            }
        }
        return false
    }


    function disarmAvailable() {
        for (var i = 0; i < selectedVehicles.count; i++) {
            var vehicle = selectedVehicles.get(i)
            // 与单机 Disarm 一致：只有地面上的已解锁载具允许上锁。
            if (vehicle.armed === true && vehicle.flying === false) {
                return true
            }
        }
        return false
    }

    function startAvailable() {
        for (var i = 0; i < selectedVehicles.count; i++) {
            var vehicle = selectedVehicles.get(i)
            if (vehicle.armed === true && vehicle.flightMode !== vehicle.missionFlightMode){
                return true
            }
        }
        return false
    }

    function takeoffAvailable() {
        for (var i = 0; i < selectedVehicles.count; i++) {
            var vehicle = selectedVehicles.get(i)
            if (!vehicle.flying) {
                return true
            }
        }
        return false
    }

    function landAvailable() {
        for (var i = 0; i < selectedVehicles.count; i++) {
            if (selectedVehicles.get(i).armed === true) {
                return true
            }
        }
        return false
    }

    function rtlAvailable() {
        for (var i = 0; i < selectedVehicles.count; i++) {
            var vehicle = selectedVehicles.get(i)
            if (vehicle.armed === true && vehicle.flying === true && vehicle.supports.guidedMode &&
                vehicle.flightMode !== vehicle.rtlFlightMode &&
                vehicle.flightMode !== vehicle.landFlightMode) {
                return true
            }
        }
        return false
    }

    function pauseAvailable() {
        for (var i = 0; i < selectedVehicles.count; i++) {
            var vehicle = selectedVehicles.get(i)
            if (vehicle.armed === true && vehicle.supports.pauseVehicle) {
                return true
            }
        }
        return false
    }

    function selectVehicle(vehicleId) {
        QGroundControl.multiVehicleManager.selectVehicle(vehicleId)
    }

    function deselectVehicle(vehicleId) {
        QGroundControl.multiVehicleManager.deselectVehicle(vehicleId)
    }

    function toggleSelect(vehicleId) {
        if (!vehicleSelected(vehicleId)) {
            selectVehicle(vehicleId)
        } else {
            deselectVehicle(vehicleId)
        }
    }

    function selectAll() {
        var vehicles = QGroundControl.multiVehicleManager.vehicles
        for (var i = 0; i < vehicles.count; i++) {
            var vehicle = vehicles.get(i)
            var vehicleId = vehicle.id
            if (!vehicleSelected(vehicleId)) {
                selectVehicle(vehicleId)
            }
        }
    }

    function deselectAll() {
        QGroundControl.multiVehicleManager.deselectAllVehicles()
    }

    function vehicleSelected(vehicleId) {
        for (var i = 0; i < selectedVehicles.count; i++ ) {
            var currentId = selectedVehicles.get(i).id
            if (vehicleId === currentId) {
                return true
            }
        }
        return false
    }

    QGCListView {
        id:                 vehicleList
        anchors.left:       parent.left
        anchors.right:      parent.right
        anchors.top:        parent.top
        anchors.bottom:     parent.bottom
        spacing:            ScreenTools.defaultFontPixelWidth * 0.55 // _layoutMargin
        orientation:        ListView.Vertical
        model:              QGroundControl.multiVehicleManager.vehicles
        cacheBuffer:        _cacheBuffer < 0 ? 0 : _cacheBuffer
        clip:               true

        property real _cacheBuffer:     height * 2

        delegate: Rectangle {
            width:          vehicleList.width
            height:         innerColumn.height + _margin * 2
            color:          _isCommLost
                            ? (QGroundControl.multiVehicleManager.activeVehicle == _vehicle ? "#C62828" : "#8B1E1E")
                            : (QGroundControl.multiVehicleManager.activeVehicle == _vehicle
                                ? _activeVehicleColor
                                : (_isFormationLeaving ? _leavingCardBg : qgcPal.button))
            radius:         ScreenTools.defaultFontPixelWidth * 0.45
            border.width:   _vehicle && vehicleSelected(_vehicle.id) ? 2 : ((_isCommLost || _isFormationLeaving) ? 1 : 0)
            border.color:   _vehicle && vehicleSelected(_vehicle.id)
                            ? qgcPal.text
                            : (_isCommLost ? "#FF5252" : (_isFormationLeaving ? _leavingColor : qgcPal.text))

            property var    _vehicle:   object
            readonly property bool _isCommLost:
                _vehicle && _vehicle.vehicleLinkManager ? _vehicle.vehicleLinkManager.communicationLost : false
            readonly property bool _isMaplrs:
                _vehicle && _vehicle.maplrsFirmware
            readonly property bool _isInFormation: {
                void multiVehicleListRoot._formationTick
                return _isMaplrs && QGroundControl.formationController.model.isInFormation(_vehicle.id)
            }
            readonly property bool _isFormationLeaving: {
                void multiVehicleListRoot._formationTick
                if (!_isMaplrs || _isCommLost) {
                    return false
                }
                const model = QGroundControl.formationController.model
                return model.isLeaving(_vehicle.id) || (model.formationMemberCount() > 0 && !model.isInFormation(_vehicle.id))
            }
            readonly property bool _isFormationLeader: {
                void multiVehicleListRoot._formationTick
                return _isMaplrs && !_isFormationLeaving && QGroundControl.formationController.model.leaderSysid === _vehicle.id
            }

            // 左侧状态色条：失联红 > 脱离编队橙 > 领航机金 > 正常编队中蓝，不被白色多选框压住
            Rectangle {
                visible:            _isCommLost || _isFormationLeaving || _isFormationLeader || _isInFormation
                anchors.left:       parent.left
                anchors.top:        parent.top
                anchors.bottom:     parent.bottom
                anchors.margins:    3
                width:              4
                radius:             2
                color:              _isCommLost ? "#FF5252"
                                    : (_isFormationLeaving ? _leavingColor
                                    : (_isFormationLeader ? _leaderColor : _inFormationColor))
            }

            QGCMouseArea {
                anchors.fill:       parent
                acceptedButtons:    Qt.LeftButton | Qt.RightButton
                onClicked: (mouse) => {
                    if (mouse.button === Qt.RightButton) {
                        // 右键：多选/取消，作为批量操作（Arm/Disarm/Takeoff 等）的对象集合
                        toggleSelect(_vehicle.id)
                    } else {
                        // 左键：只把主界面（activeVehicle）切到它，不动多选集合
                        QGroundControl.multiVehicleManager.activeVehicle = _vehicle
                    }
                }
            }

            Column {
                id:                         innerColumn
                anchors.centerIn:           parent
                spacing:                    ScreenTools.defaultFontPixelHeight * 0.28

                RowLayout {
                    anchors.horizontalCenter:   parent.horizontalCenter
                    spacing:                    _margin

                    IntegratedCompassAttitude {
                        id: compassWidget
                        compassRadius:              _widgetHeight / 2 - attitudeSize / 2
                        compassBorder:              0
                        attitudeSize:               ScreenTools.defaultFontPixelWidth * 0.45
                        attitudeSpacing:            attitudeSize / 2
                        usedByMultipleVehicleList:   true
                        vehicle:                     _vehicle
                    }

                    QGCLabel {
                        text: "|"
                        font.pointSize:       ScreenTools.mediumFontPointSize
                        color:                qgcPal.text
                        Layout.alignment:     Qt.AlignHCenter
                    }

                    ColumnLayout {
                        spacing:            ScreenTools.defaultFontPixelHeight * 0.12
                        Layout.alignment:   Qt.AlignVCenter

                        QGCLabel {
                            text:                 _vehicle ? _vehicle.id : ""
                            font.pointSize:       ScreenTools.mediumFontPointSize
                            font.bold:            _isCommLost || _isFormationLeaving || _isFormationLeader
                            color:                _isCommLost ? "#FFCDD2"
                                                  : (_isFormationLeaving ? "#FFE0B2"
                                                  : (_isFormationLeader ? _leaderColor : qgcPal.text))
                            Layout.alignment:     Qt.AlignHCenter
                        }

                        Rectangle {
                            visible:                _isCommLost || _isFormationLeaving || _isFormationLeader || _isInFormation
                            Layout.alignment:       Qt.AlignHCenter
                            Layout.preferredWidth:  leaderBadgeLabel.implicitWidth + _margin * 2
                            Layout.preferredHeight: leaderBadgeLabel.implicitHeight + ScreenTools.defaultFontPixelHeight * 0.12
                            radius:                 height / 2
                            color:                  _isCommLost ? "#D32F2F"
                                                    : (_isFormationLeaving ? _leavingBadgeColor
                                                    : (_isFormationLeader ? _leaderColor : _inFormationBadgeColor))

                            QGCLabel {
                                id:                 leaderBadgeLabel
                                anchors.centerIn:   parent
                                text:               _isCommLost
                                                    ? ("! " + qsTr("Comms Lost"))
                                                    : (_isFormationLeaving
                                                        ? qsTranslate("FormationPanel", "LEAVING")
                                                        : (_isFormationLeader
                                                            ? ("\u2605 " + qsTranslate("FormationPanel", "Leader"))
                                                            : qsTr("In Formation")))
                                font.pointSize:     ScreenTools.defaultFontPointSize - 2
                                font.bold:          true
                                color:              (_isFormationLeader && !_isCommLost && !_isFormationLeaving) ? "#1A1A1A" : "#FFFFFF"
                            }
                        }
                    }

                    QGCLabel {
                        text: "|"
                        font.pointSize:       ScreenTools.mediumFontPointSize
                        color:                qgcPal.text
                        Layout.alignment:     Qt.AlignHCenter
                    }

                    ColumnLayout {
                        spacing:              ScreenTools.defaultFontPixelHeight * 0.12
                        Layout.alignment:     Qt.AlignCenter

                        FlightModeMenu {
                            Layout.alignment:     Qt.AlignHCenter
                            font.pointSize:       ScreenTools.mediumFontPointSize
                            color:                qgcPal.text
                            currentVehicle:       _vehicle
                        }

                        QGCLabel {
                            Layout.alignment:     Qt.AlignHCenter
                            text:                 _isCommLost
                                                  ? qsTr("Comms Lost")
                                                  : (_vehicle && _vehicle.armed ? qsTr("Armed") : qsTr("Disarmed"))
                            font.bold:            _isCommLost
                            color:                _isCommLost ? "#FFCDD2" : qgcPal.text
                        }
                    }
                }

                QGCFlickable {
                    id:                         telemFlickable
                    anchors.horizontalCenter:   parent.horizontalCenter
                    width:          Math.min(contentWidth, vehicleList.width - _margin * 2)
                    height:         control.height
                    contentWidth:   control.width
                    contentHeight:  control.height
                    interactive:    false

                    TelemetryValuesBar {
                        id:                     control
                        interactive:            false
                        barMargin:              ScreenTools.defaultFontPixelWidth * 0.55
                        settingsGroup:          factValueGrid.vehicleCardSettingsGroup
                        specificVehicleForCard: _vehicle
                    }
                }

                Rectangle {
                    id:                         rfPill
                    anchors.horizontalCenter:   parent.horizontalCenter
                    visible:                    (_vehicle && _vehicle.maplrsFirmware) || _hasRfSignal
                    width:                      Math.min(vehicleList.width - _margin * 2, Math.max(rfRow.implicitWidth + _margin * 2.5, telemFlickable.width))
                    height:                     rfRow.implicitHeight + ScreenTools.defaultFontPixelHeight * 0.25
                    radius:                     ScreenTools.defaultFontPixelWidth * 0.45
                    color:                      Qt.rgba(qgcPal.window.r, qgcPal.window.g, qgcPal.window.b, 0.75)

                    readonly property var  _radioStatus: _vehicle ? _vehicle.radioStatus : null
                    readonly property bool _hasRfSignal: !_isCommLost && (_radioStatus ? _radioStatus.telemetryAvailable : false)

                    RowLayout {
                        id:                 rfRow
                        anchors.centerIn:   parent
                        spacing:            ScreenTools.defaultFontPixelWidth * 0.85

                        QGCLabel {
                            text:           "RSSI " + (rfPill._hasRfSignal ? (rfPill._radioStatus.lrssi.rawValue + " dBm") : "-- dBm")
                            color:          qgcPal.text
                            font.pointSize: ScreenTools.defaultFontPointSize
                        }

                        QGCLabel {
                            text:           "SNR " + (rfPill._hasRfSignal ? (rfPill._radioStatus.snr.rawValue + " dB") : "-- dB")
                            color:          qgcPal.text
                            font.pointSize: ScreenTools.defaultFontPointSize
                        }
                    }
                }
            }
        }
    }
}
