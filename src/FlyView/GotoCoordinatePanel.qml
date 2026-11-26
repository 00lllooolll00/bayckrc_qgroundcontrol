pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtPositioning

import QGroundControl
import QGroundControl.Controls

Item {
    id: panel

    width: ScreenTools.defaultFontPixelWidth * 34
    implicitWidth: width
    implicitHeight: content.implicitHeight + _pad * 2

    readonly property real _pad: ScreenTools.defaultFontPixelWidth
    property var _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle
    property var _flyViewSettings: QGroundControl.settingsManager.flyViewSettings
    property int _altMode: 0 // 0=保持当前, 1=相对高度(m), 2=海拔AMSL(m)
    property string _errorText: ""

    readonly property double _parsedLat: Number(latField.text)
    readonly property double _parsedLon: Number(lonField.text)
    readonly property bool _coordValid: !isNaN(_parsedLat) && !isNaN(_parsedLon)
                                        && _parsedLat >= -90 && _parsedLat <= 90
                                        && _parsedLon >= -180 && _parsedLon <= 180
                                        && !(Math.abs(_parsedLat) < 1e-6 && Math.abs(_parsedLon) < 1e-6)
    readonly property double _previewDistanceM: {
        if (!_coordValid || !_activeVehicle || !_activeVehicle.coordinate.isValid) {
            return -1
        }
        return _activeVehicle.coordinate.distanceTo(QtPositioning.coordinate(_parsedLat, _parsedLon))
    }

    Component.onCompleted: _fillFromVehicle()

    function _fillFromVehicle() {
        if (_activeVehicle && _activeVehicle.coordinate.isValid) {
            latField.text = _activeVehicle.coordinate.latitude.toFixed(7)
            lonField.text = _activeVehicle.coordinate.longitude.toFixed(7)
            const relAlt = _activeVehicle.altitudeRelative.rawValue
            if (!isNaN(relAlt)) {
                altField.text = Number(relAlt).toFixed(1)
            }
        }
    }

    QGCPalette { id: qgcPal }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: panel._pad
        spacing: ScreenTools.defaultFontPixelHeight * 0.5

        RowLayout {
            Layout.fillWidth: true
            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("输入经纬度飞往目标点")
                font.bold: true
            }
            QGCButton {
                text: qsTr("填入当前")
                enabled: panel._activeVehicle && panel._activeVehicle.coordinate.isValid
                onClicked: panel._fillFromVehicle()
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: ScreenTools.defaultFontPixelWidth
            rowSpacing: ScreenTools.defaultFontPixelHeight * 0.35

            QGCLabel { text: qsTr("纬度 (Lat)") }
            QGCTextField {
                id: latField
                Layout.fillWidth: true
                placeholderText: "-90 ~ 90"
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }

            QGCLabel { text: qsTr("经度 (Lon)") }
            QGCTextField {
                id: lonField
                Layout.fillWidth: true
                placeholderText: "-180 ~ 180"
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }

            QGCLabel { text: qsTr("高度模式") }
            QGCComboBox {
                Layout.fillWidth: true
                model: [qsTr("保持当前高度"), qsTr("相对起飞点 (m)"), qsTr("海拔 AMSL (m)")]
                currentIndex: panel._altMode
                onActivated: (idx) => panel._altMode = idx
            }

            QGCLabel {
                text: qsTr("目标高度 (m)")
                visible: panel._altMode !== 0
            }
            QGCTextField {
                id: altField
                Layout.fillWidth: true
                visible: panel._altMode !== 0
                placeholderText: "20.0"
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }
        }

        QGCLabel {
            Layout.fillWidth: true
            visible: panel._previewDistanceM >= 0
            text: qsTr("距当前载具: %1 m").arg(panel._previewDistanceM.toFixed(1))
            color: panel._previewDistanceM > panel._flyViewSettings.maxGoToLocationDistance.rawValue
                       ? qgcPal.colorRed : qgcPal.text
            font.pointSize: ScreenTools.smallFontPointSize
        }

        QGCLabel {
            Layout.fillWidth: true
            visible: panel._errorText.length > 0
            text: panel._errorText
            color: qgcPal.colorRed
            wrapMode: Text.WordWrap
            font.pointSize: ScreenTools.smallFontPointSize
        }

        QGCButton {
            Layout.fillWidth: true
            text: qsTr("飞往该点")
            primary: true
            enabled: panel._activeVehicle !== null && panel._coordValid
            onClicked: {
                panel._errorText = ""
                let targetAmsl = 0.0
                if (panel._altMode === 1) {
                    const relVal = Number(altField.text)
                    const curAmsl = panel._activeVehicle.altitudeAMSL.rawValue
                    const curRel = panel._activeVehicle.altitudeRelative.rawValue
                    if (isNaN(relVal) || isNaN(curAmsl) || isNaN(curRel)) {
                        panel._errorText = qsTr("相对高度或当前海拔无效")
                        return
                    }
                    targetAmsl = Number(curAmsl) - Number(curRel) + relVal
                } else if (panel._altMode === 2) {
                    const amslVal = Number(altField.text)
                    if (isNaN(amslVal)) {
                        panel._errorText = qsTr("请输入有效的海拔高度")
                        return
                    }
                    targetAmsl = amslVal
                }

                const coord = QtPositioning.coordinate(panel._parsedLat, panel._parsedLon, targetAmsl)
                if (!panel._activeVehicle.guidedModeGotoLocation(coord, 0)) {
                    panel._errorText = qsTr("发送失败：检查模式或最大飞行距离限制")
                }
            }
        }
    }
}
