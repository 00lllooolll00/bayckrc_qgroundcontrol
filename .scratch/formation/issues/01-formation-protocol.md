# T1 QGC：编队协议纯函数层

## Goal
新增 `FormationProtocol`（纯函数，不依赖 Qt 对象）：把一次 Apply 的队形参数打包成
`MAV_CMD_USER_1/2` 的 7 个 float 参数，并生成单调递增的 `formation_id`。

## Scope
- `src/Vehicle/FormationProtocol.{h,cc}`
- `test/Vehicle/FormationProtocolTest.cc`
- 常量：`USER_1=31010`、`USER_2=31011`、`USER_3=31012`；协议版本 1。

## Acceptance
- 给定 slot/leader/dx/dy/dz/yaw/formation_id，输出参数与 `FORMATION_DESIGN.md` §3.1 逐字段一致。
- `formation_id` 从 1 递增、跳过 0；超过 2^24 回绕。
- 单测覆盖 0/负偏移、yaw=0、id 回绕。

## Blocking edges
- 无前驱。
- 解锁：T2、T4。
