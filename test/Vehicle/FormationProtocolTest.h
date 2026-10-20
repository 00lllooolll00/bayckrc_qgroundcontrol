#pragma once

#include "UnitTest.h"

// 纯协议单测：FormationProtocol 参数打包与 formation_id 生成（详见 FORMATION_DESIGN.md §3.1）
class FormationProtocolTest : public UnitTest
{
    Q_OBJECT

public:
    explicit FormationProtocolTest(QObject* parent = nullptr) : UnitTest(parent) {}

private slots:
    void _constants();               // 命令号与协议版本
    void _assignFieldOrder();        // param1..param7 逐字段对应
    void _zeroAndNegativeOffsets();  // 0 / 负偏移原样透传
    void _yawZero();                 // yaw=0 表示保持机头
    void _formationIdSequence();     // 从 1 递增、跳过 0
    void _formationIdWrap();         // 2^24 回绕且 float 可精确表示
};
