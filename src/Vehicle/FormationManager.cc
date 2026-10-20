#include "FormationManager.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSettings>

#include "FormationProtocol.h"
#include "FormationShape.h"
#include "QGCMAVLink.h"
#include "QmlObjectListModel.h"
#include "Vehicle.h"

namespace {
constexpr int kLeaderSlot = 0;
constexpr const char* kPresetsGroup = "Formation/Presets";

bool _isMaplrs(Vehicle* vehicle)
{
    return vehicle && vehicle->firmwareType() == MAV_AUTOPILOT_SMARTAP;
}
}  // namespace

FormationManager* FormationManager::instance()
{
    static FormationManager instanceSingleton;
    return &instanceSingleton;
}

FormationManager::FormationManager(QObject* parent) : QObject(parent)
{
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

void FormationManager::setHeadingDeg(double headingDeg)
{
    if (headingDeg == _headingDeg) {
        return;
    }
    _headingDeg = headingDeg;
    emit headingDegChanged();
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
    // 队形/间距/朝向变化时保留已有分配：按槽位序号沿用
    QList<int> oldVehicleIds;
    oldVehicleIds.reserve(_slotEntries.count());
    for (const SlotEntry& entry : std::as_const(_slotEntries)) {
        oldVehicleIds.append(entry.vehicleId);
    }

    const QList<QPointF> offsets = FormationShape::slotOffsets(static_cast<FormationShape::Shape>(_shapeIndex),
                                                               _slotCount, _spacing, _headingDeg);
    _slotEntries.clear();
    _slots.clear();
    for (int i = 0; i < offsets.count(); ++i) {
        SlotEntry entry;
        entry.vehicleId = i < oldVehicleIds.count() ? oldVehicleIds[i] : 0;
        entry.offset = offsets[i];
        QVariantMap map;
        map["x"] = entry.offset.x();
        map["y"] = entry.offset.y();
        map["vehicleId"] = entry.vehicleId;
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
        if (_isMaplrs(vehicle)) {
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
    const double alt = anchor.altitude();

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
        const QGeoCoordinate slotCoord = _slotCoordinate(anchor, _slotEntries[slotIndex].offset, alt);
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

void FormationManager::matchOnline(QmlObjectListModel* vehicles)
{
    setSlotCount(qMax(1, maplrsVehicleIds(vehicles).count()));
}

QGeoCoordinate FormationManager::_slotCoordinate(const QGeoCoordinate& anchor, const QPointF& offset,
                                                 double alt) const
{
    const double dist = qHypot(offset.x(), offset.y());
    double azimuth = qRadiansToDegrees(qAtan2(offset.x(), offset.y()));  // 罗盘角：北基准顺时针
    if (azimuth < 0.0) {
        azimuth += 360.0;
    }
    QGeoCoordinate coord = anchor.atDistanceAndAzimuth(dist, azimuth);
    coord.setAltitude(alt);
    return coord;
}

void FormationManager::applyFormation(QmlObjectListModel* vehicles)
{
    if (!vehicles || _slotEntries.count() <= kLeaderSlot) {
        emit dispatchFinished(false);
        return;
    }
    // 槽 0 是领航机：必须在线且是 MAPLRS；它只做锚点，不收 FORM_ASSIGN
    Vehicle* leader = _findVehicle(vehicles, _slotEntries[kLeaderSlot].vehicleId);
    if (!_isMaplrs(leader)) {
        emit dispatchFinished(false);
        return;
    }

    _formationId = FormationProtocol::nextFormationId(_formationId);
    _setBusy(true);
    int sent = 0;
    for (int slotIndex = kLeaderSlot + 1; slotIndex < _slotEntries.count(); ++slotIndex) {
        const SlotEntry& entry = _slotEntries[slotIndex];
        if (entry.vehicleId <= 0) {
            continue;
        }
        Vehicle* vehicle = _findVehicle(vehicles, entry.vehicleId);
        if (!_isMaplrs(vehicle)) {
            continue;
        }
        FormationProtocol::FormationAssign assign;
        assign.formationId = _formationId;
        assign.slot = static_cast<uint8_t>(slotIndex);
        assign.leaderId = static_cast<uint8_t>(leader->id());
        assign.dxEast = static_cast<float>(entry.offset.x());
        assign.dyNorth = static_cast<float>(entry.offset.y());
        const FormationProtocol::CommandParams params = FormationProtocol::formAssignParams(assign);
        vehicle->sendMavCommand(vehicle->defaultComponentId(),
                                static_cast<MAV_CMD>(FormationProtocol::kMavCmdFormAssign), true,
                                params.param1, params.param2, params.param3, params.param4, params.param5,
                                params.param6, params.param7);
        ++sent;
    }
    _setBusy(false);
    emit dispatchFinished(sent > 0);
}

QString FormationManager::_presetKey(const QString& name)
{
    return QString(kPresetsGroup) + '/' + name;
}

QStringList FormationManager::presetNames() const
{
    QSettings settings;
    settings.beginGroup(kPresetsGroup);
    QStringList names = settings.childKeys();
    names.sort();
    return names;
}

bool FormationManager::savePreset(const QString& name)
{
    if (name.trimmed().isEmpty() || name.contains('/')) {
        return false;
    }

    QJsonArray assignments;
    for (int i = 0; i < _slotEntries.count(); ++i) {
        if (_slotEntries[i].vehicleId > 0) {
            QJsonObject item;
            item["slot"] = i;
            item["vehicleId"] = _slotEntries[i].vehicleId;
            assignments.append(item);
        }
    }
    QJsonObject preset;
    preset["shapeIndex"] = _shapeIndex;
    preset["spacing"] = _spacing;
    preset["headingDeg"] = _headingDeg;
    preset["slotCount"] = _slotCount;
    preset["assignments"] = assignments;

    QSettings settings;
    settings.setValue(_presetKey(name), QJsonDocument(preset).toJson(QJsonDocument::Compact));
    return settings.status() == QSettings::NoError;
}

bool FormationManager::loadPreset(const QString& name)
{
    QSettings settings;
    const QByteArray raw = settings.value(_presetKey(name)).toByteArray();
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &error);
    if (raw.isEmpty() || error.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    const QJsonObject preset = doc.object();
    setShapeIndex(preset.value("shapeIndex").toInt(_shapeIndex));
    setSpacing(preset.value("spacing").toDouble(_spacing));
    setSlotCount(preset.value("slotCount").toInt(_slotCount));
    setHeadingDeg(preset.value("headingDeg").toDouble(_headingDeg));

    clearAssignment();
    const QJsonArray assignments = preset.value("assignments").toArray();
    for (const QJsonValue& value : assignments) {
        const QJsonObject item = value.toObject();
        assignSlot(item.value("slot").toInt(-1), item.value("vehicleId").toInt());
    }
    return true;
}

bool FormationManager::deletePreset(const QString& name)
{
    QSettings settings;
    const QString key = _presetKey(name);
    const bool existed = settings.contains(key);
    settings.remove(key);
    return existed;
}

QVariantList FormationManager::formationStatus(QmlObjectListModel* vehicles) const
{
    QVariantList out;

    // 目标 = 领航机实时坐标 + 已旋转槽位偏移；领航机离线时全部无法定位
    Vehicle* leader = nullptr;
    if (_slotEntries.count() > kLeaderSlot) {
        leader = _findVehicle(vehicles, _slotEntries[kLeaderSlot].vehicleId);
    }
    const bool leaderReady = _isMaplrs(leader) && leader->coordinate().isValid();

    for (int slotIndex = 0; slotIndex < _slotEntries.count(); ++slotIndex) {
        const SlotEntry& entry = _slotEntries[slotIndex];
        QVariantMap map;
        map["slotIndex"] = slotIndex;
        map["vehicleId"] = entry.vehicleId;
        double errorM = -1.0;
        if (entry.vehicleId > 0 && leaderReady) {
            Vehicle* vehicle = _findVehicle(vehicles, entry.vehicleId);
            if (vehicle && vehicle->coordinate().isValid()) {
                const QGeoCoordinate anchor = leader->coordinate();
                const QGeoCoordinate slotCoord = _slotCoordinate(anchor, entry.offset, anchor.altitude());
                QGeoCoordinate vehicleCoord = vehicle->coordinate();
                vehicleCoord.setAltitude(slotCoord.altitude());  // 只比较水平误差
                errorM = vehicleCoord.distanceTo(slotCoord);
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
