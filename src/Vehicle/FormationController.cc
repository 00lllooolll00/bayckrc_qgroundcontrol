#include "FormationController.h"

#include <QGeoCoordinate>
#include <QSettings>

#include "FormationModel.h"
#include "FormationProtocol.h"
#include "MultiVehicleManager.h"
#include "QmlObjectListModel.h"
#include "Vehicle.h"
#include "VehicleLinkManager.h"

FormationController* FormationController::instance()
{
    static FormationController* controller = new FormationController();
    return controller;
}

FormationController::FormationController(QObject* parent)
    : QObject(parent)
    , _model(new FormationModel(this))
{
    _timeoutTimer.setSingleShot(true);
    connect(&_timeoutTimer, &QTimer::timeout, this, &FormationController::_onTimeout);

    QSettings settings;
    _formationId = static_cast<uint16_t>(settings.value(QStringLiteral("Formation/formationId"), 0).toUInt());
    _model->setFormationId(_formationId);

    MultiVehicleManager* manager = MultiVehicleManager::instance();
    _vehicles = manager->vehicles();
    // 先按当前已连接飞机建表，避免面板首次打开时成员表为空。
    _model->matchOnline(_vehicles);
    for (int i = 0; i < _vehicles->count(); ++i) {
        Vehicle* vehicle = _vehicles->value<Vehicle*>(i);
        if (vehicle) {
            connect(vehicle, &Vehicle::textMessageReceived, this, &FormationController::_onVehicleText,
                    Qt::UniqueConnection);
        }
    }

    connect(manager, &MultiVehicleManager::vehicleAdded, this, [this](Vehicle* vehicle) {
        connect(vehicle->vehicleLinkManager(), &VehicleLinkManager::allLinksRemoved, this,
                [this](Vehicle* lost) { _model->markOffline(lost->id()); });
        // 只是连接成功不保证已收到过 STATUSTEXT，这里直接订阅，保证编队状态能更新。
        connect(vehicle, &Vehicle::textMessageReceived, this, &FormationController::_onVehicleText,
                Qt::UniqueConnection);
        _model->matchOnline(_vehicles);
    });
    connect(manager, &MultiVehicleManager::vehicleRemoved, this,
            [this](Vehicle*) { _model->matchOnline(_vehicles); });

    // 队形参数或成员一改就撤掉上一次 Apply 的红色错误。
    connect(_model, &FormationModel::configChanged, this, [this]() {
        if (!_errorString.isEmpty()) {
            _setError(QString());
        }
    });
}

void FormationController::apply(QmlObjectListModel* vehicles)
{
    if (!vehicles) {
        return;
    }
    if (!_model->validate()) {
        _setError(_model->validationError());
        _setPhase(Failed);
        return;
    }

    _vehicles = vehicles;
    _formationId = FormationProtocol::nextFormationId(_formationId);
    QSettings().setValue(QStringLiteral("Formation/formationId"), _formationId);
    _model->setFormationId(_formationId);
    _pending = 0;

    for (int row = 0; row < _model->rowCount(); ++row) {
        const int sysid = _model->data(_model->index(row, 0), FormationModel::SysidRole).toInt();
        if (sysid <= 0) {
            continue;
        }
        const int flags = _model->data(_model->index(row, 0), FormationModel::FlagsRole).toInt();
        if (flags & 0x01) {
            continue;
        }
        Vehicle* vehicle = _findVehicle(vehicles, sysid);
        if (!vehicle) {
            continue;
        }
        connect(vehicle, &Vehicle::textMessageReceived, this, &FormationController::_onVehicleText,
                Qt::UniqueConnection);
        _sendConfig(vehicle, _formationId);
        ++_pending;
    }

    if (_pending == 0) {
        _setError(QStringLiteral("没有可下发的在线成员"));
        _setPhase(Failed);
        return;
    }

    _setError(QString());
    _model->setActive(true);
    _setPhase(Applying);
    _timeoutTimer.start(3000);
}

void FormationController::stop(QmlObjectListModel* vehicles, bool allMembers)
{
    if (!vehicles) {
        return;
    }

    _vehicles = vehicles;
    _pending = 0;
    for (int row = 0; row < _model->rowCount(); ++row) {
        const int sysid = _model->data(_model->index(row, 0), FormationModel::SysidRole).toInt();
        if (sysid <= 0) {
            continue;
        }
        Vehicle* vehicle = _findVehicle(vehicles, sysid);
        if (!vehicle) {
            continue;
        }
        _sendStop(vehicle, allMembers);
        ++_pending;
    }

    _model->setActive(false);
    _setPhase(_pending > 0 ? Stopping : Idle);
}

void FormationController::kick(QmlObjectListModel* vehicles, int sysid)
{
    if (!vehicles || sysid <= 0) {
        return;
    }
    if (_model->formationId() == 0) {
        _setError(QStringLiteral("还没有可用的编队配置"));
        return;
    }
    Vehicle* vehicle = _findVehicle(vehicles, sysid);
    if (!vehicle) {
        _setError(QStringLiteral("成员不在线"));
        return;
    }
    // 先判领机：markLeaving 会清掉 leaderId/flags，之后再判就看不出来了。
    const bool kickedLeader = (_model->leaderSysid() == sysid);
    _model->markLeaving(sysid);
    if (kickedLeader || _model->leaderOverride() == sysid) {
        _model->setLeader(0);
    }
    _sendMember(vehicle, static_cast<uint16_t>(_model->formationId()), false);
}

void FormationController::rejoin(QmlObjectListModel* vehicles, int sysid)
{
    if (!vehicles || sysid <= 0) {
        return;
    }
    if (_model->formationId() == 0) {
        _setError(QStringLiteral("先应用一次编队配置"));
        return;
    }
    Vehicle* vehicle = _findVehicle(vehicles, sysid);
    if (!vehicle) {
        _setError(QStringLiteral("成员不在线"));
        return;
    }
    // 先清 LEAVING 位再下发：否则 config 会被机上当作 leave 状态忽略。
    _model->markRejoining(sysid);
    _sendConfig(vehicle, static_cast<uint16_t>(_model->formationId()));
    _sendMember(vehicle, static_cast<uint16_t>(_model->formationId()), true);
}

void FormationController::takeoffAll(QmlObjectListModel* vehicles)
{
    if (!vehicles) {
        return;
    }
    for (int row = 0; row < _model->rowCount(); ++row) {
        const int sysid = _model->data(_model->index(row, 0), FormationModel::SysidRole).toInt();
        if (sysid <= 0) {
            continue;
        }
        Vehicle* vehicle = _findVehicle(vehicles, sysid);
        if (vehicle) {
            vehicle->startTakeoff();
        }
    }
}

void FormationController::landAll(QmlObjectListModel* vehicles)
{
    if (!vehicles) {
        return;
    }
    _model->setActive(false);
    for (int row = 0; row < _model->rowCount(); ++row) {
        const int sysid = _model->data(_model->index(row, 0), FormationModel::SysidRole).toInt();
        if (sysid <= 0) {
            continue;
        }
        Vehicle* vehicle = _findVehicle(vehicles, sysid);
        if (vehicle) {
            vehicle->guidedModeLand();
        }
    }
}

void FormationController::gotoLeader(QmlObjectListModel* vehicles, double latitude, double longitude, double altitude)
{
    if (!vehicles) {
        return;
    }
    const int leaderSysid = _model->leaderSysid();
    Vehicle* leader = _findVehicle(vehicles, leaderSysid);
    if (!leader) {
        _setError(QStringLiteral("领航机不在线"));
        return;
    }
    _setError(QString());
    leader->guidedModeGotoLocation(QGeoCoordinate(latitude, longitude, altitude));
}

void FormationController::handleStatustext(const QString& text)
{
    if (!text.startsWith(QStringLiteral("FORM:"))) {
        return;
    }
    const QStringList parts = text.mid(5).split(QLatin1Char(','));
    if (parts.count() != 6) {
        return;
    }

    const int node = parts[0].toInt();
    const int position = (parts[1].toInt() == 0xFF) ? -1 : parts[1].toInt();
    const int phase = parts[2].toInt();
    const int leaderId = parts[3].toInt();
    const int flags = parts[4].toInt();
    const int formationId = parts[5].toInt();
    // 机上已在跑更新的编队（例如 QGC 重启后 id 落后）：采用更大的 id，避免按旧 id 下发被丢弃。
    if (formationId > 0 && formationId <= 0xFFFF) {
        const uint16_t incomingId = static_cast<uint16_t>(formationId);
        if (incomingId > _formationId) {
            _formationId = incomingId;
            QSettings().setValue(QStringLiteral("Formation/formationId"), _formationId);
            _model->setFormationId(_formationId);
        }
    }
    _model->updateStatus(node, position, phase, leaderId, flags, formationId);

    if (phase == 2 && _phase == Applying) {
        _timeoutTimer.stop();
        _setPhase(Active);
    }
}

Vehicle* FormationController::_findVehicle(QmlObjectListModel* vehicles, int sysid) const
{
    for (int i = 0; i < vehicles->count(); ++i) {
        Vehicle* vehicle = vehicles->value<Vehicle*>(i);
        if (vehicle && vehicle->id() == sysid) {
            return vehicle;
        }
    }
    return nullptr;
}

void FormationController::_sendConfig(Vehicle* vehicle, uint16_t formationId)
{
    FormationProtocol::FormationConfig config;
    config.formationId = formationId;
    config.shape = static_cast<uint8_t>(_model->shape());
    config.spacingM = static_cast<float>(_model->spacingM());
    config.vAngleDeg = static_cast<float>(_model->vAngleDeg());
    config.leaderOverride = static_cast<uint8_t>(_model->leaderOverride());
    config.enabled = _model->enabled();

    const FormationProtocol::CommandParams params = FormationProtocol::formConfigParams(config);
    vehicle->sendMavCommand(vehicle->defaultComponentId(),
                            static_cast<MAV_CMD>(FormationProtocol::kMavCmdFormConfig),
                            true,
                            params.param1,
                            params.param2,
                            params.param3,
                            params.param4,
                            params.param5,
                            params.param6,
                            params.param7);
}

void FormationController::_sendMember(Vehicle* vehicle, uint16_t formationId, bool join)
{
    FormationProtocol::FormationMemberCommand member;
    member.formationId = formationId;
    member.join = join;

    const FormationProtocol::CommandParams params = FormationProtocol::formMemberParams(member);
    vehicle->sendMavCommand(vehicle->defaultComponentId(),
                            static_cast<MAV_CMD>(FormationProtocol::kMavCmdFormMember),
                            true,
                            params.param1,
                            params.param2,
                            params.param3,
                            params.param4,
                            params.param5,
                            params.param6,
                            params.param7);
}

void FormationController::_sendStop(Vehicle* vehicle, bool allMembers)
{
    FormationProtocol::FormationStop stop;
    stop.formationId = static_cast<uint16_t>(_model->formationId());
    stop.allMembers = allMembers;

    const FormationProtocol::CommandParams params = FormationProtocol::formStopParams(stop);
    vehicle->sendMavCommand(vehicle->defaultComponentId(),
                            static_cast<MAV_CMD>(FormationProtocol::kMavCmdFormStop),
                            true,
                            params.param1,
                            params.param2,
                            params.param3,
                            params.param4,
                            params.param5,
                            params.param6,
                            params.param7);
}

void FormationController::_onTimeout()
{
    if (_phase == Applying) {
        _setPhase(Active);
    } else if (_phase == Stopping) {
        _setPhase(Idle);
    }
}

void FormationController::_onVehicleText(int sysid,
                                         int componentid,
                                         int severity,
                                         const QString& text,
                                         const QString& description)
{
    Q_UNUSED(sysid)
    Q_UNUSED(componentid)
    Q_UNUSED(severity)
    Q_UNUSED(description)
    handleStatustext(text);
}

void FormationController::_setPhase(Phase phase)
{
    if (_phase == phase) {
        return;
    }
    _phase = phase;
    emit phaseChanged();
}

void FormationController::_setError(const QString& error)
{
    if (_errorString == error) {
        return;
    }
    _errorString = error;
    emit errorStringChanged();
}
