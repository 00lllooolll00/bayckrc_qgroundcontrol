#pragma once

#include "UnitTest.h"

// 纯协议单测：FormationProtocol 参数打包、越界校验、formation_id 生成（详见 FORMATION_DESIGN.md）
class FormationProtocolTest : public UnitTest
{
    Q_OBJECT

public:
    explicit FormationProtocolTest(QObject* parent = nullptr) : UnitTest(parent) {}

private slots:
    void _constants();               // 命令号与协议版本
    void _configFieldOrder();        // FORM_CONFIG param1..param7
    void _memberFieldOrder();        // FORM_MEMBER param1..param7
    void _stopFieldOrder();          // FORM_STOP param1..param7
    void _validateAccepts();         // 合法参数通过
    void _validateRejects();         // 越界/非法参数被拒
    void _formationIdSequence();     // 从 1 递增、跳过 0
    void _formationIdWrap();         // 0xFFFF 回绕
};
