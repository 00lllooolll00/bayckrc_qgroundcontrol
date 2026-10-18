#pragma once

#include <QtCore/QList>
#include <QtCore/QPointF>

// 编队几何：把队形模板变成相对领航机（槽 0）的槽位偏移表（x=东, y=北, 米）。
// 纯函数层，不依赖 Qt 对象与 MAVLink；供 FormationManager 与单元测试共用。
// 协议见 FORMATION_DESIGN.md。

namespace FormationShape {

enum class Shape
{
    Line,    // 横队：领航机居中，其余左右展开
    Vee,     // 人字：领航机在前，两翼斜后方
    Column,  // 纵队：全部在领航机正后方
    Circle,  // 环形：领航机圆心，其余均匀环上
};

/// 生成 count 个槽位（槽 0 = 领航机，恒 (0,0)），间距 spacing 米，队形朝向
/// headingDeg（罗盘角：0=北，顺时针）。count<=0 或 spacing<=0 返回空表。
/// 函数名用 slotOffsets：slots 是 Qt 关键字宏，函数名不能叫 slots。
QList<QPointF> slotOffsets(Shape shape, int count, double spacing, double headingDeg);

}  // namespace FormationShape
