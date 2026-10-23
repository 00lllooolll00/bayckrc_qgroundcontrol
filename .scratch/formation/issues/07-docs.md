# T7 文档：v2 协议真源

## Goal
把 `FORMATION_DESIGN.md` 从 v1（mission）改写为 v2（USER 命令 + AIR 闭环），并同步
`README.md` / `doc/config/how_to_config_inav.md`。

## Scope
- 协议表、时序、前置条件（`isGCSValid`）、失败处理、非目标。
- 修正 v1 的错误结论："INAV 不支持实时 setpoint"。

## Acceptance
- 文档与实现逐字段一致；命令编号/通道/上限无歧义。

## Blocking edges
- 无前驱（可立即做）。
