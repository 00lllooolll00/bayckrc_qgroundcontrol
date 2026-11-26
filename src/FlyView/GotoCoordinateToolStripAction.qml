import QGroundControl
import QGroundControl.FlyView

ToolStripAction {
    id: action
    text: qsTr("定点飞行")
    iconSource: "/res/GotoCoordinate.svg"

    dropPanelComponent: Component {
        GotoCoordinatePanel {
        }
    }
}
