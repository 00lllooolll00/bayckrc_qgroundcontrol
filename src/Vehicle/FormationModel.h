#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QVariantList>
#include <QtQmlIntegration/QtQmlIntegration>

class QmlObjectListModel;
class Vehicle;

// 动态编队模型：每行一个成员，另有一份全局队形配置。
// 位置由 shape + 活跃成员数在本地生成，成员按机上算法认领；QGC 只显示回传结果。
class FormationModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

public:
    enum Roles
    {
        SysidRole = Qt::UserRole + 1,
        RoleRole,        // "Leader" / "Follower"
        OnlineRole,
        PositionRole,    // 上报的位置索引；-1 未分配
        PhaseRole,
        LeaderIdRole,
        FlagsRole,
        BatteryRole,
    };
    Q_ENUM(Roles)

    explicit FormationModel(QObject *parent = nullptr);

    Q_PROPERTY(QVariantList positions READ positions NOTIFY configChanged)
    Q_PROPERTY(QVariantList sysids READ sysids NOTIFY configChanged)
    Q_PROPERTY(int shape READ shape WRITE setShape NOTIFY configChanged)
    Q_PROPERTY(double spacingM READ spacingM WRITE setSpacingM NOTIFY configChanged)
    Q_PROPERTY(double vAngleDeg READ vAngleDeg WRITE setVAngleDeg NOTIFY configChanged)
    Q_PROPERTY(int leaderOverride READ leaderOverride WRITE setLeader NOTIFY configChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY configChanged)
    Q_PROPERTY(int formationId READ formationId WRITE setFormationId NOTIFY configChanged)
    Q_PROPERTY(bool formationActive READ formationActive NOTIFY formationActiveChanged)

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void matchOnline(QmlObjectListModel *vehicles);
    Q_INVOKABLE void setActive(bool active);
    bool formationActive() const { return _active; }
    Q_INVOKABLE void markOffline(int sysid);
    /// 本机主动踢出：置 LEAVING 位并清掉位置/领机信息，避免行内残留旧状态。
    Q_INVOKABLE void markLeaving(int sysid);
    /// 本机主动重入：清 LEAVING 位，等机上回包重新分配位置。
    Q_INVOKABLE void markRejoining(int sysid);
    Q_INVOKABLE bool isOnline(int sysid) const;
    Q_INVOKABLE bool isLeaving(int sysid) const;
    Q_INVOKABLE void updateStatus(int sysid, int position, int phase, int leaderId, int flags, int formationId);
    Q_INVOKABLE bool validate() const;
    Q_INVOKABLE QString validationError() const { return _lastError; }
    Q_INVOKABLE int leaderSysid() const;
    Q_INVOKABLE int formationMemberCount() const;

    QVariantList positions() const;
    QVariantList sysids() const;
    int shape() const { return _shape; }
    double spacingM() const { return _spacingM; }
    double vAngleDeg() const { return _vAngleDeg; }
    int leaderOverride() const { return _leaderOverride; }
    bool enabled() const { return _enabled; }
    int formationId() const { return _formationId; }

    void setShape(int shape);
    void setSpacingM(double spacingM);
    void setVAngleDeg(double vAngleDeg);
    void setLeader(int sysid);
    void setEnabled(bool enabled);
    void setFormationId(int formationId);

signals:
    void configChanged();
    void formationActiveChanged();

private:
    struct Member
    {
        int sysid = 0;
        bool online = true;
        int position = -1;
        int phase = 0;
        int leaderId = 0;
        int flags = 0;
        int formationId = 0;
        double batteryPct = -1.0;
    };

    static bool _isMaplrs(Vehicle *vehicle);
    void _emitRowChanged(int row);
    int _activeMemberCount() const;
    int _rowForSysid(int sysid) const;

    QList<Member> _members;
    bool _active = false;
    mutable QString _lastError;

    int _shape = 0;
    double _spacingM = 10.0;
    double _vAngleDeg = 45.0;
    int _leaderOverride = 0;
    bool _enabled = true;
    int _formationId = 0;
};
