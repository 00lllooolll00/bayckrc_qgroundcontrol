# T8 联调验证：2 机闭环

## Goal
在 QGC → GND → AIR → INAV（SITL/台架）上验证编队 Apply/保持/停止/失联四条路径。

## Scope
- 2 台 AIR + 2 套 SITL（或实机）；QGC 面板 Apply。
- 记录：`formation_id`、每台 ACK、FC 模式、位置误差、领航失联行为。
- 通过标准：误差 < 2m 并稳定；停止后悬停；失联冻结不漂移。

## Blocking edges
- 前驱：T2、T3、T4、T5、T6。
