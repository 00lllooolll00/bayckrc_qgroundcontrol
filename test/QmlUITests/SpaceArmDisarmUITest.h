#pragma once

#include "QmlUITestBase.h"

class SpaceArmDisarmUITest : public QmlUITestBase
{
    Q_OBJECT

public:
    SpaceArmDisarmUITest() = default;

private slots:
    void _testSingleVehicleSpaceEmergencyDisarmAndHoldArm();
    void _testMultiVehicleShiftSpaceCoordination();
};
