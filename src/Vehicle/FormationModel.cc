#include "FormationModel.h"

#include <QVariantMap>
#include <QtMath>
#include <algorithm>
#include <cmath>

#include "Fact.h"
#include "FactGroup.h"
#include "FormationShape.h"
#include "QmlObjectListModel.h"
#include "Vehicle.h"

FormationModel::FormationModel(QObject* parent) : QAbstractListModel(parent)
{
    // 领机由成员状态与本地指定共同推导；任一变化后重算，只在结果变化时通知。
    connect(this, &FormationModel::configChanged, this, &FormationModel::_refreshLeader);
    connect(this, &QAbstractItemModel::dataChanged, this, &FormationModel::_refreshLeader);
    connect(this, &QAbstractItemModel::modelReset, this, &FormationModel::_refreshLeader);
}

int FormationModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : _members.count();
}

QVariant FormationModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= _members.count()) {
        return {};
    }

    const Member& member = _members[index.row()];
    switch (role) {
        case SysidRole:
            return member.sysid;
        case RoleRole:
            // 被踢出的成员即使编队 id 已被清 0，也要先显示 LEAVING，保留 Rejoin 语义。
            if (member.flags & 0x01) {
                return QStringLiteral("Leaving");
            }
            // formation_id 为 0 表示已停止/退出编队；不能再显示成 Follower。
            if (member.formationId == 0) {
                return QStringLiteral("Idle");
            }
            return (member.flags & 0x04) ? QStringLiteral("Leader") : QStringLiteral("Follower");
        case OnlineRole:
            return member.online;
        case PositionRole:
            return member.position;
        case PhaseRole:
            return member.phase;
        case LeaderIdRole:
            return member.leaderId;
        case FlagsRole:
            return member.flags;
        case BatteryRole:
            return member.batteryPct;
        default:
            return {};
    }
}

QHash<int, QByteArray> FormationModel::roleNames() const
{
    return {
        {SysidRole, "sysid"}, {RoleRole, "role"},         {OnlineRole, "online"},        {PositionRole, "position"},
        {PhaseRole, "phase"}, {LeaderIdRole, "leaderId"}, {FlagsRole, "formationFlags"}, {BatteryRole, "battery"},
    };
}

void FormationModel::matchOnline(QmlObjectListModel* vehicles)
{
    if (!vehicles) {
        return;
    }

    QList<int> online;
    for (int i = 0; i < vehicles->count(); ++i) {
        Vehicle* vehicle = vehicles->value<Vehicle*>(i);
        if (_isMaplrs(vehicle)) {
            online.append(vehicle->id());
        }
    }
    std::sort(online.begin(), online.end());

    if (_active) {
        for (int row = 0; row < _members.count(); ++row) {
            const bool present = online.contains(_members[row].sysid);
            if (_members[row].online != present) {
                _members[row].online = present;
                _emitRowChanged(row);
            }
        }
        emit configChanged();
        return;
    }

    QHash<int, Member> old;
    for (const Member& member : _members) {
        old.insert(member.sysid, member);
    }

    beginResetModel();
    _members.clear();
    for (int sysid : online) {
        Member member;
        member.sysid = sysid;
        member.online = true;
        if (old.contains(sysid)) {
            const Member previous = old.value(sysid);
            member.position = previous.position;
            member.phase = previous.phase;
            member.leaderId = previous.leaderId;
            member.flags = previous.flags;
            member.formationId = previous.formationId;
            member.batteryPct = previous.batteryPct;
        }
        _members.append(member);
    }
    endResetModel();

    emit configChanged();
}

void FormationModel::setActive(bool active)
{
    if (_active == active) {
        return;
    }
    _active = active;
    emit formationActiveChanged();
}

void FormationModel::markOffline(int sysid)
{
    const int row = _rowForSysid(sysid);
    if (row < 0 || !_members[row].online) {
        return;
    }
    _members[row].online = false;
    _emitRowChanged(row);
    emit configChanged();
}

void FormationModel::markLeaving(int sysid)
{
    const int row = _rowForSysid(sysid);
    if (row < 0) {
        return;
    }
    Member& member = _members[row];
    member.flags |= 0x01;
    member.flags &= ~0x04;
    member.position = -1;
    member.phase = 0;
    member.leaderId = 0;
    _emitRowChanged(row);
    emit configChanged();
}

void FormationModel::markRejoining(int sysid)
{
    const int row = _rowForSysid(sysid);
    if (row < 0) {
        return;
    }
    Member& member = _members[row];
    member.flags &= ~0x01;
    member.position = -1;
    member.phase = 0;
    _emitRowChanged(row);
    emit configChanged();
}

void FormationModel::clearLeaving()
{
    bool changed = false;
    for (int row = 0; row < _members.count(); ++row) {
        Member& member = _members[row];
        if ((member.flags & 0x01) == 0) {
            continue;
        }
        member.flags &= ~0x01;
        _emitRowChanged(row);
        changed = true;
    }
    if (changed) {
        emit configChanged();
    }
}

bool FormationModel::isOnline(int sysid) const
{
    const int row = _rowForSysid(sysid);
    return row >= 0 && _members[row].online;
}

bool FormationModel::isLeaving(int sysid) const
{
    const int row = _rowForSysid(sysid);
    return row >= 0 && (_members[row].flags & 0x01);
}

void FormationModel::updateStatus(int sysid, int position, int phase, int leaderId, int flags, int formationId)
{
    const int row = _rowForSysid(sysid);
    if (row < 0) {
        return;
    }

    Member& member = _members[row];
    const bool wasOnline = member.online;
    const bool wasLeaving = (member.flags & 0x01) != 0;
    member.position = position;
    member.phase = phase;
    member.leaderId = leaderId;
    member.flags = flags;
    member.formationId = formationId;
    member.online = true;
    // 本机刚执行 Kick 时，机上可能回旧的“非 LEAVING”状态，甚至已经把
    // formation_id 清成 0；保留本地 LEAVING，避免领机行闪成 Idle 并禁用 Rejoin。
    // Rejoin/Apply/Stop 会显式清本地位。
    if (wasLeaving && !(flags & 0x01)) {
        member.flags |= 0x01;
    }
    _emitRowChanged(row);

    if (!wasOnline || wasLeaving != ((flags & 0x01) != 0)) {
        emit configChanged();
    }
}

bool FormationModel::validate() const
{
    _lastError.clear();

    if (_members.isEmpty()) {
        _lastError = QStringLiteral("没有编队成员");
        return false;
    }
    if (_shape < 0 || _shape > 3) {
        _lastError = QStringLiteral("队形模板非法");
        return false;
    }
    if (!std::isfinite(_spacingM) || _spacingM < 3.0 || _spacingM > 500.0) {
        _lastError = QStringLiteral("间距必须在 3..500m");
        return false;
    }
    if (!std::isfinite(_vAngleDeg) || _vAngleDeg < 0.0 || _vAngleDeg > 90.0) {
        _lastError = QStringLiteral("V 字夹角必须在 0..90°");
        return false;
    }
    if (_leaderOverride != 0 && _rowForSysid(_leaderOverride) < 0) {
        _lastError = QStringLiteral("指定的领机不在编队成员中");
        return false;
    }
    return true;
}

int FormationModel::leaderSysid() const
{
    int leader = 0;
    for (const Member& member : _members) {
        if ((member.flags & 0x04) && member.online && !(member.flags & 0x01) &&
            (leader == 0 || member.sysid < leader)) {
            leader = member.sysid;
        }
    }
    if (leader != 0) {
        return leader;
    }
    const int overrideRow = _rowForSysid(_leaderOverride);
    if (_leaderOverride != 0 && overrideRow >= 0) {
        const Member& override = _members[overrideRow];
        if (override.online && !(override.flags & 0x01)) {
            return _leaderOverride;
        }
    }
    // 有些固件只上报 leaderId 未置 LEADER 位；只要 leaderId 仍是当前在线成员，也能聚焦。
    for (const Member& member : _members) {
        const int leaderRow = _rowForSysid(member.leaderId);
        if (!member.online || (member.flags & 0x01)) {
            continue;
        }
        if (member.leaderId > 0 && leaderRow >= 0 && _members[leaderRow].online &&
            !(_members[leaderRow].flags & 0x01)) {
            return member.leaderId;
        }
    }
    return 0;
}

namespace {

QGeoCoordinate _coordinateOf(Vehicle* vehicle)
{
    return vehicle ? vehicle->coordinate() : QGeoCoordinate();
}

double _headingOf(Vehicle* vehicle)
{
    if (!vehicle || !vehicle->vehicleFactGroup()) {
        return 0.0;
    }
    Fact* fact = vehicle->vehicleFactGroup()->getFact(QStringLiteral("heading"));
    const double value = fact ? fact->rawValue().toDouble() : 0.0;
    return std::isfinite(value) ? value : 0.0;
}

}  // namespace

Vehicle* FormationModel::_resolveLeader(QmlObjectListModel* vehicles) const
{
    if (!vehicles) {
        return nullptr;
    }

    // 领机优先取成员状态推导的领机；还没有编队状态时退回最小 sysid 的在线定位机。
    Vehicle* leader = nullptr;
    const int leaderId = leaderSysid();
    for (int i = 0; i < vehicles->count(); ++i) {
        Vehicle* vehicle = vehicles->value<Vehicle*>(i);
        if (!_isMaplrs(vehicle) || !_coordinateOf(vehicle).isValid()) {
            continue;
        }
        if (leaderId != 0) {
            if (vehicle->id() == leaderId) {
                return vehicle;
            }
            continue;
        }
        if (!leader || vehicle->id() < leader->id()) {
            leader = vehicle;
        }
    }
    return leader;
}

QVariantList FormationModel::liveLayout(QmlObjectListModel* vehicles) const
{
    QVariantList out;
    Vehicle* leader = _resolveLeader(vehicles);
    if (!leader) {
        return out;
    }

    const bool hasLeaderId = leaderSysid() != 0;
    const QGeoCoordinate origin = _coordinateOf(leader);
    const double metersPerDegLon = 111320.0 * std::cos(qDegreesToRadians(origin.latitude()));

    for (const Member& member : _members) {
        Vehicle* vehicle = nullptr;
        for (int i = 0; i < vehicles->count() && !vehicle; ++i) {
            Vehicle* candidate = vehicles->value<Vehicle*>(i);
            if (candidate && candidate->id() == member.sysid) {
                vehicle = candidate;
            }
        }
        const QGeoCoordinate coord = _coordinateOf(vehicle);
        if (!vehicle || !coord.isValid()) {
            continue;
        }

        double heading = std::fmod(_headingOf(vehicle), 360.0);
        if (heading < 0.0) {
            heading += 360.0;
        }

        QVariantMap entry;
        entry[QStringLiteral("sysid")] = member.sysid;
        entry[QStringLiteral("north")] = (coord.latitude() - origin.latitude()) * 111320.0;
        entry[QStringLiteral("east")] = (coord.longitude() - origin.longitude()) * metersPerDegLon;
        entry[QStringLiteral("heading")] = heading;
        entry[QStringLiteral("isLeader")] = (vehicle == leader) && hasLeaderId;
        entry[QStringLiteral("online")] = member.online;
        entry[QStringLiteral("leaving")] = (member.flags & 0x01) != 0;
        entry[QStringLiteral("position")] = member.position;
        out.append(entry);
    }
    return out;
}

QVariantList FormationModel::positions() const
{
    const int count = _activeMemberCount();
    const QList<FormationShape::SlotPolar> polar =
        FormationShape::slotPolar(static_cast<FormationShape::Shape>(_shape), count, _spacingM, _vAngleDeg);

    QVariantList out;
    for (const FormationShape::SlotPolar& slot : polar) {
        const double angleRad = qDegreesToRadians(slot.angleDeg);
        QVariantMap map;
        map["distance"] = slot.distanceM;
        map["angle"] = slot.angleDeg;
        map["forward"] = slot.distanceM * qCos(angleRad);
        map["right"] = slot.distanceM * qSin(angleRad);
        out.append(map);
    }
    return out;
}

QVariantList FormationModel::sysids() const
{
    QVariantList out;
    for (const Member& member : _members) {
        if (member.sysid > 0) {
            out.append(member.sysid);
        }
    }
    return out;
}

int FormationModel::formationMemberCount() const
{
    int count = 0;
    for (const Member& member : _members) {
        if (member.online && !(member.flags & 0x01) && member.formationId != 0) {
            ++count;
        }
    }
    return count;
}

void FormationModel::setShape(int shape)
{
    if (shape < 0 || shape > 3 || _shape == shape) {
        return;
    }
    _shape = shape;
    emit configChanged();
}

void FormationModel::setSpacingM(double spacingM)
{
    if (!std::isfinite(spacingM) || _spacingM == spacingM) {
        return;
    }
    _spacingM = spacingM;
    emit configChanged();
}

void FormationModel::setVAngleDeg(double vAngleDeg)
{
    if (!std::isfinite(vAngleDeg) || _vAngleDeg == vAngleDeg) {
        return;
    }
    _vAngleDeg = vAngleDeg;
    emit configChanged();
}

void FormationModel::setLeader(int sysid)
{
    if (sysid < 0 || _leaderOverride == sysid) {
        return;
    }
    _leaderOverride = sysid;
    emit configChanged();
}

void FormationModel::setEnabled(bool enabled)
{
    if (_enabled == enabled) {
        return;
    }
    _enabled = enabled;
    emit configChanged();
}

void FormationModel::setFormationId(int formationId)
{
    const uint16_t id = static_cast<uint16_t>(formationId);
    if (_formationId == id) {
        return;
    }
    _formationId = id;
    emit configChanged();
}

bool FormationModel::_isMaplrs(Vehicle* vehicle)
{
    return vehicle != nullptr && vehicle->maplrsFirmware();
}

void FormationModel::_emitRowChanged(int row)
{
    if (row < 0 || row >= _members.count()) {
        return;
    }
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx);
}

void FormationModel::_refreshLeader()
{
    const int leader = leaderSysid();
    if (leader == _cachedLeader) {
        return;
    }
    _cachedLeader = leader;
    emit leaderChanged();
}

int FormationModel::_activeMemberCount() const
{
    int count = 0;
    for (const Member& member : _members) {
        if (member.online && !(member.flags & 0x01)) {
            ++count;
        }
    }
    return count > 0 ? count : 1;
}

int FormationModel::_rowForSysid(int sysid) const
{
    for (int row = 0; row < _members.count(); ++row) {
        if (_members[row].sysid == sysid) {
            return row;
        }
    }
    return -1;
}
