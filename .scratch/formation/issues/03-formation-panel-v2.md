# T3 QGC：FormationPanel v2

## Goal
在现有面板上加 2D 预览画布（北向上）、朝向控件、在线数/匹配按钮、预设下拉与 Apply。

## Scope
- `src/FlyView/FormationPanel.qml`
- 画布：按 `FormationShape` 槽位画点，领航机高亮；v2.0 只预览、不拖拽。
- 朝向 0–359（默认 0）；间距/成员数沿用；预设保存/加载/删除。
- Apply 调 `formationManager.applyFormation(vehicles)`。

## Acceptance
- 在线 N 台时画布显示 N 个槽位；改间距/朝向实时重绘。
- Apply 后显示每台 ACK 结果（成功/超时）。
- 预设切换能恢复 shape/spacing/heading/slotCount/assignments。

## Blocking edges
- 前驱：T2。
- 解锁：T8。
