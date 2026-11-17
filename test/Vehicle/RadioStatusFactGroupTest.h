#pragma once

#include "UnitTest.h"

class RadioStatusFactGroupTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testNamedValueFloatRssiAndSnr();
    void _testNamedValueFloatIgnoresUnrelatedAndNonFinite();
};
