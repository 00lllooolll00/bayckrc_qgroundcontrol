# BayckRC 编队协议系统设计（Formation Protocol）

> 状态：v1 已实现（QGC 侧，本分支）；v2 为固件侧预留协议。本文档是协议真源。

## 1. 目标与复用原则

在 QGC 内完成：**读取当前在线的 AIR 节点 → 动态设计编队队形 → 下发到集群**。
高度复用 = 不发明新空口协议，全部踩在两端已验证的通路上：

| 复用点 | 说明 |
|---|---|
| 在线检测 | 固件 gcs_link 离线成员停发心跳（gcs_link.c:589），QGC `MultiVehicleManager::vehicles()` 天然就是在线 AIR 列表，零固件改动 |
| 命令通路 | `COMMAND_LONG` 改写转发（gcs_link.c:95-163）+ AIR 本地导航映射（radio_link.c:1031-1091），MAPLRS 插件（`src/FirmwarePlugin/BayckrcMAPLRS/`）已契约化 |
| 位置通路 | 任务上传（mission 8 类消息，gcs_link.c:258-368 整帧转发）双向可用；QGC 侧 `Vehicle::missionManager()->writeMissionItems()` 直接驱动 |
| 遥测反馈 | `GLOBAL_POSITION_INT` 已周期上报，编队误差由 QGC 计算对比，固件零遥测新增 |

## 2. 硬约束（取证结论）

- QGC 看到的 SYSID = AIR 成员 node_id（0x10 起），compid = `MAV_COMP_ID_AUTOPILOT1`，`MAV_AUTOPILOT_SMARTAP(18)`（gcs_link.c:597-605）。**GND 自身不出现**。
- 空口任务帧 ≤ 50B（`FC_BRIDGE_FRAME_MAX`，fc_wrapper.h:19），超长整帧丢弃。
- GND 命令白名单仅 7 类（MISSION_START / DO_PAUSE_CONTINUE / PREFLIGHT_REBOOT_SHUTDOWN / NAV_RETURN_TO_LAUNCH / COMPONENT_ARM_DISARM / DO_SET_MODE / NAV_LAND），其余回 `MAV_RESULT_UNSUPPORTED`；**不支持 target_system=0 广播**（查表未中即丢，gcs_link.c:95-101）。
- AIR 本地分流：MISSION_START→WP 模式、DO_PAUSE_CONTINUE→POSHOLD、NAV_RETURN_TO_LAUNCH→RTH（`cmd_wrapper_nav_set`，RC 开关映射）；ARM_DISARM→RC CH5 映射。
- 两端 MAVLink 方言（QGC CPM pin c409cf690 + fastmavlink 2026-02）均**无** FORMATION 消息/命令。
- INAV 为任务唯一真源；飞行中不支持实时位置 setpoint。

## 3. 协议 v1（已实现，纯 QGC 侧）

**载体：标准 MAVLink mission。编队 = 每个跟随机一条"单点任务"。**

```
QGC 设计队形（FormationShape 模板 → 槽位表）
  → 锚点 = 领航机(槽0)当前 coordinate + 实时航向 + 高度
  → 槽位偏移(东/北, m) 旋转到航向 → 绝对经纬度
  → 每个跟随机 mini-mission：[NAV_WAYPOINT(slot), NAV_LOITER_UNLIM(slot)]
  → 串行下发：先对所有空中机 DO_PAUSE_CONTINUE(暂停→POSHOLD)
     → 逐台 writeMissionItems（等 sendComplete 再下一台，串行避免挤爆信箱）
     → 全部上传完成后逐台 startMission（MISSION_START）
```

- 领航机（槽 0）不接收任务改写，保持现状（地面待命或悬停）。
- 编队误差 = QGC 每秒对比各机 coordinate 与槽位绝对坐标（面板实时显示）。
- 已知限制：改队形 = 重新设计 → 下发（自动暂停+上传+启动）；无实时跟随领航机的动态保持——这是 INAV 不暴露实时 setpoint 的固有限制，见 §4 v2。

## 4. 协议 v2（固件侧预留，未实现）

载体：`MAV_CMD_USER_1..4`（common.xml 31010-31013，**现成私有命令段，零方言改动**）。
固件改动点（两侧都是小改，50B 内装得下 COMMAND_LONG=36B）：

| 命令 | 值 | 参数（param1..7） | 方向 |
|---|---|---|---|
| `MAPLRS_FORM_ASSIGN` | `MAV_CMD_USER_1` 31010 | p1=formation_id, p2=slot_index, p3=leader_sysid(0=无), p4=dx_east, p5=dy_north, p6=dz, p7=flags | QGC→单机/广播 |
| `MAPLRS_FORM_START` | `MAV_CMD_USER_2` 31011 | p1=1 启动/0 停止, p2=重传轮数 | 广播 |
| `MAPLRS_FORM_QUERY` | `MAV_CMD_USER_3` 31012 | p1=查询掩码 | QGC→单机 |

固件侧需要的三处小改：
1. `gcs_link.c::_dispatch_command_long`：白名单加 3 个命令；`target_system==0` 时扇出循环全员（每成员一次改写转发）。
2. `radio_link.c` 本地分流：`FORM_ASSIGN` 拦截进 `algorithm/formation.c`（读 `g_nvs_data.node_id` 认领槽位），不进 FC 桥。
3. 领航位实时跟随：领航机经 ESP-NOW 广播位置（neighbor_table 已有），各机算误差后经 **MSP 命令**微调（INAV 实时 setpoint 通道缺失 → 需与飞控侧约定，见"开放问题"）。

## 5. 模块划分与扩展点（QGC）

```
src/Vehicle/FormationShape.{h,cc}     纯几何：模板→槽位偏移（无 Qt 对象，可单测）
src/Vehicle/FormationManager.{h,cc}   服务：槽位分配 + 串行下发状态机（单例，QML_ELEMENT）
src/FlyView/FormationToolStripAction.qml  FlyView 工具条入口
src/FlyView/FormationPanel.qml        编队面板（设计/分配/下发/误差显示）
src/QmlControls/QGroundControlQmlGlobal  暴露 formationManager 给 QML
```

- **新队形**：`FormationShape` 加一个分支（槽位生成纯函数），UI 自动出现选项。
- **新成员类型过滤**：`FormationManager::isMaplrsVehicle`（现按 `MAV_AUTOPILOT_SMARTAP`）。
- **换队形锚点策略/分配策略**：只动 FormationManager，UI 不变。

## 6. 测试

`test/Vehicle/FormationShapeTest.{h,cc}`：槽位数、对称性（横队/人字左右镜像）、朝向旋转正确性（纯逻辑，无 MockLink）。

## 7. 开放问题（后续会话决策）

1. **实时跟随的控制通路**：INAV 不接受飞行中 setpoint；候选 = INAV 固件侧新增 MSP 命令 / 改用位置容差大的"慢队形"（周期性重发任务点）/ 联系 INAV 社区 follow 能力。v2 立项前必须定。
2. **广播语义**：v2 依赖 GND fan-out 改动，固件两侧需同步发版（老固件收不到新命令）。
3. **地图上的槽位预览圈**：v1 面板只有文字槽位表，地图 overlay 排期在面板可用之后。
4. `MISSION_ITEM` 与 `MISSION_ITEM_INT` 的选择：INAV 经纬度 1e5 精度足够，v1 走 PlanManager 默认路径。
