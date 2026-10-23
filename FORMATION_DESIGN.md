# BayckRC 编队协议系统设计（Formation Protocol v3）

> 状态：v3 定稿。本文档是协议真源。
> v2 的东/北偏移与 `56B` 表述已废弃；v1（mission 上传）保留在 git 历史。

## 0. 决策记录

| 决策 | 结论 |
|---|---|
| 控制拓扑 | 分布式：每台 AIR 自算槽位目标，5Hz 喂本机飞控 |
| 传输载体 | `MAV_CMD_USER_1/2/3`（`COMMAND_LONG`），GND 白名单逐机转发，不改方言 |
| 锚点 | 跟随领机实时位置与机头：相对偏移由 AIR 用领机 `yaw` 旋转到东/北 |
| 坐标系 | QGC 编辑用极坐标（距离 + 机头基准角度），下发用机体坐标 `forward/right` |
| 领机 | 手动指定，默认在线最深 `node_id` 最小的 AIR |
| 状态回传 | `air_info` 扩到 43B（机头 + 编队状态），GND 转 `ATTITUDE` 与 `STATUSTEXT` |
| 停止 | 保持 POSHOLD 悬停，不自动 RTH |
| 持久化 | QGC 命名预设（QSettings JSON）；AIR 只保留运行态 |

## 1. 目标

QGC 面板读取当前在线 AIR → 画布拖拽或输入距离/角度 → 逐机下发 `FORM_ASSIGN` → 每台 AIR 用领航机实时机头把 `forward/right` 旋转成东/北，5Hz 调 `SET_POSITION_TARGET_GLOBAL_INT` 控制本机 INAV GUIDED。状态经 `air_info` 回 QGC，冻结/离线可区分。

## 2. 硬约束

- QGC 看到的 sysid = AIR 成员 `node_id`（0x10 起），compid = `MAV_COMP_ID_AUTOPILOT1`。
- GND 命令白名单，不支持 `target_system = 0` 广播；QGC 逐机单播。
- 空口单帧 ≤ **50B**（`FC_BRIDGE_FRAME_MAX`）；`COMMAND_LONG` 线帧 45B 装得下。TELEMETRY 载荷 = 4B 回执 + 43B `air_info` = 47B。
- GND 每成员命令槽单槽、每超级帧一帧；10 机约 1.2~1.4s。
- AIR→FC 直连 UART，不受 GND 白名单约束。
- INAV GUIDED 门槛 `isGCSValid()`：已解锁 + `estPosStatus >= EST_TRUSTED` + `gpsOrigin.valid` + CH10 GCS NAV + `navState == NAV_STATE_POSHOLD_3D_IN_PROGRESS`。
- INAV 的 `SET_POSITION_TARGET_GLOBAL_INT` 完整位置分支已实现（`mavlink_guided.c`），`MAV_FRAME_GLOBAL_RELATIVE_ALT_INT` 写 `setWaypoint(255)`。
- `neighbor_table` 提供 `nt_relative_ned()` 与 `nt_node_t.hdg`（v3 起该字段存机头）。

## 3. 协议 v3

### 3.1 控制面（`COMMAND_LONG`，`target_system` = 成员 node_id）

| 命令 | 值 | p1 | p2 | p3 | p4 | p5 | p6 | p7 |
|---|---|---|---|---|---|---|---|---|
| `FORM_ASSIGN`（USER_1） | 31010 | formation_id | slot | leader_id | forward_m | right_m | dz_m | yaw_deg |
| `FORM_STOP`（USER_2） | 31011 | formation_id | scope（0 本机 / 1 全队） | — | — | — | — | — |
| `FORM_QUERY`（USER_3，预留） | 31012 | formation_id | mask | — | — | — | — | — |

- `forward_m/right_m` 是机体坐标（前 +、右 +）。QGC 从极坐标换算：`forward = r·cosθ`、`right = r·sinθ`；θ 以领机机头为 0°、顺时针。
- AIR 用领航机实时 yaw 旋转到东/北：`east = forward·sinψ + right·cosψ`、`north = forward·cosψ − right·sinψ`。
- `dz` 相对领机相对高度；`yaw_deg`：0 = 保持当前机头，1..359 = 目标机头。
- `formation_id` 从 1 起、跳过 0、上限 `2^24`；同一 Apply 的所有成员共用一个 id；AIR 丢弃比当前小的 id。
- `leader_id = 0` 等同停止本机；领机（slot 0）不收 `FORM_ASSIGN`。
- 限幅：`|forward|,|right| ≤ 500`、`|dz| ≤ 100`、`slot ≤ 15`；越界回 `MAV_RESULT_DENIED`。
- 速率/死区是固件常量：5Hz / 0.3m。

### 3.2 状态面

`air_info` 从 40B 扩到 43B，尾部追加：

```
[40..41] yaw u16 BE（机头，百分之一度）
[42]     formation_state：slot(bit0-3) | phase(bit4-5) | leader_online(bit6) | reserved(bit7)
```

`phase`：`0 = Idle`、`1 = WaitingGCS`、`2 = Streaming`、`3 = Frozen`。

GND 在 phase 变化或每 5s 发 `STATUSTEXT`：

```
FORM:<node_id>,<slot>,<phase>,<leader_online>
```

GND 同时用 `air_info.yaw` 构造 `ATTITUDE`（roll/pitch = 0）发给 QGC，QGC 的 `VehicleFactGroup.heading` 即机头。

### 3.3 时序

```
QGC                      GND                      AIR                      INAV
 |-- USER_1 (逐机) ------>|-- COMMAND_LONG -------->|                          |
 |                        |  (per member slot)      |-- formation_assign()     |
 |                        |                         |-- CH7=POSHOLD ---------->|
 |                        |                         |-- CH10=1500 ------------>|
 |                        |                         |<-- ATTITUDE/心跳 --------|
 |                        |                         |-- SET_POSITION_TARGET -->
 |                        |                         |   GLOBAL_INT (5Hz)       |-- isGCSValid()
 |                        |<-- TELEMETRY(air_info) -|<-- GLOBAL_POSITION_INT --|   setWaypoint(255)
 |<-- STATUSTEXT/ATTITUDE-|                         |                          |
```

### 3.4 目标计算（AIR）

```
rel = nt_relative_ned(leader_id)      // x = 北, y = 东, z = 下, 米
north = rel.x + forward·cosψ − right·sinψ
east  = rel.y + forward·sinψ + right·cosψ
alt   = self_rel_alt − rel.z + dz

lat = self_lat + north / 111320
lon = self_lon + east  / (111320·cos(self_lat))
```

`ψ` 是领航机机头（百分之一度 → 度）。领航数据龄期 > 1s 进 `Frozen`，停止更新目标。

### 3.5 启动 / 停止 / 失败

- **启动前提**：已解锁、定位可用、已进入 POSHOLD。条件不满足时停在 `WaitingGCS`，不流目标。
- **停止**：`FORM_STOP` 或上锁 → 停止流目标，CH10 拉低，保持 POSHOLD 悬停。
- **领航失联**：`Frozen`，冻结目标；QGC 面板黄色。
- **编队 id 过期**：丢弃。
- **碰撞**：QGC 在 Apply 前校验相邻槽位最小间距 ≥ 3m；不把 APF/CPA 混进 GUIDED 目标。

## 4. 模块

### QGC

| 文件 | 职责 |
|---|---|
| `src/Vehicle/FormationProtocol.{h,cc}` | 纯函数：`FORM_ASSIGN/STOP` 打包、越界校验、`formation_id` 生成 |
| `src/Vehicle/FormationShape.{h,cc}` | 纯几何：Line/Vee/Column/Grid → 极坐标槽位 |
| `src/Vehicle/FormationModel.{h,cc}` | `QAbstractListModel`：节点、分配、极坐标双向绑定、间距校验 |
| `src/Vehicle/FormationController.{h,cc}` | 状态机、ACK、超时重试、`STATUSTEXT` 聚合 |
| `src/FlyView/FormationCanvas.qml` | 画布：网格、缩放、平移、拖拽、连线、电量、实时位置 |
| `src/FlyView/FormationPanel.qml` | 面板：参数、成员表、Apply/Stop、预设 |

### 固件（maplrs）

| 文件 | 改动 |
|---|---|
| `src/components/wrapper/inc/air_info.h` | 40→43B，加 `yaw`、`formation_state` |
| `src/main/app/air/mesh_link.c` | `GLOBAL_POSITION_INT.hdg` 改传机头 |
| `src/components/wrapper/src/cmd_wrapper.c` | 白名单加 `USER_1/2/3`（`local = true`） |
| `src/main/app/air/radio_link_air.c` | 拦截 `USER_*` → `formation_*`，本地 ACK |
| `src/main/app/air/formation.{h,c}`（新） | 状态机、参数、5Hz tick、状态字节 |
| `src/main/app/air/formation_geometry.{h,c}`（新） | 纯函数：相对 NED + 偏移 → 目标经纬度 |
| `src/main/app/air/fc_link.c` | 新增 `fc_link_send_guided_target()` |
| `src/main/app/gnd/gcs_link.c` | 转发 `ATTITUDE`；`STATUSTEXT` 状态上报 |

## 5. 测试

- QGC：`FormationProtocol` 打包/校验、`FormationShape` 四模板极坐标、`FormationModel` 分配与间距、`FormationController` 状态机。
- 固件：`formation_geometry` 纯函数单测；`formation.c` 状态转换；`air_info` 43B 往返。
- 联调（2 机）：Apply → Streaming → Stop；领航失联 → Frozen；越界拒绝。
