#include "FormationShapeTest.h"

#include <QPair>
#include <QtMath>

#include "FormationShape.h"

UT_REGISTER_TEST(FormationShapeTest)

using FormationShape::Shape;
using FormationShape::SlotPolar;

namespace {

// 极坐标 -> (forward, right)，用于和固件 NED 位置直接对比。
QPair<double, double> _forwardRight(const SlotPolar& slot)
{
    const double rad = qDegreesToRadians(slot.angleDeg);
    return {slot.distanceM * qCos(rad), slot.distanceM * qSin(rad)};
}

}  // namespace

void FormationShapeTest::_leaderSlotAtOrigin()
{
    for (Shape shape : {Shape::Line, Shape::Vee, Shape::Column, Shape::Grid}) {
        const QList<SlotPolar> polar = FormationShape::slotPolar(shape, 5, 10.0);
        QCOMPARE(polar.count(), 5);
        QCOMPARE(polar[0].distanceM, 0.0);
        QCOMPARE(polar[0].angleDeg, 0.0);
    }
}

void FormationShapeTest::_lineSymmetry()
{
    const QList<SlotPolar> polar = FormationShape::slotPolar(Shape::Line, 5, 10.0);
    QCOMPARE(polar.count(), 5);
    QCOMPARE(polar[1].angleDeg, 90.0);
    QCOMPARE(polar[2].angleDeg, 270.0);
    QCOMPARE(polar[1].distanceM, polar[2].distanceM);
    QCOMPARE(polar[3].distanceM, 20.0);
    QCOMPARE(polar[4].distanceM, 20.0);
}

void FormationShapeTest::_veeSymmetry()
{
    const QList<SlotPolar> polar = FormationShape::slotPolar(Shape::Vee, 5, 10.0, 45.0);
    QCOMPARE(polar.count(), 5);
    QCOMPARE(polar[1].angleDeg, 22.5);
    QCOMPARE(polar[2].angleDeg, 337.5);
    QCOMPARE(polar[1].distanceM, polar[2].distanceM);
    QVERIFY(polar[1].distanceM > 10.0);  // 斜边大于同侧间距
}

void FormationShapeTest::_columnRearwardSpaced()
{
    const QList<SlotPolar> polar = FormationShape::slotPolar(Shape::Column, 4, 10.0);
    QCOMPARE(polar.count(), 4);
    QCOMPARE(polar[1].angleDeg, 180.0);
    QCOMPARE(polar[1].distanceM, 10.0);
    QCOMPARE(polar[3].distanceM, 30.0);
}

void FormationShapeTest::_gridCount()
{
    const QList<SlotPolar> polar = FormationShape::slotPolar(Shape::Grid, 4, 10.0);
    QCOMPARE(polar.count(), 4);
    // 槽位按 (forward, right) 校验，与固件 formation_shape_positions 一致。
    const QList<QPair<double, double>> expected = {{0.0, 0.0}, {0.0, -5.0}, {0.0, 5.0}, {-10.0, -5.0}};
    for (int i = 0; i < expected.count(); ++i) {
        const QPair<double, double> fr = _forwardRight(polar[i]);
        QVERIFY(qAbs(fr.first - expected[i].first) < 1e-9);
        QVERIFY(qAbs(fr.second - expected[i].second) < 1e-9);
    }
}

void FormationShapeTest::_gridNoLeaderOverlap()
{
    // 奇数 cols 的 row 0 中心格就是领机，跟随槽不得落在 (0,0)。
    for (int count : {5, 6, 9, 10}) {
        const QList<SlotPolar> polar = FormationShape::slotPolar(Shape::Grid, count, 10.0);
        QCOMPARE(polar.count(), count);
        for (int i = 1; i < polar.count(); ++i) {
            const QPair<double, double> fr = _forwardRight(polar[i]);
            QVERIFY(qAbs(fr.first) > 1e-9 || qAbs(fr.second) > 1e-9);
        }
    }
}

void FormationShapeTest::_invalidInput()
{
    QVERIFY(FormationShape::slotPolar(Shape::Line, 0, 10.0).isEmpty());
    QVERIFY(FormationShape::slotPolar(Shape::Line, 5, 0.0).isEmpty());
    QVERIFY(FormationShape::slotPolar(Shape::Line, -1, -3.0).isEmpty());
}

void FormationShapeTest::_slotCountFromPositions()
{
    using FormationShape::slotCountFromPositions;

    // 无有效槽位（地面阶段）→ 回退本地计数。
    QCOMPARE(slotCountFromPositions({}, 3), 3);
    QCOMPARE(slotCountFromPositions(QList<int>{-1, -1}, 3), 3);

    // 连续占满 0..2 → 3 槽。
    QCOMPARE(slotCountFromPositions(QList<int>{0, 1, 2}, 3), 3);

    // 反复出现的现场：3 架机占了槽 0/1/3，机上实际 4 槽，本地只数到 3。
    QCOMPARE(slotCountFromPositions(QList<int>{0, 1, 3}, 3), 4);

    // 越界槽位号忽略，不能把面板撑成 43 槽。
    QCOMPARE(slotCountFromPositions(QList<int>{42}, 3), 3);

    // 有成员未分配（-1/0xFF）或两从机暂时撞槽时，槽位数绝不能塌陷到小于在线成员数 fallback。
    QCOMPARE(slotCountFromPositions(QList<int>{0, 1, -1}, 3), 3);
    QCOMPARE(slotCountFromPositions(QList<int>{0, 1, 1}, 3), 3);
}

void FormationShapeTest::_bodyOffsetFromNE()
{
    // 领机朝北：北 = 前，东 = 右。
    FormationShape::BodyOffset off = FormationShape::bodyOffsetFromNE(10.0, 0.0, 0.0);
    QVERIFY(qAbs(off.forwardM - 10.0) < 1e-9);
    QVERIFY(qAbs(off.rightM) < 1e-9);

    // 领机朝东：东 = 前，南 = 右。
    off = FormationShape::bodyOffsetFromNE(0.0, 10.0, 90.0);
    QVERIFY(qAbs(off.forwardM - 10.0) < 1e-9);
    QVERIFY(qAbs(off.rightM) < 1e-9);
    off = FormationShape::bodyOffsetFromNE(-10.0, 0.0, 90.0);
    QVERIFY(qAbs(off.forwardM) < 1e-9);
    QVERIFY(qAbs(off.rightM - 10.0) < 1e-9);

    // 领机朝南：北 = 后。
    off = FormationShape::bodyOffsetFromNE(10.0, 0.0, 180.0);
    QVERIFY(qAbs(off.forwardM + 10.0) < 1e-9);

    // 非有限航向按 0 处理。
    off = FormationShape::bodyOffsetFromNE(5.0, 3.0, qQNaN());
    QVERIFY(qAbs(off.forwardM - 5.0) < 1e-9);
    QVERIFY(qAbs(off.rightM - 3.0) < 1e-9);
}

void FormationShapeTest::_northEastFromBodyOffset()
{
    // 领机朝东：前 = 东，右 = 南。
    FormationShape::NorthEastOffset ne = FormationShape::northEastFromBodyOffset(10.0, 0.0, 90.0);
    QVERIFY(qAbs(ne.northM) < 1e-9);
    QVERIFY(qAbs(ne.eastM - 10.0) < 1e-9);
    ne = FormationShape::northEastFromBodyOffset(0.0, 10.0, 90.0);
    QVERIFY(qAbs(ne.northM + 10.0) < 1e-9);
    QVERIFY(qAbs(ne.eastM) < 1e-9);

    // 与 bodyOffsetFromNE 互逆。
    const FormationShape::BodyOffset body = FormationShape::bodyOffsetFromNE(7.0, -3.0, 37.0);
    ne = FormationShape::northEastFromBodyOffset(body.forwardM, body.rightM, 37.0);
    QVERIFY(qAbs(ne.northM - 7.0) < 1e-9);
    QVERIFY(qAbs(ne.eastM + 3.0) < 1e-9);
}
