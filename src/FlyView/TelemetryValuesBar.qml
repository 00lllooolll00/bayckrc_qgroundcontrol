import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

Item {
    id:             control
    implicitWidth:  mainLayout.width + (barMargin * 2)
    implicitHeight: mainLayout.height + (barMargin * 2)

    property real extraWidth: 0 ///< Extra width to add to the background rectangle
    property bool interactive: true
    property real barMargin:   _toolsMargin

    property alias factValueGrid:           factValueGrid
    property alias settingsGroup:           factValueGrid.settingsGroup
    property alias specificVehicleForCard:  factValueGrid.specificVehicleForCard

    onInteractiveChanged: {
        if (!interactive) {
            factValueGrid.settingsUnlocked = false
        }
    }

    Rectangle {
        id:         backgroundRect
        width:      control.width + extraWidth
        height:     control.height
        color:      qgcPal.window
        radius:     ScreenTools.defaultFontPixelWidth / 2
        opacity:    0.75
    }

    ColumnLayout {
        id:                 mainLayout
        anchors.margins:    barMargin
        anchors.bottom:     parent.bottom
        anchors.left:       parent.left

        RowLayout {
            visible: control.interactive && factValueGrid.settingsUnlocked

            QGCColoredImage {
                source:             "qrc:/InstrumentValueIcons/lock-open.svg"
                mipmap:             true
                width:              ScreenTools.minTouchPixels * 0.75
                height:             width
                sourceSize.width:   width
                color:              qgcPal.text
                fillMode:           Image.PreserveAspectFit

                QGCMouseArea {
                    anchors.fill: parent
                    onClicked:    factValueGrid.settingsUnlocked = false
                }
            }
        }

        HorizontalFactValueGrid {
            id: factValueGrid
        }
    }

    QGCMouseArea {
        id:                         mouseArea
        x:                          mainLayout.x
        y:                          mainLayout.y
        width:                      mainLayout.width
        height:                     mainLayout.height
        acceptedButtons:            Qt.LeftButton | Qt.RightButton
        propagateComposedEvents:    true
        enabled:                    control.interactive
        visible:                    control.interactive && !factValueGrid.settingsUnlocked

        onClicked: (mouse) => {
            if (!ScreenTools.isMobile && mouse.button === Qt.RightButton) {
                factValueGrid.settingsUnlocked = true
                mouse.accepted = true
            }
        }

        onPressAndHold: (mouse) => {
            factValueGrid.settingsUnlocked = true
            mouse.accepted = true
        }
    }
}
