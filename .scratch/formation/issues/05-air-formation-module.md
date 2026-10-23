# T5 固件 AIR：formation 模块与 5Hz 目标流

## Goal
AIR 拦截 `USER_1/2`，保存队形状态，按领航机相对位置计算自己的目标，并 5Hz 流式
`SET_POSITION_TARGET_GLOBAL_INT` 给本机飞控；停止/失锁/领航失联按设计处理。

## Scope
- 新 `src/main/app/air/formation.{h,c}`：状态、目标计算、tick。
- `src/main/app/air/radio_link_air.c`：`USER_1/2` 分支（local，不进 FC）；置 CH7/CH10；ACK。
- `src/main/app/air/fc_link.c`：workqueue 挂 5Hz tick；上锁/失锁停编队。
- `src/main/app/air/mesh_link.c`：无改动（用 `nt_relative_ned`）。
- 任务表无需新增（复用 `fc_link` workqueue）。

## Acceptance
- 单测：给定本机 GPI + 领航相对 NED + 偏移 → 目标 lat/lon/alt 正确（含负偏移/南半球）。
- 台架：Apply 后 AIR 发 `SET_POSITION_TARGET_GLOBAL_INT`，INAV 进 GUIDED 并收敛。
- 领航失联 1s 后目标冻结；`FORM_STOP` 后停流并保持 POSHOLD。
- `just build` + `just test-build` 通过。

## Blocking edges
- 前驱：T4、T6。
- 解锁：T8。
