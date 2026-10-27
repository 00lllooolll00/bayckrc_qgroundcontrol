#include "FormationModel.h"

#include <QVariantMap>
#include <QtMath>
#include <algorithm>
#include <cmath>

#include "FormationShape.h"
#include "QmlObjectListModel.h"
#include "Vehicle.h"

FormationModel::FormationModel(QObject* parent) : QAbstractListModel(parent) {}

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
            // formation_id 为 0 表示已停止/退出编队；不能再显示成 Follower。
            if (member.formationId == 0) {
                return QStringLiteral("Idle");
            }
            // LEAVING 优先于 LEADER：被踢的领机也不能继续显示为领机。
            if (member.flags & 0x01) {
                return QStringLiteral("Leaving");
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
    // 本机刚执行 Kick 时，机上可能还在回旧的“非 LEAVING”状态；保留本地
    // LEAVING，避免角色/按钮在命令生效前闪回 Follower。Rejoin 会先清本地位。
    if (formationId != 0 && wasLeaving && !(flags & 0x01)) {
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
