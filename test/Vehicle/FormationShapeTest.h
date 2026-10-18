#pragma once

#include "UnitTest.h"

// 纯几何单测：FormationShape 槽位生成（详见 FORMATION_DESIGN.md §6）
class FormationShapeTest : public UnitTest
{
    Q_OBJECT

public:
    explicit FormationShapeTest(QObject* parent = nullptr) : UnitTest(parent) {}

private slots:
    void _leaderSlotAtOrigin();      // 所有队形槽 0 都在原点
    void _lineSymmetry();            // 横队左右镜像对称
    void _veeSymmetryAndRearward();  // 人字镜像对称且在领航机后方
    void _columnRearwardSpaced();    // 纵队正后方等距
    void _circleRadiusUniform();     // 环形各槽等半径
    void _headingRotation();         // 朝向旋转正确（纵队 + 90° → 正东后方）
    void _invalidInput();            // 非法入参返回空表
};
