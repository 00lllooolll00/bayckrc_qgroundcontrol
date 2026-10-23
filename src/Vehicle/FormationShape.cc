#include "FormationShape.h"

#include <QtMath>

#include <cmath>

namespace FormationShape {

namespace {

SlotPolar polarFromForwardRight(double forward, double right)
{
    SlotPolar slot;
    slot.distanceM = std::hypot(forward, right);
    double angle = qRadiansToDegrees(std::atan2(right, forward));
    if (angle < 0.0) {
        angle += 360.0;
    }
    slot.angleDeg = angle;
    return slot;
}

}  // namespace

QList<SlotPolar> slotPolar(Shape shape, int count, double spacing, double vAngleDeg)
{
    QList<SlotPolar> out;
    if (count <= 0 || spacing <= 0.0) {
        return out;
    }
    out.append(SlotPolar{0.0, 0.0});  // 槽 0 = 领机

    switch (shape) {
        case Shape::Line:
            for (int i = 1; i < count; ++i) {
                const double distance = spacing * ((i + 1) / 2);
                const double angle = (i % 2 == 1) ? 90.0 : 270.0;
                out.append(SlotPolar{distance, angle});
            }
            break;

        case Shape::Vee: {
            const double half = vAngleDeg * 0.5;
            const double cosHalf = qCos(qDegreesToRadians(half));
            for (int i = 1; i < count; ++i) {
                const double distance = spacing * ((i + 1) / 2) / cosHalf;
                const double angle = (i % 2 == 1) ? half : (360.0 - half);
                out.append(SlotPolar{distance, angle});
            }
            break;
        }

        case Shape::Column:
            for (int i = 1; i < count; ++i) {
                out.append(SlotPolar{spacing * i, 180.0});
            }
            break;

        case Shape::Grid: {
            const int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count))));
            const double center = (cols - 1) * 0.5;
            // 按 row-major 顺序取格；cols 为奇数时 row 0 的中心格正是领机 (0,0)，跳过。
            for (int cell = 0; out.count() < count; ++cell) {
                const int row = cell / cols;
                const int col = cell % cols;
                if (row == 0 && std::fabs(col - center) < 1e-9) {
                    continue;
                }
                out.append(polarFromForwardRight(-spacing * row, (col - center) * spacing));
            }
            break;
        }
    }
    return out;
}

}  // namespace FormationShape
