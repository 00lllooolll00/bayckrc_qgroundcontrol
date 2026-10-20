#include "FormationProtocolTest.h"

#include <limits>

#include "FormationProtocol.h"

using namespace FormationProtocol;

void FormationProtocolTest::_constants()
{
    QCOMPARE(kMavCmdFormAssign, 31010);
    QCOMPARE(kMavCmdFormStop, 31011);
    QCOMPARE(kMavCmdFormQuery, 31012);
    QCOMPARE(kProtocolVersion, 1);
}

void FormationProtocolTest::_assignFieldOrder()
{
    FormationAssign assign;
    assign.formationId = 42;
    assign.slot = 3;
    assign.leaderId = 1;
    assign.dxEast = 12.5f;
    assign.dyNorth = -7.25f;
    assign.dz = 2.5f;
    assign.yawDeg = 270.0f;

    const CommandParams p = formAssignParams(assign);
    QCOMPARE(p.param1, 42.0f);
    QCOMPARE(p.param2, 3.0f);
    QCOMPARE(p.param3, 1.0f);
    QCOMPARE(p.param4, 12.5f);
    QCOMPARE(p.param5, -7.25f);
    QCOMPARE(p.param6, 2.5f);
    QCOMPARE(p.param7, 270.0f);
}

void FormationProtocolTest::_zeroAndNegativeOffsets()
{
    FormationAssign assign;
    assign.formationId = 7;
    assign.slot = 2;
    assign.leaderId = 5;
    assign.dxEast = 0.0f;
    assign.dyNorth = -3.5f;
    assign.dz = -1.25f;
    assign.yawDeg = 359.0f;

    const CommandParams p = formAssignParams(assign);
    QCOMPARE(p.param4, 0.0f);
    QCOMPARE(p.param5, -3.5f);
    QCOMPARE(p.param6, -1.25f);
    QCOMPARE(p.param7, 359.0f);
}

void FormationProtocolTest::_yawZero()
{
    FormationAssign assign;
    assign.formationId = 1;
    assign.yawDeg = 0.0f;

    QCOMPARE(formAssignParams(assign).param7, 0.0f);
}

void FormationProtocolTest::_formationIdSequence()
{
    QCOMPARE(nextFormationId(0), 1u);
    QCOMPARE(nextFormationId(1), 2u);
    QCOMPARE(nextFormationId(999), 1000u);
}

void FormationProtocolTest::_formationIdWrap()
{
    QCOMPARE(nextFormationId(kFormationIdLimit - 2), kFormationIdLimit - 1);
    QCOMPARE(nextFormationId(kFormationIdLimit - 1), 1u);
    QCOMPARE(nextFormationId(std::numeric_limits<uint32_t>::max()), 1u);

    // 最大 id 打包后仍可被 float 精确表示
    FormationAssign assign;
    assign.formationId = kFormationIdLimit - 1;
    QCOMPARE(formAssignParams(assign).param1, static_cast<float>(kFormationIdLimit - 1));
}
