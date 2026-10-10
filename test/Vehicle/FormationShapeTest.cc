#include "FormationShapeTest.h"

#include <QtGui/QVector2D>

#include "FormationShape.h"

using namespace FormationShape;

namespace {
constexpr double kEpsilon = 1e-6;
}

void FormationShapeTest::_leaderSlotAtOrigin()
{
    for (int shape = 0; shape < 4; ++shape) {
        const QList<QPointF> s = slotOffsets(static_cast<Shape>(shape), 5, 10.0, 0.0);
        QCOMPARE(s.count(), 5);
        QVERIFY(qFuzzyIsNull(s[0].x()));
        QVERIFY(qFuzzyIsNull(s[0].y()));
    }
}

void FormationShapeTest::_lineSymmetry()
{
    const QList<QPointF> s = slotOffsets(Shape::Line, 5, 10.0, 0.0);
    QCOMPARE(s.count(), 5);
    // 左右镜像：x 成对相反，y 全为 0（与领航机同一连线）
    for (int i = 1; i < 5; ++i) {
        QVERIFY(qFuzzyIsNull(s[i].y()));
    }
    QVERIFY(qFuzzyCompare(s[1].x(), -s[2].x()));
    QVERIFY(qFuzzyCompare(s[3].x(), -s[4].x()));
    QVERIFY(qFuzzyCompare(s[1].x(), 10.0));
}

void FormationShapeTest::_veeSymmetryAndRearward()
{
    const QList<QPointF> s = slotOffsets(Shape::Vee, 5, 10.0, 0.0);
    QCOMPARE(s.count(), 5);
    // 人字两翼镜像对称
    QVERIFY(qFuzzyCompare(s[1].x(), -s[2].x()));
    QVERIFY(qFuzzyCompare(s[3].x(), -s[4].x()));
    // 全部在领航机后方（y < 0）
    for (int i = 1; i < 5; ++i) {
        QVERIFY(s[i].y() < 0.0);
    }
}

void FormationShapeTest::_columnRearwardSpaced()
{
    const QList<QPointF> s = slotOffsets(Shape::Column, 4, 10.0, 0.0);
    QCOMPARE(s.count(), 4);
    for (int i = 1; i < 4; ++i) {
        QVERIFY(qFuzzyIsNull(s[i].x()));
        QVERIFY(qFuzzyCompare(s[i].y(), -i * 10.0));
    }
}

void FormationShapeTest::_circleRadiusUniform()
{
    const double radius = 10.0;
    const QList<QPointF> s = slotOffsets(Shape::Circle, 6, radius, 0.0);
    QCOMPARE(s.count(), 6);
    for (int i = 1; i < 6; ++i) {
        const double r = qSqrt(s[i].x() * s[i].x() + s[i].y() * s[i].y());
        QVERIFY(qAbs(r - radius) < kEpsilon);
    }
}

void FormationShapeTest::_headingRotation()
{
    // 纵队（0°=北）本身在正北后方 (0,-d)；朝向 90°（东）应整体旋转到西南象限，槽 1 变西侧
    const QPointF slot1 = slotOffsets(Shape::Column, 2, 10.0, 90.0)[1];
    QVERIFY(slot1.x() < kEpsilon);
    QVERIFY(qAbs(slot1.x() + 10.0) < kEpsilon);
    QVERIFY(qAbs(slot1.y()) < kEpsilon);
}

void FormationShapeTest::_invalidInput()
{
    QVERIFY(slotOffsets(Shape::Line, 0, 10.0, 0.0).isEmpty());
    QVERIFY(slotOffsets(Shape::Line, 5, 0.0, 0.0).isEmpty());
    QVERIFY(slotOffsets(Shape::Line, -1, -3.0, 0.0).isEmpty());
}

UT_REGISTER_TEST(FormationShapeTest, TestLabel::Unit)
