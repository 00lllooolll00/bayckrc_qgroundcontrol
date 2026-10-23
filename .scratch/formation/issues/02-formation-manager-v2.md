# T2 QGC：FormationManager v2

## Goal
把 mission 上传的 `dispatch()` 替换为 `applyFormation()`：在线成员 → 槽位 → 逐台
`sendMavCommand(USER_1)`；增加命名预设（QSettings JSON）与 `matchOnline()`。

## Scope
- `src/Vehicle/FormationManager.{h,cc}`
- 删除 mission 上传状态机（`DispatchStep` / `_uploadNext` / `_startAll` / `_makeItems`）与相关 include。
- 预设 key：`Formation/Presets/<name>`，内容 JSON（shape/spacing/heading/slotCount/assignments）。
- 保留 `formationStatus()` 误差显示（目标 = 领航机实时位置 + 偏移）。
- 领航机不接收 `USER_1`，只作为锚点。

## Acceptance
- Apply 对 N 台在线 MAPLRS 跟随机各发一条 `USER_1`，参数与 T1 一致；`formation_id` 自增。
- 保存/加载预设往返一致；重启 QGC 后预设仍在。
- 在线数变化不自动重排；`matchOnline()` 只改 `slotCount`。

## Blocking edges
- 前驱：T1。
- 解锁：T3、T8。
