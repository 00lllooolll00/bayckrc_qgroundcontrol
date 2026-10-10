#include "FormationManager.h"

#include <QtCore/QTimer>

#include "FormationShape.h"
#include "MissionItem.h"
#include "MissionManager.h"
#include "MultiVehicleManager.h"
#include "QGCMAVLink.h"
#include "QmlObjectListModel.h"
#include "Vehicle.h"
#include "VehicleFactGroup.h"

namespace {
constexpr int kLeaderSlot = 0;
constexpr int kPauseGraceMSecs = 1000;

/// Vehicle::vehicleFactGroup() 返回基类指针；编队需要的 heading/altitudeRelative 在 VehicleFactGroup 上
VehicleFactGroup* _vehicleFacts(Vehicle* vehicle)
{
    return qobject_cast<VehicleFactGroup*>(vehicle->vehicleFactGroup());
}
}  // namespace

FormationManager* FormationManager::instance()
{
    static FormationManager instanceSingleton;
    return &instanceSingleton;
}

FormationManager::FormationManager(QObject* parent) : QObject(parent)
{
    connect(&_timeoutTimer, &QTimer::timeout, this, [this]() { _startAll(false); });
    _timeoutTimer.setSingleShot(true);
    _regenSlots();
}

void FormationManager::setShapeIndex(int shapeIndex)
{
    if (shapeIndex == _shapeIndex) {
        return;
    }
    _shapeIndex = shapeIndex;
    emit shapeIndexChanged();
    _regenSlots();
}

void FormationManager::setSpacing(double spacing)
{
    if (spacing == _spacing) {
        return;
    }
    _spacing = spacing;
    emit spacingChanged();
    _regenSlots();
}

void FormationManager::setSlotCount(int slotCount)
{
    if (slotCount == _slotCount) {
        return;
    }
    _slotCount = slotCount;
    emit slotCountChanged();
    _regenSlots();
}

void FormationManager::_regenSlots()
{
    const QList<QPointF> offsets =
        FormationShape::slotOffsets(static_cast<FormationShape::Shape>(_shapeIndex), _slotCount, _spacing, 0.0);
    _slotEntries.clear();
    _slots.clear();
    for (const QPointF& offset : offsets) {
        SlotEntry entry;
        entry.vehicleId = 0;
        entry.offset = offset;
        QVariantMap map;
        map["x"] = offset.x();
        map["y"] = offset.y();
        map["vehicleId"] = 0;
        _slotEntries.append(entry);
        _slots.append(map);
    }
    emit slotsChanged();
}

void FormationManager::assignSlot(int slotIndex, int vehicleId)
{
    if (slotIndex < 0 || slotIndex >= _slotEntries.count()) {
        return;
    }
    // 同一台车换槽时让出旧槽，避免一车多用
    for (SlotEntry& entry : _slotEntries) {
        if (entry.vehicleId == vehicleId && vehicleId != 0) {
            entry.vehicleId = 0;
        }
    }
    _slotEntries[slotIndex].vehicleId = vehicleId;
    QVariantMap map = _slots[slotIndex].toMap();
    map["vehicleId"] = vehicleId;
    _slots[slotIndex] = map;
    emit slotsChanged();
}

void FormationManager::clearAssignment()
{
    for (SlotEntry& entry : _slotEntries) {
        entry.vehicleId = 0;
    }
    for (int i = 0; i < _slots.count(); ++i) {
        QVariantMap map = _slots[i].toMap();
        map["vehicleId"] = 0;
        _slots[i] = map;
    }
    emit slotsChanged();
}

QVariantList FormationManager::maplrsVehicleIds(QmlObjectListModel* vehicles) const
{
    QVariantList out;
    if (!vehicles) {
        return out;
    }
    for (int i = 0; i < vehicles->count(); ++i) {
        auto* vehicle = qobject_cast<Vehicle*>(vehicles->get(i));
        if (vehicle && vehicle->firmwareType() == MAV_AUTOPILOT_SMARTAP) {
            out.append(vehicle->id());
        }
    }
    return out;
}

Vehicle* FormationManager::_findVehicle(QmlObjectListModel* vehicles, int vehicleId) const
{
    if (!vehicles) {
        return nullptr;
    }
    for (int i = 0; i < vehicles->count(); ++i) {
        auto* vehicle = qobject_cast<Vehicle*>(vehicles->get(i));
        if (vehicle && vehicle->id() == vehicleId) {
            return vehicle;
        }
    }
    return nullptr;
}

void FormationManager::autoAssign(QmlObjectListModel* vehicles)
{
    clearAssignment();

    // 领航机 = 第一台在线 MAPLRS 车；槽位绝对坐标锚在其当前位置
    Vehicle* leader = nullptr;
    const QVariantList ids = maplrsVehicleIds(vehicles);
    for (const QVariant& idVar : ids) {
        Vehicle* vehicle = _findVehicle(vehicles, idVar.toInt());
        if (vehicle && vehicle->coordinate().isValid()) {
            leader = vehicle;
            break;
        }
    }
    if (!leader) {
        return;
    }
    assignSlot(kLeaderSlot, leader->id());

    const QGeoCoordinate anchor = leader->coordinate();
    const double altRel = _vehicleFacts(leader)->altitudeRelative()->rawValue().toDouble();

    QList<Vehicle*> unassigned;
    for (const QVariant& idVar : ids) {
        Vehicle* vehicle = _findVehicle(vehicles, idVar.toInt());
        if (vehicle && vehicle->id() != leader->id() && vehicle->coordinate().isValid()) {
            unassigned.append(vehicle);
        }
    }
    // 贪心：槽位按序取离该槽绝对坐标最近的车
    for (int slotIndex = kLeaderSlot + 1; slotIndex < _slotEntries.count(); ++slotIndex) {
        if (unassigned.isEmpty()) {
            break;
        }
        const QGeoCoordinate slotCoord = _slotCoordinate(anchor, _slotEntries[slotIndex].offset, altRel);
        int nearest = -1;
        double nearestDist = 0.0;
        for (int i = 0; i < unassigned.count(); ++i) {
            const double dist = unassigned[i]->coordinate().distanceTo(slotCoord);
            if (nearest < 0 || dist < nearestDist) {
                nearestDist = dist;
                nearest = i;
            }
        }
        if (nearest >= 0) {
            const int vehicleId = unassigned.takeAt(nearest)->id();
            assignSlot(slotIndex, vehicleId);
        }
    }
}

QGeoCoordinate FormationManager::_slotCoordinate(const QGeoCoordinate& anchor, const QPointF& offset,
                                                 double altRel) const
{
    const double dist = qHypot(offset.x(), offset.y());
    double azimuth = qRadiansToDegrees(qAtan2(offset.x(), offset.y()));  // 罗盘角：北基准顺时针
    if (azimuth < 0.0) {
        azimuth += 360.0;
    }
    QGeoCoordinate coord = anchor.atDistanceAndAzimuth(dist, azimuth);
    coord.setAltitude(altRel);
    return coord;
}

QList<MissionItem*> FormationManager::_makeItems(const QGeoCoordinate& coord) const
{
    QList<MissionItem*> items;
    // 所有权归 PlanManager（它会 delete 载入列表，见 PlanManager.cc:_clearMissionItems）
    items.append(new MissionItem(1, MAV_CMD_NAV_WAYPOINT, MAV_FRAME_GLOBAL_RELATIVE_ALT, 0, 0, 0, 0, coord.latitude(),
                                 coord.longitude(), coord.altitude(), true /* autoContinue */,
                                 false /* isCurrentItem */, nullptr));
    items.append(new MissionItem(2, MAV_CMD_NAV_LOITER_UNLIM, MAV_FRAME_GLOBAL_RELATIVE_ALT, 0, 0, 0, 0,
                                 coord.latitude(), coord.longitude(), coord.altitude(), true, false, nullptr));
    return items;
}

void FormationManager::dispatch(QmlObjectListModel* vehicles)
{
    if (_busy || vehicles == nullptr) {
        return;
    }
    // 领航机只认槽 0；槽 0 未分配或不能定位则直接失败
    Vehicle* leader = nullptr;
    if (_slotEntries.count() > kLeaderSlot && _slotEntries[kLeaderSlot].vehicleId > 0) {
        leader = _findVehicle(vehicles, _slotEntries[kLeaderSlot].vehicleId);
    }
    if (!leader || !leader->coordinate().isValid()) {
        emit dispatchFinished(false);
        return;
    }

    const QGeoCoordinate anchor = leader->coordinate();
    const double headingDeg = _vehicleFacts(leader)->heading()->rawValue().toDouble();
    const double altRel = _vehicleFacts(leader)->altitudeRelative()->rawValue().toDouble();

    _queue.clear();
    _uploaded.clear();
    for (int slotIndex = kLeaderSlot + 1; slotIndex < _slotEntries.count(); ++slotIndex) {
        const int vehicleId = _slotEntries[slotIndex].vehicleId;
        if (vehicleId <= 0) {
            continue;
        }
        Vehicle* vehicle = _findVehicle(vehicles, vehicleId);
        if (!vehicle) {
            continue;
        }
        // 槽位偏移按领航机实时航向从本体系旋到地理系（东/北）
        const double rad = qDegreesToRadians(headingDeg);
        const double c = qCos(rad);
        const double s = qSin(rad);
        const QPointF& rel = _slotEntries[slotIndex].offset;
        const QPointF offset(rel.x() * c + rel.y() * s, -rel.x() * s + rel.y() * c);
        DispatchStep step;
        step.vehicle = vehicle;
        step.items = _makeItems(_slotCoordinate(anchor, offset, altRel));
        _queue.append(step);
    }
    if (_queue.isEmpty()) {
        emit dispatchFinished(false);
        return;
    }

    _setBusy(true);
    // 空中跟随者先暂停（POSHOLD），避免上传期间飘移
    for (const DispatchStep& step : std::as_const(_queue)) {
        if (step.vehicle && step.vehicle->flying()) {
            step.vehicle->sendMavCommand(step.vehicle->defaultComponentId(), MAV_CMD_DO_PAUSE_CONTINUE,
                                         false /* showError */, 0.0f /* pause */);
        }
    }
    QTimer::singleShot(kPauseGraceMSecs, this, &FormationManager::_uploadNext);
}

void FormationManager::_uploadNext()
{
    if (!_busy) {
        return;
    }
    // 半路掉线的车直接跳过
    while (!_queue.isEmpty() && _queue.first().vehicle == nullptr) {
        _queue.takeFirst();
    }
    if (_queue.isEmpty()) {
        _startAll(true);
        return;
    }

    const DispatchStep step = _queue.takeFirst();
    QPointer<Vehicle> vehicle = step.vehicle;
    MissionManager* missionManager = vehicle ? vehicle->missionManager() : nullptr;
    if (!vehicle || !missionManager || missionManager->inProgress()) {
        _startAll(false);
        return;
    }

    if (_sendCompleteConn) {
        disconnect(_sendCompleteConn);
        _sendCompleteConn = QMetaObject::Connection();
    }
    _sendCompleteConn = connect(missionManager, &MissionManager::sendComplete, this, [this, vehicle](bool error) {
        if (_sendCompleteConn) {
            disconnect(_sendCompleteConn);
            _sendCompleteConn = QMetaObject::Connection();
        }
        if (error) {
            _startAll(false);
            return;
        }
        _uploaded.append(vehicle);
        QTimer::singleShot(0, this, &FormationManager::_uploadNext);
    });
    _timeoutTimer.start(kUploadTimeoutMSecs);

    missionManager->writeMissionItems(step.items);
}

void FormationManager::_startAll(bool success)
{
    if (_sendCompleteConn) {
        disconnect(_sendCompleteConn);
        _sendCompleteConn = QMetaObject::Connection();
    }
    _timeoutTimer.stop();
    // 上传有任何一个失败都不能放开编队；全部成功后逐台 MISSION_START（单帧，快）
    const bool started = success && !_uploaded.isEmpty();
    if (started) {
        for (const QPointer<Vehicle>& vehicle : std::as_const(_uploaded)) {
            if (vehicle) {
                vehicle->startMission();
            }
        }
    }
    _queue.clear();
    _uploaded.clear();
    _setBusy(false);
    emit dispatchFinished(started);
}

QVariantList FormationManager::formationStatus(QmlObjectListModel* vehicles) const
{
    QVariantList out;
    for (int slotIndex = 0; slotIndex < _slotEntries.count(); ++slotIndex) {
        const SlotEntry& entry = _slotEntries[slotIndex];
        QVariantMap map;
        map["slotIndex"] = slotIndex;
        map["vehicleId"] = entry.vehicleId;
        double errorM = -1.0;
        if (entry.vehicleId > 0) {
            Vehicle* vehicle = _findVehicle(vehicles, entry.vehicleId);
            if (vehicle && vehicle->coordinate().isValid()) {
                // 领航机为锚：误差 = 本机当前位置到其槽位绝对坐标
                Vehicle* leader = vehicle;
                if (slotIndex != kLeaderSlot) {
                    if (_slotEntries[kLeaderSlot].vehicleId > 0) {
                        leader = _findVehicle(vehicles, _slotEntries[kLeaderSlot].vehicleId);
                    } else {
                        leader = nullptr;
                    }
                }
                if (leader && leader->coordinate().isValid()) {
                    const double headingDeg = _vehicleFacts(leader)->heading()->rawValue().toDouble();
                    const double rad = qDegreesToRadians(headingDeg);
                    const double c = qCos(rad);
                    const double s = qSin(rad);
                    const QPointF offset(entry.offset.x() * c + entry.offset.y() * s,
                                         -entry.offset.x() * s + entry.offset.y() * c);
                    const QGeoCoordinate slotCoord = _slotCoordinate(
                        leader->coordinate(), offset, _vehicleFacts(leader)->altitudeRelative()->rawValue().toDouble());
                    errorM = vehicle->coordinate().distanceTo(slotCoord);
                }
            }
        }
        map["errorM"] = errorM;
        out.append(map);
    }
    return out;
}

void FormationManager::_setBusy(bool busy)
{
    if (_busy == busy) {
        return;
    }
    _busy = busy;
    emit busyChanged();
}
