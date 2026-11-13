#pragma once

#include "VehicleTestManualConnect.h"

// 编队模型行为测试：验证在线成员槽位变化通知、未分配成员不塌陷与领航机切换实时通知。
class FormationModelTest : public VehicleTestManualConnect
{
    Q_OBJECT

public:
    explicit FormationModelTest(QObject* parent = nullptr) : VehicleTestManualConnect(parent) {}

protected slots:
    void cleanup() override;

private slots:
    void _positionUpdateEmitsConfigChanged();
    void _unassignedFollowerKeepsSlotCount();
    void _leaderSwitchNotifiesLeaderChanged();

private:
    bool _startSmartApVehicle();
};
