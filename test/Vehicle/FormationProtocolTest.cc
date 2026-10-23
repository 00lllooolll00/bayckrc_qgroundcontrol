#include "FormationProtocolTest.h"

#include "FormationProtocol.h"

UT_REGISTER_TEST(FormationProtocolTest)

void FormationProtocolTest::_constants()
{
    QCOMPARE(FormationProtocol::kMavCmdFormStop, 31011);
    QCOMPARE(FormationProtocol::kMavCmdFormConfig, 31012);
    QCOMPARE(FormationProtocol::kMavCmdFormMember, 31013);
    QCOMPARE(FormationProtocol::kProtocolVersion, 2);
    QCOMPARE(FormationProtocol::kMaxShape, static_cast<uint8_t>(3));
    QCOMPARE(FormationProtocol::kMinSpacingM, 3.0f);
    QCOMPARE(FormationProtocol::kMaxSpacingM, 500.0f);
    QCOMPARE(FormationProtocol::kMaxVAngleDeg, 90.0f);
}

void FormationProtocolTest::_configFieldOrder()
{
    FormationProtocol::FormationConfig config;
    config.formationId = 42;
    config.shape = 2;
    config.spacingM = 12.5f;
    config.vAngleDeg = 30.0f;
    config.leaderOverride = 0x10;
    config.enabled = true;

    const FormationProtocol::CommandParams params = FormationProtocol::formConfigParams(config);
    QCOMPARE(params.param1, 42.0f);
    QCOMPARE(params.param2, 2.0f);
    QCOMPARE(params.param3, 12.5f);
    QCOMPARE(params.param4, 30.0f);
    QCOMPARE(params.param5, 16.0f);
    QCOMPARE(params.param6, 1.0f);
    QCOMPARE(params.param7, 0.0f);

    config.enabled = false;
    QCOMPARE(FormationProtocol::formConfigParams(config).param6, 0.0f);
}

void FormationProtocolTest::_memberFieldOrder()
{
    FormationProtocol::FormationMemberCommand member;
    member.formationId = 42;
    member.join = true;

    const FormationProtocol::CommandParams params = FormationProtocol::formMemberParams(member);
    QCOMPARE(params.param1, 42.0f);
    QCOMPARE(params.param2, 1.0f);
    QCOMPARE(params.param3, 0.0f);
    QCOMPARE(params.param4, 0.0f);
    QCOMPARE(params.param5, 0.0f);
    QCOMPARE(params.param6, 0.0f);
    QCOMPARE(params.param7, 0.0f);

    member.join = false;
    QCOMPARE(FormationProtocol::formMemberParams(member).param2, 0.0f);
}

void FormationProtocolTest::_stopFieldOrder()
{
    FormationProtocol::FormationStop stop;
    stop.formationId = 42;
    stop.allMembers = true;

    const FormationProtocol::CommandParams params = FormationProtocol::formStopParams(stop);
    QCOMPARE(params.param1, 42.0f);
    QCOMPARE(params.param2, 1.0f);
}

void FormationProtocolTest::_validateAccepts()
{
    FormationProtocol::FormationConfig config;
    config.formationId = 1;
    config.shape = 1;
    config.spacingM = 10.0f;
    config.vAngleDeg = 45.0f;
    config.leaderOverride = 0;

    QString error;
    QVERIFY(FormationProtocol::validateConfig(config, &error));
    QVERIFY(error.isEmpty());

    config.leaderOverride = 0x10;
    QVERIFY(FormationProtocol::validateConfig(config, &error));
}

void FormationProtocolTest::_validateRejects()
{
    FormationProtocol::FormationConfig config;
    config.formationId = 1;
    QString error;

    config.formationId = 0;
    QVERIFY(!FormationProtocol::validateConfig(config, &error));
    config.formationId = 1;

    config.shape = 4;
    QVERIFY(!FormationProtocol::validateConfig(config, &error));
    config.shape = 0;

    config.spacingM = 2.0f;
    QVERIFY(!FormationProtocol::validateConfig(config, &error));
    config.spacingM = 501.0f;
    QVERIFY(!FormationProtocol::validateConfig(config, &error));
    config.spacingM = 10.0f;

    config.vAngleDeg = -1.0f;
    QVERIFY(!FormationProtocol::validateConfig(config, &error));
    config.vAngleDeg = 91.0f;
    QVERIFY(!FormationProtocol::validateConfig(config, &error));
    config.vAngleDeg = 45.0f;

    config.leaderOverride = 0x01;
    QVERIFY(!FormationProtocol::validateConfig(config, &error));
    config.leaderOverride = 0;

    QVERIFY(FormationProtocol::validateConfig(config, &error));
}

void FormationProtocolTest::_formationIdSequence()
{
    QCOMPARE(FormationProtocol::nextFormationId(0), static_cast<uint16_t>(1));
    QCOMPARE(FormationProtocol::nextFormationId(1), static_cast<uint16_t>(2));
    QCOMPARE(FormationProtocol::nextFormationId(41), static_cast<uint16_t>(42));
}

void FormationProtocolTest::_formationIdWrap()
{
    QCOMPARE(FormationProtocol::nextFormationId(FormationProtocol::kFormationIdLimit), static_cast<uint16_t>(1));
    QCOMPARE(FormationProtocol::nextFormationId(FormationProtocol::kFormationIdLimit - 1),
             FormationProtocol::kFormationIdLimit);
}
