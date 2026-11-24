import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.FactControls
import QGroundControl.Controls
import QGroundControl.FlyView
import QGroundControl.FlightMap

Rectangle {
    id:             topRightPanel
    width:          contentWidth
    height:         Math.max(contentHeight, minimumHeight)
    color:          qgcPal.toolbarBackground
    radius:         ScreenTools.defaultFontPixelHeight * 0.45
    visible:        !QGroundControl.videoManager.fullScreen && _multipleVehicles && _settingEnableMVPanel
    clip:           true

    property real  _panelMargin:            ScreenTools.defaultFontPixelHeight * 0.4
    property real  _pageContentMargin:      ScreenTools.defaultFontPixelHeight * 0.8
    property real  _buttonHeight:           ScreenTools.defaultFontPixelHeight * 1.45
    property real  _actionButtonMinWidth:   ScreenTools.defaultFontPixelHeight * 2.45

    property bool _settingEnableMVPanel:    QGroundControl.settingsManager.appSettings.enableMultiVehiclePanel.value
    property bool  _multipleVehicles:       QGroundControl.multiVehicleManager.vehicles.count > 1
    property var   vehicles:                QGroundControl.multiVehicleManager.vehicles
    property var   selectedVehicles:        QGroundControl.multiVehicleManager.selectedVehicles
    property real  contentWidth:            Math.max(
                                                multiVehicleList.implicitWidth,
                                                swipeViewContainer.implicitWidth
                                            ) + _panelMargin * 2
    property real  contentHeight:           Math.min(
                                                maximumHeight,
                                                topRightPanelColumnLayout.implicitHeight + topRightPanelColumnLayout.anchors.margins * 2
                                            )
    property real  minimumHeight:           swipeViewContainer.implicitHeight
    property real  maximumHeight

    QGCPalette { id: qgcPal }

    DeadMouseArea {
        anchors.fill:       parent
    }

    ColumnLayout {
        id:                 topRightPanelColumnLayout
        anchors.fill:       parent
        anchors.margins:    topRightPanel.color.a ? topRightPanel._panelMargin : 0
        spacing:            ScreenTools.defaultFontPixelWidth * 0.6 // _layoutMargin

        MultiVehicleList {
            id:                    multiVehicleList
            Layout.fillWidth:      true
            Layout.fillHeight:     true

            Rectangle {
                anchors.fill: parent
                visible:      topRightPanel.height === maximumHeight

                Rectangle {
                    anchors.left:       parent.left
                    anchors.right:      parent.right
                    anchors.top:        parent.top
                    anchors.margins:    0
                    height:             1
                    color:              QGroundControl.globalPalette.groupBorder
                }

                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.00; color: topRightPanel.color }
                    GradientStop { position: 0.05; color: "transparent" }

                    GradientStop { position: 0.95; color: "transparent" }
                    GradientStop { position: 1.00; color: topRightPanel.color }
                }

                Rectangle {
                    anchors.left:       parent.left
                    anchors.right:      parent.right
                    anchors.bottom:     parent.bottom
                    anchors.margins:    0
                    height:             1
                    color:              QGroundControl.globalPalette.groupBorder
                }
            }

        }

        Rectangle {
            id:                     swipeViewContainer
            Layout.fillWidth:       true
            implicitHeight:         swipePages.implicitHeight
            implicitWidth:          swipePages.implicitWidth
            color:                  "transparent"

            QGCSwipeView {
                id:                swipePages
                anchors.fill:      parent
                spacing:           ScreenTools.defaultFontPixelHeight
                implicitHeight:    Math.max(buttonsPage.implicitHeight, photoVideoPage.implicitHeight)
                implicitWidth:     Math.max(buttonsPage.implicitWidth, photoVideoPage.implicitWidth)

                MvPanelPage {
                    id:                buttonsPage
                    contentMargin:     topRightPanel._pageContentMargin
                    implicitHeight:    buttonsColumnLayout.implicitHeight + contentMargin * 2.5
                    implicitWidth:     Math.max(selectionRowLayout.implicitWidth, actionRowLayout.implicitWidth, multiActionLabel.implicitWidth) + contentMargin * 2

                    ColumnLayout {
                        id:                     buttonsColumnLayout
                        anchors.right:          parent.right
                        anchors.left:           parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        spacing:                ScreenTools.defaultFontPixelHeight * 0.35

                        QGCLabel {
                            text: {
                                let ids = Array.from({length: selectedVehicles.count}, (_, i) =>
                                    selectedVehicles.get(i).id
                                ).sort((a, b) => a - b)
                                .join(", ")
                                return qsTr("Vehicles Selected: ") + (ids ? ids : "-")
                            }
                            Layout.fillWidth:    true
                            horizontalAlignment: Text.AlignHCenter
                            elide:               Text.ElideRight
                        }

                        RowLayout {
                            id:                 selectionRowLayout
                            Layout.alignment:   Qt.AlignHCenter
                            spacing:            ScreenTools.defaultFontPixelWidth * 0.75

                            QGCButton {
                                text:                  qsTr("Select All")
                                enabled:               multiVehicleList.selectedVehicles && multiVehicleList.selectedVehicles.count !== QGroundControl.multiVehicleManager.vehicles.count
                                onClicked:             multiVehicleList.selectAll()
                                topPadding:            0
                                bottomPadding:         0
                                leftPadding:           ScreenTools.defaultFontPixelWidth * 1.5
                                rightPadding:          ScreenTools.defaultFontPixelWidth * 1.5
                                Layout.preferredHeight: topRightPanel._buttonHeight
                            }

                            QGCButton {
                                text:                  qsTr("Deselect All")
                                enabled:               multiVehicleList.selectedVehicles && multiVehicleList.selectedVehicles.count > 0
                                onClicked:             multiVehicleList.deselectAll()
                                topPadding:            0
                                bottomPadding:         0
                                leftPadding:           ScreenTools.defaultFontPixelWidth * 1.5
                                rightPadding:          ScreenTools.defaultFontPixelWidth * 1.5
                                Layout.preferredHeight: topRightPanel._buttonHeight
                            }

                        }


                        QGCLabel {
                            id:                multiActionLabel
                            text:              qsTr("Multi Vehicle Actions")
                            Layout.alignment:  Qt.AlignHCenter
                        }

                        RowLayout {
                            id:                actionRowLayout
                            Layout.alignment:  Qt.AlignHCenter
                            spacing:           ScreenTools.defaultFontPixelWidth * 0.45

                            QGCButton {
                                text:                   qsTr("Arm")
                                enabled:                multiVehicleList.armAvailable()
                                onClicked:              _guidedController.confirmAction(_guidedController.actionMVArm)
                                topPadding:             0
                                bottomPadding:          0
                                leftPadding:            0
                                rightPadding:           0
                                Layout.preferredHeight: topRightPanel._buttonHeight
                                Layout.preferredWidth:  Math.max(topRightPanel._actionButtonMinWidth, implicitContentWidth + ScreenTools.defaultFontPixelWidth)
                            }

                            QGCButton {
                                text:                   qsTr("Disarm")
                                enabled:                multiVehicleList.disarmAvailable()
                                onClicked:              _guidedController.confirmAction(_guidedController.actionMVDisarm)
                                topPadding:             0
                                bottomPadding:          0
                                leftPadding:            0
                                rightPadding:           0
                                Layout.preferredHeight: topRightPanel._buttonHeight
                                Layout.preferredWidth:  Math.max(topRightPanel._actionButtonMinWidth, implicitContentWidth + ScreenTools.defaultFontPixelWidth)
                            }

                            QGCButton {
                                text:                   qsTr("Takeoff")
                                enabled:                multiVehicleList.takeoffAvailable()
                                onClicked:              _guidedController.confirmAction(_guidedController.actionMVTakeoff)
                                topPadding:             0
                                bottomPadding:          0
                                leftPadding:            0
                                rightPadding:           0
                                Layout.preferredHeight: topRightPanel._buttonHeight
                                Layout.preferredWidth:  Math.max(topRightPanel._actionButtonMinWidth, implicitContentWidth + ScreenTools.defaultFontPixelWidth)
                            }

                            QGCButton {
                                text:                   qsTr("Land")
                                enabled:                multiVehicleList.landAvailable()
                                onClicked:              _guidedController.confirmAction(_guidedController.actionMVLand)
                                topPadding:             0
                                bottomPadding:          0
                                leftPadding:            0
                                rightPadding:           0
                                Layout.preferredHeight: topRightPanel._buttonHeight
                                Layout.preferredWidth:  Math.max(topRightPanel._actionButtonMinWidth, implicitContentWidth + ScreenTools.defaultFontPixelWidth)
                            }

                            QGCButton {
                                text:                   qsTr("Return")
                                enabled:                multiVehicleList.rtlAvailable()
                                onClicked:              _guidedController.confirmAction(_guidedController.actionMVRTL)
                                topPadding:             0
                                bottomPadding:          0
                                leftPadding:            0
                                rightPadding:           0
                                Layout.preferredHeight: topRightPanel._buttonHeight
                                Layout.preferredWidth:  Math.max(topRightPanel._actionButtonMinWidth, implicitContentWidth + ScreenTools.defaultFontPixelWidth)
                            }

                            QGCButton {
                                text:                   qsTr("Start")
                                enabled:                multiVehicleList.startAvailable()
                                onClicked:              _guidedController.confirmAction(_guidedController.actionMVStartMission)
                                topPadding:             0
                                bottomPadding:          0
                                leftPadding:            0
                                rightPadding:           0
                                Layout.preferredHeight: topRightPanel._buttonHeight
                                Layout.preferredWidth:  Math.max(topRightPanel._actionButtonMinWidth, implicitContentWidth + ScreenTools.defaultFontPixelWidth)
                            }

                            QGCButton {
                                text:                   qsTr("Pause")
                                enabled:                multiVehicleList.pauseAvailable()
                                onClicked:              _guidedController.confirmAction(_guidedController.actionMVPause)
                                topPadding:             0
                                bottomPadding:          0
                                leftPadding:            0
                                rightPadding:           0
                                Layout.preferredHeight: topRightPanel._buttonHeight
                                Layout.preferredWidth:  Math.max(topRightPanel._actionButtonMinWidth, implicitContentWidth + ScreenTools.defaultFontPixelWidth)
                            }
                        }
                    }
                } // Page 1

                MvPanelPage {

                    id:                  photoVideoPage
                    contentMargin:       topRightPanel._pageContentMargin
                    implicitHeight:      photoVideoControlLoader.implicitHeight + contentMargin * 2.5
                    implicitWidth:       photoVideoControlLoader.implicitWidth + contentMargin

                    // We use a Loader to load the photoVideoControlComponent only when the active vehicle is not null
                    // This make it easier to implement PhotoVideoControl without having to check for the mavlink camera
                    // to be null all over the place

                    Loader {
                        id:                         photoVideoControlLoader
                        anchors.horizontalCenter:   parent.horizontalCenter
                        sourceComponent:            globals.activeVehicle ? photoVideoControlComponent : undefined

                        property real rightEdgeCenterInset: visible ? parent.width - x : 0

                        Component {
                            id: photoVideoControlComponent

                            PhotoVideoControl {
                            }
                        }
                    }
                } // Page 2
            } // QGCSwipeView

            QGCPageIndicator {
                id:                       pageIndicator
                count:                    swipePages.count
                currentIndex:             swipePages.currentIndex
                anchors.bottom:           parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.margins:          ScreenTools.defaultFontPixelHeight * 0.25

                delegate: Rectangle {
                    height:    ScreenTools.defaultFontPixelHeight * 0.42
                    width:     height
                    radius:    width / 2
                    color:     model.index === pageIndicator.currentIndex ? qgcPal.text : qgcPal.button
                    opacity:   model.index === pageIndicator.currentIndex ? 0.9 : 0.3
                }
            }
        }
    }
}
