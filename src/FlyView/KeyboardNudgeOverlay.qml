import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

Item {
    id: _root
    objectName: "keyboardNudgeOverlay"

    required property var targetWindow
    property bool shortcutEnabled: true
    property bool panelExpanded: false

    property string _feedbackText: ""
    property var _flyViewSettings: QGroundControl.settingsManager.flyViewSettings
    property var _activeVehicle: QGroundControl.multiVehicleManager.activeVehicle

    QGCPalette {
        id: qgcPal
        colorGroupEnabled: true
    }

    KeyboardNudgeController {
        id: nudgeController
        objectName: "keyboardNudgeController"
        window: _root.targetWindow
        enabled: _root.shortcutEnabled

        onNudgeFeedback: (message, targetCoord) => {
            _root._feedbackText = message
            feedbackTimer.restart()
        }
    }

    Timer {
        id: feedbackTimer
        interval: 2200
        repeat: false
    }

    // 右下角紧凑方向控制卡（默认折叠，不遮挡地图与仪表）
    Rectangle {
        id: nudgePanel
        visible: _root.shortcutEnabled && _activeVehicle !== null
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: ScreenTools.defaultFontPixelWidth * 1.5
        anchors.bottomMargin: ScreenTools.defaultFontPixelHeight * 4.5
        width: panelCol.implicitWidth + ScreenTools.defaultFontPixelWidth * 1.6
        height: panelCol.implicitHeight + ScreenTools.defaultFontPixelHeight * 1.0
        radius: ScreenTools.defaultBorderRadius
        color: qgcPal.windowShadeDark
        border.color: qgcPal.colorBlue
        border.width: 1
        opacity: 0.92

        ColumnLayout {
            id: panelCol
            anchors.centerIn: parent
            spacing: ScreenTools.defaultFontPixelHeight * 0.35

            RowLayout {
                Layout.fillWidth: true
                spacing: ScreenTools.defaultFontPixelWidth * 0.5

                QGCLabel {
                    text: qsTr("WSAD %1m").arg(_flyViewSettings.keyboardNudgeStepMeters.rawValue.toFixed(0))
                    font.pointSize: ScreenTools.smallFontPointSize
                    font.bold: true
                }

                QGCButton {
                    text: _root.panelExpanded ? qsTr("收起") : qsTr("方向键")
                    onClicked: _root.panelExpanded = !_root.panelExpanded
                }
            }

            ColumnLayout {
                visible: _root.panelExpanded
                spacing: ScreenTools.defaultFontPixelHeight * 0.3

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    QGCButton {
                        text: qsTr("前 W")
                        onClicked: nudgeController.nudge(1, 0)
                    }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: ScreenTools.defaultFontPixelWidth * 0.4
                    QGCButton {
                        text: qsTr("左 A")
                        onClicked: nudgeController.nudge(0, -1)
                    }
                    QGCButton {
                        text: qsTr("后 S")
                        onClicked: nudgeController.nudge(-1, 0)
                    }
                    QGCButton {
                        text: qsTr("右 D")
                        onClicked: nudgeController.nudge(0, 1)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: ScreenTools.defaultFontPixelWidth * 0.4
                    QGCButton {
                        Layout.fillWidth: true
                        text: nudgeController.headingRelative ? qsTr("机头向") : qsTr("正北向")
                        onClicked: nudgeController.headingRelative = !nudgeController.headingRelative
                    }
                }
            }
        }
    }

    Rectangle {
        id: nudgeToast
        visible: feedbackTimer.running && _root._feedbackText.length > 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: ScreenTools.defaultFontPixelHeight * 7.5
        width: toastLabel.implicitWidth + ScreenTools.defaultFontPixelWidth * 3
        height: toastLabel.implicitHeight + ScreenTools.defaultFontPixelHeight * 0.9
        radius: ScreenTools.defaultBorderRadius
        color: qgcPal.windowShade
        border.color: qgcPal.colorBlue
        border.width: 1
        opacity: 0.95

        QGCLabel {
            id: toastLabel
            anchors.centerIn: parent
            text: _root._feedbackText
            color: qgcPal.text
            font.bold: true
        }
    }
}
