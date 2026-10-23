# T4 固件 GND：白名单放行 USER_1/2/3

## Goal
GND 接受 QGC 的 FORM 命令并按 `target_system` 转发到对应成员时隙；参数原样透传。

## Scope
- `src/components/wrapper/src/cmd_wrapper.c`：`s_cmd_table` 加 `USER_1/2/3`，`local=true`。
- 不改 GND 的 per-member 转发逻辑（已有）。
- 更新 `README.md` 命令表与 `doc/config/how_to_config_inav.md`。

## Acceptance
- 抓包：QGC 发 `USER_1` 后 AIR 侧收到同参数 `COMMAND_LONG`，且 AIR 不转发给 FC。
- 未列入白名单的命令仍回 `UNSUPPORTED`。
- `just build` + `just test-build` 通过。

## Blocking edges
- 前驱：T1（命令字/参数表定稿）。
- 解锁：T5。
