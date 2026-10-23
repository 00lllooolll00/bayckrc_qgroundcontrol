#pragma once

#include "UnitTest.h"

// 纯几何单测：FormationShape 极坐标槽位生成（详见 FORMATION_DESIGN.md §6）
class FormationShapeTest : public UnitTest
{
    Q_OBJECT

public:
    explicit FormationShapeTest(QObject* parent = nullptr) : UnitTest(parent) {}

private slots:
    void _leaderSlotAtOrigin();      // 所有队形槽 0 都在原点
    void _lineSymmetry();            // 横队左右镜像对称
    void _veeSymmetry();             // 人字镜像对称且向后展开
    void _columnRearwardSpaced();    // 纵队正后方等距
    void _gridCount();               // 方阵槽位数正确且按 row-major 顺序
    void _gridNoLeaderOverlap();     // 奇数/偶数 cols 下跟随槽不落在领机 (0,0)
    void _invalidInput();            // 非法入参返回空表
};
