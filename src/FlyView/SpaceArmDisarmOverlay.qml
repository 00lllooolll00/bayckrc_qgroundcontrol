import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

Item {
    id: _root
    objectName: "spaceArmDisarmOverlay"

    required property var targetWindow
    property bool shortcutEnabled: true
    readonly property alias controller: spaceController

    property string _feedbackText: ""
    property bool _feedbackEmergency: false

    QGCPalette {
        id: qgcPal
        colorGroupEnabled: true
    }

    SpaceArmDisarmController {
        id: spaceController
        objectName: "spaceArmDisarmController"
        window: _root.targetWindow
        enabled: _root.shortcutEnabled

        onActionFeedback: (message, isEmergency) => {
            _root._feedbackText = message
            _root._feedbackEmergency = isEmergency
            feedbackTimer.restart()
        }
    }

    Timer {
        id: feedbackTimer
        interval: 2200
        repeat: false
    }

    Rectangle {
        id: holdIndicator
        objectName: "spaceArmHoldIndicator"
        visible: spaceController.holding
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: ScreenTools.defaultFontPixelHeight * 4
        width: Math.max(ScreenTools.defaultFontPixelWidth * 32, holdColumn.implicitWidth + ScreenTools.defaultFontPixelWidth * 3)
        height: holdColumn.implicitHeight + ScreenTools.defaultFontPixelHeight * 1.2
        radius: ScreenTools.defaultBorderRadius
        color: qgcPal.windowShadeDark
        border.color: spaceController.holdingEmergencyDisarm ? qgcPal.alertBorder : qgcPal.colorBlue
        border.width: spaceController.holdingEmergencyDisarm ? 2 : 1
        opacity: 0.94

        ColumnLayout {
            id: holdColumn
            anchors.centerIn: parent
            width: parent.width - ScreenTools.defaultFontPixelWidth * 2
            spacing: ScreenTools.defaultFontPixelHeight * 0.35

            RowLayout {
                Layout.fillWidth: true
                spacing: ScreenTools.defaultFontPixelWidth

                QGCLabel {
                    Layout.fillWidth: true
                    text: spaceController.holdPromptText
                    color: qgcPal.text
                    font.bold: true
                    elide: Text.ElideRight
                }

                QGCLabel {
                    text: qsTr("松开取消")
                    color: qgcPal.colorGrey
                    font.pointSize: ScreenTools.smallFontPointSize
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(6, Math.round(ScreenTools.defaultFontPixelHeight * 0.35))
                radius: Layout.preferredHeight / 2
                color: qgcPal.window

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: Math.max(0, Math.min(parent.width, parent.width * spaceController.holdProgress))
                    radius: parent.radius
                    color: spaceController.holdingEmergencyDisarm
                               ? qgcPal.colorRed
                               : (spaceController.multiVehicleHold ? qgcPal.colorOrange : qgcPal.colorGreen)
                }
            }
        }
    }

    Rectangle {
        id: feedbackToast
        objectName: "spaceArmFeedbackToast"
        visible: !spaceController.holding && feedbackTimer.running && _root._feedbackText.length > 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: ScreenTools.defaultFontPixelHeight * 4
        width: feedbackLabel.implicitWidth + ScreenTools.defaultFontPixelWidth * 3
        height: feedbackLabel.implicitHeight + ScreenTools.defaultFontPixelHeight * 1.0
        radius: ScreenTools.defaultBorderRadius
        color: _root._feedbackEmergency ? qgcPal.alertBackground : qgcPal.windowShade
        border.color: _root._feedbackEmergency ? qgcPal.alertBorder : qgcPal.colorBlue
        border.width: _root._feedbackEmergency ? 2 : 1
        opacity: 0.96

        QGCLabel {
            id: feedbackLabel
            objectName: "spaceArmFeedbackLabel"
            anchors.centerIn: parent
            text: _root._feedbackText
            color: _root._feedbackEmergency ? qgcPal.alertText : qgcPal.text
            font.bold: true
        }
    }
}
