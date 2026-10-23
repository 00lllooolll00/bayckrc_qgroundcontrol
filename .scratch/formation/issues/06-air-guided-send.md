# T6 固件 AIR：GUIDED 目标发送接口

## Goal
给 formation 模块一个"发 `SET_POSITION_TARGET_GLOBAL_INT`"的稳定接口，并封装
POSHOLD + CH10 的前置时序。

## Scope
- `src/main/app/air/fc_link.c`（或新 `guided_target.c`）：打包
  `MAV_FRAME_GLOBAL_RELATIVE_ALT_INT`、`type_mask`=位置+yaw、target=FC sysid/AUTOPILOT1，
  走 `_send_mavlink_frame`。
- 进入 GUIDED：`cmd_wrapper_nav_set(POSHOLD)` + `cmd_wrapper_gcs_nav_set(true)`；
  心跳 `custom_mode==16` 后才开始流。
- 退出：`cmd_wrapper_gcs_nav_set(false)`，保持 POSHOLD。

## Acceptance
- 抓 UART：帧长/字段/CRC 正确；FC 侧 `isGCSValid()` 成立后 `setWaypoint(255)`。
- 未进 POSHOLD 时不流（或流了被忽略）且不阻塞其它链路。

## Blocking edges
- 无前驱。
- 解锁：T5。
