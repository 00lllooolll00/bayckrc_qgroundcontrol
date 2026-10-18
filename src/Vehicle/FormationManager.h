#pragma once

#include <QtCore/QList>
#include <QtCore/QMetaObject>
#include <QtCore/QObject>
#include <QtCore/QPointF>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtCore/QVariantList>
#include <QtPositioning/QGeoCoordinate>
#include <QtQmlIntegration/QtQmlIntegration>

class MissionItem;
class QmlObjectListModel;
class Vehicle;

Q_MOC_INCLUDE("QmlObjectListModel.h")

// 编队管理服务：队形模板 → 槽位 → 成员分配 → 串行任务下发（上传 LOITER 点后统一起飞）。
// v1 载体是标准 MAVLink mission（INAV 兼容通路）；v2 固件 FORM_* 命令协议见 FORMATION_DESIGN.md。
class FormationManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")
    Q_PROPERTY(int shapeIndex READ shapeIndex WRITE setShapeIndex NOTIFY shapeIndexChanged)
    Q_PROPERTY(double spacing READ spacing WRITE setSpacing NOTIFY spacingChanged)
    Q_PROPERTY(int slotCount READ slotCount WRITE setSlotCount NOTIFY slotCountChanged)
    Q_PROPERTY(QVariantList formationSlots READ formationSlots NOTIFY slotsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    static FormationManager* instance();

    /// 在线车辆中属于 MAPLRS 集群的 vehicleId 列表（MAV_AUTOPILOT_SMARTAP）
    Q_INVOKABLE QVariantList maplrsVehicleIds(QmlObjectListModel* vehicles) const;

    Q_INVOKABLE void assignSlot(int slotIndex, int vehicleId);
    Q_INVOKABLE void clearAssignment();
    /// 在线 MAPLRS 车辆按距槽位就近贪心分配（槽 0 = 领航机取已分配者或第一台）
    Q_INVOKABLE void autoAssign(QmlObjectListModel* vehicles);
    /// 串行下发：暂停空中机 → 逐台上传 mini-mission → 全部完成后逐台 startMission
    Q_INVOKABLE void dispatch(QmlObjectListModel* vehicles);

    /// 每槽位实时编队误差：[{slotIndex, vehicleId, errorM}]，vehicleId=0 或车已离线时 errorM=-1
    Q_INVOKABLE QVariantList formationStatus(QmlObjectListModel* vehicles) const;

    int shapeIndex() const { return _shapeIndex; }

    void setShapeIndex(int shapeIndex);

    double spacing() const { return _spacing; }

    void setSpacing(double spacing);

    int slotCount() const { return _slotCount; }

    void setSlotCount(int slotCount);

    QVariantList formationSlots() const { return _slots; }

    bool busy() const { return _busy; }

signals:
    void shapeIndexChanged();
    void spacingChanged();
    void slotCountChanged();
    void slotsChanged();
    void busyChanged();
    void dispatchFinished(bool success);

private:
    explicit FormationManager(QObject* parent = nullptr);

    struct SlotEntry
    {
        int vehicleId = 0;
        QPointF offset;  // x=东, y=北（米），未旋转
    };

    struct DispatchStep
    {
        QPointer<Vehicle> vehicle;
        QList<MissionItem*> items;
    };

    void _regenSlots();
    Vehicle* _findVehicle(QmlObjectListModel* vehicles, int vehicleId) const;
    QGeoCoordinate _slotCoordinate(const QGeoCoordinate& anchor, const QPointF& offset, double altRel) const;
    QList<MissionItem*> _makeItems(const QGeoCoordinate& coord) const;
    void _uploadNext();
    void _startAll(bool success);
    void _setBusy(bool busy);

    int _shapeIndex = 0;  // FormationShape::Shape 枚举序
    double _spacing = 10.0;
    int _slotCount = 2;
    QList<SlotEntry> _slotEntries;
    QVariantList _slots;

    bool _busy = false;
    QList<DispatchStep> _queue;
    QList<QPointer<Vehicle>> _uploaded;  // 上传成功、待统一起飞的跟随者
    QMetaObject::Connection _sendCompleteConn;
    QTimer _timeoutTimer;

    static constexpr int kUploadTimeoutMSecs = 30000;
};
