# BayckRC 编队协议系统设计（Formation Protocol）

> 状态：v2 设计定稿（QGC 侧 + 固件侧待实现）。本文档是协议真源。
> v1（mission 上传）已废弃，代码保留在 git 历史（`786056e6`）。

## 0. 决策记录（2026-10）

| 决策 | 结论 |
|---|---|
| 控制拓扑 | **分布式**：每台 AIR 自己算槽位目标，5Hz 喂自己的飞控 |
| 传输载体 | `MAV_CMD_USER_1/2/3`（`COMMAND_LONG`），GND 白名单 + 逐机转发；不改方言 |
| 锚点 | **跟随领航机实时位置**（相对偏移，持续保持） |
| 编辑器 | v2.0 模板 + 间距/朝向；自由拖拽排 v2.1 |
| 持久化 | QGC 命名预设（QSettings JSON）；AIR 只保留运行态 |

## 1. 目标

QGC 面板读取当前在线的 AIR 节点 → 编辑队形 → 通过 MAVLink 下发到 GND → GND 按成员转发
→ 每台 AIR 自主计算自己的槽位目标，并用 GUIDED（CH10 GCS NAV）持续控制本机飞控。

## 2. 硬约束（取证结论）

- QGC 看到的 sysid = AIR 成员 node_id（0x10 起），compid = `MAV_COMP_ID_AUTOPILOT1`。
- GND 命令白名单 + **不支持 `target_system=0` 广播**（v2.0 由 QGC 逐机单播）。
- 空口单帧 ≤ **56B**（`FC_BRIDGE_FRAME_MAX`，2026-10 由 50 放宽）；`COMMAND_LONG` 线帧 45B，装得下。
- GND 每成员命令槽单槽、每超级帧一帧：N 台 × 1 条命令 ≈ 1 个超级帧（10 机 1.2~1.4s）。
- **AIR→FC 是直连 UART，不受 GND 白名单约束**。
- INAV GUIDED 门槛 `isGCSValid()`：已解锁 + `estPosStatus>=EST_TRUSTED` + `gpsOrigin.valid`
  + **CH10 GCS NAV** + `navState == NAV_STATE_POSHOLD_3D_IN_PROGRESS`。
- INAV 的 `SET_POSITION_TARGET_GLOBAL_INT` **完整位置分支已实现**（`mavlink_guided.c:126-165`）：
  x/y 不忽略时写 `setWaypoint(255)`，lat/lon/alt/yaw 全部生效；相对帧 `p3=0`。
- `air_info` 带 lat/lon/alt/relative_alt，**不含航向**；`neighbor_table` 提供
  `nt_relative_ned()` 与 leader/follower 角色。

## 3. 协议 v2

### 3.1 命令参数（`COMMAND_LONG`，`target_system` = 成员 node_id）

| 命令 | 值 | p1 | p2 | p3 | p4 | p5 | p6 | p7 |
|---|---|---|---|---|---|---|---|---|
| `FORM_ASSIGN`（USER_1） | 31010 | formation_id | slot_index | leader_id | dx_east_m | dy_north_m | dz_m | yaw_deg |
| `FORM_STOP`（USER_2） | 31011 | formation_id | 0=本机 / 1=全队 | — | — | — | — | — |
| `FORM_QUERY`（USER_3，预留） | 31012 | formation_id | mask | — | — | — | — | — |

- `dx/dy` 是**已按队形朝向旋转到东/北**的偏移（QGC 负责旋转），AIR 不再旋转。
- `dz` 相对领航机相对高度；`yaw_deg`：0 = 保持当前机头，1..359 = 目标机头（INAV 只认 1..359）。
- `formation_id` 单调递增（QGC 每次 Apply +1，跳过 0）；AIR 丢弃比当前小的 id。
- `leader_id=0` 视为停止本机编队。
- 速率/死区是固件常量：**5Hz / 0.3m**（v2.0 不下发）。
- 领航机（slot 0）**不接收** `FORM_ASSIGN`：它保持当前模式作为锚点，跟随者用它的 mesh 位置。

### 3.2 时序

```
QGC                     GND                     AIR（每台跟随机）            INAV
 |-- USER_1 (逐机) ----->|-- COMMAND_LONG ------>|                            |
 |   formation_id/slot   |   (per member slot)   |-- formation_set()           |
 |                       |                       |-- CH7=POSHOLD ------------->|
 |                       |                       |-- CH10=1500 ---------------->|
 |                       |                       |<-- heartbeat custom_mode=16-|
 |                       |                       |-- SET_POSITION_TARGET ------>
 |                       |                       |   GLOBAL_INT (5Hz)          |-- isGCSValid()
 |                       |                       |<-- GLOBAL_POSITION_INT -----|   setWaypoint(255)
 |<-- 心跳/GPI ----------|<-- air_info ----------|                            |
```

### 3.3 目标计算（AIR）

- 领航机相对向量：`nt_relative_ned(leader_id, &rel)`（x=北，y=东，z=下，米）。
- 本机 GPI：`g_snapshot.gpi`（lat/lon 1e7，relative_alt 毫米）。
- 目标北 = 本机北 + `rel.x` + `dy_north`；目标东 = 本机东 + `rel.y` + `dx_east`。
- 目标相对高度 = 本机 relative_alt − `rel.z` + `dz`。
- 经纬度换算：`dlat = north / 111320`；`dlon = east / (111320 * cos(lat))`。
- 领航数据龄期 > **1s** → 冻结目标（不再更新），FC 保持最后一点。

### 3.4 启动 / 停止 / 失败

- **启动前提**：已解锁、FC 位置/高度估计可用、已进入 POSHOLD。条件不满足时只存参数，
  不流目标，等条件满足再流。
- **停止**：`FORM_STOP` 或上锁 → 停止流目标，`CH10=false`，**保持 POSHOLD 悬停**，不自动 RTL。
- **领航失联**：冻结最后目标；QGC 面板标红，不自动重排。
- **编队 id 过期**：丢弃。
- **碰撞**：v2.0 由 QGC 在 Apply 前校验最小间距（默认 ≥3m）；**不把 APF/CPA 混进 GUIDED 目标**
  （避免与飞控打架）。

## 4. 模块改动

### QGC（`feat/formation`）

| 文件 | 改动 |
|---|---|
| `src/Vehicle/FormationShape.{h,cc}` | 复用；补 Grid |
| `src/Vehicle/FormationProtocol.{h,cc}`（新） | 纯函数：命令参数打包、formation_id 生成 |
| `src/Vehicle/FormationManager.{h,cc}` | `dispatch()` → `applyFormation()`；命名预设；`matchOnline()`；保留槽位/误差 |
| `src/FlyView/FormationPanel.qml` | 2D 预览画布、朝向、在线数、预设、Apply |
| `test/Vehicle/FormationShapeTest.cc` | 补 Grid/旋转用例 |
| `test/Vehicle/FormationProtocolTest.cc`（新） | 参数打包/边界 |

### 固件（maplrs `feat/formation`）

| 文件 | 改动 |
|---|---|
| `src/components/wrapper/src/cmd_wrapper.c` | 白名单加 `USER_1/2/3`（`local=true`） |
| `src/main/app/air/radio_link_air.c` | 拦截 `USER_*` → `formation_*`；本地 ACK；置 CH7/CH10 |
| `src/main/app/air/formation.{h,c}`（新） | 状态 + 目标计算 + 5Hz tick |
| `src/main/app/air/fc_link.c` | 暴露 GUIDED 发送接口；workqueue 挂 5Hz tick；上锁/失锁停编队 |
| `README.md`、`doc/config/how_to_config_inav.md` | 命令表 / 前置条件 / 频率 |

## 5. 非目标（v2.0）

- 自由拖拽任意槽位（v2.1）。
- 队形朝向跟随领航机航向（需要 `air_info` 加航向字段）。
- GND 广播扇出（QGC 逐机单播）。
- AIR 侧 NVS 持久化。
- 编队中混用 APF/CPA。

## 6. 测试

- QGC：`FormationShape` / `FormationProtocol` 纯函数单测；Manager 预设序列化单测。
- 固件：目标计算纯函数单测（给定本机 GPI + 领航相对 NED + 偏移 → 目标 lat/lon/alt）。
- 联调（2 机）：Apply 后进入 POSHOLD+GUIDED 且误差收敛；停止后悬停；领航失联冻结。
