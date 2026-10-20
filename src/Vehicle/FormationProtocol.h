#pragma once

#include <cstdint>

// 编队协议 v2 纯函数层：FORM_ASSIGN 参数打包与 formation_id 生成。
// 协议见 FORMATION_DESIGN.md §3.1；不依赖 Qt 与 MAVLink，供 FormationManager 与单测共用。

namespace FormationProtocol {

// 与 MAVLink common.xml 的 MAV_CMD_USER_1..3 同值
constexpr int kMavCmdFormAssign = 31010;  // USER_1
constexpr int kMavCmdFormStop = 31011;    // USER_2
constexpr int kMavCmdFormQuery = 31012;   // USER_3

constexpr int kProtocolVersion = 1;

// formation_id 上限：float 只能精确表示 < 2^24 的整数
constexpr uint32_t kFormationIdLimit = 1u << 24;

// 一次 FORM_ASSIGN（COMMAND_LONG，target_system = 成员 node_id）
struct FormationAssign
{
    uint32_t formationId = 0;  // p1
    uint8_t slot = 0;          // p2 槽位索引，槽 0 = 领航机（领航机不收 FORM_ASSIGN）
    uint8_t leaderId = 0;      // p3 领航机 node_id，0 视为停止本机编队
    float dxEast = 0.0f;       // p4 已按队形朝向旋转的东向偏移，米
    float dyNorth = 0.0f;      // p5 北向偏移，米
    float dz = 0.0f;           // p6 相对领航机的相对高度，米
    float yawDeg = 0.0f;       // p7 0 = 保持当前机头；1..359 = 目标机头
};

// COMMAND_LONG 的 7 个 float 参数，字段顺序即线上顺序
struct CommandParams
{
    float param1 = 0.0f;
    float param2 = 0.0f;
    float param3 = 0.0f;
    float param4 = 0.0f;
    float param5 = 0.0f;
    float param6 = 0.0f;
    float param7 = 0.0f;
};

/// FORM_ASSIGN → COMMAND_LONG param1..param7（逐字段对应设计文档 §3.1）
CommandParams formAssignParams(const FormationAssign& assign);

/// 下一个 formation_id：从 1 递增、跳过 0、到 2^24 回绕
uint32_t nextFormationId(uint32_t current);

}  // namespace FormationProtocol
