#pragma once

#include <QList>

// 编队几何：把队形模板展开成相对领机（槽 0）的极坐标槽位。
// 0° = 领机机头正前，顺时针为正；距离单位米。
// 纯函数层，不依赖 Qt 对象与 MAVLink；供 FormationModel 与单元测试共用。
// 协议见 FORMATION_DESIGN.md。

namespace FormationShape {

enum class Shape
{
    Line,    // 横队：领机居中，其余左右展开
    Vee,     // 人字：领机在前，两翼斜后方
    Column,  // 纵队：全部在领机正后方
    Grid,    // 方阵：按 sqrt(n) 列排布
};

struct SlotPolar
{
    double distanceM = 0.0;
    double angleDeg = 0.0;  // 0 = 机头正前，顺时针
};

/// 生成 count 个槽位（槽 0 = 领机，恒 (0,0)），相邻间距 spacing 米。
/// vAngleDeg 仅 Vee 使用，表示两翼夹角。count<=0 或 spacing<=0 返回空表。
QList<SlotPolar> slotPolar(Shape shape, int count, double spacing, double vAngleDeg = 45.0);

struct BodyOffset
{
    double forwardM = 0.0;  // 领机机头方向为正
    double rightM = 0.0;    // 领机右侧为正
};

/// 把以领机为原点的北/东偏移旋转到领机机体坐标（forward/right）。
/// headingDeg 为领机机头，0 = 正北、顺时针；非有限值按 0 处理。
BodyOffset bodyOffsetFromNE(double northM, double eastM, double headingDeg);

struct NorthEastOffset
{
    double northM = 0.0;
    double eastM = 0.0;
};

/// bodyOffsetFromNE 的逆运算：领机机体坐标（forward/right）旋转到北/东。
NorthEastOffset northEastFromBodyOffset(double forwardM, double rightM, double headingDeg);

}  // namespace FormationShape
