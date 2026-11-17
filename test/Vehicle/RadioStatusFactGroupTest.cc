#include "RadioStatusFactGroupTest.h"

#include <QtTest/QTest>
#include <limits>

#include "QGCMAVLink.h"
#include "RadioStatusFactGroup.h"

void RadioStatusFactGroupTest::_testNamedValueFloatRssiAndSnr()
{
    RadioStatusFactGroup group;
    QVERIFY(!group.telemetryAvailable());

    mavlink_message_t msg{};
    static constexpr char kRssiName[MAVLINK_MSG_NAMED_VALUE_FLOAT_FIELD_NAME_LEN] = "RSSI";
    mavlink_msg_named_value_float_pack(16, MAV_COMP_ID_AUTOPILOT1, &msg, 1000, kRssiName, -67.4f);
    group.handleMessage(nullptr, msg);

    QVERIFY(group.telemetryAvailable());
    QCOMPARE(group.lrssi()->rawValue().toInt(), -67);

    static constexpr char kSnrName[MAVLINK_MSG_NAMED_VALUE_FLOAT_FIELD_NAME_LEN] = "SNR";
    mavlink_msg_named_value_float_pack(16, MAV_COMP_ID_AUTOPILOT1, &msg, 1200, kSnrName, -7.6f);
    group.handleMessage(nullptr, msg);

    QCOMPARE(group.snr()->rawValue().toInt(), -8);
}

void RadioStatusFactGroupTest::_testNamedValueFloatIgnoresUnrelatedAndNonFinite()
{
    RadioStatusFactGroup group;

    mavlink_message_t msg{};
    static constexpr char kOtherName[MAVLINK_MSG_NAMED_VALUE_FLOAT_FIELD_NAME_LEN] = "sin_wave";
    mavlink_msg_named_value_float_pack(1, MAV_COMP_ID_AUTOPILOT1, &msg, 1000, kOtherName, 12.0f);
    group.handleMessage(nullptr, msg);
    QVERIFY(!group.telemetryAvailable());

    static constexpr char kRssiName[MAVLINK_MSG_NAMED_VALUE_FLOAT_FIELD_NAME_LEN] = "RSSI";
    mavlink_msg_named_value_float_pack(16, MAV_COMP_ID_AUTOPILOT1, &msg, 1500, kRssiName,
                                       std::numeric_limits<float>::quiet_NaN());
    group.handleMessage(nullptr, msg);
    QVERIFY(!group.telemetryAvailable());
}

UT_REGISTER_TEST(RadioStatusFactGroupTest, TestLabel::Unit, TestLabel::Vehicle)
