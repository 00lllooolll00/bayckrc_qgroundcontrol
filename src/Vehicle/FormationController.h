#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include <QtQmlIntegration/QtQmlIntegration>

#include "FormationModel.h"
#include "FormationProtocol.h"

class QmlObjectListModel;
class Vehicle;

// 编队执行层：状态机、逐机下发 config/member/stop、超时、状态回传聚合。
// 协议见 FORMATION_DESIGN.md；QML 通过 QGroundControl.formationController 访问。
class FormationController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

    Q_PROPERTY(FormationModel* model READ model CONSTANT)
    Q_PROPERTY(Phase phase READ phase NOTIFY phaseChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    enum Phase
    {
        Idle,
        Applying,
        Active,
        Stopping,
        Failed,
    };
    Q_ENUM(Phase)

    static FormationController* instance();

    FormationModel* model() const { return _model; }
    Phase phase() const { return _phase; }
    QString errorString() const { return _errorString; }

    /// 校验通过后逐机发 FORM_CONFIG；全部发完进入 Applying，靠状态回传转 Active。
    Q_INVOKABLE void apply(QmlObjectListModel* vehicles);
    /// 逐机或全队发 FORM_STOP。
    Q_INVOKABLE void stop(QmlObjectListModel* vehicles, bool allMembers);
    /// 点名脱离编队（FORM_MEMBER leave）。
    Q_INVOKABLE void kick(QmlObjectListModel* vehicles, int sysid);
    /// 让成员重新加入：先补发当前 config，再发 FORM_MEMBER join。
    Q_INVOKABLE void rejoin(QmlObjectListModel* vehicles, int sysid);
    /// 所有在线成员一起起飞。
    Q_INVOKABLE void takeoffAll(QmlObjectListModel* vehicles);
    /// 所有在线成员原地降落。
    Q_INVOKABLE void landAll(QmlObjectListModel* vehicles);
    /// 只给领机下 GUIDED 目标，跟随者靠实时相对位置跟随。
    Q_INVOKABLE void gotoLeader(QmlObjectListModel* vehicles, double latitude, double longitude, double altitude);
    /// 解析 GND 转来的 STATUSTEXT（FORM:<node>,<position>,<phase>,<leader_id>,<flags>,<formation_id>）。
    Q_INVOKABLE void handleStatustext(const QString& text);

signals:
    void phaseChanged();
    void errorStringChanged();

private:
    explicit FormationController(QObject* parent = nullptr);

    void _setPhase(Phase phase);
    void _setError(const QString& error);
    Vehicle* _findVehicle(QmlObjectListModel* vehicles, int sysid) const;
    void _sendConfig(Vehicle* vehicle, uint16_t formationId);
    void _sendMember(Vehicle* vehicle, uint16_t formationId, bool join);
    void _sendStop(Vehicle* vehicle, bool allMembers);
    void _onTimeout();
    void _onVehicleText(int sysid, int componentid, int severity, const QString& text, const QString& description);

    FormationModel* _model = nullptr;
    QTimer _timeoutTimer;
    QmlObjectListModel* _vehicles = nullptr;
    Phase _phase = Idle;
    QString _errorString;
    uint16_t _formationId = 0;
    int _pending = 0;
};
