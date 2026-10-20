#pragma once

#include <cstdint>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointF>
#include <QtCore/QStringList>
#include <QtCore/QVariantList>
#include <QtPositioning/QGeoCoordinate>
#include <QtQmlIntegration/QtQmlIntegration>

class QmlObjectListModel;
class Vehicle;

Q_MOC_INCLUDE("QmlObjectListModel.h")

// 编队管理服务：队形模板 → 槽位 → 成员分配 → 逐机 FORM_ASSIGN（USER_1）。
// 协议见 FORMATION_DESIGN.md；槽 0 为领航机，不接收命令，仅作为跟随者的锚点。
class FormationManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")
    Q_PROPERTY(int shapeIndex READ shapeIndex WRITE setShapeIndex NOTIFY shapeIndexChanged)
    Q_PROPERTY(double spacing READ spacing WRITE setSpacing NOTIFY spacingChanged)
    Q_PROPERTY(double headingDeg READ headingDeg WRITE setHeadingDeg NOTIFY headingDegChanged)
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
    /// 槽位数对齐在线 MAPLRS 数量（至少 1），不重排已有分配
    Q_INVOKABLE void matchOnline(QmlObjectListModel* vehicles);
    /// 已分配槽位的跟随者逐台单播 FORM_ASSIGN；领航机（槽 0）不收命令
    Q_INVOKABLE void applyFormation(QmlObjectListModel* vehicles);
    /// QML 旧入口的薄别名
    Q_INVOKABLE void dispatch(QmlObjectListModel* vehicles) { applyFormation(vehicles); }

    /// 命名预设（QSettings / Formation/Presets/<name>，JSON）
    Q_INVOKABLE QStringList presetNames() const;
    Q_INVOKABLE bool savePreset(const QString& name);
    Q_INVOKABLE bool loadPreset(const QString& name);
    Q_INVOKABLE bool deletePreset(const QString& name);

    /// 每槽位实时编队误差：[{slotIndex, vehicleId, errorM}]，vehicleId=0 或车已离线时 errorM=-1
    Q_INVOKABLE QVariantList formationStatus(QmlObjectListModel* vehicles) const;

    int shapeIndex() const { return _shapeIndex; }

    void setShapeIndex(int shapeIndex);

    double spacing() const { return _spacing; }

    void setSpacing(double spacing);

    double headingDeg() const { return _headingDeg; }

    void setHeadingDeg(double headingDeg);

    int slotCount() const { return _slotCount; }

    void setSlotCount(int slotCount);

    QVariantList formationSlots() const { return _slots; }

    bool busy() const { return _busy; }

signals:
    void shapeIndexChanged();
    void spacingChanged();
    void headingDegChanged();
    void slotCountChanged();
    void slotsChanged();
    void busyChanged();
    void dispatchFinished(bool success);

private:
    explicit FormationManager(QObject* parent = nullptr);

    struct SlotEntry
    {
        int vehicleId = 0;
        QPointF offset;  // x=东, y=北（米），已按 headingDeg 旋转
    };

    static QString _presetKey(const QString& name);
    void _regenSlots();
    Vehicle* _findVehicle(QmlObjectListModel* vehicles, int vehicleId) const;
    QGeoCoordinate _slotCoordinate(const QGeoCoordinate& anchor, const QPointF& offset, double alt) const;
    void _setBusy(bool busy);

    int _shapeIndex = 0;  // FormationShape::Shape 枚举序
    double _spacing = 10.0;
    double _headingDeg = 0.0;
    int _slotCount = 2;
    QList<SlotEntry> _slotEntries;
    QVariantList _slots;

    uint32_t _formationId = 0;

    bool _busy = false;
};
