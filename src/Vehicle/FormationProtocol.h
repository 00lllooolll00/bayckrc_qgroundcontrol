#pragma once

#include <QString>

#include <cstdint>

// 编队协议 v2 纯函数层：FORM_CONFIG / FORM_MEMBER / FORM_STOP 参数打包与校验。
// 协议见 FORMATION_DESIGN.md；不依赖 Qt 对象与 MAVLink，供 FormationModel/Controller 与单测共用。

namespace FormationProtocol {

// 与 bayckrc.xml 的 MAV_CMD_FORM_* 同值
constexpr int kMavCmdFormConfig = 31020;  // MAV_CMD_FORM_CONFIG
constexpr int kMavCmdFormMember = 31021;  // MAV_CMD_FORM_MEMBER
constexpr int kMavCmdFormStop = 31022;    // MAV_CMD_FORM_STOP

constexpr int kProtocolVersion = 2;

// formation_id 为 16 位，从 1 递增，0 保留
constexpr uint16_t kFormationIdLimit = 0xFFFF;

// 协议限幅，QGC 与 AIR 双端一致
constexpr float kMinSpacingM = 3.0f;
constexpr float kMaxSpacingM = 500.0f;
constexpr float kMaxVAngleDeg = 90.0f;
constexpr uint8_t kMaxShape = 3;

// 一次 FORM_CONFIG（COMMAND_LONG，target_system = 成员 node_id）
struct FormationConfig
{
    uint16_t formationId = 0;       // p1
    uint8_t shape = 0;              // p2 0 Line / 1 Vee / 2 Column / 3 Grid
    float spacingM = 10.0f;         // p3
    float vAngleDeg = 45.0f;        // p4，仅 Vee 使用
    uint8_t leaderOverride = 0;     // p5 0 = 自动选举，否则指定领机 node_id
    bool enabled = true;            // p6 bit0
};

struct FormationMemberCommand
{
    uint16_t formationId = 0;       // p1
    bool join = false;              // p2 0 = leave / 1 = join
};

struct FormationStop
{
    uint16_t formationId = 0;
    bool allMembers = false;
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

/// FORM_CONFIG → COMMAND_LONG param1..param7
CommandParams formConfigParams(const FormationConfig& config);

/// FORM_MEMBER → COMMAND_LONG param1..param7
CommandParams formMemberParams(const FormationMemberCommand& member);

/// FORM_STOP → COMMAND_LONG param1..param7
CommandParams formStopParams(const FormationStop& stop);

/// 校验一次 FORM_CONFIG；失败时把中文原因写进 error（可传 nullptr）
bool validateConfig(const FormationConfig& config, QString* error);

/// 下一个 formation_id：从 1 递增、跳过 0、到 0xFFFF 回绕
uint16_t nextFormationId(uint16_t current);

}  // namespace FormationProtocol
