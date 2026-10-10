#include "FormationShape.h"

#include <QtMath>

namespace FormationShape {

namespace {

/// 本体系偏移绕队形朝向旋转（罗盘角 0=北顺时针）。
QPointF rotate(const QPointF& offset, double headingDeg)
{
    const double rad = qDegreesToRadians(headingDeg);
    const double c = qCos(rad);
    const double s = qSin(rad);
    return QPointF(offset.x() * c + offset.y() * s, -offset.x() * s + offset.y() * c);
}

}  // namespace

QList<QPointF> slotOffsets(Shape shape, int count, double spacing, double headingDeg)
{
    QList<QPointF> out;
    if (count <= 0 || spacing <= 0) {
        return out;
    }
    out.append(QPointF(0, 0));
    switch (shape) {
        case Shape::Column:
            for (int i = 1; i < count; ++i) {
                out.append(QPointF(0, -i * spacing));
            }
            break;
        case Shape::Line:
            for (int i = 1; i < count; ++i) {
                const int k = (i + 1) / 2;
                const int side = (i % 2 == 1) ? 1 : -1;
                out.append(QPointF(side * k * spacing, 0));
            }
            break;
        case Shape::Vee:
            for (int i = 1; i < count; ++i) {
                const int k = (i + 1) / 2;
                const int side = (i % 2 == 1) ? 1 : -1;
                out.append(QPointF(side * k * spacing * M_SQRT1_2, -k * spacing * M_SQRT1_2));
            }
            break;
        case Shape::Circle:
            for (int i = 1; i < count; ++i) {
                const double rad = qDegreesToRadians((360.0 * i) / count);
                out.append(QPointF(spacing * qSin(rad), spacing * qCos(rad)));
            }
            break;
    }
    if (headingDeg != 0) {
        for (QPointF& p : out) {
            p = rotate(p, headingDeg);
        }
    }
    return out;
}

}  // namespace FormationShape
