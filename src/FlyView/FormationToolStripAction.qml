import QGroundControl
import QGroundControl.FlyView

// FlyView 左侧工具条的编队入口（参照 GripperButton 模式）
ToolStripAction {
    id:         action
    text:       qsTr("编队")
    iconSource: "/res/FlyingPaperPlane.svg"

    dropPanelComponent: Component {
        FormationPanel {
        }
    }
}
